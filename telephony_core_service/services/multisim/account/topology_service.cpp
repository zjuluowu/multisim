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

#include "account/topology_service.h"
#include <set>
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
TopologyService::TopologyService(const SlotCatalog &catalog, AccountStore &store, RuntimeEpoch epoch)
    : bindings_(catalog), observations_(epoch), store_(store),
      useLegacySingleSlotRoles_(catalog.Capacities().configured == 1), discoveryEpoch_(epoch),
      supportsMetadata_(store.SupportsMetadata()), metadataFieldsIndependent_(store.HasIndependentMetadataFields()),
      metadataOperations_(epoch, RequestOwner::AccountMetadata), activationProgress_(epoch) {}

void TopologyService::BindRolePlanner(AccountRolePlanningPort &roles)
{
    roles_ = &roles; // H仅在发布前绑定一次；它和TopologyService不互相拥有生命周期。
}
bool TopologyService::AcceptSlotAssembly(SlotId slot, bool assembled)
{ std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return bindings_.AcceptAssemblyState(slot, assembled); }

std::optional<ActivationTransition> TopologyService::PrepareActivationProgress(const ActivationTransition &transition)
{ return activationProgress_.Prepare(transition); }
bool TopologyService::BeginActivationProgress(const ActivationTransition &transition)
{ return activationProgress_.Begin(transition); }
bool TopologyService::AcceptActivationProgress(const StepRef &owner, const SessionRef &session, bool inProgress)
{ return activationProgress_.Accept(owner, session, inProgress); }
bool TopologyService::ReadLegacyActivationProgress(SlotId slot) const
{ return activationProgress_.ReadLegacy(slot); }

RequestId TopologyService::BeginRouteMapping(const StepRef &, SlotTarget)
{ throw std::logic_error("third-card route mapping is outside the Target C0 topology closure"); }

std::optional<PrimaryTargetFacts> TopologyService::ReadPrimaryTarget(SlotId slot) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto binding = bindings_.Find(slot);
    if (!binding || !bindings_.StillMatches(*binding)) { return std::nullopt; }
    const auto account = accounts_.Read(binding->subscription);
    if (!account || !(account->subscription == binding->subscription) || account->iccid.empty() ||
        !account->current) {
        return std::nullopt;
    }
    return PrimaryTargetFacts {*binding, account->iccid, account->current->active};
}

bool TopologyService::WithCurrentPrimaryTarget(const PrimaryTargetFacts &expected,
    const std::function<void()> &admit) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto actual = ReadPrimaryTarget(expected.binding.slot);
    if (!actual || !actual->active || actual->binding.bindingRevision != expected.binding.bindingRevision ||
        !(actual->binding.subscription == expected.binding.subscription) || actual->iccid != expected.iccid) {
        return false;
    }
    admit(); // A's short effect registration only. No I/O or delivery under T's source domain.
    return true;
}

void TopologyService::AcceptCardDiscoveryObservation(const CardDiscoveryObservation &observation)
{
    (void)DiscoverAccount(observation);
}

bool TopologyService::RegisterCardDiscoveryObservation(const CardDiscoveryObservation &observation)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return acceptingDiscovery_ && bindings_.CanObserve(observation.session.application) &&
        observations_.AcceptDiscovery(observation);
}

std::optional<bool> TopologyService::MatchesObservedIdentity(const SessionRef &session, const std::string &iccid) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return observations_.MatchesIdentity(session, iccid);
}

bool TopologyService::RevokePendingIdentity(const SessionRef &session, uint64_t sequence)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto source = bindings_.Source(session.application.slot);
    return (!source || !(*source == session)) && observations_.RevokeIdentity(session, sequence);
}

