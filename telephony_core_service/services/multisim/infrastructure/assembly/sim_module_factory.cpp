/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "infrastructure/assembly/sim_module.h"
#include "infrastructure/compatibility/api/legacy_sim_facade.h"

#include <atomic>
#include <limits>
#include <stdexcept>
#include <set>
#include <utility>

namespace OHOS::Telephony::MultiSim {
namespace {
std::optional<RuntimeEpoch> AllocateEpoch()
{
    // A process-local generation; cross-process recovery must establish its own initial evidence.
    static std::atomic<uint64_t> next {1};
    auto current = next.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<uint64_t>::max()) {
        if (next.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) {
            return RuntimeEpoch {0, current};
        }
    }
    return std::nullopt;
}

bool ValidCatalog(const SlotCatalog &catalog)
{
    std::set<SlotId> slots;
    for (const auto &slot : catalog.Slots()) {
        if (slot.slot.legacySlotId < 0 || !slots.insert(slot.slot).second) {
            return false;
        }
    }
    return !slots.empty();
}
} // namespace

SimModule::SimModule(CoreSimModulePorts ports, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch)
    : ports_(std::move(ports)), catalog_(std::move(catalog)), epoch_(epoch), resources_(epoch_),
      radio_(resources_, *ports_.radio, {primaryResource_}, ResourceRadioRuntime::Mode::ObserveOnly),
      sessions_(epoch_),
      topology_(*catalog_, *ports_.accountStore, epoch_), card_(*catalog_, sessions_, topology_, epoch_),
      completion_(*ports_.completion, ports_.primaryHost.get()),
      assignment_(std::make_shared<AssignmentService>(*catalog_, *ports_.assignmentStore, radio_,
          *ports_.output, topology_, completion_, epoch_, AssignmentService::PrimaryScope::C0Only)),
      lifecycle_(assignment_, radio_, *ports_.scheduler, ports_.ownerDispatch)
{
    for (const auto &slot : catalog_->Slots()) {
        (void)topology_.AcceptSlotAssembly(slot.slot, true);
    }
    topology_.BindRolePlanner(*assignment_);
    assignment_->BindRetryScheduler(ports_.scheduler);
    radio_.BindResultSink(assignment_);
    radio_.BindOwnerDispatch(ports_.ownerDispatch);
}

bool SimModule::InitializeResources()
{
    if (!resources_.RegisterForInitialCheck(primaryResource_)) { return false; }
    auto provider = std::move(ports_.initialEvidence);
    const auto evidence = provider(primaryResource_, epoch_);
    return evidence && resources_.AcceptInitialRecoveryEvidence(primaryResource_, *evidence);
}

bool SimModule::SeedAccount(const ConfirmedSlotAccount &confirmed)
{
    const auto slot = confirmed.application.slot;
    if (!catalog_->Find(slot) || topology_.FindBinding(slot) || confirmed.application.applicationKey.empty()) {
        return false;
    }
    const SessionRef session {epoch_, confirmed.application, confirmed.generation};
    return sessions_.PublishVerifiedSession(session) && topology_.AcceptSlotAssembly(slot, true) &&
        topology_.AcceptStoredDiscovery({session, 1, confirmed.account.iccid}, confirmed.account, confirmed.stored);
}

std::optional<BindingRef> SimModule::FindBinding(SlotId slot) const
{
    return topology_.FindBinding(slot);
}

std::optional<AccountView> SimModule::ReadAccount(SubscriptionId subscription, AccountReadView view) const
{
    return topology_.ReadAccount(subscription, view);
}

int32_t SimModule::SetShowName(int32_t slot, const std::u16string &name, LegacyCallerRef caller)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitModuleCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const ModuleCall call {shared_from_this()};
    return LegacySimFacade::SetShowName({slot}, caller, *catalog_, *ports_.primaryHost, completion_,
        [this](SlotId target) { return card_.HasVerifiedCard(target); },
        [&](const CompletionPlan &plan) { return topology_.SetDisplayName({{slot}}, name, plan); });
}

int32_t SimModule::GetShowName(int32_t slot, std::u16string &name, LegacyCallerRef caller)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitModuleCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const ModuleCall call {shared_from_this()};
    return LegacySimFacade::GetShowName({slot}, caller, *catalog_, *ports_.primaryHost, name,
        [this](SlotId target) { return card_.HasVerifiedCard(target); },
        [&] { return topology_.ReadAccountAtSlot({{slot}}); });
}

int32_t SimModule::SetSimLabelIndexByIccId(const std::string &iccid, int32_t label)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitModuleCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const ModuleCall call {shared_from_this()};
    return LegacySimFacade::SetSimLabelIndexByIccId(*ports_.primaryHost, completion_,
        [&](const CompletionPlan &plan) { return topology_.SetSimLabel(iccid, label, plan); });
}

bool SimModule::ReconcileAccountMetadata(const AccountMetadataOperation &operation)
{
    if (!AdmitModuleCall(true)) { return false; }
    const ModuleCall call {shared_from_this()};
    return topology_.ReconcileMetadata(operation);
}

