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
// Host-only protocol checks: actual pure rules, explicit primitive outcomes, no production success stubs.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <variant>
#include "assignment/domain/assignment_aggregate.h"
#include "assignment/primary/primary_switch_plan.h"
#include "assignment/account_roles/account_role_protocol.h"
#include "compatibility/output/completion_cell.h"
#include "compatibility/output/legacy_profile_result.h"
using namespace OHOS::Telephony::MultiSim;
namespace {
using Action = PrimaryAction;
bool Has(const std::vector<Action> &actions, Action wanted)
{
    return std::find(actions.begin(), actions.end(), wanted) != actions.end();
}
void Before(const std::vector<Action> &actions, Action first, Action second)
{
    const auto a = std::find(actions.begin(), actions.end(), first);
    const auto b = std::find(actions.begin(), actions.end(), second);
    assert(a != actions.end() && b != actions.end() && a < b);
}
void PrimaryModes()
{
    SlotDescriptor slot {SlotId {1}, EndpointKind::Physical, 1, RadioDomainId {"shared"},
        VisibilityClass::Ordinary, false};
    PrimaryDecisionFacts facts {slot, false, false, false, false};
    auto plan = PlanPrimarySwitch(facts).value();
    assert(plan.mode == PrimarySwitchMode::ThreePhaseProtocol);
    assert(plan.parameterWrite == ParameterWritePosition::BeforeDevice);
    Before(plan.beforeDevice, Action::WritePrimaryParameters, Action::ExecuteThreePhase);
    Before(plan.onDeviceSuccess, Action::PublishMainCard, Action::SaveDefaultData);
    Before(plan.onDeviceSuccess, Action::SaveDefaultData, Action::FinishOrdinary);
    Before(plan.onDeviceFailure, Action::RestoreValidOldParameters, Action::FinishOrdinary);
    Before(plan.onDeviceFailure, Action::FinishOrdinary, Action::ScheduleLegacyRetry);
    Before(plan.onDeviceFailure, Action::ScheduleLegacyRetry, Action::ReturnOriginalResult);
    // A missing old controller does not mean M0: the original ordinary path still prewrites then fails.
    assert(Has(plan.beforeDevice, Action::ExecuteThreePhase));
    facts.alreadyOnPrimaryModem = true;
    plan = PlanPrimarySwitch(facts).value();
    assert(plan.mode == PrimarySwitchMode::AlreadyPrimary);
    assert(plan.parameterWrite == ParameterWritePosition::WithoutDevice);
    assert(!Has(plan.beforeDevice, Action::ExecuteThreePhase));
    assert(!Has(plan.onDeviceSuccess, Action::FinishOrdinary));
    Before(plan.onDeviceSuccess, Action::WritePrimaryParameters, Action::PublishMainCard);
    Before(plan.onDeviceSuccess, Action::PublishMainCard, Action::SaveDefaultData);
    facts.userSet = true;
    facts.dedicatedPrimaryRilSupported = true;
    plan = PlanPrimarySwitch(facts).value();
    assert(plan.mode == PrimarySwitchMode::WithoutModemRestart);
    assert(plan.parameterWrite == ParameterWritePosition::AfterDevice);
    assert(Has(plan.beforeDevice, Action::ExecuteDedicatedRil));
    assert(!Has(plan.beforeDevice, Action::WritePrimaryParameters));
    Before(plan.onDeviceSuccess, Action::WritePrimaryParameters, Action::SaveDefaultData);
    Before(plan.onDeviceSuccess, Action::SaveDefaultData, Action::FinishDedicated);
    assert(!Has(plan.onDeviceFailure, Action::WritePrimaryParameters));
    assert(!Has(plan.onDeviceFailure, Action::RestoreValidOldParameters));
    assert(!Has(plan.onDeviceFailure, Action::ScheduleLegacyRetry));
    Before(plan.onDeviceFailure, Action::FinishDedicated, Action::ReturnOriginalResult);
    facts.vendorMappingRequired = true;
    plan = PlanPrimarySwitch(facts).value();
    assert(plan.mode == PrimarySwitchMode::VendorRouteMapping); // third-card branch has priority.
    assert(plan.beforeDevice == std::vector<Action> {Action::RequestThirdCardRoute});
    assert(plan.onDeviceSuccess == std::vector<Action> {Action::ReturnOriginalResult});
    assert(plan.onDeviceFailure == std::vector<Action> {Action::ReturnOriginalResult});
    facts.vendorMappingRequired = false;
    facts.userSet = false;
    facts.alreadyOnPrimaryModem = false;
    assert(PlanPrimarySwitch(facts)->mode == PrimarySwitchMode::ThreePhaseProtocol);
    facts.target.radioDomain.reset();
    assert(!PlanPrimarySwitch(facts));
}
void RequestAndConflictIdentity()
{
    RequestId a {1, RuntimeEpoch {3,4}, RequestOwner::Assignment};
    RequestId b {1, RuntimeEpoch {3,4}, RequestOwner::Activation};
    RequestId c {1, RuntimeEpoch {3,5}, RequestOwner::Assignment};
    assert(!(a == b) && !(a == c));
    assert(!(StepRef {a,2,1} == StepRef {a,2,2}));
    assert(!(StepRef {a,2,1} == StepRef {b,2,1}));
    assert((StepRef {a,2,1} == StepRef {a,2,1}));
    SessionRef session {RuntimeEpoch {3,4}, CardApplicationTarget {SlotId {1}, "sim-app"}, 5};
    ActivationConflictLease lease {StepRef {b,1,1}, session, RadioDomainId {"shared"}, 7};
    auto wrong = lease;
    wrong.token++; assert(!(lease == wrong));
    wrong = lease; wrong.owner.request.owner = RequestOwner::Assignment; assert(!(lease == wrong));
    wrong = lease; wrong.sentSession.bindingGeneration++; assert(!(lease == wrong));
    wrong = lease; wrong.domain.key = "other"; assert(!(lease == wrong));
    assert(!MayReleaseActivationConflict({EffectKnowledge::Unknown, true}));
    assert(!MayReleaseActivationConflict({EffectKnowledge::ConfirmedApplied, false}));
    assert(MayReleaseActivationConflict({EffectKnowledge::ConfirmedApplied, true}));
    assert(MayReleaseActivationConflict({EffectKnowledge::NotSent, true}));
    assert(!MayFinalizeInitialRoles({false, EffectKnowledge::ConfirmedApplied, true}));
    assert(!MayFinalizeInitialRoles({true, EffectKnowledge::Unknown, true}));
    assert(!MayFinalizeInitialRoles({true, EffectKnowledge::ConfirmedApplied, false}));
    assert(MayFinalizeInitialRoles({true, EffectKnowledge::ConfirmedApplied, true}));
}
void CompletionBoundaries()
{
    CompletionCell early;
    assert(!early.Result());
    assert(early.Deliver(-23)); // result can arrive before the old synchronous adapter starts waiting.
    assert(early.Result() == -23 && early.State() == WaitState::Delivered);
    assert(!early.Deliver(0) && !early.EndWait(WaitState::TimedOut));
    assert(early.Result() == -23);
    CompletionCell waiting;
    assert(waiting.State() == WaitState::Waiting && waiting.Deliver(0));
    assert(waiting.Result() == 0);
    for (auto reason : {WaitState::TimedOut, WaitState::CallerDied, WaitState::CancelledWait}) {
        CompletionCell ended;
        assert(ended.EndWait(reason));
        assert(!ended.Deliver(0) && !ended.Result());
        assert(!ended.EndWait(WaitState::TimedOut));
        assert(ended.State() == reason);
    }
    CompletionCell invalid;
    assert(!invalid.EndWait(WaitState::Delivered) && invalid.State() == WaitState::Waiting);
    CompletionRoute route = SynchronousCompletionRoute {SynchronousWaitRef {4}};
    assert(std::holds_alternative<SynchronousCompletionRoute>(route));
    route = CallbackCompletionRoute {CompletionId {5}};
    assert(std::holds_alternative<CallbackCompletionRoute>(route));
    route = InternalCompletionRoute {InternalReplyRef {6}};
    assert(std::holds_alternative<InternalCompletionRoute>(route));
}
void JointRowsAndIndependentRoles()
{
    AccountRoleProtocol roles;
    const AccountRolePlanningInput singleNew {std::nullopt, {0}, true};
    auto plan = roles.Prepare(singleNew).value();
    assert(!plan.subscription && plan.includeInStorage && plan.voice && plan.sms &&
        plan.mainCardColumn && plan.dataCardColumn);
    assert(!roles.Prepare(singleNew)); // one role conflict reservation covers the store call.
    assert(roles.AcceptDiscovered(plan, {9}));
    assert(!roles.Finalize(plan, {true, EffectKnowledge::Unknown, true}));
    assert(!roles.Prepare(singleNew));
    assert(roles.Finalize(plan, {true, EffectKnowledge::ConfirmedApplied, true}));
    assert(!roles.Finalize(plan, {true, EffectKnowledge::ConfirmedApplied, true}));
    const AccountRolePlanningInput singleOld {SubscriptionId {9}, {0}, true};
    plan = roles.Prepare(singleOld).value();
    assert(plan.includeInStorage && plan.voice && plan.sms && plan.mainCardColumn && plan.dataCardColumn);
    assert(!roles.AcceptDiscovered(plan, {10}));
    assert(roles.Finalize(plan, {true, EffectKnowledge::ConfirmedNotApplied, true}));
    const AccountRolePlanningInput multiOld {SubscriptionId {9}, {1}, false};
    plan = roles.Prepare(multiOld).value();
    assert(!plan.includeInStorage); // original multi-slot rebind preserves role columns.
    assert(roles.StillOwns(plan));
    roles.NoteLocalRoleMutation();
    assert(!roles.StillOwns(plan) && !roles.AcceptDiscovered(plan, {9}));
    assert(!roles.Finalize(plan, {true, EffectKnowledge::ConfirmedApplied, false}));
    assert(roles.Finalize(plan, {true, EffectKnowledge::ConfirmedApplied, true})); // reconciled old storage effects.
    auto current = roles.Revision(false);
    auto all = roles.Revision(true);
    assert(roles.AcceptReloadSet({{{9}, true, false, true, false}}, 1, current, false));
    assert(roles.Projection(false).size() == 1 && roles.Projection(true).empty());
    // Failed historical read does not call acceptance and therefore leaves current usable.
    assert(roles.AcceptReloadSet({{{9}, false, true, false, true}}, 1, all, true));
    current = roles.Revision(false);
    all = roles.Revision(true);
    assert(!roles.AcceptReloadSet({}, 1, current, false)); // repeated attempt cannot erase the view.
    assert(roles.AcceptReloadSet({}, 2, current, false));
    assert(roles.Projection(false).empty() && roles.Projection(true).size() == 1);
    assert(!roles.AcceptReloadSet({{{9}, true, false, false, false}, {{9}, false, true, false, false}},
        2, all, true)); // malformed duplicate identities leave historical projection unchanged.
    assert(roles.Projection(true).size() == 1);
    roles.NoteLocalRoleMutation();
    assert(!roles.AcceptReloadSet({}, 3, current, false)); // load predating local role change is stale.
}
struct PrimitiveBoundary final : LocalCardPort {
    bool valid = true;
    LocalProfileExecutionResult result {{0, 47}, true, EffectKnowledge::ConfirmedNotApplied, true};
    std::optional<SwitchProfileRequest> received;
    std::vector<int> &trace;
    explicit PrimitiveBoundary(std::vector<int> &events) : trace(events) {}
    void VerifyPin(const VerifyPinRequest &) override { assert(false); }
    void ReadElementaryFile(const ReadEfRequest &) override { assert(false); }
    std::optional<LocalProfileTransitionRef> RegisterExpectedProfileTransition(const SwitchProfileRequest &request) override
    {
        if (!valid) { return std::nullopt; }
        assert(request.context.sentSession);
        trace.push_back(1);
        return LocalProfileTransitionRef {request.context.step, *request.context.sentSession};
    }
    LocalProfileExecutionResult SwitchLocalProfile(const SwitchProfileRequest &request) override
    {
        received = request;
        trace.push_back(2);
        if (!valid) { return {{-91, std::nullopt}, false, EffectKnowledge::NotSent, true}; }
        return result;
    }
    void EndLocalProfileWait(const LocalProfileTransitionRef &transition, EffectKnowledge effect) override
    {
        assert(received && transition.step == received->context.step);
        assert(effect == result.effect);
        trace.push_back(4);
    }
};
struct PreferenceBoundary final : ProfilePreferencePort {
    std::vector<int> &trace;
    explicit PreferenceBoundary(std::vector<int> &events) : trace(events) {}
    void AcceptLocalProfilePreference(const StepRef &source, SlotId slot) override
    {
        assert(source.request.owner == RequestOwner::LocalCard && slot.legacySlotId == 1);
        trace.push_back(3);
    }
};
void ProfileParametersAndResults()
{
    std::vector<int> events;
    PrimitiveBoundary primitive(events);
    PreferenceBoundary preference(events);
    LocalCardRequests local(primitive, preference);
    StepRef step {{7, {3,4}, RequestOwner::LocalCard}, 1, 1};
    SessionRef session {{3,4}, {{1}, "euicc"}, 6};
    RequestContext context {step, std::nullopt, SlotTarget {{1}}, session,
        {InternalCompletionRoute {InternalReplyRef {8}}, false}};
    SwitchProfileRequest request {context, {{1}, "euicc"}, "profile-id", -7, true,
        VerifiedPortRoute {"physical-euicc", 4, 2}, 200};
    int32_t enumOut = -999;
    auto result = local.SwitchLocalProfile(request);
    assert(AssignLocalProfileLegacyResult(result, enumOut) == 0 && enumOut == 47);
    assert((events == std::vector<int> {1,2,3,4})); // enum failure still reaches original preference node.
    assert(primitive.received->legacyPortIndex == -7 && primitive.received->forceDisableProfile &&
        primitive.received->iccid == request.iccid && primitive.received->verifiedRoute->portIndex == 4);
    primitive.valid = false;
    events.clear(); enumOut = 123;
    result = local.SwitchLocalProfile(request);
    assert(AssignLocalProfileLegacyResult(result, enumOut) == -91 && enumOut == 123);
    assert((events == std::vector<int> {2})); // invalid slot/File registers no expectation or preference.
    primitive.valid = true;
    primitive.result = {{0, 58}, true, EffectKnowledge::Unknown, false};
    events.clear();
    result = local.SwitchLocalProfile(request);
    assert(AssignLocalProfileLegacyResult(result, enumOut) == 0 && enumOut == 58);
    assert((events == std::vector<int> {1,2,3,4})); // timeout/unknown stays a distinct internal effect.
    primitive.result = {{0, 0}, true, EffectKnowledge::ConfirmedApplied, false};
    events.clear();
    result = local.SwitchLocalProfile(request);
    assert(AssignLocalProfileLegacyResult(result, enumOut) == 0 && enumOut == 0);
    assert((events == std::vector<int> {1,2,3,4})); // card success cannot prove the channel closed.
}
}
int main()
{
    PrimaryModes();
    RequestAndConflictIdentity();
    CompletionBoundaries();
    JointRowsAndIndependentRoles();
    ProfileParametersAndResults();
    std::cout << "assignment / completion / Profile protocol scenarios passed\n";
}
