/* Copyright (c) 2026 multisim contributors. Licensed under the Apache License, Version 2.0. */
// Host-only tests of PROPOSED pure admission rules; no production writer or device path.
#include <cassert>
#include <iostream>
#include "topology/domain/account_registry.h"
#include "topology/domain/binding_index.h"
#include "topology/application/activation_progress.h"
#include "topology/application/discovery_admission.h"
#include "topology/application/show_number_plan.h"
using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr auto CURRENT = AccountReadView::Current;
constexpr auto ALL = AccountReadView::Historical;
AccountStorageRow Row(int id, std::u16string number = u"old", bool active = false)
{
    return {{id}, "iccid" + std::to_string(id), {SlotId {id - 1}, u"name", number, active, 2, 1}};
}
SessionRef Session(int slot, uint64_t generation)
{
    return {{7, 9}, {{slot}, "sim"}, generation};
}
SlotCatalog Catalog()
{
    return {{{{0}, EndpointKind::Physical, 0, RadioDomainId {"shared"}, VisibilityClass::Ordinary, false},
             {{1}, EndpointKind::Physical, 1, RadioDomainId {"shared"}, VisibilityClass::Ordinary, false}}, {2, 2, 2, 2, 2}};
}
void IndependentReloads()
{
    AccountRegistry accounts;
    auto current = accounts.BeginReload(CURRENT, 0);
    auto all = accounts.BeginReload(ALL, 0);
    assert(accounts.AcceptReload(current, true, {Row(1)}).acceptedAttempt);
    assert(!accounts.AcceptReload(all, false, {}).acceptedAttempt);
    assert(accounts.Read({1})->current && !accounts.Read({1})->historical);
    all = accounts.BeginReload(ALL, 0);
    assert(accounts.AcceptReload(all, true, {Row(1, u"history")}).acceptedAttempt);
    current = accounts.BeginReload(CURRENT, 0);
    assert(!accounts.AcceptReload(current, false, {}).acceptedAttempt);
    assert(accounts.Read({1})->current->data.displayNumber == u"old");
    assert(accounts.Read({1})->historical->data.displayNumber == u"history");
    current = accounts.BeginReload(CURRENT, 0);
    assert(accounts.AcceptReload(current, true, {Row(1, u"new")}).acceptedAttempt);
    assert(accounts.Read({1})->historical->data.displayNumber == u"history");
    const auto first = accounts.BeginReload(CURRENT, 0);
    const auto second = accounts.BeginReload(CURRENT, 0);
    assert(!accounts.AcceptReload(first, true, {Row(1, u"late")}).acceptedAttempt);
    assert(accounts.AcceptReload(second, true, {Row(1, u"latest")}).acceptedAttempt);
    assert(!accounts.AcceptReload(second, true, {}).acceptedAttempt);
    // Empty successful query replaces only this view; failure never clears it.
    all = accounts.BeginReload(ALL, 0);
    accounts.AcceptReload(all, true, {});
    assert(accounts.Read({1})->current && !accounts.Read({1})->historical);
}
void FieldsAndPartialVisibility()
{
    AccountRegistry accounts;
    accounts.AcceptReload(accounts.BeginReload(CURRENT, 0), true, {Row(1)});
    accounts.AcceptReload(accounts.BeginReload(ALL, 0), true, {Row(1)});
    const auto number = accounts.ReadNumberVersions({1}).value();
    auto renameReload = accounts.BeginReload(CURRENT, 0);
    auto onlyName = Row(1);
    onlyName.projection.displayName = u"reload name";
    accounts.AcceptReload(renameReload, true, {onlyName});
    assert(accounts.NumberInputsMatch({1}, number)); // unchanged number value retains its version.
    const auto staleReload = accounts.BeginReload(CURRENT, 0);
    assert(accounts.AcceptDisplayName({1}, accounts.Read({1})->current->versions.name, CURRENT, u"renamed") ==
        AccountAcceptance::Accepted);
    assert(accounts.NumberInputsMatch({1}, number)); // unrelated name/label do not reject number.
    assert(accounts.AcceptNumberVisibility({1}, *number.current, CURRENT, u"card number") == AccountAcceptance::Accepted);
    const auto loaded = accounts.AcceptReload(staleReload, true, {Row(1, u"stale")});
    assert(loaded.acceptedAttempt && loaded.conflictedFields == 2);
    assert(accounts.Read({1})->current->data.displayName == u"renamed");
    assert(accounts.Read({1})->current->data.displayNumber == u"card number");
    assert(!accounts.NumberInputsMatch({1}, number));
    assert(accounts.AcceptNumberVisibility({1}, *number.current, CURRENT, u"conflict") == AccountAcceptance::ChangedInputs);
    // DB effect may already exist; keep first cache node when historical node has disappeared.
    const auto currentActiveVersion = accounts.Read({1})->current->versions.active;
    const auto historicalActiveVersion = accounts.Read({1})->historical->versions.active;
    accounts.AcceptReload(accounts.BeginReload(ALL, 0), true, {});
    assert(accounts.AcceptActiveProjection({1}, currentActiveVersion, CURRENT, true) == AccountAcceptance::Accepted);
    assert(accounts.AcceptActiveProjection({1}, historicalActiveVersion, ALL, true) == AccountAcceptance::MissingAccount);
    assert(accounts.Read({1})->current->data.active);
    // A load with an older snapshot cannot remove a locally changed row.
    const auto deletion = accounts.BeginReload(CURRENT, 0);
    accounts.AcceptDisplayName({1}, accounts.Read({1})->current->versions.name, CURRENT, u"keep");
    const auto outcome = accounts.AcceptReload(deletion, true, {});
    assert(outcome.conflictedFields == 1 && accounts.Read({1})->current);
}
void BindingAndDiscovery()
{
    auto catalog = Catalog();
    BindingIndex bindings(catalog);
    DiscoveryAdmission admission({7, 9});
    assert(bindings.Revision({0}) == 0);
    auto a = Session(0, 1);
    assert(admission.Fence(a, 1));
    const auto first = admission.Begin({a, 1, "A"}, bindings).value();
    assert(!admission.Begin({a, 1, "A"}, bindings)); // duplicate observation consumed once.
    assert(admission.IsCurrent(first, bindings));
    auto b = Session(0, 2);
    assert(admission.Fence(b, 2)); // replacement invalidates before old DB returns.
    assert(!admission.IsCurrent(first, bindings));
    assert(!admission.Finish(first, bindings));
    assert(!admission.Fence(a, 1));
    assert(!admission.Begin({a, 1, "A"}, bindings));
    // Even if A's provider write happened, it grants no binding publication qualification.
    const auto lateStorage = admission.CompleteStored(first, EffectKnowledge::ConfirmedApplied, SubscriptionId {1}, true, bindings);
    assert(lateStorage.needsFieldReconciliation && !lateStorage.bindingAccepted && !bindings.Find({0}));
    auto second = admission.Begin({b, 2, "B"}, bindings).value();
    auto stored = admission.CompleteStored(second, EffectKnowledge::ConfirmedApplied, SubscriptionId {2}, true, bindings);
    assert(stored.bindingAccepted && !stored.needsFieldReconciliation);
    assert(admission.CompleteStored(second, EffectKnowledge::ConfirmedApplied, SubscriptionId {2}, true, bindings).duplicate);
    assert(bindings.Revision({0}) == 1);
    assert(!bindings.AcceptBinding({1}, {2}, 0)); // same account cannot occupy two current slots.
    const auto bound = bindings.Find({0}).value();
    assert(bindings.RemoveBinding(bound));
    assert(bindings.Revision({0}) == 2 && !bindings.Find({0}));
    assert(!bindings.RemoveBinding(bound));
    assert(!bindings.AcceptBinding({0}, {2}, 1));
    assert(bindings.AcceptBinding({0}, {2}, 2));
    assert(bindings.Find({0})->bindingRevision == 3);
    auto other = Session(1, 1);
    assert(admission.Fence(other, 1));
    auto unaffected = admission.Begin({other, 1, "other"}, bindings).value();
    assert(admission.Fence(Session(0, 3), 3));
    assert(admission.IsCurrent(unaffected, bindings)); // unrelated slot continues.
    auto unknown = admission.CompleteStored(unaffected, EffectKnowledge::Unknown, std::nullopt, true, bindings);
    assert(unknown.needsFieldReconciliation && !unknown.bindingAccepted && !bindings.Find({1}));
    assert(!admission.RetryAfterKnownNoEffect(unaffected, bindings)); // Unknown cannot rearm despite ended wait.
    assert(admission.Fence(Session(1, 2), 2));
    auto failedTicket = admission.Begin({Session(1, 2), 2, "other"}, bindings).value();
    auto failed = admission.CompleteStored(failedTicket, EffectKnowledge::ConfirmedNotApplied, std::nullopt, true, bindings);
    assert(!failed.needsFieldReconciliation && !failed.bindingAccepted);
    auto wrongIdentity = failedTicket;
    wrongIdentity.observation.iccid = "different ICCID";
    assert(!admission.RetryAfterKnownNoEffect(wrongIdentity, bindings));
    const auto retry = admission.RetryAfterKnownNoEffect(failedTicket, bindings).value();
    assert(retry.admission != failedTicket.admission && retry.observation.observationSequence == 2);
    assert(!admission.Begin({Session(1, 2), 2, "other"}, bindings)); // duplicate input still does not retry.
    assert(!admission.RetryAfterKnownNoEffect(failedTicket, bindings)); // old failure cannot rearm new request.
    admission.CompleteStored(failedTicket, EffectKnowledge::ConfirmedNotApplied, std::nullopt, true, bindings);
    assert(admission.IsCurrent(retry, bindings)); // repeated old failure does not end current retry.
    const auto retried = admission.CompleteStored(retry, EffectKnowledge::ConfirmedApplied, SubscriptionId {3}, true, bindings);
    assert(retried.bindingAccepted && !retried.needsFieldReconciliation);
    assert(!admission.RetryAfterKnownNoEffect(retry, bindings)); // applied writes are never automatically retried.
    assert(bindings.RemoveBinding(bindings.Find({1}).value()));
    assert(admission.Fence(Session(1, 3), 3));
    auto unresolvedIdentity = admission.Begin({Session(1, 3), 3, "other"}, bindings).value();
    auto unresolved = admission.CompleteStored(unresolvedIdentity, EffectKnowledge::ConfirmedApplied, std::nullopt, true, bindings);
    assert(unresolved.needsFieldReconciliation && !unresolved.bindingAccepted);
    assert(admission.Fence(Session(1, 4), 4));
    auto missingIdentity = admission.Begin({Session(1, 4), 4, "other"}, bindings).value();
    auto conflicted = admission.CompleteStored(missingIdentity, EffectKnowledge::ConfirmedApplied, SubscriptionId {2}, true, bindings);
    assert(conflicted.needsFieldReconciliation && !conflicted.bindingAccepted); // account remains on slot0.
    assert(admission.Fence(Session(1, 5), 5));
    auto preparation = admission.Begin({Session(1, 5), 5, "other"}, bindings).value();
    assert(!admission.EndPreparationFailure(preparation, false, bindings));
    assert(!admission.RetryAfterKnownNoEffect(preparation, bindings));
    assert(admission.EndPreparationFailure(preparation, true, bindings));
    auto preparationRetry = admission.RetryAfterKnownNoEffect(preparation, bindings).value();
    assert(preparationRetry.observation.observationSequence == 5 && preparationRetry.admission != preparation.admission);
    assert(!admission.EndPreparationFailure(preparation, true, bindings));
    assert(admission.IsCurrent(preparationRetry, bindings));
    // A failure code without call-end evidence still grants no retry.
    admission.CompleteStored(preparationRetry, EffectKnowledge::ConfirmedNotApplied, std::nullopt, false, bindings);
    assert(!admission.RetryAfterKnownNoEffect(preparationRetry, bindings));
    assert(admission.Fence(Session(1, 6), 6));
    auto revisionChanged = admission.Begin({Session(1, 6), 6, "other"}, bindings).value();
    admission.CompleteStored(revisionChanged, EffectKnowledge::ConfirmedNotApplied, std::nullopt, true, bindings);
    assert(bindings.AcceptBinding({1}, {4}, bindings.Revision({1})));
    assert(!admission.RetryAfterKnownNoEffect(revisionChanged, bindings));
    assert(admission.Fence(Session(8, 1), 1));
    assert(!admission.Begin({Session(8, 1), 1, "unknown slot"}, bindings));
    auto oldEpoch = other;
    oldEpoch.epoch = {0, 0};
    assert(!admission.Fence(oldEpoch, 100));
}
void ActivationCleanup()
{
    ActivationProgress progress;
    const auto old = progress.Prepare({10}, {0}, Session(0, 1)).value();
    assert(progress.Begin(old) && progress.IsInProgress({0}));
    assert(!progress.Begin(old));
    const auto newer = progress.Prepare({11}, {0}, Session(0, 2)).value();
    assert(progress.Begin(newer));
    assert(!progress.Clear(old) && progress.IsInProgress({0}));
    auto wrongSession = newer;
    wrongSession.sentSession.bindingGeneration = 1;
    assert(!progress.Clear(wrongSession));
    wrongSession = newer;
    wrongSession.sentSession.application.slot = {1};
    assert(!progress.Clear(wrongSession));
    assert(progress.Clear(newer) && !progress.IsInProgress({0}));
    assert(!progress.Clear(newer));
    assert(!progress.Begin(newer)); // cleared request cannot resurrect.
    assert(!progress.Begin(old)); // older start cannot overwrite after new request ended.
    // account movement does not move a slot's old in-progress projection.
    const auto moved = progress.Prepare({12}, {1}, Session(1, 1)).value();
    const auto restarted = progress.Prepare({13}, {0}, Session(0, 3)).value();
    assert(progress.Begin(restarted) && progress.Begin(moved));
    assert(progress.Clear(restarted) && progress.IsInProgress({1}));
    assert(progress.Clear(moved));
    assert(!progress.Prepare({14}, {0}, Session(1, 1)));

}
}
int main()
{
    IndependentReloads();
    FieldsAndPartialVisibility();
    BindingAndDiscovery();
    ActivationCleanup();
    const auto plan = PlanShowNumberUpdate();
    assert(plan.orderedNodes.front() == NumberUpdateNode::CardMsisdn && !plan.rollbackCardOnAccountFailure);
    std::cout << "topology protocol scenarios: PASS\n";
}
