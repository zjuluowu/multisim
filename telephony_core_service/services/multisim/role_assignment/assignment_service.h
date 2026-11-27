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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H

#include "infrastructure/request/runtime_operation_store.h"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include "role_assignment/assignment_requests.h"
#include "role_assignment/assignment_aggregate.h"
#include "role_assignment/account_role_protocol.h"
#include "role_assignment/assignment_store.h"
#include "role_assignment/primary/radio_execution_port.h"
#include "role_assignment/primary/primary_topology_port.h"
#include "role_assignment/profile_preference_port.h"
#include "account/account_role_planning_port.h"
#include "account/activation/activation_assignment_port.h"

namespace OHOS::Telephony::MultiSim {
// A唯一应用写入口；跨Owner只依消费方所需窄端口，不认识T具体Service。
class AssignmentService final : public RadioResultSink, public ProfilePreferencePort,
    public AccountRolePlanningPort, public ActivationAssignmentPort, public ScheduledRetrySink,
    public std::enable_shared_from_this<AssignmentService> {
public:
    enum class PrimaryScope { ExistingBranches, C0Only, TrackedDedicated };
    enum class C0Stage { Preference, PrimaryParameters, MainNotification, DataParameters, DataNotification, Completion };
    struct C0Receipt {
        RequestId request;
        C0Stage outstanding;
        BindingRef binding;
        std::array<std::optional<C0ParameterEvidence>, static_cast<size_t>(C0ParameterKey::Count)> writes {};
        bool businessSucceeded {false};
        uint32_t writeSet {kPrimaryRoleWriteSet};
    };

    AssignmentService(const SlotCatalog &catalog, AssignmentStore &store, RadioExecutionPort &radio,
        AssignmentOutputPort &output, PrimaryTopologyPort &topology, LegacyIntCompletionPort &completion,
        RuntimeEpoch epoch, PrimaryScope primaryScope = PrimaryScope::ExistingBranches);
    int32_t SetDefaultRole(const SetRoleCommand &command);
    void SetDefaultRole(const SetRoleCommand &command, const BindingRef &binding, const CompletionPlan &completion);
    void SetDefaultRole(const SetRoleCommand &command, const std::optional<BindingRef> &binding,
        const CompletionPlan &completion);
    std::optional<RoleProviderEvidence> ReadRoleProviderReceipt(DefaultRole role) const;
    bool ReconcileRoleProvider(const RoleProviderOperation &operation);
    std::optional<C0ParameterEvidence> ReadDataParameterReceipt() const;
    RequestId StartPrimarySwitch(const SwitchPrimaryCommand &command);
    void AcceptRadioStep(const RadioSwitchResult &result) override;
    void AcceptLateRadioEvidence(const RadioSwitchResult &result) override;
    std::optional<EffectKnowledge> ReadPrimaryDeviceEvidence(const StepRef &original) const;
    bool AcceptPrimarySwitchTimeout(RequestId request, uint64_t elapsedMilliseconds);
    bool AcceptC1RetryDue(RequestId request);
    void BindRetryScheduler(const std::shared_ptr<RetrySchedulerPort> &scheduler);
    bool AcceptScheduledC1Retry(const ScheduledRetryEvent &event) override;
    void RequestStopAccepting();
    bool StopAcceptingAndCancelScheduledRetries();
    bool IsStoppingPrimaryRequests() const;
    bool HasPendingDeviceEffects() const;
    std::optional<C0Receipt> ReadC0Receipt() const;
    bool ReconcileC0Parameter(const C0ParameterOperation &operation);
    void RetryPrimaryAfterReconciliation(RequestId request);
    AssignmentView ReadLegacyAssignment() const;
    LegacyActivationWaitResult WaitForLegacyPrimaryBeforeActivation(SlotId slot) override;
    std::optional<ActivationConflictLease> AdmitActivationBeforeDevice(
        const ActivationConflictRequest &request) override;
    bool ReleaseActivationConflict(const ActivationConflictLease &lease,
        const ActivationConflictEndEvidence &evidence) override;
    bool TransferActivationConflictToReconciliation(const ActivationConflictLease &lease,
        EffectKnowledge effect) override;
    void AcceptEntryPrimaryPreference(bool userSet) override;
    void RecheckPrimaryAfterAccountChange(const PrimaryRecheck &cause) override;
    void AcceptLocalProfilePreference(const StepRef &source, SlotId slot) override;
    std::optional<AccountRoleFragment> PrepareAccountRoleFragment(const AccountRolePlanningInput &input) override;
    bool AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned) override;
    bool StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const override;
    bool FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
        const InitialRoleFinalizationEvidence &evidence) override;
    void ConfirmInitialRoleReceipt(const AccountRoleFragment &planned,
        const InitialRoleFinalizationEvidence &evidence) override;
    size_t PendingDiscoveryRoles() const;
    std::optional<ConfirmedDiscoveryRoles> ReadConfirmedDiscoveryRoles(SubscriptionId id) const;
    uint64_t ReadRoleProjectionRevision(bool historicalView) const override;
    std::optional<StoredAccountRoleFragment> ReadStoredAccountRoles(SubscriptionId id, bool historical) const;
    void AcceptReloadedRoleProjectionSet(const RoleReloadDelivery &delivery) override;