DiscoveryAcceptance TopologyService::DiscoverAccount(const CardDiscoveryObservation &observation)
{
    DiscoveryAcceptance rejected {};
    std::optional<DiscoveryStoreTicket> ticket;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        if (!acceptingDiscovery_ || !roles_ || !bindings_.CanObserve(observation.session.application) ||
            MetadataConflictsWithDiscovery(observation.iccid) ||
            (!observations_.StillCurrent(observation) && !observations_.AcceptDiscovery(observation))) {
            return rejected;
        }
        ticket = observations_.BeginStore(observation, bindings_);
        if (!ticket) { return rejected; }
        rejected.observationAdmitted = true;
        try {
            pendingDiscoveries_.emplace(ticket->admission, PendingDiscovery {*ticket});
        } catch (const std::exception &) {
            observations_.ReleaseStore(*ticket);
            throw;
        }
    }
    const DiscoveryCallRef call {discoveryEpoch_, ticket->admission};

    try {
        auto matched = store_.MatchDiscoveryAccount(call, observation.iccid);
        rejected.storage = {matched.legacyResult, false, matched.callFinished};
        if (!matched.callFinished) {
            std::lock_guard<std::recursive_mutex> guard(commitLock_);
            auto &pending = pendingDiscoveries_.at(call.admission);
            pending.phase = DiscoveryPhase::WaitingMatch;
            pending.executing = false;
            return rejected;
        }

        {
            std::lock_guard<std::recursive_mutex> guard(commitLock_);
            pendingDiscoveries_.at(call.admission).phase = DiscoveryPhase::Planning;
        }
        const auto identity = matched.existing ? std::make_optional(matched.existing->subscription) : std::nullopt;
        {
            std::lock_guard<std::recursive_mutex> guard(commitLock_);
            const auto known = accounts_.FindByIccid(observation.iccid);
            if (matched.legacyResult != 0 ||
                (matched.existing && (matched.existing->iccid != observation.iccid ||
                    matched.existing->subscription.legacySimId < 0)) ||
                (known && !(identity == known)) || !acceptingDiscovery_ ||
                !observations_.CanPublish(*ticket, bindings_) ||
                (identity && !bindings_.PrepareBinding(observation.session.application.slot, *identity,
                    ticket->bindingRevision))) {
                FinishWithoutWrite(call);
                return rejected;
            }
        }

        const auto role = roles_->PrepareAccountRoleFragment(
            {identity, observation.session.application.slot, useLegacySingleSlotRoles_, call});
        if (!role) {
            FinishWithoutWrite(call);
            return rejected;
        }
        {
            std::lock_guard<std::recursive_mutex> guard(commitLock_);
            pendingDiscoveries_.at(call.admission).rolePlan = role;
        }
        const AccountDiscoveryWritePlan plan {observation, ticket->bindingRevision, identity,
            {observation.session.application.slot, observation.iccid,
             observation.legacyLabel ? observation.legacyLabel :
                 (matched.existing ? matched.existing->legacyLabel : std::nullopt),
             observation.active, observation.isEsim}, *role, call};
        const bool roleOwned = roles_->StillOwnsInitialRoleReservation(*role);
        bool send = false;
        {
            std::lock_guard<std::recursive_mutex> guard(commitLock_);
            auto &pending = pendingDiscoveries_.at(call.admission);
            if (acceptingDiscovery_ && roleOwned && observations_.CanPublish(*ticket, bindings_) &&
                bindings_.CanObserve(observation.session.application)) {
                pending.matched = std::move(matched.existing);
                pending.plan = plan;
                pending.phase = DiscoveryPhase::Writing;
                send = true; // Seal and the one original send grant share this domain.
            }
        }
        if (!send) {
            FinishWithoutWrite(call);
            return rejected;
        }
        auto accepted = ContinueDiscovery(call, store_.WriteDiscoveredAccount(plan));
        accepted.observationAdmitted = true;
        return accepted;
    } catch (const std::exception &) {
        const auto receipt = ReadDiscoveryReceipt(call);
        if (receipt && receipt->phase == DiscoveryPhase::Planning) {
            try {
                FinishWithoutWrite(call);
            } catch (const std::exception &) {
                MarkDiscoveryReturned(call);
            }
        } else {
            MarkDiscoveryReturned(call);
        }
        return rejected;
    }
}

