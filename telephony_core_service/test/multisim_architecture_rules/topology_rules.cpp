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
#include "topology/domain/account_registry.h"
#include "topology/domain/binding_index.h"
#include "topology/application/discovery_observation_ledger.h"
#include "topology/application/activation_progress.h"
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
    assert(progress.Begin({first, {0}, sessionA, SubscriptionId {3}, true}));
    assert(progress.ReadLegacy({0}));
    assert(progress.Accept(first, sessionA, false));
    assert(!progress.ReadLegacy({0}));
    assert(progress.Begin({second, {0}, sessionB, SubscriptionId {3}, true}));
    assert(!progress.Accept(first, sessionA, false)); // 旧请求清理不得清新槽操作。
    assert(progress.ReadLegacy({0}));
    auto wrongOwner = second;
    wrongOwner.request.owner = RequestOwner::ShowNumber;
    assert(!progress.Accept(wrongOwner, sessionB, false));
    assert(!progress.Begin({second, {0}, sessionB, SubscriptionId {3}, true}));
    assert(progress.Accept(second, sessionB, false));
    assert(!progress.Accept(second, sessionB, true));
    std::cout << "PASS binding ABA/assembly, independent projections/field reload, discovery replacement, activation cleanup ownership\n";
}
