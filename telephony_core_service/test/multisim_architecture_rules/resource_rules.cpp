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
#include "runtime/resource/resource_barrier.h"
using namespace OHOS::Telephony::MultiSim;
static StepRef Step(uint64_t id, RuntimeEpoch epoch, RequestOwner owner = RequestOwner::ShowNumber)
{
    return {{id, epoch, owner}, 1, 1};
}
int main()
{
    const RuntimeEpoch epoch {11, 29};
    const RuntimeEpoch otherEpoch {11, 30};
    const ResourceKey first {"verified-modem"};
    const ResourceKey second {"verified-euicc"};
    const ResourceKey missing {"unregistered"};
    ResourceBarrier barrier(epoch);
    assert(!barrier.ReadState(missing));
    assert(!barrier.RegisterForInitialCheck({""}));
    assert(barrier.RegisterForInitialCheck(first));
    assert(!barrier.RegisterForInitialCheck(first));
    assert(barrier.ReadState(first) == ResourceState::Checking);
    assert(!barrier.Acquire({first}, Step(1, epoch)));
    assert(!barrier.AcceptInitialRecoveryEvidence(first, {otherEpoch, true, true}));
    assert(!barrier.AcceptInitialRecoveryEvidence(first, {epoch, false, true}));
    assert(barrier.AcceptInitialRecoveryEvidence(first, {epoch, true, true}));
    assert(!barrier.Acquire({first}, Step(0, epoch)));
    assert(!barrier.Acquire({first}, Step(1, epoch, RequestOwner::Unspecified)));
    assert(!barrier.Acquire({first, missing}, Step(1, epoch)));
    auto unsent = barrier.Acquire({first}, Step(1, epoch));
    assert(unsent);
    assert(barrier.CancelConfirmedNotSent(*unsent));
    auto write = barrier.Acquire({first}, Step(2, epoch));
    assert(write);
    assert(!barrier.Complete(*write, {write->holder, EffectKnowledge::ConfirmedApplied, true, true}));
    assert(barrier.MarkSent(*write));
    assert(!barrier.Complete(*write, {write->holder, EffectKnowledge::NotSent, true, true}));
    assert(!barrier.MarkSent(*write));
    assert(!barrier.CancelConfirmedNotSent(*write));
    assert(!barrier.Complete(*write, {write->holder, EffectKnowledge::Unknown, true, true}));
    assert(barrier.ReadState(first) == ResourceState::InFlight);
    assert(barrier.Quarantine(*write, EffectKnowledge::Unknown));
    assert(!barrier.Acquire({first}, Step(3, epoch)));
    assert(!barrier.AcceptInitialRecoveryEvidence(first, {epoch, true, true}));
    assert(!barrier.AcceptRecoveryEvidence(*write,
        {Step(2, epoch, RequestOwner::Assignment), EffectKnowledge::ConfirmedApplied, true, true}));
    assert(!barrier.AcceptRecoveryEvidence(*write,
        {write->holder, EffectKnowledge::ConfirmedApplied, false, true}));
    assert(!barrier.AcceptRecoveryEvidence(*write,
        {write->holder, EffectKnowledge::Unknown, true, true}));
    assert(!barrier.AcceptRecoveryEvidence(*write,
        {write->holder, EffectKnowledge::NotSent, true, true}));
    assert(barrier.AcceptRecoveryEvidence(*write,
        {write->holder, EffectKnowledge::ConfirmedApplied, true, true}));
    auto next = barrier.Acquire({first}, Step(3, epoch));
    assert(next && next->fence != write->fence);
    assert(!barrier.Complete(*write, {write->holder, EffectKnowledge::ConfirmedApplied, true, true}));
    assert(barrier.ReadState(first) == ResourceState::InFlight);
    assert(barrier.CancelConfirmedNotSent(*next));
    auto read = barrier.Acquire({first}, Step(4, epoch), EffectRequirement::NoBusinessWrite);
    assert(read && barrier.MarkSent(*read));
    assert(barrier.Complete(*read, {read->holder, EffectKnowledge::Unknown, true, true}));
    auto readLost = barrier.Acquire({first}, Step(7, epoch), EffectRequirement::NoBusinessWrite);
    assert(readLost && barrier.MarkSent(*readLost));
    assert(barrier.Quarantine(*readLost, EffectKnowledge::Unknown));
    assert(barrier.AcceptRecoveryEvidence(*readLost,
        {readLost->holder, EffectKnowledge::Unknown, true, true}));
    assert(barrier.RegisterForInitialCheck(second));
    assert(barrier.AcceptInitialRecoveryEvidence(second, {epoch, true, true}));
    assert(!barrier.Acquire({first, first}, Step(5, epoch)));
    auto combined = barrier.Acquire({first, second}, Step(5, epoch));
    assert(combined);
    auto forged = *combined;
    forged.resources = {first, first};
    assert(!barrier.CancelConfirmedNotSent(forged));
    assert(barrier.ReadState(first) == ResourceState::InFlight);
    assert(barrier.ReadState(second) == ResourceState::InFlight);
    assert(barrier.CancelConfirmedNotSent(*combined));
    assert(!barrier.Acquire({first}, Step(6, otherEpoch)));
    ResourceBarrier rebuilt(otherEpoch);
    assert(rebuilt.RegisterForInitialCheck(first));
    assert(rebuilt.AcceptInitialRecoveryEvidence(first, {otherEpoch, true, true}));
    assert(!rebuilt.MarkSent(*next));
}