void TopologyService::MarkDiscoveryReturned(DiscoveryCallRef call)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto found = pendingDiscoveries_.find(call.admission);
    if (found == pendingDiscoveries_.end()) { return; }
    auto &pending = found->second;
    if (pending.phase == DiscoveryPhase::Matching) { pending.phase = DiscoveryPhase::WaitingMatch; }
    if (pending.phase == DiscoveryPhase::Writing) { pending.phase = DiscoveryPhase::WaitingEvidence; }
    pending.executing = false;
}

void TopologyService::FinishWithoutWrite(DiscoveryCallRef call)
{
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        auto &pending = pendingDiscoveries_.at(call.admission);
        if (pending.rolePlan) {
            pending.finalization = InitialRoleFinalizationEvidence {true, EffectKnowledge::NotSent, true, call};
            pending.phase = DiscoveryPhase::Finalizing;
        } else {
            observations_.ReleaseStore(pending.ticket);
            pendingDiscoveries_.erase(call.admission);
            return;
        }
    }
    (void)FinalizeDiscovery(call);
}

DiscoveryAcceptance TopologyService::ContinueDiscovery(DiscoveryCallRef call,
    const std::optional<AccountDiscoveryStoreResult> &result)
{
    DiscoveryAcceptance acceptance {};
    std::optional<AccountRoleFragment> role;
    std::optional<SubscriptionId> identity;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        auto &pending = pendingDiscoveries_.at(call.admission);
        const auto &plan = *pending.plan;
        const auto columns = plan.Columns();
        if (result && pending.phase == DiscoveryPhase::Writing) {
            pending.originalLegacyResult = result->legacyResult;
        }
        acceptance.legacyResult = pending.originalLegacyResult;
        if (result) {
            acceptance.storedSubscription = result->assignedSubscription;
            acceptance.storage = {pending.originalLegacyResult, result->writeConfirmed, result->callFinished};
        }
        pending.phase = DiscoveryPhase::WaitingEvidence;
        if (result && result->evidence && result->evidence->call == call) {
            const auto &evidence = *result->evidence;
            if (((evidence.applied | evidence.notApplied) & ~columns) != 0 ||
                ((evidence.applied | pending.applied) & (evidence.notApplied | pending.notApplied)) != 0 ||
                (pending.assigned && result->assignedSubscription && !(pending.assigned == result->assignedSubscription)) ||
                (plan.existingSubscription && result->assignedSubscription &&
                    !(plan.existingSubscription == result->assignedSubscription))) {
                pending.ambiguous = true;
            } else {
                if (evidence.applied == columns && pending.applied != columns) { pending.position = result->position; }
                pending.applied |= evidence.applied;
                pending.notApplied |= evidence.notApplied;
                if (result->assignedSubscription) { pending.assigned = result->assignedSubscription; }
                pending.callFinished = pending.callFinished || result->callFinished;
            }
        }
        if (!pending.ambiguous && pending.callFinished && pending.notApplied == columns) {
            pending.finalization = InitialRoleFinalizationEvidence {true, EffectKnowledge::ConfirmedNotApplied,
                true, call};
            pending.phase = DiscoveryPhase::Finalizing;
        } else if (!pending.ambiguous && pending.callFinished && pending.applied == columns && pending.assigned &&
            pending.assigned->legacySimId >= 0) {
            role = pending.rolePlan;
            identity = pending.assigned;
        } else {
            pending.executing = false;
            return acceptance;
        }
    }
    if (role) {
        if (!roles_->AcceptDiscoveredRoleProjection(*role, *identity)) {
            MarkDiscoveryReturned(call);
            return acceptance;
        }
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        auto &pending = pendingDiscoveries_.at(call.admission);
        const auto &plan = *pending.plan;
        const auto &source = pending.ticket.observation.session;
        const bool current = observations_.CanPublish(pending.ticket, bindings_) &&
            bindings_.CanObserve(source.application);
        const auto prepared = current ?
            bindings_.PrepareBinding(source.application.slot, *identity, pending.ticket.bindingRevision) : std::nullopt;
        if (current && !prepared) {
            pending.executing = false;
            return acceptance;
        }
        std::optional<SessionRef> preparedSource = prepared ? std::make_optional(source) : std::nullopt;
        AccountProjection projection {{}, {}, plan.accountFields.legacyLabel, std::nullopt,
            plan.accountFields.active, {}, plan.accountFields.legacyPosition};
        if (pending.matched) {
            projection.displayName = pending.matched->name;
            projection.displayNumber = pending.matched->number;
            projection.imsPreference = pending.matched->imsPreference;
        }
        const AccountRecord account {*identity, plan.accountFields.iccid, 1,
            current ? std::make_optional(projection) : std::nullopt, projection};
        if (accounts_.PublishDiscovery(account, plan.accountFields.legacyLabel.has_value(), pending.position) !=
            AccountAcceptance::Accepted) {
            pending.executing = false;
            return acceptance;
        }
        RevokeReloadRoles();
        // The binding and the original witness become visible in the same T domain.
        if (prepared) { bindings_.PublishBinding(*prepared, std::move(preparedSource)); }
        pending.published = prepared;
        pending.finalization = InitialRoleFinalizationEvidence {true, EffectKnowledge::ConfirmedApplied,
            true, call, identity, !current, pending.position};
        pending.phase = DiscoveryPhase::Finalizing;
        acceptance.bindingAccepted = prepared.has_value();
        acceptance.storedSubscription = identity;
    }
    (void)FinalizeDiscovery(call);
    return acceptance;
}

