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
#include "card/state/session_registry.h"

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
    std::optional<AccountView> ReadAccount(SubscriptionId subscription) const;
    AssignmentView ReadAssignment() const;
    std::optional<AssignmentService::C0Receipt> ReadC0Receipt() const;
    // A disengaged result means the module rejected a new call before entering A.
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
    void ReturnPrimaryCall();

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
    std::shared_ptr<AssignmentService> assignment_;
    PrimaryHostLifecycle lifecycle_;
    std::mutex admissionMutex_;
    bool accepting_ {true};
    bool primaryCallActive_ {false};
    bool joining_ {false};
    bool drained_ {false};
    // A non-drained graph and its external ports stay alive until a subsequent accurate drain result.
    CoreSimModuleHandle closeHold_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H
