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
      topology_(*catalog_, *ports_.accountStore, epoch_),
      assignment_(std::make_shared<AssignmentService>(*catalog_, *ports_.assignmentStore, radio_,
          *ports_.output, topology_, *ports_.completion, epoch_, AssignmentService::PrimaryScope::C0Only)),
      lifecycle_(assignment_, radio_, *ports_.scheduler, ports_.ownerDispatch)
{
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

std::optional<AccountView> SimModule::ReadAccount(SubscriptionId subscription) const
{
    return topology_.ReadAccount(subscription);
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

std::optional<RequestId> SimModule::StartPrimarySwitch(const SwitchPrimaryCommand &command)
{
    const auto self = shared_from_this();
    {
        std::lock_guard<std::mutex> guard(admissionMutex_);
        if (!accepting_ || primaryCallActive_ || closeHold_) { return std::nullopt; }
        primaryCallActive_ = true;
    }
    const PrimaryCall call {self};
    return assignment_->StartPrimarySwitch(command);
}

void SimModule::ReturnPrimaryCall()
{
    const auto receipt = assignment_->ReadC0Receipt();
    std::lock_guard<std::mutex> guard(admissionMutex_);
    if (receipt) {
        closeHold_ = shared_from_this();
    }
    primaryCallActive_ = false;
}

SimModule::DrainResult SimModule::StopAcceptingAndDrain()
{
    const auto self = shared_from_this();
    {
        std::lock_guard<std::mutex> guard(admissionMutex_);
        accepting_ = false;
        if (drained_) { return DrainResult::Drained; }
        closeHold_ = self;
        if (primaryCallActive_ || joining_) { return DrainResult::CallbacksStillInFlight; }
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
    if (assignment_->ReadC0Receipt()) { return DrainResult::BusinessEffectsUnresolved; }
    drained_ = true;
    closeHold_.reset();
    return DrainResult::Drained;
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
