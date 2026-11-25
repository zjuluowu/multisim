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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.2、§7.1、§14.2～14.3、§22.1。
// M1 core assembly; Legacy attachment and product startup remain separate.

#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H

#include "infrastructure/assembly/sim_module_factory.h"
#include "infrastructure/assembly/primary_host_lifecycle.h"
#include "account/topology_service.h"
#include "infrastructure/compatibility/events/legacy_completion.h"
#include "card/runtime/card_observation_runtime.h"

namespace OHOS::Telephony::MultiSim {
// Startup adapters are dedicated to this instance, except for the borrowed platform service.
struct CoreSimModulePorts {
    std::shared_ptr<AccountStore> accountStore;
    std::shared_ptr<AssignmentStore> assignmentStore;
    std::shared_ptr<AssignmentOutputPort> output;
    std::shared_ptr<LegacyIntCompletionPort> completion;
    std::shared_ptr<RadioPlatformPort> radio;
    std::shared_ptr<RetrySchedulerPort> scheduler;
    std::shared_ptr<RadioOwnerDispatchPort> ownerDispatch;
    std::function<std::optional<InitialRecoveryEvidence>(const ResourceKey &, RuntimeEpoch)> initialEvidence;
    std::shared_ptr<LegacyPrimaryHostPort> primaryHost {};
};

// Account and card facts confirmed by the startup source. M2 adds live observation and provider writes.
struct ConfirmedSlotAccount {
    CardApplicationTarget application;
    uint64_t generation;
    AccountRecord account;
    AccountDiscoveryStoreResult stored;
};

class SimModule final : public std::enable_shared_from_this<SimModule> {
public:
    enum class DrainResult { Drained, CallbacksStillInFlight, DeviceEffectsUnresolved, BusinessEffectsUnresolved };
    RuntimeEpoch Epoch() const { return epoch_; }
    const SlotCatalog &Catalog() const { return *catalog_; }
    std::optional<BindingRef> FindBinding(SlotId slot) const;
    std::optional<AccountView> ReadAccount(SubscriptionId subscription,
        AccountReadView view = AccountReadView::Current) const;
    int32_t SetShowName(int32_t slot, const std::u16string &name, LegacyCallerRef caller);
    int32_t GetShowName(int32_t slot, std::u16string &name, LegacyCallerRef caller);
    int32_t SetSimLabelIndexByIccId(const std::string &iccid, int32_t label);
    bool ReconcileAccountMetadata(const AccountMetadataOperation &operation);
    AssignmentView ReadAssignment() const;
    std::optional<SessionRef> ObserveVerifiedCard(const CardApplicationTarget &application,
        uint64_t generation, const std::string &iccid);
    bool InvalidateVerifiedCard(const SessionRef &session);
    bool ReconcileDiscovery(DiscoveryCallRef call);
    std::optional<TopologyService::DiscoveryReceipt> ReadDiscoveryReceipt(DiscoveryCallRef call) const;
    size_t PendingDiscoveryRoles() const;
    std::optional<ConfirmedDiscoveryRoles> ReadConfirmedDiscoveryRoles(SubscriptionId id) const;
    std::optional<AssignmentService::C0Receipt> ReadC0Receipt() const;
    // A disengaged result means the module rejected a new call before entering A.
    bool ReconcilePrimaryParameter(const C0ParameterOperation &operation);
    int32_t SetDefaultVoiceSlotId(int32_t slot, LegacyCallerRef caller);
    int32_t SetDefaultSmsSlotId(int32_t slot, LegacyCallerRef caller);
    std::optional<RoleProviderEvidence> ReadRoleProviderReceipt(DefaultRole role) const;
    bool ReconcileRoleProvider(const RoleProviderOperation &operation);
    int32_t SetDefaultCellularDataSlotId(int32_t slot, LegacyCallerRef caller);
    std::optional<C0ParameterEvidence> ReadDataParameterReceipt() const;
    bool ReconcileDataParameter(const C0ParameterOperation &operation);
    int32_t SetPrimarySlotId(int32_t slot, LegacyCallerRef caller);
    std::optional<RequestId> StartPrimarySwitch(const SwitchPrimaryCommand &command);
    DrainResult StopAcceptingAndDrain();
    std::optional<ResourceState> ReadPrimaryResourceState() const;
private:
    friend CoreSimModuleCreationResult CreateCoreSimModule(CoreSimModulePorts ports,
        std::shared_ptr<const SlotCatalog> catalog, const std::vector<ConfirmedSlotAccount> &accounts);
    friend struct SimModuleDeleter;
    SimModule(CoreSimModulePorts ports, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch);
    ~SimModule() = default;
    bool InitializeResources();
    bool SeedAccount(const ConfirmedSlotAccount &confirmed);
    bool AdmitPrimaryCall(bool originalOnly = false);
    void ReturnPrimaryCall();
    bool AdmitModuleCall(bool originalOnly = false);
    void ReturnModuleCall();
    bool HasPendingBusinessEffects() const;
    int32_t SetDefaultCallMessageRole(DefaultRole role, int32_t slot, LegacyCallerRef caller);

    struct ModuleCall {
        CoreSimModuleHandle module;
        ~ModuleCall() { module->ReturnModuleCall(); }
    };

    struct PrimaryCall {
        CoreSimModuleHandle module;
        ~PrimaryCall() { module->ReturnPrimaryCall(); }
    };

    CoreSimModulePorts ports_;
    const std::shared_ptr<const SlotCatalog> catalog_;
    const RuntimeEpoch epoch_;
    const ResourceKey primaryResource_ {"primary-shared"};
    ResourceBarrier resources_;
    ResourceRadioRuntime radio_;
    SessionRegistry sessions_;
    TopologyService topology_;
    CardObservationRuntime card_;
    LegacyCompletion completion_;
    std::shared_ptr<AssignmentService> assignment_;
    PrimaryHostLifecycle lifecycle_;
    std::mutex admissionMutex_;
    bool accepting_ {true};
    size_t moduleCalls_ {0};
    bool primaryEffectUnresolved_ {false};
    bool primaryCallActive_ {false};
    bool joining_ {false};
    bool drained_ {false};
    // A non-drained graph and its external ports stay alive until a subsequent accurate drain result.
    CoreSimModuleHandle closeHold_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H
