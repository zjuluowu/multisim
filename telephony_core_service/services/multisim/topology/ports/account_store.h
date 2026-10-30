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
#include "assignment/api/assignment_requests.h"

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
    std::optional<int32_t> legacyLabel;
    std::optional<int32_t> imsPreference;
};
struct AccountLoadRequest { AccountReadView view; uint64_t loadAttempt; };
struct AccountLoadResult {
    AccountLoadRequest request;
    int32_t legacyResult;
    bool complete; // 原Query成功且结果可用于替换；不等于旧bool返回。
    bool legacyReturn; // 原GetList/GetAllList均为result != INVALID_VALUE。
    std::vector<AccountStorageRow> rows;
    std::vector<StoredAccountRoleFragment> storedRoles; // 同次旧读取所得，交A接纳，不能用A当前值补造。
};
struct AccountIdentityMatch {
    int32_t legacyResult;
    bool callFinished;
    std::optional<AccountStorageRow> existing;
};
struct AccountDiscoveryFields {
    SlotId legacyPosition;
    std::string iccid;
    std::optional<int32_t> legacyLabel;
    bool active;
    bool isEsim;
};
struct AccountDiscoveryWritePlan {
    CardDiscoveryObservation observation;
    uint64_t expectedBindingRevision;
    std::optional<SubscriptionId> existingSubscription;
    AccountDiscoveryFields accountFields;
    AccountRoleFragment roleFields; // A规划并预留；I执行一次原Insert/Update，不另定角色。
};
struct AccountDiscoveryStoreResult {
    int32_t legacyResult;
    std::optional<SubscriptionId> assignedSubscription; // 原provider的返回身份，不从slot推导。
    bool writeConfirmed;
    bool callFinished;
    EffectKnowledge effect; // 错误返回不补造ConfirmedNotApplied。
};
struct AccountRemovalPatch {
    SubscriptionId subscription;
    std::optional<int32_t> legacyLabel;
    std::optional<bool> active;
}; // 同一原Update将位置置-1，并按旧条件写标签/active，不夹带角色。
// T消费、I实现。每种view保留原查询/失败/重试，不声称provider CAS/跨行事务。
class AccountStore {
public:
    virtual ~AccountStore() = default;
    virtual AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) = 0;
    virtual AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) = 0;
    virtual AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &patch) = 0;
    virtual AccountLoadResult LoadAccounts(const AccountLoadRequest &request) = 0;
    virtual AccountIdentityMatch MatchAccountByIccid(const std::string &iccid) = 0;
    virtual AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &plan) = 0;
    virtual AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &patch) = 0;
    virtual int32_t QueryImsPreferenceByIccid(const std::string &iccid, int32_t &value) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_ACCOUNT_STORE_H
