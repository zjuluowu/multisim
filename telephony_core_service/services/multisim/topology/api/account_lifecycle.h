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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2～4.3、§6.3、§8～9。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ACCOUNT_LIFECYCLE_H
#define OHOS_TELEPHONY_MULTISIM_ACCOUNT_LIFECYCLE_H

#include <map>
#include <optional>
#include <string>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class AccountReadView { Current, Historical };
// 每字段版本只由 T 产生；无关名称变化不撤销号码资格。
struct AccountFieldVersions {
    uint64_t position = 0;
    uint64_t name = 0;
    uint64_t number = 0;
    uint64_t active = 0;
    uint64_t label = 0;
    uint64_t ims = 0;
};
struct AccountReloadTicket {
    AccountReadView view;
    uint64_t attempt;
    std::map<SubscriptionId, AccountFieldVersions> fields;
    uint64_t mutationWatermark;
    uint64_t assignmentRevision; // A 在同一次读取发起前捕获；T 不比较/接纳角色。
};
struct CardDiscoveryObservation {
    SessionRef session;
    uint64_t observationSequence; // R 在实例内按槽单调产生，换 session 不归零。
    std::string iccid;
};
struct DiscoveryTicket {
    CardDiscoveryObservation observation;
    uint64_t admission;
    uint64_t bindingRevision;
};
struct DiscoveryRoleContext {
    SlotId slot;
    std::string iccid;
    std::optional<SubscriptionId> matchedSubscription; // 首次建行前为空。
    uint64_t observationSequence;
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_ACCOUNT_LIFECYCLE_H
