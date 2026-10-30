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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class AccountReadView { Current, Historical };
struct AccountFieldRevisions {
    uint64_t name {0};
    uint64_t number {0};
    uint64_t active {0};
    uint64_t label {0};
    uint64_t imsPreference {0};
    uint64_t position {0};
};
struct AccountProjection {
    std::u16string displayName;
    std::u16string displayNumber;
    std::optional<int32_t> legacyLabel;
    std::optional<int32_t> imsPreference;
    bool active {false};
    AccountFieldRevisions revisions;
    std::optional<SlotId> legacyPosition; // current/all的旧位置读源可暂不同；不代替已接受Binding。
};
// WU-01/02/05 同一 T Owner 的两个兼容投影，不是两个独立业务写者。
// 原 QueryAllValidData/QueryAllData 分别加载、失败、重试；同字段也可能暂时不同。
struct AccountRecord {
    SubscriptionId subscription;
    std::string iccid;
    uint64_t identityRevision {0};
    std::optional<AccountProjection> current;
    std::optional<AccountProjection> historical;
};
enum class AccountAcceptance { Accepted, MissingAccount, MissingProjection, ChangedInputs };
struct AccountReloadAcceptance {
    bool identityAccepted {false};
    bool nameAccepted {false};
    bool numberAccepted {false};
    bool activeAccepted {false};
    bool labelAccepted {false};
    bool imsAccepted {false};
    bool positionAccepted {false};
};
struct AccountReloadInput { uint64_t identityRevision; AccountFieldRevisions fields; };
struct AccountReloadTicket {
    AccountReadView view;
    uint64_t attempt;
    uint64_t membershipWatermark;
    std::map<SubscriptionId, AccountReloadInput> expected;
};
struct AccountReloadResult { bool acceptedAttempt; size_t acceptedFields; size_t conflictedFields; };
class AccountRegistry final {
public:
    std::optional<AccountRecord> Read(SubscriptionId subscription) const;
    AccountReloadTicket BeginReload(AccountReadView view);
    AccountReloadResult AcceptReload(const AccountReloadTicket &ticket, bool replaceable,
        const std::vector<AccountRecord> &loaded);
    std::optional<AccountProjection> ReadProjection(SubscriptionId subscription, AccountReadView view) const;
    AccountAcceptance AcceptNumberVisibility(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedNumberRevision, const std::u16string &number);
    AccountAcceptance AcceptActiveProjection(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedActiveRevision, bool active);
    AccountAcceptance AcceptDisplayName(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedNameRevision, const std::u16string &name);
    AccountAcceptance AcceptLabel(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedLabelRevision, int32_t legacyLabel);
    AccountAcceptance AcceptImsPreference(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedImsRevision, int32_t preference);
    // 仅首次导入已由旧provider分配的身份；已有账户通过字段接纳，不整行覆盖。
    AccountAcceptance AcceptDiscoveredAccount(const AccountRecord &account);
    AccountReloadAcceptance AcceptReloadProjection(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedIdentityRevision, const AccountFieldRevisions &expected,
        const AccountProjection &loaded);
private:
    std::optional<AccountProjection> &Projection(AccountRecord &record, AccountReadView view);
    std::map<SubscriptionId, AccountRecord> accounts_;
    std::map<AccountReadView, uint64_t> issuedLoads_;
    std::map<AccountReadView, uint64_t> consumedLoads_;
    std::map<AccountReadView, std::map<SubscriptionId, uint64_t>> membershipVersions_;
    std::map<AccountReadView, std::map<SubscriptionId, AccountFieldRevisions>> retiredFieldRevisions_;
    // 移除投影只撤销内容；重建时字段版本继续，防旧号码资格在同一绑定上ABA。
    uint64_t membershipClock_ {0}; // 删除不归零；旧读取不得复活已移除的投影。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H