bool TopologyService::FinalizeDiscovery(DiscoveryCallRef call)
{
    std::optional<AccountRoleFragment> role;
    std::optional<InitialRoleFinalizationEvidence> disposition;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        const auto &pending = pendingDiscoveries_.at(call.admission);
        role = pending.rolePlan;
        disposition = pending.finalization;
    }
    // A retains the exact result until T acknowledges the returned disposition.
    if (!roles_->FinalizeInitialRoleReservation(*role, *disposition)) {
        MarkDiscoveryReturned(call);
        return false;
    }
    roles_->ConfirmInitialRoleReceipt(*role, *disposition);
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto found = pendingDiscoveries_.find(call.admission);
    observations_.ReleaseStore(found->second.ticket);
    pendingDiscoveries_.erase(found);
    return true;
}

bool TopologyService::ReconcileDiscovery(DiscoveryCallRef call)
{
    DiscoveryPhase phase;
    bool alreadyProven = false;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        const auto found = pendingDiscoveries_.find(call.admission);
        if (call.epoch != discoveryEpoch_ || found == pendingDiscoveries_.end() || found->second.executing ||
            found->second.ambiguous) { return false; }
        auto &pending = found->second;
        pending.executing = true;
        phase = pending.phase;
        alreadyProven = pending.plan && pending.callFinished &&
            (pending.applied == pending.plan->Columns() || pending.notApplied == pending.plan->Columns());
    }
    try {
        if (phase == DiscoveryPhase::Finalizing) { return FinalizeDiscovery(call); }
        if (phase == DiscoveryPhase::WaitingMatch && store_.DiscoveryMatchReturned(call)) {
            FinishWithoutWrite(call);
            return !ReadDiscoveryReceipt(call);
        }
        if (phase == DiscoveryPhase::WaitingEvidence) {
            const auto evidence = alreadyProven ? std::nullopt : store_.ReadDiscoveryEvidence(call);
            if (alreadyProven || evidence) {
                (void)ContinueDiscovery(call, *evidence);
                return !ReadDiscoveryReceipt(call);
            }
        }
    } catch (const std::exception &) {
        MarkDiscoveryReturned(call);
        return false;
    }
    MarkDiscoveryReturned(call);
    return false;
}

