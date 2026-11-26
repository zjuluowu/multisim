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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "account/account_registry.h"
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

std::optional<SubscriptionId> AccountRegistry::FindByIccid(const std::string &iccid) const
{
    for (const auto &[subscription, account] : accounts_) {
        if (account.iccid == iccid) { return subscription; }
    }
    return std::nullopt;
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

AccountAcceptance AccountRegistry::PublishDiscovery(AccountRecord account, bool rewriteLabel)
{
    const auto found = accounts_.find(account.subscription);
    if (account.identityRevision == 0 || (found != accounts_.end() && found->second.iccid != account.iccid)) {
        return AccountAcceptance::ChangedInputs;
    }
    const auto sameIccid = FindByIccid(account.iccid);
    if (sameIccid && !(*sameIccid == account.subscription)) { return AccountAcceptance::ChangedInputs; }
    auto prepared = found == accounts_.end() ? account : found->second;
    uint64_t nextClock = membershipClock_;
    std::map<SubscriptionId, uint64_t> currentNode;
    std::map<SubscriptionId, uint64_t> historicalNode;
    for (auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        auto &input = Projection(account, view);
        if (!input) { continue; }
        if (nextClock == std::numeric_limits<uint64_t>::max()) { return AccountAcceptance::ChangedInputs; }
        auto &output = Projection(prepared, view);
        if (found == accounts_.end() || !output) {
            output = std::move(input);
            const auto retired = retiredFieldRevisions_[view].find(account.subscription);
            if (retired != retiredFieldRevisions_[view].end()) {
                if (!MayRecreate(retired->second)) { return AccountAcceptance::ChangedInputs; }
                ContinueAfterRemoval(output->revisions, retired->second);
            }
        } else {
            auto &current = *output;
            auto &versions = current.revisions;
            if (current.active != input->active) {
                if (!MayAdvance(versions.active, versions.active)) { return AccountAcceptance::ChangedInputs; }
                current.active = input->active;
                ++versions.active;
            }
            if (!(current.legacyPosition == input->legacyPosition)) {
                if (!MayAdvance(versions.position, versions.position)) {
                    return AccountAcceptance::ChangedInputs;
                }
                current.legacyPosition = input->legacyPosition;
                ++versions.position;
            }
            if (rewriteLabel && current.legacyLabel != input->legacyLabel) {
                if (!MayAdvance(versions.label, versions.label)) { return AccountAcceptance::ChangedInputs; }
                current.legacyLabel = input->legacyLabel;
                ++versions.label;
            }
        }
        // Only reserve the touched member, including reaffirmation with unchanged fields.
        membershipVersions_.try_emplace(view);
        auto &node = view == AccountReadView::Current ? currentNode : historicalNode;
        node.emplace(account.subscription, ++nextClock);
    }
    std::map<SubscriptionId, AccountRecord> node;
    node.emplace(account.subscription, std::move(prepared));

    // Account/member maps use primitive subscription comparisons. Inserting prepared nodes cannot allocate.
    if (found != accounts_.end()) { accounts_.erase(found); }
    accounts_.insert(node.extract(node.begin()));
    for (auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        auto &preparedNode = view == AccountReadView::Current ? currentNode : historicalNode;
        if (preparedNode.empty()) { continue; }
        auto &members = membershipVersions_.find(view)->second;
        members.erase(account.subscription);
        members.insert(preparedNode.extract(preparedNode.begin()));
    }
    membershipClock_ = nextClock;
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
    if (projection->displayName == name) {
        return projection->revisions.name == expectedNameRevision ?
            AccountAcceptance::Accepted : AccountAcceptance::ChangedInputs;
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
    if (projection->legacyLabel == legacyLabel) {
        return projection->revisions.label == expectedLabelRevision ?
            AccountAcceptance::Accepted : AccountAcceptance::ChangedInputs;
    }
    if (!MayAdvance(projection->revisions.label, expectedLabelRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    projection->legacyLabel = legacyLabel;
    ++projection->revisions.label;
    return AccountAcceptance::Accepted;
}

AccountAcceptance AccountRegistry::ConfirmImsPreferenceWrite(SubscriptionId subscription, AccountReadView view,
    uint64_t expectedImsRevision)
{
    auto account = accounts_.find(subscription);
    if (account == accounts_.end()) { return AccountAcceptance::MissingAccount; }
    auto &projection = Projection(account->second, view);
    if (!projection) { return AccountAcceptance::MissingProjection; }
    if (!MayAdvance(projection->revisions.imsPreference, expectedImsRevision)) {
        return AccountAcceptance::ChangedInputs;
    }
    // SaveImsSwitch never publishes a value to either account cache.
    ++projection->revisions.imsPreference;
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
    uint64_t expectedIdentityRevision, const AccountFieldRevisions &expected, const AccountProjection &loaded,
    uint8_t allowedFields)
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
    if ((allowedFields & ReloadName) && MayAdvance(projection->revisions.name, expected.name)) {
        if (created || projection->displayName != loaded.displayName) {
            projection->displayName = loaded.displayName;
            ++projection->revisions.name;
        }
        accepted.nameAccepted = true;
    }
    if ((allowedFields & ReloadNumber) && MayAdvance(projection->revisions.number, expected.number)) {
        if (created || projection->displayNumber != loaded.displayNumber) {
            projection->displayNumber = loaded.displayNumber;
            ++projection->revisions.number;
        }
        accepted.numberAccepted = true;
    }
    if ((allowedFields & ReloadActive) && MayAdvance(projection->revisions.active, expected.active)) {
        if (created || projection->active != loaded.active) {
            projection->active = loaded.active;
            ++projection->revisions.active;
        }
        accepted.activeAccepted = true;
    }
    if ((allowedFields & ReloadLabel) && MayAdvance(projection->revisions.label, expected.label)) {
        if (created || projection->legacyLabel != loaded.legacyLabel) {
            projection->legacyLabel = loaded.legacyLabel;
            ++projection->revisions.label;
        }
        accepted.labelAccepted = true;
    }
    if ((allowedFields & ReloadIms) && MayAdvance(projection->revisions.imsPreference, expected.imsPreference)) {
        if (created || projection->imsPreference != loaded.imsPreference) {
            projection->imsPreference = loaded.imsPreference;
            ++projection->revisions.imsPreference;
        }
        accepted.imsAccepted = true;
    }
    if ((allowedFields & ReloadPosition) && MayAdvance(projection->revisions.position, expected.position)) {
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
void AccountRegistry::ProtectReloadFields(SubscriptionId id, AccountReadView view, uint8_t fields,
    std::optional<StorePosition> position)
{
    auto &member = reloadMembers_[view][id];
    for (size_t field = 0; field < member.fields.size(); ++field) {
        if (fields & (1U << field)) { member.fields[field].Set(position); }
    }
}

void AccountRegistry::ProtectDiscovery(SubscriptionId id, bool current, bool label,
    std::optional<StorePosition> position)
{
    const auto &account = accounts_.at(id);
    for (const auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        if (view == AccountReadView::Current && !current) { continue; }
        auto &member = reloadMembers_[view][id];
        member.iccid = account.iccid;
        if (!member.continuousSince) {
            const auto version = membershipVersions_[view].find(id);
            if (version == membershipVersions_[view].end()) { continue; }
            member.continuousSince = version->second;
        }
        member.protection.Set(position);
        ProtectReloadFields(id, view, ReloadPosition | ReloadActive | (label ? ReloadLabel : 0), position);
    }
}

AccountReloadTicket AccountRegistry::BeginReload(AccountReadView view)
{
    auto &issued = issuedLoads_[view];
    if (issued == std::numeric_limits<uint64_t>::max()) { return {view, 0, 0, {}}; }
    AccountReloadTicket ticket {view, issued + 1, membershipClock_, {}};
    for (const auto &[id, account] : accounts_) {
        const auto projection = ReadProjection(id, view);
        if (projection) { ticket.expected.emplace(id, AccountReloadInput {account.identityRevision, projection->revisions}); }
    }
    issued = ticket.attempt;
    return ticket;
}

AccountReloadResult AccountRegistry::AcceptReload(const AccountReloadTicket &ticket, bool replaceable,
    const std::vector<AccountRecord> &loaded, const std::vector<uint8_t> &validFields,
    const std::optional<AccountLoadImage> &image, const std::vector<AccountReloadPending> &pending,
    bool membershipPending, bool requireCertifiedRemoval)
{
    AccountReloadResult result;
    if (ticket.attempt == 0 || issuedLoads_[ticket.view] != ticket.attempt ||
        consumedLoads_[ticket.view] >= ticket.attempt) { return result; }
    consumedLoads_[ticket.view] = ticket.attempt;
    if (!replaceable || (!validFields.empty() && validFields.size() != loaded.size())) { return result; }

    // Prepare the whole T candidate before publishing. The private copy is never used as a second read source.
    auto accounts = accounts_;
    auto members = reloadMembers_[ticket.view];
    auto versions = membershipVersions_[ticket.view];
    auto retired = retiredFieldRevisions_[ticket.view];
    auto clock = membershipClock_;
    std::set<SubscriptionId> present;
    std::set<std::string> identities;
    for (const auto &row : loaded) {
        const auto &projection = ticket.view == AccountReadView::Current ? row.current : row.historical;
        const auto current = accounts.find(row.subscription);
        const auto sameIccid = FindByIccid(row.iccid);
        const auto member = members.find(row.subscription);
        if (!projection || row.subscription.legacySimId < 0 || row.iccid.empty() || row.identityRevision == 0 ||
            !present.insert(row.subscription).second || !identities.insert(row.iccid).second ||
            (current != accounts.end() && current->second.iccid != row.iccid) ||
            (sameIccid && !(*sameIccid == row.subscription)) ||
            (member != members.end() && !member->second.iccid.empty() && member->second.iccid != row.iccid)) {
            return result;
        }
    }

    const auto membershipImage = image && image->coversMembership ?
        std::make_optional(image->position) : std::nullopt;
    const auto pendingFields = [&](SubscriptionId id) {
        uint8_t fields = 0;
        for (const auto &item : pending) { if (item.subscription == id) { fields |= item.fields; } }
        return fields;
    };
    const auto fieldImage = [&](unsigned bit) {
        return image && (image->coveredFields & bit) ? std::make_optional(image->position) : std::nullopt;
    };
    const auto sameFields = [](const AccountFieldRevisions &a, const AccountFieldRevisions &b) {
        return a.name == b.name && a.number == b.number && a.active == b.active && a.label == b.label &&
            a.imsPreference == b.imsPreference && a.position == b.position;
    };

    // Reuse the field merge against the candidate; only its selected view will be published.
    AccountRegistry candidate;
    candidate.accounts_.swap(accounts);
    candidate.retiredFieldRevisions_[ticket.view] = retired;
    result.acceptedAttempt = true;
    result.roleMembershipEligible = !membershipPending;
    for (size_t index = 0; index < loaded.size(); ++index) {
        const auto &row = loaded[index];
        const auto id = row.subscription;
        auto existing = candidate.accounts_.find(id);
        const auto expected = ticket.expected.find(id);
        const bool existed = existing != candidate.accounts_.end() && Projection(existing->second, ticket.view);
        auto &member = members[id];
        const auto memberVersion = versions[id];
        const bool newerThanTombstone = !retired.count(id) || existed ||
            (membershipImage && (!member.protection.floor || !(*membershipImage == *member.protection.floor)));
        const bool stableMember = memberVersion <= ticket.membershipWatermark &&
            member.protection.CoveredBy(membershipImage) && (!requireCertifiedRemoval || newerThanTombstone);
        result.roleMembershipEligible &= stableMember;
        const auto columns = validFields.empty() ? ReloadAllFields : validFields[index];
        if ((expected == ticket.expected.end() && (existed || membershipPending || !stableMember ||
            columns != ReloadAllFields || clock == std::numeric_limits<uint64_t>::max() ||
            (retired.count(id) && !MayRecreate(retired.at(id))))) ||
            (expected != ticket.expected.end() && (!existed || member.continuousSince > ticket.membershipWatermark ||
            existing->second.identityRevision != expected->second.identityRevision))) {
            result.roleMembershipEligible = false;
            result.conflictedFields += 6;
            continue;
        }

        const auto &incoming = ticket.view == AccountReadView::Current ? row.current : row.historical;
        uint8_t allowed = columns & ~pendingFields(id);
        std::array<bool, 6> unchanged {};
        if (existed) {
            const auto &previous = *Projection(existing->second, ticket.view);
            unchanged = {previous.displayName == incoming->displayName,
                previous.displayNumber == incoming->displayNumber, previous.active == incoming->active,
                previous.legacyLabel == incoming->legacyLabel, previous.imsPreference == incoming->imsPreference,
                previous.legacyPosition == incoming->legacyPosition};
        }
        for (size_t field = 0; field < member.fields.size(); ++field) {
            const unsigned bit = 1U << field;
            const auto sample = fieldImage(bit);
            const auto &protectedField = member.fields[field];
            if (!protectedField.CoveredBy(sample) ||
                (sample && protectedField.observed && sample == protectedField.floor && !unchanged[field])) {
                allowed &= ~bit;
            }
        }
        // A newly created projection must carry every field; no partial row can revive a tombstone.
        if (!existed && allowed != ReloadAllFields) {
            result.roleMembershipEligible = false;
            result.conflictedFields += 6;
            continue;
        }
        if (existing == candidate.accounts_.end()) {
            existing = candidate.accounts_.emplace(id, AccountRecord {id, row.iccid, row.identityRevision, {}, {}}).first;
        }
        const auto accepted = candidate.AcceptReloadProjection(id, ticket.view, existing->second.identityRevision,
            expected == ticket.expected.end() ? AccountFieldRevisions {} : expected->second.fields, *incoming, allowed);
        const std::array fields {accepted.nameAccepted, accepted.numberAccepted, accepted.activeAccepted,
            accepted.labelAccepted, accepted.imsAccepted, accepted.positionAccepted};
        for (size_t field = 0; field < fields.size(); ++field) {
            if (fields[field]) {
                ++result.acceptedFields;
                if (const auto position = fieldImage(1U << field)) { member.fields[field].Set(position, true); }
            } else { ++result.conflictedFields; }
        }
        if (!existed) {
            versions[id] = ++clock;
            member.continuousSince = clock;
            member.iccid = row.iccid;
        }
        if (stableMember && membershipImage) { member.protection.Set(membershipImage, true); }
    }

    for (auto it = candidate.accounts_.begin(); it != candidate.accounts_.end();) {
        const auto id = it->first;
        auto &projection = Projection(it->second, ticket.view);
        if (projection && !present.count(id)) {
            const auto expected = ticket.expected.find(id);
            auto &member = members[id];
            bool covered = member.protection.CoveredBy(membershipImage);
            for (size_t field = 0; field < member.fields.size(); ++field) {
                covered &= member.fields[field].CoveredBy(fieldImage(1U << field));
            }
            const bool newerMembership = membershipImage && (!member.protection.floor ||
                !(*membershipImage == *member.protection.floor));
            if (!membershipPending && !pendingFields(id) && covered &&
                (!requireCertifiedRemoval || newerMembership) && versions[id] <= ticket.membershipWatermark &&
                expected != ticket.expected.end() && it->second.identityRevision == expected->second.identityRevision &&
                sameFields(projection->revisions, expected->second.fields) &&
                clock != std::numeric_limits<uint64_t>::max()) {
                retired[id] = projection->revisions;
                projection.reset();
                versions[id] = ++clock;
                member.iccid = it->second.iccid;
                member.continuousSince = 0;
                if (membershipImage) { member.protection.Set(membershipImage, true); }
            } else {
                result.roleMembershipEligible = false;
                ++result.conflictedFields;
            }
        }
        if (!it->second.current && !it->second.historical) { it = candidate.accounts_.erase(it); }
        else { ++it; }
    }
    for (const auto &[id, member] : members) {
        if (!member.continuousSince && versions[id] > ticket.membershipWatermark && !retired.count(id)) {
            result.roleMembershipEligible = false;
        }
        if (!member.continuousSince && !present.count(id) && !member.protection.CoveredBy(membershipImage)) {
            result.roleMembershipEligible = false;
        }
    }
    accounts_.swap(candidate.accounts_);
    reloadMembers_[ticket.view].swap(members);
    membershipVersions_[ticket.view].swap(versions);
    retiredFieldRevisions_[ticket.view].swap(retired);
    membershipClock_ = clock;
    return result;
}
} // namespace OHOS::Telephony::MultiSim
