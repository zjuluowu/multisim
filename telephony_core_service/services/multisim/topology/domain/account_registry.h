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
#include "topology/api/account_lifecycle.h"

namespace OHOS::Telephony::MultiSim {
struct AccountProjectionData {
    std::optional<SlotId> legacyPosition;
    std::u16string displayName;
    std::u16string displayNumber;
    bool active = false;
    std::optional<int32_t> legacyLabel;
    std::optional<int32_t> imsPreference;
};
struct AccountProjection {
    AccountProjectionData data;
    AccountFieldVersions versions;
};
// WU-01/02/05：独立 current/all 投影片段，同一个 T Owner。
struct AccountRecord {
    SubscriptionId subscription;
    std::string iccid;
    std::optional<AccountProjection> current;
    std::optional<AccountProjection> historical;
};
struct AccountStorageRow {
    SubscriptionId subscription;
    std::string iccid;
    AccountProjectionData projection;
};
enum class AccountAcceptance { Accepted, MissingAccount, ChangedInputs };
using NumberVisibilityNode = AccountReadView;
struct NumberFieldVersions { std::optional<uint64_t> current; std::optional<uint64_t> historical; };
struct AccountReloadAcceptance {
    bool acceptedAttempt;
    size_t acceptedFields;
    size_t conflictedFields;
};
class AccountRegistry final {
public:
    std::optional<AccountRecord> Read(SubscriptionId subscription) const;
    AccountReloadTicket BeginReload(AccountReadView view, uint64_t assignmentRevision);
    AccountReloadAcceptance AcceptReload(const AccountReloadTicket &ticket,
        bool projectionReplaceable, const std::vector<AccountStorageRow> &rows);
    std::optional<NumberFieldVersions> ReadNumberVersions(SubscriptionId subscription) const;
    bool NumberInputsMatch(SubscriptionId subscription, const NumberFieldVersions &expected) const;
    AccountAcceptance AcceptNumberVisibility(SubscriptionId subscription, uint64_t expectedNumberVersion,
        NumberVisibilityNode node, const std::u16string &number);
    AccountAcceptance AcceptActiveProjection(SubscriptionId subscription, uint64_t expectedVersion,
        AccountReadView view, bool active);
    AccountAcceptance AcceptDisplayName(SubscriptionId subscription, uint64_t version,
        AccountReadView view, const std::u16string &name);
    AccountAcceptance AcceptLabel(SubscriptionId subscription, uint64_t version, AccountReadView view, int32_t label);
    AccountAcceptance AcceptImsPreference(SubscriptionId subscription, uint64_t version,
        AccountReadView view, int32_t preference);
private:
    AccountProjection *FindProjection(SubscriptionId subscription, AccountReadView view);
    std::map<SubscriptionId, AccountRecord> accounts_;
    std::map<AccountReadView, uint64_t> issuedAttempts_;
    std::map<AccountReadView, uint64_t> consumedAttempts_;
    std::map<AccountReadView, std::map<SubscriptionId, uint64_t>> membershipVersions_;
    uint64_t mutationClock_ = 0; // 不随删除/重建归零，避免同值版本 ABA。
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H