private:
    mutable std::recursive_mutex primaryCommitLock_;
    void CompleteOnce(RequestId request, const CompletionPlan &plan, int32_t result,
        TerminalState terminal, std::unique_lock<std::recursive_mutex> *commit = nullptr);
    bool WriteTrackedParameter(C0ParameterKey key, StepRef &step, const PrimaryTargetFacts &facts, bool userSet,
        std::unique_lock<std::recursive_mutex> &guard);
    void RunTrackedPrimaryParameters(StepRef step, TransitionPhase phase, const SwitchPrimaryCommand &command,
        const SlotDescriptor &target, const PrimaryTargetFacts &facts, std::unique_lock<std::recursive_mutex> &guard);
    bool ScheduleC1Retry(RequestId request, uint32_t attempt);
    struct RoleProviderReceipt {
        RoleProviderEvidence evidence;
        uint32_t writeSet;
        bool reconcilable {false};
    };
    std::array<std::optional<RoleProviderReceipt>, 2> providerReceipts_;
    const bool supportsRoleProvider_;
    const bool independentRoleTransactions_;
    const SlotCatalog &catalog_;
    const PrimaryScope primaryScope_;
    const RuntimeEpoch reloadEpoch_;
    std::optional<C0Receipt> c0Receipt_;
    AssignmentAggregate assignment_;
    RuntimeOperationStore operations_;
    AssignmentStore &store_;
    RadioExecutionPort &radio_;
    AssignmentOutputPort &output_;
    PrimaryTopologyPort &topology_;
    LegacyIntCompletionPort &completion_;
    AccountRoleProtocol accountRoles_; // 原角色资格/两投影唯一状态，不与另一套角色缓存并存。
    enum class ConflictKind { Primary, Activation };
    struct AssignmentConflictRegistration {
        StepRef owner;
        ConflictKind kind;
        std::optional<SessionRef> sentSession;
        std::vector<RadioDomainId> radioDomains;
        bool reservesPrimaryParameterDomain;
        bool reconciling;
    };
    // Primary与Activation在这一个A准入登记处核验完整冲突集合并一次预留，不先后持域嵌套等。
    // Release/Transfer核验epoch/owner/request/step、domain和token；旧完成不清新登记。
    std::map<uint64_t, AssignmentConflictRegistration> assignmentConflicts_;
    struct PendingPrimarySwitch {
        StepRef step;
        PrimaryTargetFacts facts;
        SlotDescriptor target;
        std::string iccid;
        bool userSet;
        CompletionPlan completion;
        PrimarySwitchMode mode;
        uint32_t phase {0};
        uint32_t protocolSession {0};
        uint32_t targetMember;
        std::vector<RadioProtocolMember> members;
        std::vector<SlotId> completedMembers;
        PrimaryParameterSnapshot oldParameters;
        bool roundFailed {false};
        uint32_t attemptCount {0};
        bool completionDelivered {false};
        bool retryReady {false};
        bool retryScheduled {false};
        uint64_t retryScheduleGeneration {0};
        std::optional<uint64_t> retrySchedulerTask;
        std::optional<EffectKnowledge> lateDeviceEffect;
        bool timeoutDeliveryInFlight {false};
        bool noDeviceEffect {false};
    };
    bool AcceptC1RetryDue(RequestId request, std::unique_lock<std::recursive_mutex> &guard);
    RadioSubmitDisposition SubmitRadioOutsideCommit(const RadioSwitchRequest &device, const PrimaryTargetFacts &facts,
        std::unique_lock<std::recursive_mutex> &guard);
    void FinishPrimaryWithNoDeviceEffect(const StepRef &step);
    bool SubmitC1Round(const PendingPrimarySwitch &round, std::unique_lock<std::recursive_mutex> &guard);
    std::map<uint64_t, PendingPrimarySwitch> pendingPrimary_;
    std::weak_ptr<RetrySchedulerPort> retryScheduler_;
    bool retrySchedulerBound_ {false};
    std::atomic<bool> stopping_ {false};
    uint64_t nextRetryScheduleGeneration_ {1};
    uint32_t nextProtocolSession_ {1};
    uint64_t nextConflictToken_ {1};
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H
