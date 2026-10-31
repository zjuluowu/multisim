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

#include "topology/accounts/account_registry.h"
#include <algorithm>
#include <limits>
#include <set>

namespace OHOS::Telephony::MultiSim {
namespace {
bool MayAdvance(uint64_t current, uint64_t expected)
{
    return current == expected && current != std::numeric_limits<uint64_t>::max();
}
bool MayRecreate(const AccountFieldRevisions &retired)
{
    const auto maximum = std::numeric_limits<uint64_t>::max();
    return retired.name != maximum && retired.number != maximum && retired.active != maximum &&
        retired.label != maximum && retired.imsPreference != maximum && retired.position != maximum;
}
void ContinueAfterRemoval(AccountFieldRevisions &revisions, const AccountFieldRevisions &retired)
{
    revisions.name = std::max(revisions.name, retired.name + 1);
    revisions.number = std::max(revisions.number, retired.number + 1);
    revisions.active = std::max(revisions.active, retired.active + 1);
    revisions.label = std::max(revisions.label, retired.label + 1);
    revisions.imsPreference = std::max(revisions.imsPreference, retired.imsPreference + 1);
    revisions.position = std::max(revisions.position, retired.position + 1);
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
    auto imported = account;
    for (const auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        auto &projection = Projection(imported, view);
        const auto retired = retiredFieldRevisions_[view].find(account.subscription);
        if (projection && retired != retiredFieldRevisions_[view].end()) {
            if (!MayRecreate(retired->second)) { return AccountAcceptance::ChangedInputs; }
            ContinueAfterRemoval(projection->revisions, retired->second);
        }
    }
    accounts_.emplace(account.subscription, std::move(imported));
    if (account.current) { membershipVersions_[AccountReadView::Current][account.subscription] = ++membershipClock_; }
    if (account.historical) { membershipVersions_[AccountReadView::Historical][account.subscription] = ++membershipClock_; }
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
    const bool created = !projection;
    const auto retired = retiredFieldRevisions_[view].find(subscription);
    if (!projection) {
        if (expected.name != 0 || expected.number != 0 || expected.active != 0 ||
            expected.label != 0 || expected.imsPreference != 0 || expected.position != 0) {
            return accepted;
        }
        if (retired != retiredFieldRevisions_[view].end() && !MayRecreate(retired->second)) {
            return accepted;
        }
        projection.emplace();
    }
    if (MayAdvance(projection->revisions.name, expected.name)) {
        if (created || projection->displayName != loaded.displayName) {
            projection->displayName = loaded.displayName;
            ++projection->revisions.name;
        }
        accepted.nameAccepted = true;
    }
    if (MayAdvance(projection->revisions.number, expected.number)) {
        if (created || projection->displayNumber != loaded.displayNumber) {
            projection->displayNumber = loaded.displayNumber;
            ++projection->revisions.number;
        }
        accepted.numberAccepted = true;
    }
    if (MayAdvance(projection->revisions.active, expected.active)) {
        if (created || projection->active != loaded.active) {
            projection->active = loaded.active;
            ++projection->revisions.active;
        }
        accepted.activeAccepted = true;
    }
    if (MayAdvance(projection->revisions.label, expected.label)) {
        if (created || projection->legacyLabel != loaded.legacyLabel) {
            projection->legacyLabel = loaded.legacyLabel;
            ++projection->revisions.label;
        }
        accepted.labelAccepted = true;
    }
    if (MayAdvance(projection->revisions.imsPreference, expected.imsPreference)) {
        if (created || projection->imsPreference != loaded.imsPreference) {
            projection->imsPreference = loaded.imsPreference;
            ++projection->revisions.imsPreference;
        }
        accepted.imsAccepted = true;
    }
    if (MayAdvance(projection->revisions.position, expected.position)) {
        const bool samePosition = (!projection->legacyPosition && !loaded.legacyPosition) ||
            (projection->legacyPosition && loaded.legacyPosition &&
                *projection->legacyPosition == *loaded.legacyPosition);
        if (created || !samePosition) {
            projection->legacyPosition = loaded.legacyPosition;
            ++projection->revisions.position;
        }
        accepted.positionAccepted = true;
    }
    if (created && retired != retiredFieldRevisions_[view].end()) {
        ContinueAfterRemoval(projection->revisions, retired->second);
    }
    return accepted;
}
AccountReloadTicket AccountRegistry::BeginReload(AccountReadView view)
{
    AccountReloadTicket ticket {view, ++issuedLoads_[view], membershipClock_, {}};
    for (const auto &[id, account] : accounts_) {
        const auto projection = ReadProjection(id, view);
        if (projection) { ticket.expected.emplace(id, AccountReloadInput {account.identityRevision, projection->revisions}); }
    }
    return ticket;
}

AccountReloadResult AccountRegistry::AcceptReload(const AccountReloadTicket &ticket, bool replaceable,
    const std::vector<AccountRecord> &loaded)
{
    AccountReloadResult result {false, 0, 0};
    if (ticket.attempt == 0 || issuedLoads_[ticket.view] != ticket.attempt ||
        consumedLoads_[ticket.view] >= ticket.attempt) { return result; }
    consumedLoads_[ticket.view] = ticket.attempt;
    if (!replaceable) { return result; } // 失败保留本view，另一view独立前进。
    std::set<SubscriptionId> present;
    for (const auto &row : loaded) {
        const auto &projection = ticket.view == AccountReadView::Current ? row.current : row.historical;
        const auto current = accounts_.find(row.subscription);
        if (!projection || row.identityRevision == 0 || !present.insert(row.subscription).second ||
            (current != accounts_.end() && current->second.iccid != row.iccid)) { return result; }
    }
    result.acceptedAttempt = true;
    for (const auto &row : loaded) {
        auto current = accounts_.find(row.subscription);
        const auto expected = ticket.expected.find(row.subscription);
        if (current == accounts_.end()) {
            if (expected != ticket.expected.end() ||
                membershipVersions_[ticket.view][row.subscription] > ticket.membershipWatermark) {
                ++result.conflictedFields; continue;
            }
            current = accounts_.emplace(row.subscription,
                AccountRecord {row.subscription, row.iccid, row.identityRevision, {}, {}}).first;
        }
        auto &projection = Projection(current->second, ticket.view);
        if (expected == ticket.expected.end() && (projection ||
            membershipVersions_[ticket.view][row.subscription] > ticket.membershipWatermark)) {
            ++result.conflictedFields; continue;
        }
        const auto &incoming = ticket.view == AccountReadView::Current ? row.current : row.historical;
        const auto accepted = AcceptReloadProjection(row.subscription, ticket.view,
            expected == ticket.expected.end() ? row.identityRevision : expected->second.identityRevision,
            expected == ticket.expected.end() ? AccountFieldRevisions {} : expected->second.fields, *incoming);
        const size_t fields = static_cast<size_t>(accepted.nameAccepted) + accepted.numberAccepted +
            accepted.activeAccepted + accepted.labelAccepted + accepted.imsAccepted + accepted.positionAccepted;
        result.acceptedFields += fields;
        result.conflictedFields += 6 - fields;
        if (expected == ticket.expected.end() && projection) {
            membershipVersions_[ticket.view][row.subscription] = ++membershipClock_;
        }
    }
    for (auto it = accounts_.begin(); it != accounts_.end();) {
        auto &projection = Projection(it->second, ticket.view);
        if (projection && present.count(it->first) == 0) {
            const auto expected = ticket.expected.find(it->first);
            const auto &v = projection->revisions;
            const bool unchanged = expected != ticket.expected.end() &&
                it->second.identityRevision == expected->second.identityRevision &&
                v.name == expected->second.fields.name && v.number == expected->second.fields.number &&
                v.active == expected->second.fields.active && v.label == expected->second.fields.label &&
                v.imsPreference == expected->second.fields.imsPreference && v.position == expected->second.fields.position;
            if (unchanged) {
                retiredFieldRevisions_[ticket.view][it->first] = projection->revisions;
                projection.reset();
                membershipVersions_[ticket.view][it->first] = ++membershipClock_;
            } else { ++result.conflictedFields; }
        }
        if (!it->second.current && !it->second.historical) { it = accounts_.erase(it); }
        else { ++it; }
    }
    return result;
}
} // namespace OHOS::Telephony::MultiSim