bool TopologyService::ReconcileDiscovery(const CardDiscoveryObservation &observation)
{
    std::optional<DiscoveryCallRef> call;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        for (const auto &[id, pending] : pendingDiscoveries_) {
            const auto &original = pending.ticket.observation;
            if (original.session.epoch == observation.session.epoch &&
                original.session.application.slot == observation.session.application.slot &&
                original.session.application.applicationKey == observation.session.application.applicationKey &&
                original.session.bindingGeneration == observation.session.bindingGeneration &&
                original.observationSequence == observation.observationSequence && original.iccid == observation.iccid) {
                call = DiscoveryCallRef {discoveryEpoch_, id};
                break;
            }
        }
    }
    return call && ReconcileDiscovery(*call);
}

void TopologyService::SealDiscovery()
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    acceptingDiscovery_ = false;
}

size_t TopologyService::PendingDiscoveryCount() const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return pendingDiscoveries_.size();
}

std::optional<TopologyService::DiscoveryReceipt> TopologyService::ReadDiscoveryReceipt(DiscoveryCallRef call) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto found = pendingDiscoveries_.find(call.admission);
    if (call.epoch != discoveryEpoch_ || found == pendingDiscoveries_.end()) { return std::nullopt; }
    return MakeDiscoveryReceipt(call, found->second);
}

TopologyService::DiscoveryReceipt TopologyService::MakeDiscoveryReceipt(DiscoveryCallRef call,
    const PendingDiscovery &pending)
{
    return DiscoveryReceipt {call, pending.phase, pending.executing, pending.applied, pending.notApplied,
        pending.rolePlan ? std::make_optional(pending.rolePlan->reservation.token) : std::nullopt, pending.published};
}

std::vector<TopologyService::DiscoveryReceipt> TopologyService::ReadPendingDiscoveries() const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    std::vector<DiscoveryReceipt> receipts;
    receipts.reserve(pendingDiscoveries_.size());
    for (const auto &[admission, pending] : pendingDiscoveries_) {
        receipts.push_back(MakeDiscoveryReceipt({discoveryEpoch_, admission}, pending));
    }
    return receipts;
}

void TopologyService::AcceptCardSessionInvalidation(const CardSessionInvalidation &invalidation)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    if (observations_.AcceptInvalidation(invalidation)) {
        bindings_.InvalidateSource(invalidation.invalidatedSession);
    }
}
void TopologyService::AcceptCardRemoval(const BindingRef &expected)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    bindings_.RemoveBinding(expected);
}

std::optional<BindingRef> TopologyService::FindBinding(SlotId slot) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return bindings_.Find(slot);
}
std::optional<AccountView> TopologyService::ReadAccount(SubscriptionId subscription, AccountReadView view) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto record = accounts_.Read(subscription);
    const auto projection = accounts_.ReadProjection(subscription, view);
    if (!record || !projection) { return std::nullopt; }
    return AccountView {subscription, view, projection->displayName, projection->displayNumber,
        projection->active, record->identityRevision, projection->revisions, projection->legacyLabel};
}

AccountAcceptance TopologyService::AcceptActivationProjection(SubscriptionId subscription,
    AccountReadView view, uint64_t expectedActiveRevision, bool active)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    accounts_.ReserveReloadFieldProtection(subscription);
    const auto accepted = accounts_.AcceptActiveProjection(subscription, view, expectedActiveRevision, active);
    if (accepted == AccountAcceptance::Accepted) {
        accounts_.ProtectReloadFields(subscription, view, ReloadActive, {});
    }
    return accepted;
}