AssignmentView SimModule::ReadAssignment() const
{
    return assignment_->ReadLegacyAssignment();
}

std::optional<AssignmentService::C0Receipt> SimModule::ReadC0Receipt() const
{
    return assignment_->ReadC0Receipt();
}

std::optional<ResourceState> SimModule::ReadPrimaryResourceState() const
{
    return resources_.ReadState(primaryResource_);
}

bool SimModule::AdmitPrimaryCall(bool originalOnly)
{
    std::lock_guard<std::mutex> guard(admissionMutex_);
    if (drained_ || joining_ || primaryCallActive_) { return false; }
    if (originalOnly ? !primaryEffectUnresolved_ : (!accepting_ || primaryEffectUnresolved_)) { return false; }
    primaryCallActive_ = true;
    return true;
}

int32_t SimModule::SetPrimarySlotId(int32_t slot, LegacyCallerRef caller)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitPrimaryCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const PrimaryCall call {shared_from_this()};
    return LegacySimFacade::SetPrimarySlotId({slot}, caller, *catalog_, *ports_.primaryHost, topology_, completion_,
        [this](SlotId target) { return card_.HasVerifiedCard(target); },
        [this](const SwitchPrimaryCommand &command) { (void)StartLegacyPrimarySwitch(*assignment_, command); });
}

int32_t SimModule::SetDefaultCellularDataSlotId(int32_t slot, LegacyCallerRef caller)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitModuleCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const ModuleCall call {shared_from_this()};
    return LegacySimFacade::SetDefaultCellularDataSlotId({slot}, caller, *catalog_, *ports_.primaryHost, topology_,
        completion_, *assignment_, [this](SlotId target) { return card_.HasVerifiedCard(target); });
}

int32_t SimModule::SetDefaultVoiceSlotId(int32_t slot, LegacyCallerRef caller)
{
    return SetDefaultCallMessageRole(DefaultRole::Voice, slot, caller);
}

int32_t SimModule::SetDefaultSmsSlotId(int32_t slot, LegacyCallerRef caller)
{
    return SetDefaultCallMessageRole(DefaultRole::Sms, slot, caller);
}

int32_t SimModule::SetDefaultCallMessageRole(DefaultRole role, int32_t slot, LegacyCallerRef caller)
{
    if (!ports_.primaryHost) { return -1; }
    if (!AdmitModuleCall()) { return ports_.primaryHost->PrimaryError(LegacyPrimaryError::Unavailable); }
    const ModuleCall call {shared_from_this()};
    return LegacySimFacade::SetDefaultCallMessageRole(role, {slot}, caller, *catalog_, *ports_.primaryHost,
        topology_, completion_, *assignment_, [this](SlotId target) { return card_.HasVerifiedCard(target); });
}

std::optional<RoleProviderEvidence> SimModule::ReadRoleProviderReceipt(DefaultRole role) const
{
    return assignment_->ReadRoleProviderReceipt(role);
}

bool SimModule::ReconcileRoleProvider(const RoleProviderOperation &operation)
{
    if (!AdmitModuleCall(true)) { return false; }
    const ModuleCall call {shared_from_this()};
    return assignment_->ReconcileRoleProvider(operation);
}

std::optional<C0ParameterEvidence> SimModule::ReadDataParameterReceipt() const
{
    return assignment_->ReadDataParameterReceipt();
}

bool SimModule::ReconcileDataParameter(const C0ParameterOperation &operation)
{
    if (!AdmitModuleCall(true)) { return false; }
    const ModuleCall call {shared_from_this()};
    const auto receipt = assignment_->ReadDataParameterReceipt();
    return receipt && receipt->operation == operation && assignment_->ReconcileC0Parameter(operation);
}

std::optional<RequestId> SimModule::StartPrimarySwitch(const SwitchPrimaryCommand &command)
{
    if (!AdmitPrimaryCall()) { return std::nullopt; }
    const PrimaryCall call {shared_from_this()};
    return assignment_->StartPrimarySwitch(command);
}

bool SimModule::ReconcilePrimaryParameter(const C0ParameterOperation &operation)
{
    if (!AdmitPrimaryCall(true)) { return false; }
    const PrimaryCall call {shared_from_this()};
    return assignment_->ReconcileC0Parameter(operation);
}

void SimModule::ReturnPrimaryCall()
{
    const auto receipt = assignment_->ReadC0Receipt();
    std::lock_guard<std::mutex> guard(admissionMutex_);
    primaryEffectUnresolved_ = receipt.has_value();
    if (HasPendingBusinessEffects()) {
        closeHold_ = shared_from_this();
    } else if (accepting_) {
        closeHold_.reset();
    }
    primaryCallActive_ = false;
}

