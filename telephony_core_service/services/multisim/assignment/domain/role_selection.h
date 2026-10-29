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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ROLE_SELECTION_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ROLE_SELECTION_H

#include <optional>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class DefaultRole { Voice, Sms, Data };
struct RoleSelection {
    DefaultRole role;
    std::optional<SubscriptionId> preference;
    std::optional<SubscriptionId> effective;
};
// A的角色约束域中的纯值；AssignmentAggregate唯一接纳，api命令复用角色枚举。
// Voice无命中为INVALID、SMS有首个active回退等旧投影规则不在此统一。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_DOMAIN_ROLE_SELECTION_H