void TopologyService::RevokeReloadRoles()
{
    for (auto &permit : reloadDeliveries_) {
        if (const auto delivery = permit.lock()) { delivery->Revoke(); }
    }
}

void TopologyService::SealReload()
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    reloadOpen_ = false;
    RevokeReloadRoles();
}

ReloadRequest TopologyService::PrepareAccountReload(AccountReadView view, uint64_t)
{
    const bool historical = view == AccountReadView::Historical;
    const auto expectedRoleRevision = roles_ ? roles_->ReadRoleProjectionRevision(historical) : 0;
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    if (!reloadOpen_) { return {{view, 0, discoveryEpoch_}, {view, 0, 0, {}}, 0, {}}; }
    if (const auto delivery = reloadDeliveries_[historical].lock()) { delivery->Revoke(); }
    auto ticket = accounts_.BeginReload(view);
    std::vector<ReloadAccountInput> affected;
    for (const auto &[subscription, expected] : ticket.expected) {
        affected.push_back({subscription, expected.identityRevision, expected.fields});
    }
    const auto attempt = ticket.attempt;
    return {{view, attempt, discoveryEpoch_}, std::move(ticket), expectedRoleRevision, std::move(affected)};
}

ReloadResult TopologyService::ReloadAccounts(AccountReadView view)
{
    const auto request = PrepareAccountReload(view, 0);
    if (!request.storeRequest.loadAttempt) { return {}; }
    AccountLoadResult loaded {request.storeRequest, -1, false, false, {}, {}};
    try {
        loaded = store_.LoadAccounts(request.storeRequest);
    } catch (const std::exception &) {
        // A failed read leaves both projections untouched.
    }
    return AcceptAccountReload(request, loaded);
}

ReloadResult TopologyService::AcceptAccountReload(const ReloadRequest &request, const AccountLoadResult &loaded)
{
    ReloadResult result {true, loaded.legacyResult, loaded.legacyReturn};
    const auto view = request.storeRequest.view;
    const bool historical = view == AccountReadView::Historical;
    const auto sameRead = loaded.request == request.storeRequest &&
        request.storeRequest.epoch == discoveryEpoch_ && request.acceptance.view == view &&
        request.acceptance.attempt == request.storeRequest.loadAttempt;
    const bool imageValid = !loaded.image ||
        (loaded.image->sample == loaded.request && loaded.image->position.Valid());
    bool complete = sameRead && imageValid && loaded.complete && loaded.legacyResult == 0;
    std::vector<AccountRecord> records;
    std::vector<uint8_t> columns;
    std::set<SubscriptionId> identities;
    std::set<SubscriptionId> roleIdentities;
    if (complete) {
        records.reserve(loaded.rows.size());
        columns.reserve(loaded.rows.size());
        for (const auto &row : loaded.rows) {
            complete &= identities.insert(row.subscription).second;
            if (!historical && (!(row.validReloadFields & ReloadPosition) ||
                !row.legacyPosition || row.legacyPosition->legacySlotId < 0)) { complete = false; }
            const AccountProjection projection {row.name, row.number, row.legacyLabel, row.imsPreference,
                row.active, {}, row.legacyPosition};
            records.push_back({row.subscription, row.iccid, 1,
                historical ? std::nullopt : std::make_optional(projection),
                historical ? std::make_optional(projection) : std::nullopt});
            columns.push_back(row.validReloadFields);
        }
    }
    bool roleRowsComplete = loaded.roleColumnsValid;
    if (complete && roleRowsComplete) {
        for (const auto &row : loaded.storedRoles) {
            roleRowsComplete &= roleIdentities.insert(row.subscription).second;
        }
        roleRowsComplete &= roleIdentities == identities;
    }
    std::shared_ptr<RoleReloadDelivery> delivery;
    if (complete && roleRowsComplete && roles_) {
        delivery = std::make_shared<RoleReloadDelivery>();
        delivery->epoch = discoveryEpoch_;
        delivery->attempt = request.storeRequest.loadAttempt;
        delivery->expectedRevision = request.expectedRoleProjectionRevision;
        delivery->historical = historical;
        if (loaded.image && loaded.image->coversRoles) { delivery->position = loaded.image->position; }
        delivery->rows = loaded.storedRoles;
    }
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        if (!reloadOpen_) { result.roles = RoleReloadDisposition::Revoked; return result; }
        std::vector<AccountReloadPending> pending;
        pending.reserve(pendingMetadata_.size() + pendingDiscoveries_.size());
        for (const auto &[id, entry] : pendingMetadata_) {
            (void)id;
            const auto &operation = entry.evidence.operation;
            pending.push_back({operation.subscription, ReloadField(operation.field)});
        }
        for (const auto &[id, entry] : pendingDiscoveries_) {
            (void)id;
            const auto subscription = accounts_.FindByIccid(entry.ticket.observation.iccid);
            if (subscription) {
                const auto fields = ReloadPosition | ReloadActive |
                    (entry.plan && entry.plan->accountFields.legacyLabel ? ReloadLabel : 0);
                pending.push_back({*subscription, static_cast<uint8_t>(fields)});
            }
        }
        result.accounts = accounts_.AcceptReload(request.acceptance, complete, records, columns,
            loaded.image, pending, !pendingDiscoveries_.empty(), true);
        if (delivery && result.accounts.roleMembershipEligible) { reloadDeliveries_[historical] = delivery; }
        else { delivery.reset(); }
    }
    if (delivery) {
        try {
            roles_->AcceptReloadedRoleProjectionSet(*delivery);
        } catch (const std::exception &) {
            auto available = RoleReloadDisposition::Available;
            (void)delivery->receipt.compare_exchange_strong(available, RoleReloadDisposition::Rejected);
        }
        result.roles = delivery->receipt.load();
    }
    return result;
}

