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
#include <vector>
#include "application/orchestration/request_context.h"
#include "assignment/domain/role_selection.h"
#include "topology/api/account_lifecycle.h"

namespace OHOS::Telephony::MultiSim {
struct SetRoleCommand { DefaultRole role; std::optional<SubscriptionId> subscription; SlotId legacySlot; };
struct SwitchPrimaryCommand { SlotTarget target; bool userSet; CompletionPlan completion; };
struct AssignmentView {
    std::optional<SubscriptionId> defaultVoice;
    std::optional<SubscriptionId> defaultSms;
    std::optional<SlotId> lastPrimary;
    std::optional<SlotId> lastData;
    bool primarySwitchInProgress;
    uint64_t assignmentRevision;
};
struct AccountRoleFragment {
    std::optional<SubscriptionId> subscription; // 新行尚无simId。
    uint64_t assignmentRevision;
    uint64_t rolePlanId; // 发起发现的observationSequence；不跨运行实例复用。
    bool includeInStorage; // 新行或旧单卡恢复写初始角色；多卡旧卡rebind保留角色。
    bool voice;
    bool sms;
    bool mainCardColumn;
    bool dataCardColumn;
}; // A 所有，T 建行/重载仅组合只读片段；同名 DB列不替代 lastPrimary/lastData。
struct RoleReloadTicket { AccountReadView view; uint64_t attempt; uint64_t assignmentRevision; };
enum class RoleFragmentAcceptance { NoEffect, ReadyForOwnerReload, NeedsReconciliation };
struct ActivationAdmission { bool mayProceed; bool legacyBoundedWaitExpired; };
struct PrimaryRecheck { SlotId changedSlot; bool causedByActivation; };
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_REQUESTS_H
