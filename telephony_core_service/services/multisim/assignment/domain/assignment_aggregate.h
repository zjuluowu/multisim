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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H

#include <map>
#include <optional>
#include "foundation/slot_catalog.h"
#include "assignment/domain/role_selection.h"

namespace OHOS::Telephony::MultiSim {
enum class TransitionPhase { Admitted, ParametersWritten, DevicePending, Reconciling, Completed, Failed };
struct AssignmentTransition {
    RadioDomainId domain;
    SlotId target;
    uint64_t transitionToken; // A本地资格，旧请求不能结束新切换。
    TransitionPhase phase;
    uint32_t businessAttempt;
};
enum class TransitionAdmission { Admitted, DomainBusy, UnknownResourceDomain };
class AssignmentAggregate final {
public:
    bool AcceptRoleSelection(const RoleSelection &selection, uint64_t expectedAssignmentRevision);
    std::optional<RoleSelection> ReadRoleSelection(DefaultRole role) const;
    TransitionAdmission AdmitTransition(const SlotDescriptor &target, uint64_t transitionToken);
    bool AcceptTransitionPhase(const RadioDomainId &domain, uint64_t transitionToken,
        TransitionPhase expected, TransitionPhase next);
    bool IsSwitchInProgress(const RadioDomainId &domain) const;
    void AcceptPrimaryParameterProjection(SlotId lastPrimary);
    void AcceptDataParameterProjection(SlotId lastData);
private:
    std::map<DefaultRole, RoleSelection> roles_; // 全局角色约束域不随Modem数量复制。
    uint64_t revision_ {0};
    std::map<RadioDomainId, AssignmentTransition> transitions_;
    std::optional<SlotId> lastPrimary_;
    std::optional<SlotId> lastData_;
    // current/all角色列按各自旧节点接纳，重载版本分别保存，不交T整行覆盖。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H
