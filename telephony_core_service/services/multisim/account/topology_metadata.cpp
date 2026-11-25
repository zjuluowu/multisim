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
#include "account/topology_service.h"

#include <algorithm>
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
std::optional<AccountView> TopologyService::ReadAccountAtSlot(SlotTarget origin) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto binding = bindings_.Find(origin.slot);
    return binding && bindings_.StillMatches(*binding) ? ReadAccount(binding->subscription) : std::nullopt;
}

AccountMetadataResult TopologyService::SetDisplayName(SlotTarget origin, const std::u16string &name,
    const CompletionPlan &completion)
{
    return UpdateMetadata(AccountMetadataField::DisplayName, origin, name, completion).acceptance;
}

AccountMetadataResult TopologyService::SetSimLabel(const std::string &iccid, int32_t legacyLabel,
    const CompletionPlan &completion)
{
    if (iccid.empty() || legacyLabel < 1 || legacyLabel > 10) { return AccountMetadataResult::InvalidLabel; }
    return UpdateMetadata(AccountMetadataField::Label, iccid, legacyLabel, completion).acceptance;
}

int32_t TopologyService::SaveImsPreference(SlotTarget origin, int32_t preference, const CompletionPlan &completion)
{
    return UpdateMetadata(AccountMetadataField::ImsPreference, origin, preference, completion).rawResult;
}

int32_t TopologyService::QueryImsPreference(SlotTarget origin, int32_t &preference)
{
    preference = -1; // Legacy UNKNOWN, also the fallback for a missing Provider row or failed query.
    std::string iccid;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        const auto binding = bindings_.Find(origin.slot);
        const auto account = binding ? accounts_.Read(binding->subscription) : std::nullopt;
        if (!supportsMetadata_ || !account || !account->current || account->iccid.empty() ||
            !bindings_.StillMatches(*binding)) {
            return -1;
        }
        iccid = account->iccid;
    }
    try {
        const auto read = store_.QueryImsPreferenceByIccid(iccid);
        if (read.status == ImsPreferenceReadStatus::Value) { preference = read.value; }
    } catch (const std::exception &) {
        // The valid-target Legacy query returns success with UNKNOWN when the Provider cannot supply a value.
    }
    return 0;
}

bool TopologyService::MetadataConflictsWithDiscovery(const std::string &iccid) const
{
    if (pendingMetadata_.empty()) { return false; }
    const auto subscription = accounts_.FindByIccid(iccid);
    return std::any_of(pendingMetadata_.begin(), pendingMetadata_.end(), [&](const auto &entry) {
        const auto &operation = entry.second.evidence.operation;
        return subscription == operation.subscription &&
            (!metadataFieldsIndependent_ || operation.field == AccountMetadataField::Label);
    });
}

TopologyService::MetadataWriteResult TopologyService::UpdateMetadata(AccountMetadataField field,
    const MetadataTarget &target, const MetadataValue &value, const CompletionPlan &completion)
{
    std::unique_lock<std::recursive_mutex> guard(commitLock_);
    if (!acceptingDiscovery_) { return {AccountMetadataResult::Conflict}; }
    if (!supportsMetadata_) { return {AccountMetadataResult::DatabaseWriteFailed}; }

    const auto *location = std::get_if<SlotTarget>(&target);
    const auto binding = location ? bindings_.Find(location->slot) : std::nullopt;
    const auto identity = location ? (binding ? std::make_optional(binding->subscription) : std::nullopt) :
        accounts_.FindByIccid(std::get<std::string>(target));
    const auto account = identity ? accounts_.Read(*identity) : std::nullopt;
    if (!account || account->iccid.empty() ||
        (location && (!binding || !bindings_.StillMatches(*binding) || !account->current))) {
        return {AccountMetadataResult::NoAccount};
    }
    const auto overlaps = [&](const auto &entry) {
        const auto &operation = entry.second.evidence.operation;
        return operation.subscription == *identity &&
            (!metadataFieldsIndependent_ || operation.field == field);
    };
    if (std::any_of(pendingMetadata_.begin(), pendingMetadata_.end(), overlaps) ||
        ((!metadataFieldsIndependent_ || field == AccountMetadataField::Label) &&
         std::any_of(pendingDiscoveries_.begin(), pendingDiscoveries_.end(), [&](const auto &entry) {
             return entry.second.ticket.observation.iccid == account->iccid;
         }))) {
        return {AccountMetadataResult::Conflict};
    }

    using Patch = std::variant<AccountNamePatch, AccountLabelPatch, AccountImsPreferencePatch>;
    const auto prepare = [&]() -> Patch {
        switch (field) {
            case AccountMetadataField::DisplayName:
                return AccountNamePatch {*identity, std::get<std::u16string>(value)};
            case AccountMetadataField::Label:
                return AccountLabelPatch {*identity, std::get<int32_t>(value), account->iccid};
            case AccountMetadataField::ImsPreference:
                return AccountImsPreferencePatch {*identity, std::get<int32_t>(value), account->iccid};
        }
        throw std::logic_error("Invalid account metadata field");
    };
    const auto write = prepare();
    const auto request = metadataOperations_.Begin(SubscriptionTarget {*identity}, completion, std::nullopt);
    const AccountMetadataOperation operation {{request, 1, 0}, *identity, field};
    try {
        if (!metadataOperations_.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt)) {
            metadataOperations_.MarkTerminal(request, TerminalState::Failed);
            metadataOperations_.RetireAfterEvidenceRetention(request);
            return {AccountMetadataResult::Conflict};
        }
        pendingMetadata_.emplace(request, PendingMetadata {{operation, {}}});
    } catch (const std::exception &) {
        metadataOperations_.MarkTerminal(request, TerminalState::Failed);
        metadataOperations_.RetireAfterEvidenceRetention(request);
        return {AccountMetadataResult::Conflict};
    }

    guard.unlock();
    AccountMetadataOutcome outcome;
    try {
        switch (field) {
            case AccountMetadataField::DisplayName:
                outcome = store_.WriteDisplayName(operation, std::get<AccountNamePatch>(write));
                break;
            case AccountMetadataField::Label:
                outcome = store_.WriteLabel(operation, std::get<AccountLabelPatch>(write));
                break;
            case AccountMetadataField::ImsPreference:
                outcome = store_.WriteImsPreference(operation, std::get<AccountImsPreferencePatch>(write));
                break;
        }
    } catch (const std::exception &) {
        // The platform may already hold the original call.
    }
    guard.lock();

    const bool unresolved = !outcome.Settled();
    metadataOperations_.AcceptResult(operation.original, unresolved ? EffectKnowledge::Unknown : outcome.effect);
    auto result = AccountMetadataResult::DatabaseWriteFailed;
    const bool mayAccept = field == AccountMetadataField::ImsPreference ||
        (field == AccountMetadataField::DisplayName ? outcome.legacyResult != -1 : outcome.legacyResult == 0);
    if (outcome.callFinished && outcome.effect == EffectKnowledge::ConfirmedApplied && mayAccept) {
        try {
            result = AcceptMetadata(*account, binding, field, value);
        } catch (const std::exception &) {
            result = AccountMetadataResult::Conflict;
        }
    }
    auto &pending = pendingMetadata_.at(request);
    pending.evidence.outcome = outcome;
    metadataOperations_.MarkTerminal(request, unresolved ? TerminalState::NeedsReconciliation :
        (result == AccountMetadataResult::Success ? TerminalState::Succeeded : TerminalState::Failed));
    if (!metadataOperations_.ClaimCompletion(request)) { return {AccountMetadataResult::Conflict}; }
    if (unresolved) {
        pending.reconcilable = true;
    } else {
        metadataOperations_.RetireAfterEvidenceRetention(request);
        pendingMetadata_.erase(request);
    }
    return {result, outcome.callFinished ? outcome.legacyResult : -1};
}

