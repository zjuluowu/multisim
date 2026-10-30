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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 尚未接入生产或构建，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H

#include "foundation/orchestration/runtime_operation_store.h"
#include "assignment/api/assignment_requests.h"
#include "assignment/domain/assignment_aggregate.h"
#include "assignment/account_roles/account_role_protocol.h"
#include "assignment/api/assignment_store.h"
#include "assignment/api/radio_execution_port.h"
#include "assignment/api/primary_topology_port.h"
#include "assignment/api/profile_preference_port.h"
#include "topology/api/account_role_planning_port.h"
#include "topology/api/activation_assignment_port.h"

namespace OHOS::Telephony::MultiSim {
// A唯一应用写入口；跨Owner只依消费方所需窄端口，不认识T具体Service。
class AssignmentService final : public RadioResultSink, public ProfilePreferencePort,
    public AccountRolePlanningPort, public ActivationAssignmentPort {
public:
    AssignmentService(const SlotCatalog &catalog, AssignmentStore &store, RadioExecutionPort &radio,
        AssignmentOutputPort &output, PrimaryTopologyPort &topology, LegacyIntCompletionPort &completion,
        RuntimeEpoch epoch);
    int32_t SetDefaultRole(const SetRoleCommand &command);
    RequestId StartPrimarySwitch(const SwitchPrimaryCommand &command);
    void AcceptRadioStep(const RadioSwitchResult &result) override;
    void AcceptLateRadioEvidence(const RadioSwitchResult &result) override;
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
    uint64_t ReadRoleProjectionRevision(bool historicalView) const override;
    bool AcceptReloadedRoleProjectionSet(const std::vector<StoredAccountRoleFragment> &loaded, uint64_t loadAttempt,
        uint64_t expectedAssignmentRevision, bool historicalView) override;
    bool AcceptReloadedRoleProjection(const StoredAccountRoleFragment &loaded, uint64_t loadAttempt,
        uint64_t expectedAssignmentRevision, bool historicalView) override;
private:
    const SlotCatalog &catalog_;
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
    uint64_t nextConflictToken_ {1};
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ASSIGNMENT_SERVICE_H
