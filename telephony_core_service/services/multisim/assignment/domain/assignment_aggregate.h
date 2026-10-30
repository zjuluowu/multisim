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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2～4.3、§9.3、§10.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H

#include <map>
#include <optional>
#include "model/slot_catalog.h"
#include "assignment/domain/role_selection.h"

namespace OHOS::Telephony::MultiSim {
enum class TransitionPhase { Admitted, ParametersWritten, DevicePending, Reconciling, Completed, Failed };
struct AssignmentTransition {
    RadioDomainId domain;
    SlotId target;
    TransitionPhase phase;
    uint32_t businessAttempt;
    // A 唯一业务重试与进行中事实；不含 RIL session、资源 fence、callback 或等待设施。
};
enum class TransitionAdmission { Admitted, DomainBusy, UnknownResourceDomain };
// A 纯规则与状态；同一角色约束域保证唯一默认选择，同一 Radio 域至多一个切换。
// 未证明资源独立前保守共享；目录条目数不决定并发或部署数量。
class AssignmentAggregate final {
public:
    bool AcceptRoleSelection(const RoleSelection &selection, uint64_t expectedAssignmentRevision);
    std::optional<RoleSelection> ReadRoleSelection(DefaultRole role) const;
    TransitionAdmission AdmitTransition(const SlotDescriptor &target);
    bool AcceptTransitionPhase(const RadioDomainId &domain, TransitionPhase expected, TransitionPhase next);
    bool IsSwitchInProgress(const RadioDomainId &domain) const;
    void AcceptPrimaryParameterProjection(SlotId lastPrimary);
    void AcceptDataParameterProjection(SlotId lastData);
private:
    std::map<DefaultRole, RoleSelection> roles_; // 唯一默认角色域，不按独立Modem各存一份默认值。
    uint64_t revision_ {0};
    std::map<RadioDomainId, AssignmentTransition> transitions_;
    std::optional<SlotId> lastPrimary_;
    std::optional<SlotId> lastData_;
    // Voice/SMS 当前与历史角色投影分别按旧可见节点迁入；不得由 T 整行覆盖。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ASSIGNMENT_AGGREGATE_H