AccountMetadataResult TopologyService::AcceptMetadata(const AccountRecord &original,
    const std::optional<BindingRef> &binding, AccountMetadataField field, const MetadataValue &value)
{
    const auto current = accounts_.Read(original.subscription);
    if (!current || current->identityRevision != original.identityRevision || current->iccid != original.iccid ||
        (binding && !bindings_.StillMatches(*binding))) {
        return AccountMetadataResult::Conflict;
    }
    bool accepted = true;
    const auto apply = [&](AccountReadView view, const std::optional<AccountProjection> &projection) {
        if (!projection) { return; }
        auto result = AccountAcceptance::ChangedInputs;
        switch (field) {
            case AccountMetadataField::DisplayName:
                result = accounts_.AcceptDisplayName(original.subscription, view, projection->revisions.name,
                    std::get<std::u16string>(value));
                break;
            case AccountMetadataField::Label:
                result = accounts_.AcceptLabel(original.subscription, view, projection->revisions.label,
                    std::get<int32_t>(value));
                break;
            case AccountMetadataField::ImsPreference:
                result = accounts_.ConfirmImsPreferenceWrite(original.subscription, view,
                    projection->revisions.imsPreference);
                break;
        }
        accepted = accepted && result == AccountAcceptance::Accepted;
    };
    apply(AccountReadView::Current, original.current);
    apply(AccountReadView::Historical, original.historical);
    if (!accepted) { return AccountMetadataResult::Conflict; }
    return field == AccountMetadataField::DisplayName && !original.historical ? AccountMetadataResult::HistoricalMissing : AccountMetadataResult::Success;
}

size_t TopologyService::PendingMetadataCount() const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return pendingMetadata_.size();
}

bool TopologyService::ReconcileMetadata(const AccountMetadataOperation &operation)
{
    std::unique_lock<std::recursive_mutex> guard(commitLock_);
    const auto matches = [&] {
        const auto found = pendingMetadata_.find(operation.original.request);
        return found != pendingMetadata_.end() && found->second.reconcilable &&
            found->second.evidence.operation == operation;
    };
    if (!matches()) { return false; }
    guard.unlock();
    std::optional<AccountMetadataEvidence> evidence;
    try {
        evidence = store_.ReadMetadataEvidence(operation);
    } catch (const std::exception &) {
        return false;
    }
    guard.lock();
    if (!matches() || !evidence || !(evidence->operation == operation) || !evidence->outcome.Settled()) { return false; }
    const auto previous = pendingMetadata_.at(operation.original.request).evidence.outcome.effect;
    if (previous != EffectKnowledge::Unknown && previous != evidence->outcome.effect) { return false; }
    if (!metadataOperations_.AcceptLateEvidence(operation.original, evidence->outcome.effect)) { return false; }
    metadataOperations_.RetireAfterEvidenceRetention(operation.original.request);
    pendingMetadata_.erase(operation.original.request);
    return true;
}
} // namespace OHOS::Telephony::MultiSim
