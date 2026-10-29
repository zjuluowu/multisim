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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_REQUESTS_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_REQUESTS_H

#include <optional>
#include <string>
#include <vector>
#include "application/orchestration/request_context.h"
#include "assignment/domain/role_selection.h"

namespace OHOS::Telephony::MultiSim {
struct SetRoleCommand { DefaultRole role; std::optional<SubscriptionId> subscription; SlotId legacySlot; };
struct SwitchPrimaryCommand {
    SlotTarget target;
    bool userSet;
    CompletionPlan completion;
    std::optional<StepRef> parent;
};
struct RadioAssignmentView {
    SlotId slot;
    int32_t modemId;
    uint64_t observationRevision;
}; // 当前Radio观察归A接纳，不放只读SlotCatalog。
struct AssignmentView {
    std::optional<SubscriptionId> defaultVoice;
    std::optional<SubscriptionId> defaultSms;
    std::optional<SlotId> lastPrimary;
    std::optional<SlotId> lastData;
    bool primarySwitchInProgress;
    uint64_t assignmentRevision;
    std::vector<RadioAssignmentView> radioAssignments; // C按原规则做第三卡投影。
};
struct AccountRolePlanningInput {
    std::optional<SubscriptionId> existingSubscription;
    SlotId discoveredSlot;
    bool useLegacySingleSlotRoles;
};
struct AccountRoleReservation { uint64_t token; uint64_t assignmentRevision; };
struct AccountRoleFragment {
    std::optional<SubscriptionId> subscription;
    AccountRoleReservation reservation;
    bool voice;
    bool sms;
    bool mainCardColumn;
    bool dataCardColumn;
}; // 新simId由原Insert返回；A资格覆盖建行I/O，不是只读快照后任意写。
struct StoredAccountRoleFragment {
    SubscriptionId subscription;
    bool voice;
    bool sms;
    bool mainCardColumn;
    bool dataCardColumn;
}; // 原DB读取载荷；不能以A当前态填回旧DB读取结果。
struct LegacyActivationWaitResult {
    bool primaryStillInProgress;
    bool boundedWaitExpired;
}; // 仅描述G2原有界等待，超时仍继续；不授予新执行租约。
struct ActivationConflictRequest { StepRef step; SessionRef sentSession; };
struct ActivationConflictLease {
    StepRef owner;
    SessionRef sentSession;
    RadioDomainId domain;
    uint64_t token;
    bool operator==(const ActivationConflictLease &other) const
    {
        return owner == other.owner && sentSession.epoch == other.sentSession.epoch &&
            sentSession.application.slot == other.sentSession.application.slot &&
            sentSession.application.applicationKey == other.sentSession.application.applicationKey &&
            sentSession.bindingGeneration == other.sentSession.bindingGeneration &&
            domain.key == other.domain.key && token == other.token;
    }
}; // A登记的应用冲突资格，不替代R真实设备租约。
struct ActivationConflictEndEvidence { EffectKnowledge effect; bool deviceStepEnded; };
inline bool MayReleaseActivationConflict(const ActivationConflictEndEvidence &evidence)
{
    return evidence.deviceStepEnded && evidence.effect != EffectKnowledge::Unknown;
}
struct InitialRoleFinalizationEvidence {
    bool callFinished;
    EffectKnowledge storageEffect;
    bool projectionsAcceptedOrReconciled;
};
inline bool MayFinalizeInitialRoles(const InitialRoleFinalizationEvidence &evidence)
{
    return evidence.callFinished && evidence.storageEffect != EffectKnowledge::Unknown &&
        evidence.projectionsAcceptedOrReconciled;
}
struct PrimaryRecheck { SlotId changedSlot; bool causedByActivation; };
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_REQUESTS_H
