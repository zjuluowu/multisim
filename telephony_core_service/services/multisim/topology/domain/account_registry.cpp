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
#include <algorithm>
#include <set>

namespace OHOS::Telephony::MultiSim {
namespace {
std::optional<AccountProjection> &Projection(AccountRecord &record, AccountReadView view)
{
    return view == AccountReadView::Current ? record.current : record.historical;
}
const std::optional<AccountProjection> &Projection(const AccountRecord &record, AccountReadView view)
{
    return view == AccountReadView::Current ? record.current : record.historical;
}
uint64_t Maximum(const AccountFieldVersions &v)
{
    return std::max({v.position, v.name, v.number, v.active, v.label, v.ims});
}
}
std::optional<AccountRecord> AccountRegistry::Read(SubscriptionId subscription) const
{
    const auto found = accounts_.find(subscription);
    return found == accounts_.end() ? std::nullopt : std::optional<AccountRecord>(found->second);
}
AccountProjection *AccountRegistry::FindProjection(SubscriptionId subscription, AccountReadView view)
{
    auto found = accounts_.find(subscription);
    if (found == accounts_.end()) { return nullptr; }
    auto &projection = Projection(found->second, view);
    return projection ? &*projection : nullptr;
}
AccountReloadTicket AccountRegistry::BeginReload(AccountReadView view, uint64_t assignmentRevision)
{
    AccountReloadTicket ticket {view, ++issuedAttempts_[view], {}, mutationClock_, assignmentRevision};
    for (const auto &[id, account] : accounts_) {
        const auto &projection = Projection(account, view);
        if (projection) { ticket.fields.emplace(id, projection->versions); }
    }
    return ticket;
}
AccountReloadAcceptance AccountRegistry::AcceptReload(const AccountReloadTicket &ticket,
    bool projectionReplaceable, const std::vector<AccountStorageRow> &rows)
{
    AccountReloadAcceptance outcome {false, 0, 0};
    if (issuedAttempts_[ticket.view] != ticket.attempt || consumedAttempts_[ticket.view] >= ticket.attempt) {
        return outcome;
    }
    consumedAttempts_[ticket.view] = ticket.attempt;
    if (!projectionReplaceable) { return outcome; } // 失败不清空任何投影。
    // 在改变状态前拒绝畸形/重复身份，避免半次重载。
    std::set<SubscriptionId> present;
    for (const auto &row : rows) {
        if (!present.insert(row.subscription).second) { return outcome; }
        const auto old = accounts_.find(row.subscription);
        if (old != accounts_.end() && old->second.iccid != row.iccid) { return outcome; }
    }
    outcome.acceptedAttempt = true;
    for (const auto &row : rows) {
        auto found = accounts_.find(row.subscription);
        const auto snapshot = ticket.fields.find(row.subscription);
        if (found == accounts_.end()) {
            // 发起后曾删除的身份也不由旧重载复活。
            if (snapshot != ticket.fields.end() ||
                membershipVersions_[ticket.view][row.subscription] > ticket.mutationWatermark) {
                ++outcome.conflictedFields; continue;
            }
            found = accounts_.emplace(row.subscription, AccountRecord {row.subscription, row.iccid, {}, {}}).first;
        }
        auto &projection = Projection(found->second, ticket.view);
        if (!projection) {
            if (snapshot != ticket.fields.end() ||
                membershipVersions_[ticket.view][row.subscription] > ticket.mutationWatermark) {
                ++outcome.conflictedFields; continue;
            }
            const uint64_t v = ++mutationClock_;
            projection = AccountProjection {row.projection, {v, v, v, v, v, v}};
            membershipVersions_[ticket.view][row.subscription] = v;
            outcome.acceptedFields += 6;
            continue;
        }
        if (snapshot == ticket.fields.end()) { ++outcome.conflictedFields; continue; }
        auto accept = [&](auto &value, const auto &incoming, uint64_t &version, uint64_t expected) {
            if (version != expected) { ++outcome.conflictedFields; return; }
            if (value == incoming) { ++outcome.acceptedFields; return; }
            value = incoming;
            version = ++mutationClock_;
            ++outcome.acceptedFields;
        };
        auto &v = projection->versions;
        const auto &e = snapshot->second;
        accept(projection->data.legacyPosition, row.projection.legacyPosition, v.position, e.position);
        accept(projection->data.displayName, row.projection.displayName, v.name, e.name);
        accept(projection->data.displayNumber, row.projection.displayNumber, v.number, e.number);
        accept(projection->data.active, row.projection.active, v.active, e.active);
        accept(projection->data.legacyLabel, row.projection.legacyLabel, v.label, e.label);
        accept(projection->data.imsPreference, row.projection.imsPreference, v.ims, e.ims);
    }
    for (auto it = accounts_.begin(); it != accounts_.end();) {
        auto &projection = Projection(it->second, ticket.view);
        if (projection && present.count(it->first) == 0) {
            const auto snapshot = ticket.fields.find(it->first);
            if (snapshot != ticket.fields.end() && Maximum(projection->versions) <= ticket.mutationWatermark) {
                projection.reset();
                membershipVersions_[ticket.view][it->first] = ++mutationClock_;
            } else { ++outcome.conflictedFields; }
        }
        if (!it->second.current && !it->second.historical) { it = accounts_.erase(it); }
        else { ++it; }
    }
    return outcome;
}
std::optional<NumberFieldVersions> AccountRegistry::ReadNumberVersions(SubscriptionId subscription) const
{
    const auto account = Read(subscription);
    if (!account) { return std::nullopt; }
    return NumberFieldVersions {
        account->current ? std::optional<uint64_t>(account->current->versions.number) : std::nullopt,
        account->historical ? std::optional<uint64_t>(account->historical->versions.number) : std::nullopt};
}
bool AccountRegistry::NumberInputsMatch(SubscriptionId subscription, const NumberFieldVersions &expected) const
{
    const auto versions = ReadNumberVersions(subscription);
    return versions && versions->current == expected.current && versions->historical == expected.historical;
}
AccountAcceptance AccountRegistry::AcceptNumberVisibility(SubscriptionId subscription, uint64_t expected,
    NumberVisibilityNode view, const std::u16string &number)
{
    auto *projection = FindProjection(subscription, view);
    if (!projection) { return AccountAcceptance::MissingAccount; }
    if (projection->versions.number != expected) { return AccountAcceptance::ChangedInputs; }
    projection->data.displayNumber = number;
    projection->versions.number = ++mutationClock_;
    return AccountAcceptance::Accepted;
}
AccountAcceptance AccountRegistry::AcceptActiveProjection(SubscriptionId subscription, uint64_t expected,
    AccountReadView view, bool active)
{
    auto *projection = FindProjection(subscription, view);
    if (!projection) { return AccountAcceptance::MissingAccount; }
    if (projection->versions.active != expected) { return AccountAcceptance::ChangedInputs; }
    projection->data.active = active;
    projection->versions.active = ++mutationClock_;
    return AccountAcceptance::Accepted;
}
AccountAcceptance AccountRegistry::AcceptDisplayName(SubscriptionId subscription, uint64_t expected,
    AccountReadView view, const std::u16string &name)
{
    auto *projection = FindProjection(subscription, view);
    if (!projection) { return AccountAcceptance::MissingAccount; }
    if (projection->versions.name != expected) { return AccountAcceptance::ChangedInputs; }
    projection->data.displayName = name;
    projection->versions.name = ++mutationClock_;
    return AccountAcceptance::Accepted;
}
AccountAcceptance AccountRegistry::AcceptLabel(SubscriptionId subscription, uint64_t expected,
    AccountReadView view, int32_t label)
{
    auto *projection = FindProjection(subscription, view);
    if (!projection) { return AccountAcceptance::MissingAccount; }
    if (projection->versions.label != expected) { return AccountAcceptance::ChangedInputs; }
    projection->data.legacyLabel = label;
    projection->versions.label = ++mutationClock_;
    return AccountAcceptance::Accepted;
}
AccountAcceptance AccountRegistry::AcceptImsPreference(SubscriptionId subscription, uint64_t expected,
    AccountReadView view, int32_t preference)
{
    auto *projection = FindProjection(subscription, view);
    if (!projection) { return AccountAcceptance::MissingAccount; }
    if (projection->versions.ims != expected) { return AccountAcceptance::ChangedInputs; }
    projection->data.imsPreference = preference;
    projection->versions.ims = ++mutationClock_;
    return AccountAcceptance::Accepted;
}
} // namespace OHOS::Telephony::MultiSim
