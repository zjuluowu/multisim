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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_ROLE_PLANNING_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_ROLE_PLANNING_PORT_H

#include <optional>
#include "assignment/api/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
// T发现/重载实际所需A应用入口；H注入，资格覆盖原存储调用全过程。
// 父子关联且不嵌套等待；A提供角色字段，不成为T账户写者。
class AccountRolePlanningPort {
public:
    virtual ~AccountRolePlanningPort() = default;
    virtual uint64_t ReadRoleProjectionRevision(bool historicalView) const = 0;
    virtual bool AcceptReloadedRoleProjection(const StoredAccountRoleFragment &loaded, uint64_t loadAttempt,
        uint64_t expectedAssignmentRevision, bool historicalView) = 0;
    virtual std::optional<AccountRoleFragment> PrepareAccountRoleFragment(const AccountRolePlanningInput &input) = 0;
    virtual bool AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned) = 0;
    virtual bool StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const = 0;
    virtual bool FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
        const InitialRoleFinalizationEvidence &evidence) = 0;
    // 调用结束不等于可释放：已写但观察失效时保留资格直至投影接纳或核对完成。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_ROLE_PLANNING_PORT_H
