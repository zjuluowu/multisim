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
#include <variant>
#include "assignment/domain/assignment_aggregate.h"
#include "application/orchestration/request_context.h"
#include "assignment/api/assignment_requests.h"
using namespace OHOS::Telephony::MultiSim;
int main()
{
    AssignmentAggregate rules;
    SlotDescriptor slot {SlotId {1}, EndpointKind::Physical, 1, RadioDomainId {"shared"},
        VisibilityClass::Ordinary, false};
    PrimaryDecisionFacts facts {slot, false, false, false, false};
    auto plan = rules.PlanPrimarySwitch(facts);
    assert(plan && plan->mode == PrimarySwitchMode::ThreePhaseProtocol);
    assert(plan->parameterWrite == ParameterWritePosition::BeforeDevice);
    facts.alreadyOnPrimaryModem = true;
    plan = rules.PlanPrimarySwitch(facts);
    assert(plan && plan->mode == PrimarySwitchMode::AlreadyPrimary);
    assert(plan->parameterWrite == ParameterWritePosition::WithoutDevice);
    facts.userSet = true;
    facts.dedicatedPrimaryRilSupported = true;
    plan = rules.PlanPrimarySwitch(facts);
    assert(plan && plan->mode == PrimarySwitchMode::WithoutModemRestart);
    assert(plan->parameterWrite == ParameterWritePosition::AfterDevice);
    facts.vendorMappingRequired = true;
    plan = rules.PlanPrimarySwitch(facts);
    assert(plan && plan->mode == PrimarySwitchMode::VendorRouteMapping);
    assert(plan->parameterWrite == ParameterWritePosition::AfterDevice);
    facts.target.radioDomain.reset();
    assert(!rules.PlanPrimarySwitch(facts));
    RequestId a {1, RuntimeEpoch {3,4}, RequestOwner::Assignment};
    RequestId b {1, RuntimeEpoch {3,4}, RequestOwner::Activation};
    RequestId c {1, RuntimeEpoch {3,5}, RequestOwner::Assignment};
    assert(!(a == b));
    assert(!(a == c));
    assert(!(StepRef {a,2,1} == StepRef {a,2,2}));
    assert(!(StepRef {a,2,1} == StepRef {b,2,1}));
    assert((StepRef {a,2,1} == StepRef {a,2,1}));
    SessionRef session {RuntimeEpoch {3,4}, CardApplicationTarget {SlotId {1}, "sim-app"}, 5};
    ActivationConflictLease lease {StepRef {b,1,1}, session, RadioDomainId {"shared"}, 7};
    auto wrong = lease;
    wrong.token++;
    assert(!(lease == wrong));
    wrong = lease;
    wrong.owner.request.owner = RequestOwner::Assignment;
    assert(!(lease == wrong));
    wrong = lease;
    wrong.sentSession.bindingGeneration++;
    assert(!(lease == wrong));
    wrong = lease;
    wrong.domain.key = "other";
    assert(!(lease == wrong));
    assert(!MayReleaseActivationConflict({EffectKnowledge::Unknown, true}));
    assert(!MayReleaseActivationConflict({EffectKnowledge::ConfirmedApplied, false}));
    assert(MayReleaseActivationConflict({EffectKnowledge::ConfirmedApplied, true}));
    assert(MayReleaseActivationConflict({EffectKnowledge::NotSent, true}));
    assert(!MayFinalizeInitialRoles({false, EffectKnowledge::ConfirmedApplied, true}));
    assert(!MayFinalizeInitialRoles({true, EffectKnowledge::Unknown, true}));
    assert(!MayFinalizeInitialRoles({true, EffectKnowledge::ConfirmedApplied, false}));
    assert(MayFinalizeInitialRoles({true, EffectKnowledge::ConfirmedApplied, true}));
    CompletionRoute route = SynchronousCompletionRoute {SynchronousWaitRef {4}};
    assert(std::holds_alternative<SynchronousCompletionRoute>(route));
    route = CallbackCompletionRoute {CompletionId {5}};
    assert(std::holds_alternative<CallbackCompletionRoute>(route));
    route = InternalCompletionRoute {InternalReplyRef {6}};
    assert(std::holds_alternative<InternalCompletionRoute>(route));
}