std::optional<BindingRef> TopologyService::AcceptStoredDiscovery(const CardDiscoveryObservation &observation,
    const AccountRecord &account, const AccountDiscoveryStoreResult &stored)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto existingIdentity = accounts_.FindByIccid(observation.iccid);
    if ((existingIdentity && !(*existingIdentity == account.subscription)) ||
        !stored.callFinished || !stored.writeConfirmed || stored.effect != EffectKnowledge::ConfirmedApplied ||
        !stored.assignedSubscription || !(*stored.assignedSubscription == account.subscription) ||
        account.iccid != observation.iccid || account.identityRevision == 0) {
        return std::nullopt;
    }
    (void)observations_.AcceptDiscovery(observation);
    const auto ticket = observations_.BeginStore(observation, bindings_);
    if (!ticket) { return std::nullopt; }
    const auto existing = accounts_.Read(account.subscription);
    if (!existing) {
        if (accounts_.AcceptDiscoveredAccount(account) != AccountAcceptance::Accepted) { return std::nullopt; }
    } else if (existing->iccid != account.iccid || existing->identityRevision != account.identityRevision) {
        return std::nullopt;
    }
    const auto accepted = observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
        stored.callFinished, bindings_);
    if (!accepted.bindingAccepted) { return std::nullopt; }
    const auto binding = bindings_.Find(observation.session.application.slot);
    accounts_.ProtectDiscovery(account.subscription, account.current.has_value(), false, stored.position);
    RevokeReloadRoles();
    bindings_.PublishBinding(*binding, observation.session);
    return binding;
}

