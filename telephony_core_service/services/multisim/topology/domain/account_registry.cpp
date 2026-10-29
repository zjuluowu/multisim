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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/domain/account_registry.h"
#include <limits>

namespace OHOS::Telephony::MultiSim {
namespace {
bool MayAdvance(uint64_t current, uint64_t expected)
{
    return current == expected && current != std::numeric_limits<uint64_t>::max();
}
} // namespace

std::optional<AccountRecord> AccountRegistry::Read(SubscriptionId subscription) const
{
    auto account = accounts_.find(subscription);
    return account == accounts_.end() ? std::nullopt : std::optional<AccountRecord>(account->second);
}

std::optional<AccountProjection> &AccountRegistry::Projection(AccountRecord &record, AccountReadView view)
{
    return view == AccountReadView::Current ? record.current : record.historical;
}

std::optional<AccountProjection> AccountRegistry::ReadProjection(SubscriptionId subscription, AccountReadView view) const
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return std::nullopt;
    }
    return view == AccountReadView::Current ? account->second.current : account->second.historical;
}

AccountAcceptance AccountRegistry::AcceptDiscoveredAccount(const AccountRecord &account)
{
    if (account.identityRevision == 0 || accounts_.find(account.subscription) != accounts_.end()) {
        return AccountAcceptance::ChangedInputs;
    }
    accounts_.emplace(account.subscription, account);
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::AcceptNumberVisibility(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedNumberRevision, const std::u16string &number)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return AccountAcceptance::MissingAccount;
    }
    auto &projection = Projection(account->second, view);
    if (!projection) {
        return AccountAcceptance::MissingProjection;
    }
    if (!MayAdvance(projection->revisions.number, expectedNumberRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->displayNumber = number;
    ++projection->revisions.number;
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::AcceptActiveProjection(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedActiveRevision, bool active)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return AccountAcceptance::MissingAccount;
    }
    auto &projection = Projection(account->second, view);
    if (!projection) {
        return AccountAcceptance::MissingProjection;
    }
    if (!MayAdvance(projection->revisions.active, expectedActiveRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->active = active;
    ++projection->revisions.active;
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::AcceptDisplayName(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedNameRevision, const std::u16string &name)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return AccountAcceptance::MissingAccount;
    }
    auto &projection = Projection(account->second, view);
    if (!projection) {
        return AccountAcceptance::MissingProjection;
    }
    if (!MayAdvance(projection->revisions.name, expectedNameRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->displayName = name;
    ++projection->revisions.name;
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::AcceptLabel(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedLabelRevision, int32_t legacyLabel)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return AccountAcceptance::MissingAccount;
    }
    auto &projection = Projection(account->second, view);
    if (!projection) {
        return AccountAcceptance::MissingProjection;
    }
    if (!MayAdvance(projection->revisions.label, expectedLabelRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->legacyLabel = legacyLabel;
    ++projection->revisions.label;
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::AcceptImsPreference(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedImsRevision, int32_t preference)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) {
        return AccountAcceptance::MissingAccount;
    }
    auto &projection = Projection(account->second, view);
    if (!projection) {
        return AccountAcceptance::MissingProjection;
    }
    if (!MayAdvance(projection->revisions.imsPreference, expectedImsRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->imsPreference = preference;
    ++projection->revisions.imsPreference;
    return AccountAcceptance::Accepted;
}

AccountReloadAcceptance AccountRegistry::AcceptReloadProjection(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedIdentityRevision, const AccountFieldRevisions &expected, const AccountProjection &loaded)
{
    AccountReloadAcceptance accepted;
    auto account = accounts_.find(subscription);
    if (account == accounts_.end() || account->second.identityRevision != expectedIdentityRevision) {
        return accepted;
    }
    accepted.identityAccepted = true;
    auto &projection = Projection(account->second, view);
    if (!projection) {
        if (expected.name != 0 || expected.number != 0 || expected.active != 0 ||
            expected.label != 0 || expected.imsPreference != 0) {
            return accepted;
        }
        projection.emplace();
    }
    if (MayAdvance(projection->revisions.name, expected.name)) {
        projection->displayName = loaded.displayName;
        ++projection->revisions.name;
        accepted.nameAccepted = true;
    }
    if (MayAdvance(projection->revisions.number, expected.number)) {
        projection->displayNumber = loaded.displayNumber;
        ++projection->revisions.number;
        accepted.numberAccepted = true;
    }
    if (MayAdvance(projection->revisions.active, expected.active)) {
        projection->active = loaded.active;
        ++projection->revisions.active;
        accepted.activeAccepted = true;
    }
    if (MayAdvance(projection->revisions.label, expected.label)) {
        projection->legacyLabel = loaded.legacyLabel;
        ++projection->revisions.label;
        accepted.labelAccepted = true;
    }
    if (MayAdvance(projection->revisions.imsPreference, expected.imsPreference)) {
        projection->imsPreference = loaded.imsPreference;
        ++projection->revisions.imsPreference;
        accepted.imsAccepted = true;
    }
    return accepted;
}
} // namespace OHOS::Telephony::MultiSim