SimModule::DrainResult SimModule::StopAcceptingAndDrain()
{
    const auto self = shared_from_this();
    {
        std::lock_guard<std::mutex> guard(admissionMutex_);
        accepting_ = false;
        topology_.SealDiscovery();
        if (drained_) { return DrainResult::Drained; }
        closeHold_ = self;
        if (primaryCallActive_ || moduleCalls_ || joining_) { return DrainResult::CallbacksStillInFlight; }
        joining_ = true;
    }

    PrimaryHostLifecycle::DrainResult primary;
    try {
        primary = lifecycle_.StopAcceptingAndDrain();
    } catch (const std::exception &) {
        std::lock_guard<std::mutex> guard(admissionMutex_);
        joining_ = false;
        return DrainResult::CallbacksStillInFlight;
    }
    std::lock_guard<std::mutex> guard(admissionMutex_);
    joining_ = false;
    if (primary == PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight) {
        return DrainResult::CallbacksStillInFlight;
    }
    if (primary == PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved) {
        return DrainResult::DeviceEffectsUnresolved;
    }
    if (primaryCallActive_ || moduleCalls_) { return DrainResult::CallbacksStillInFlight; }
    if (HasPendingBusinessEffects()) {
        return DrainResult::BusinessEffectsUnresolved;
    }
    drained_ = true;
    closeHold_.reset();
    return DrainResult::Drained;
}

bool SimModule::AdmitModuleCall(bool originalOnly)
{
    std::lock_guard<std::mutex> guard(admissionMutex_);
    if (drained_ || (!originalOnly && !accepting_)) { return false; }
    ++moduleCalls_;
    return true;
}

void SimModule::ReturnModuleCall()
{
    std::lock_guard<std::mutex> guard(admissionMutex_);
    --moduleCalls_;
    if (HasPendingBusinessEffects()) {
        closeHold_ = shared_from_this();
    } else if (accepting_) {
        closeHold_.reset();
    }
}

bool SimModule::HasPendingBusinessEffects() const
{
    return primaryEffectUnresolved_ || assignment_->ReadDataParameterReceipt() ||
        assignment_->ReadRoleProviderReceipt(DefaultRole::Voice) || assignment_->ReadRoleProviderReceipt(DefaultRole::Sms) ||
        topology_.PendingDiscoveryCount() || topology_.PendingMetadataCount() || assignment_->PendingDiscoveryRoles();
}

std::optional<SessionRef> SimModule::ObserveVerifiedCard(const CardApplicationTarget &application,
    uint64_t generation, const std::string &iccid)
{
    const auto self = shared_from_this();
    if (!AdmitModuleCall()) { return std::nullopt; }
    const ModuleCall call {self};
    return card_.ObserveVerifiedCard(application, generation, iccid);
}

bool SimModule::InvalidateVerifiedCard(const SessionRef &session)
{
    const auto self = shared_from_this();
    if (!AdmitModuleCall(true)) { return false; }
    const ModuleCall call {self};
    return card_.InvalidateVerifiedCard(session);
}

bool SimModule::ReconcileDiscovery(DiscoveryCallRef original)
{
    const auto self = shared_from_this();
    if (!AdmitModuleCall(true)) { return false; }
    const ModuleCall call {self};
    return topology_.ReconcileDiscovery(original);
}

std::optional<TopologyService::DiscoveryReceipt> SimModule::ReadDiscoveryReceipt(DiscoveryCallRef call) const
{
    return topology_.ReadDiscoveryReceipt(call);
}

std::optional<ConfirmedDiscoveryRoles> SimModule::ReadConfirmedDiscoveryRoles(SubscriptionId id) const
{
    return assignment_->ReadConfirmedDiscoveryRoles(id);
}

size_t SimModule::PendingDiscoveryRoles() const
{
    return assignment_->PendingDiscoveryRoles();
}

void SimModuleDeleter::operator()(SimModule *module) const
{
    delete module;
}

CoreSimModuleCreationResult CreateCoreSimModule(CoreSimModulePorts ports,
    std::shared_ptr<const SlotCatalog> catalog, const std::vector<ConfirmedSlotAccount> &accounts)
{
    using Stage = SimModuleInitFailure::Stage;
    Stage stage = Stage::ValidateCatalog;
    try {
        if (!catalog || !ValidCatalog(*catalog)) {
            return SimModuleInitFailure {stage, -1};
        }
        stage = Stage::BindHostPorts;
        if (!ports.accountStore || !ports.assignmentStore || !ports.output || !ports.completion ||
            !ports.radio || !ports.scheduler || !ports.ownerDispatch || !ports.initialEvidence) {
            return SimModuleInitFailure {stage, -1};
        }
        stage = Stage::AllocateCore;
        const auto epoch = AllocateEpoch();
        if (!epoch) { return SimModuleInitFailure {stage, -1}; }
        CoreSimModuleHandle module(new SimModule(std::move(ports), std::move(catalog), *epoch), SimModuleDeleter {});
        stage = Stage::InitializeResources;
        if (!module->InitializeResources()) { return SimModuleInitFailure {stage, -1}; }
        stage = Stage::SeedAccount;
        for (const auto &account : accounts) {
            if (!module->SeedAccount(account)) { return SimModuleInitFailure {stage, -1}; }
        }
        return module;
    } catch (const std::exception &) {
        return SimModuleInitFailure {stage, -1};
    }
}
} // namespace OHOS::Telephony::MultiSim
