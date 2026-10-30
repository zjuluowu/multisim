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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H

#include <optional>
#include <string>
#include <vector>
#include "topology/api/topology_requests.h"
#include "topology/domain/account_registry.h"
#include "assignment/api/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
struct AccountNumberPatch { SubscriptionId subscription; std::u16string number; };
struct AccountActivePatch { SubscriptionId subscription; bool active; };
struct AccountNamePatch { SubscriptionId subscription; std::u16string name; };
struct AccountLabelPatch { SubscriptionId subscription; int32_t legacyLabel; };
struct AccountImsPreferencePatch { SubscriptionId subscription; int32_t preference; };
struct AccountLoadResult {
    AccountReadView view;
    uint64_t attempt;
    int32_t legacyResult; // 原 helper 结果，不等同于缓存替换资格。
    bool legacyReturn; // 旧 GetList/GetAllList 为 result != INVALID_VALUE。
    bool projectionReplaceable; // 只有原 Query 成功才替换；失败保留原投影。
    std::vector<AccountStorageRow> rows;
    std::vector<AccountRoleFragment> roles; // I 同次读取按列拆分，只交 A 接纳。
};
struct AccountMatchResult {
    int32_t legacyResult;
    bool querySucceeded;
    std::optional<AccountStorageRow> matched;
    bool matchedIsEsim;
};
enum class AccountRowMutation { Insert, Rebind, Unbind };
struct AccountRowPlan {
    AccountRowMutation mutation;
    std::string iccid;
    std::optional<SubscriptionId> existingIdentity;
    AccountProjectionData fields;
    bool isEsim;
    std::optional<AccountRoleFragment> roles; // Unbind 的旧路径无角色副作用时为空。
};
struct AccountRowResult {
    AccountStoreWriteResult storage;
    EffectKnowledge effect; // 真实provider证据映射；callFinished本身不证明未产生部分效果。
    std::optional<SubscriptionId> storedIdentity; // Insert 返回ID或沿原查询解析，不能由T造号。
    bool identityResolved;
};
// T 消费，I 执行原 SimRdbHelper 协议；无整行反灌、CAS 或虚构跨行事务。
class AccountStore {
public:
    virtual ~AccountStore() = default;
    virtual AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &patch) = 0;
    virtual AccountLoadResult LoadAccounts(const AccountReloadTicket &ticket) = 0;
    virtual AccountMatchResult MatchAccountByIccid(const std::string &iccid) = 0;
    virtual AccountRowResult ApplyAccountRow(const AccountRowPlan &plan) = 0;
    virtual int32_t QueryImsPreferenceByIccid(const std::string &iccid, int32_t &value) = 0;
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H