/* DiscoverAccount 已执行写前资格、锁外I/O与最终接纳；下列其余方法仍保留协议说明：
 *
 * AcceptNumberAfterCardWrite / AcceptIndependentNumberBackwrite：
 * 1. T短提交域核查binding/session、身份及号码字段版本，ReserveNumberField。
 *    显示名、IMS、label、active改变不使号码资格失效；同号码冲突资格覆盖整个调用。
 * 2. 锁外WriteDisplayNumber；返回后复核并依旧节点分别接纳Current/Historical，
 *    每投影使用自己的numberRevision，当前成功/历史失败保留原部分效果。
 * 3. 调用已结束且结果已接纳后才释放；已写但目标失效进入核对，不盲重发写卡/写库。
 *
 * AcceptCardDiscoveryObservation：
 * 1. 与AcceptCardSessionInvalidation同一T接纳序列，observations_.AcceptDiscovery。
 *    observations_.BeginStore固定独立admission/绑定revision；序列/epoch失效停止尚未发出的步骤。
 *    匹配读取或纯规划失败走EndPreparationFailure；已结束确认未生效时原用例按旧预算
 *    RetryAfterKnownNoEffect在同一有效观察上取得新admission，不要求新R事件、不因重复观察重发。
 * 2. 锁外MatchAccountByIccid；返回后复核观察及绑定键。向roles_请求角色片段，输入
 *    可以只有位置/既有策略，existingSubscription为空不强求插入前未知的新simId。
 *    父子关联、不嵌套等待；跨Owner冲突集合一次登记，失败不得持一个Owner资格等另一个。
 * 3. 经roles_->StillOwnsInitialRoleReservation复核A角色预留及T受影响键资格后，
 *    锁外WriteDiscoveredAccount一次旧Insert/Update。
 * 4. 返回后observations_.StillCurrent、绑定键和roles_->StillOwnsInitialRoleReservation分别复核。
 *    provider返回身份/effect/callFinished交observations_.CompleteStored最终接纳绑定或登记核对；
 *    Unknown/已写或未结束不能重试，旧失败不影响后来票据；
 *    角色片段经roles_->AcceptDiscoveredRoleProjection交A；不整行回灌或恢复旧快照。
 *    若已写而观察失效，保存assignedSubscription/部分效果进入核对，不绑定到新卡。
 *    通过FinalizeInitialRoleReservation(planned,{callFinished,storageEffect,projectionsAcceptedOrReconciled})
 *    交A核验释放；DB已写但观察失效时先核对，不以callFinished单独释放；T资格遵守相同结束/核对条件。
 * AcceptCardSessionInvalidation：按同一序列失效观察，再以原BindingRef撤销相关键；
 *    R合法观察校验不替代本T序列及锁外I/O后的复核，禁止回包时补贴当前会话。
 *
 * PrepareAccountReload / AcceptAccountReload：Current与Historical分别发起LoadAccounts，
 * 保存本次view/attempt、相关身份/各字段版本及A角色投影版本。失败只保留该view旧值并
 * 沿原预算重试，不阻止另一view成功；同次读取的storedRoles交roles_->AcceptReloadedRoleProjectionSet。
 * T在发起时accounts_.BeginReload生成独立view/attempt及成员快照；结果按accounts_.AcceptReload
 * 接纳整次读取与成员移除，再用AcceptReloadProjection按相关版本接纳字段；同值重载不增号码版本，
 * 旧读取不覆盖新写的号码等字段，也不能删除读取后新增/更新的成员；
 * 同一行较新名称可以保留，而未冲突号码/active仍可接受。不要从A当前态重造旧DB角色。
 *
 * 原执行接纳点先PrepareActivationProgress取得T admission，再BeginActivationProgress；
 * AcceptActivationProgress按完整epoch/owner/request/step/attempt/session准入，
 * 在原两次Hook及失败出口更新槽进度；false不释放R资源、不证明设备静止。
 *
 * G2仍保留SetShowNumberToDB在该步骤GetTargetSimId(slot)的解析时点及旧错误轨迹；
 * 固定目标/陈旧拒绝为G3增强。名称、IMS、标签、初始化、current/all重载、Monitor、
 * provider/用户重连、卡探测、eSIM插入和普通/force/卫星旧写者全部按WU委托，不能双写。
 */
} // namespace OHOS::Telephony::MultiSim
