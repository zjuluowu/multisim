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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3、§8.2、§9.2、§13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H

#include <optional>
#include <string>
#include <vector>
#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
struct AccountNumberPatch { SubscriptionId subscription; std::u16string number; };
struct AccountActivePatch { SubscriptionId subscription; bool active; };
struct AccountNamePatch { SubscriptionId subscription; std::u16string name; };
struct AccountLabelPatch { SubscriptionId subscription; int32_t legacyLabel; };
struct AccountImsPreferencePatch { SubscriptionId subscription; int32_t preference; };
struct AccountStorageRow {
    SubscriptionId subscription;
    std::string iccid;
    std::optional<SlotId> legacyPosition;
    std::u16string name;
    std::u16string number;
    bool active;
    // 不含 A 的角色片段：建行/重载由 T 调 A 的应用入口组合，不能整行反灌。
};
struct AccountLoadResult {
    int32_t legacyResult;
    bool complete;
    std::vector<AccountStorageRow> rows; // 空结果与失败分开，不用空列表伪装成功。
};
// T TopologyService 消费，I AccountStoreAdapter 实现。沿 SimRdbHelper/DataShare，
// 按 Owner 字段写，无 Save(wholeAccount)，不声称 provider CAS 或跨行事务。
class AccountStore {
public:
    virtual ~AccountStore() = default;
    virtual AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &patch) = 0;
    virtual AccountLoadResult LoadAccounts() = 0;
    virtual int32_t QueryImsPreferenceByIccid(const std::string &iccid, int32_t &value) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H
