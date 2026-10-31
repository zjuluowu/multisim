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
// Host-only pure rule checks. No production objects, I/O, or device capability proof.

#include <cassert>
#include <iostream>
#include "topology/accounts/account_registry.h"
#include "topology/accounts/binding_index.h"
#include "topology/discovery/discovery_observation_ledger.h"
#include "topology/activation/activation_progress.h"
using namespace OHOS::Telephony::MultiSim;

int main()
{
    const RuntimeEpoch epoch {7, 8};
    SlotCatalog catalog({
        {{0}, EndpointKind::Physical, 0, RadioDomainId {"shared"}, VisibilityClass::Ordinary, false},
        {{1}, EndpointKind::Physical, 1, RadioDomainId {"shared"}, VisibilityClass::Ordinary, false}}, {});
    BindingIndex bindings(catalog);
    assert(!bindings.AcceptBinding({0}, {3}, 0)); // 目录存在不代表已装配。
    assert(bindings.AcceptAssemblyState({0}, true));
    assert(bindings.AcceptAssemblyState({1}, true));
    assert(!bindings.AcceptAssemblyState({99}, true));
    assert(bindings.AcceptBinding({0}, {3}, 0));
    auto old = *bindings.Find({0});
    assert(old.bindingRevision == 1);
    assert(!bindings.AcceptBinding({1}, {3}, 0)); // 同一身份不可隐式跨键双绑定。
    assert(bindings.RemoveBinding(old));
    assert(*bindings.ReadPositionRevision({0}) == 2);
    assert(!bindings.AcceptBinding({0}, {3}, 0));
    assert(bindings.AcceptBinding({0}, {3}, 2));
    assert(bindings.Find({0})->bindingRevision == 3);
    assert(!bindings.StillMatches(old));
    assert(!bindings.RemoveBinding(old));
    assert(!bindings.AcceptAssemblyState({0}, false));

    AccountRegistry accounts;
    AccountRecord account {{3}, "iccid-A", 1, AccountProjection {}, AccountProjection {}};
    assert(accounts.AcceptDiscoveredAccount(account) == AccountAcceptance::Accepted);
    assert(accounts.AcceptDisplayName({3}, AccountReadView::Current, 0, u"new-name") == AccountAcceptance::Accepted);
    assert(accounts.AcceptNumberVisibility({3}, AccountReadView::Current, 0, u"123") == AccountAcceptance::Accepted);
    assert(accounts.ReadProjection({3}, AccountReadView::Current)->displayName == u"new-name");
    assert(accounts.ReadProjection({3}, AccountReadView::Historical)->displayNumber.empty());
    assert(accounts.AcceptNumberVisibility({3}, AccountReadView::Current, 0, u"stale") == AccountAcceptance::ChangedInputs);
    AccountProjection delayed;
    delayed.displayName = u"old-name";
    delayed.displayNumber = u"old-number";
    delayed.active = true;
    auto accepted = accounts.AcceptReloadProjection({3}, AccountReadView::Current, 1, {}, delayed);
    assert(accepted.identityAccepted && !accepted.nameAccepted && !accepted.numberAccepted && accepted.activeAccepted);
    assert(accounts.ReadProjection({3}, AccountReadView::Current)->displayNumber == u"123");
    assert(accounts.ReadProjection({3}, AccountReadView::Current)->active);
    assert(!accounts.ReadProjection({3}, AccountReadView::Historical)->active);
    assert(!accounts.AcceptReloadProjection({3}, AccountReadView::Historical, 99, {}, delayed).identityAccepted);
    assert(accounts.AcceptNumberVisibility({3}, AccountReadView::Historical, 0, u"historical") == AccountAcceptance::Accepted);
    assert(accounts.ReadProjection({3}, AccountReadView::Current)->displayNumber == u"123");

    SessionRef sessionA {epoch, {{0}, "USIM"}, 1};
    SessionRef sessionB {epoch, {{0}, "USIM"}, 2};
    DiscoveryObservationLedger observations(epoch);
    CardDiscoveryObservation a {sessionA, 1, "iccid-A"};
    CardDiscoveryObservation b {sessionB, 3, "iccid-B"};
    assert(observations.AcceptDiscovery(a));
    assert(!observations.AcceptDiscovery(a));
    assert(observations.AcceptInvalidation({sessionA, 2}));
    assert(!observations.StillCurrent(a));
    assert(observations.AcceptDiscovery(b));
    assert(!observations.AcceptDiscovery(a));
    assert(!observations.StillCurrent(a)); // A发现I/O迟到不能覆盖已接受B。
    assert(!observations.AcceptInvalidation({sessionA, 4}));
    assert(observations.StillCurrent(b));
    auto wrongEpoch = b;
    wrongEpoch.session.epoch = {9, 9};
    wrongEpoch.observationSequence = 99;
    assert(!observations.AcceptDiscovery(wrongEpoch));
    SessionRef independent {epoch, {{1}, "USIM"}, 1};
    assert(observations.AcceptDiscovery({independent, 1, "iccid-C"}));
    assert(observations.StillCurrent(b));

    // 同值重载只改名称，不撤销在途号码资格。
    auto beforeNameLoad = *accounts.ReadProjection({3}, AccountReadView::Current);
    auto nameOnly = beforeNameLoad;
    nameOnly.displayName = u"loaded-name";
    assert(accounts.AcceptReloadProjection({3}, AccountReadView::Current, 1,
        beforeNameLoad.revisions, nameOnly).numberAccepted);
    assert(accounts.ReadProjection({3}, AccountReadView::Current)->revisions.number ==
        beforeNameLoad.revisions.number);

    AccountRegistry lists;
    AccountRecord currentRow {{9}, "row-9", 1, AccountProjection {}, {}};
    currentRow.current->displayNumber = u"current";
    AccountRecord historyRow {{9}, "row-9", 1, {}, AccountProjection {}};
    historyRow.historical->displayNumber = u"history";
    auto currentLoad = lists.BeginReload(AccountReadView::Current);
    auto historyLoad = lists.BeginReload(AccountReadView::Historical);
    assert(lists.AcceptReload(currentLoad, true, {currentRow}).acceptedAttempt);
    assert(!lists.AcceptReload(historyLoad, false, {}).acceptedAttempt);
    assert(lists.Read({9})->current && !lists.Read({9})->historical);

    // 同一账户/绑定中，current清空后重载不得复用旧字段资格；historical保持独立。
    auto historyAgain = lists.BeginReload(AccountReadView::Historical);
    assert(lists.AcceptReload(historyAgain, true, {historyRow}).acceptedAttempt);
    const auto previousNumber = lists.ReadProjection({9}, AccountReadView::Current)->revisions.number;
    auto removeCurrent = lists.BeginReload(AccountReadView::Current);
    assert(lists.AcceptReload(removeCurrent, true, {}).acceptedAttempt);
    assert(!lists.Read({9})->current && lists.Read({9})->historical);
    auto restoreCurrent = lists.BeginReload(AccountReadView::Current);
    assert(lists.AcceptReload(restoreCurrent, true, {currentRow}).acceptedAttempt);
    assert(lists.ReadProjection({9}, AccountReadView::Current)->revisions.number > previousNumber);
    assert(lists.AcceptNumberVisibility({9}, AccountReadView::Current, previousNumber, u"late") ==
        AccountAcceptance::ChangedInputs);
    // 全部投影都移除后，再次发现亦不得复用原投影版本。
    const auto restoredNumber = lists.ReadProjection({9}, AccountReadView::Current)->revisions.number;
    assert(lists.AcceptReload(lists.BeginReload(AccountReadView::Current), true, {}).acceptedAttempt);
    assert(lists.AcceptReload(lists.BeginReload(AccountReadView::Historical), true, {}).acceptedAttempt);
    assert(!lists.Read({9}));
    assert(lists.AcceptDiscoveredAccount(currentRow) == AccountAcceptance::Accepted);
    assert(lists.ReadProjection({9}, AccountReadView::Current)->revisions.number > restoredNumber);
    historyLoad = lists.BeginReload(AccountReadView::Historical);
    assert(lists.AcceptReload(historyLoad, true, {historyRow}).acceptedAttempt);
    currentLoad = lists.BeginReload(AccountReadView::Current);
    assert(!lists.AcceptReload(currentLoad, false, {}).acceptedAttempt);
    assert(lists.Read({9})->historical->displayNumber == u"history");
    const auto oldList = lists.BeginReload(AccountReadView::Current);
    const auto newerList = lists.BeginReload(AccountReadView::Current);
    assert(!lists.AcceptReload(oldList, true, {}).acceptedAttempt);
    assert(lists.AcceptReload(newerList, true, {currentRow}).acceptedAttempt);
    assert(!lists.AcceptReload(newerList, true, {}).acceptedAttempt);
    auto removal = lists.BeginReload(AccountReadView::Current);
    const auto loadedCurrent = *lists.ReadProjection({9}, AccountReadView::Current);
    assert(lists.AcceptDisplayName({9}, AccountReadView::Current, loadedCurrent.revisions.name,
        u"updated-after-read") == AccountAcceptance::Accepted);
    assert(lists.AcceptReload(removal, true, {}).conflictedFields == 1);
    assert(lists.Read({9})->current);
    removal = lists.BeginReload(AccountReadView::Historical);
    assert(lists.AcceptReload(removal, true, {}).acceptedAttempt);
    assert(lists.Read({9})->current && !lists.Read({9})->historical);

    // 真实存后纯接纳函数收到已写但过期输入：不可绑定到B；必须交字段核对。
    BindingIndex storedBindings(catalog);
    assert(storedBindings.AcceptAssemblyState({0}, true));
    DiscoveryObservationLedger storedLedger(epoch);
    assert(storedLedger.AcceptDiscovery(a));
    const auto aTicket = storedLedger.BeginStore(a, storedBindings).value();
    assert(!storedLedger.BeginStore(a, storedBindings));
    assert(storedLedger.AcceptInvalidation({sessionA, 2}));
    assert(storedLedger.AcceptDiscovery(b));
    const auto late = storedLedger.CompleteStored(aTicket, EffectKnowledge::ConfirmedApplied,
        SubscriptionId {3}, true, storedBindings);
    assert(!late.bindingAccepted && late.needsFieldReconciliation && !storedBindings.Find({0}));
    auto bTicket = storedLedger.BeginStore(b, storedBindings).value();
    assert(!storedLedger.CompleteStored(bTicket, EffectKnowledge::ConfirmedNotApplied,
        {}, true, storedBindings).bindingAccepted);
    const auto retry = storedLedger.RetryAfterKnownNoEffect(bTicket, storedBindings).value();
    assert(!storedLedger.EndPreparationFailure(bTicket, true, storedBindings));
    assert(storedLedger.CompleteStored(retry, EffectKnowledge::ConfirmedApplied,
        SubscriptionId {4}, true, storedBindings).bindingAccepted);
    assert(storedLedger.CompleteStored(retry, EffectKnowledge::ConfirmedApplied,
        SubscriptionId {4}, true, storedBindings).duplicate);
    assert(storedBindings.RemoveBinding(*storedBindings.Find({0})));
    CardDiscoveryObservation third {{epoch, {{0}, "USIM"}, 3}, 5, "iccid-D"};
    assert(storedLedger.AcceptDiscovery(third));
    auto unknown = storedLedger.BeginStore(third, storedBindings).value();
    assert(storedLedger.CompleteStored(unknown, EffectKnowledge::Unknown, {}, true,
        storedBindings).needsFieldReconciliation);
    assert(!storedLedger.RetryAfterKnownNoEffect(unknown, storedBindings));
    CardDiscoveryObservation fourth {{epoch, {{0}, "USIM"}, 4}, 6, "iccid-E"};
    assert(storedLedger.AcceptDiscovery(fourth));
    auto preparation = storedLedger.BeginStore(fourth, storedBindings).value();
    assert(!storedLedger.EndPreparationFailure(preparation, false, storedBindings));
    assert(storedLedger.EndPreparationFailure(preparation, true, storedBindings));
    assert(storedLedger.RetryAfterKnownNoEffect(preparation, storedBindings));

    ActivationProgress progress(epoch);
    StepRef first {{1, epoch, RequestOwner::Activation}, 0, 1};
    StepRef second {{2, epoch, RequestOwner::Activation}, 0, 1};
    auto invalidBegin = first;
    invalidBegin.request.value = 0;
    assert(!progress.Begin({invalidBegin, {0}, sessionA, SubscriptionId {3}, true}));
    invalidBegin = first;
    invalidBegin.request.owner = RequestOwner::Assignment;
    assert(!progress.Begin({invalidBegin, {0}, sessionA, SubscriptionId {3}, true}));
    invalidBegin.request.owner = RequestOwner::Unspecified;
    assert(!progress.Begin({invalidBegin, {0}, sessionA, SubscriptionId {3}, true}));
    const auto preparedFirst = progress.Prepare({first, {0}, sessionA, SubscriptionId {3}, true}).value();
    assert(progress.Begin(preparedFirst));
    assert(progress.ReadLegacy({0}));
    assert(progress.Accept(first, sessionA, false));
    assert(!progress.ReadLegacy({0}));
    const auto preparedSecond = progress.Prepare({second, {0}, sessionB, SubscriptionId {3}, true}).value();
    assert(progress.Begin(preparedSecond));
    assert(!progress.Accept(first, sessionA, false)); // 旧请求清理不得清新槽操作。
    assert(progress.ReadLegacy({0}));
    auto wrongOwner = second;
    wrongOwner.request.owner = RequestOwner::ShowNumber;
    assert(!progress.Accept(wrongOwner, sessionB, false));
    assert(!progress.Begin({second, {0}, sessionB, SubscriptionId {3}, true}));
    assert(progress.Accept(second, sessionB, false));
    assert(!progress.Accept(second, sessionB, true));
    assert(!progress.Begin(preparedFirst)); // 已清理后旧Begin不能复活。
    assert(!progress.Begin(preparedSecond));
    auto wrongSlot = preparedSecond;
    wrongSlot.sentSession.application.slot = {1};
    assert(!progress.Accept(second, wrongSlot.sentSession, false));
    std::cout << "PASS binding ABA/assembly, independent projections/field reload, discovery replacement, activation cleanup ownership\n";
}
