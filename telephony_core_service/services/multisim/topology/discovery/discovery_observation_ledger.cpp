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

#include "topology/discovery/discovery_observation_ledger.h"

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameSession(const SessionRef &left, const SessionRef &right)
{
    return left.epoch == right.epoch && left.application.slot == right.application.slot &&
        left.application.applicationKey == right.application.applicationKey &&
        left.bindingGeneration == right.bindingGeneration;
}
} // namespace

bool DiscoveryObservationLedger::AcceptDiscovery(const CardDiscoveryObservation &observation)
{
    if (observation.session.epoch != epoch_ || observation.observationSequence == 0 || observation.iccid.empty()) {
        return false;
    }
    Key key {observation.session.application.slot, observation.session.application.applicationKey};
    auto current = accepted_.find(key);
    if (current != accepted_.end() && observation.observationSequence <= current->second.sequence) {
        return false;
    }
    accepted_.insert_or_assign(key, Entry {observation.session, observation.observationSequence, true, observation.iccid});
    return true;
}

bool DiscoveryObservationLedger::AcceptInvalidation(const CardSessionInvalidation &invalidation)
{
    if (invalidation.invalidatedSession.epoch != epoch_ || invalidation.observationSequence == 0) {
        return false;
    }
    Key key {invalidation.invalidatedSession.application.slot,
        invalidation.invalidatedSession.application.applicationKey};
    auto current = accepted_.find(key);
    if (current != accepted_.end() && (invalidation.observationSequence <= current->second.sequence ||
        !SameSession(current->second.session, invalidation.invalidatedSession))) {
        return false; // 旧会话撤销不清除已接受的新会话。
    }
    accepted_.insert_or_assign(key,
        Entry {invalidation.invalidatedSession, invalidation.observationSequence, false, {}});
    return true;
}

bool DiscoveryObservationLedger::StillCurrent(const CardDiscoveryObservation &observation) const
{
    Key key {observation.session.application.slot, observation.session.application.applicationKey};
    auto current = accepted_.find(key);
    return current != accepted_.end() && current->second.valid &&
        current->second.sequence == observation.observationSequence && current->second.iccid == observation.iccid &&
        SameSession(current->second.session, observation.session);
}
bool DiscoveryObservationLedger::MatchesStore(const DiscoveryStoreTicket &ticket, const BindingIndex &bindings) const
{
    const auto revision = bindings.ReadPositionRevision(ticket.observation.session.application.slot);
    const Key key {ticket.observation.session.application.slot, ticket.observation.session.application.applicationKey};
    const auto found = accepted_.find(key);
    return revision && *revision == ticket.bindingRevision && StillCurrent(ticket.observation) &&
        found != accepted_.end() && found->second.admission == ticket.admission;
}
std::optional<DiscoveryStoreTicket> DiscoveryObservationLedger::BeginStore(
    const CardDiscoveryObservation &observation, const BindingIndex &bindings)
{
    const auto revision = bindings.ReadPositionRevision(observation.session.application.slot);
    if (!revision || !StillCurrent(observation)) { return std::nullopt; }
    auto &entry = accepted_.at({observation.session.application.slot, observation.session.application.applicationKey});
    if (entry.admission != 0) { return std::nullopt; } // 重复观察不自动重放建行。
    entry.admission = ++nextAdmission_;
    entry.pending = true;
    return DiscoveryStoreTicket {observation, entry.admission, *revision};
}
bool DiscoveryObservationLedger::EndPreparationFailure(const DiscoveryStoreTicket &ticket, bool callFinished,
    const BindingIndex &bindings)
{
    if (!callFinished || !MatchesStore(ticket, bindings)) { return false; }
    auto &entry = accepted_.at({ticket.observation.session.application.slot,
        ticket.observation.session.application.applicationKey});
    if (!entry.pending) { return false; }
    entry.pending = false;
    entry.effect = EffectKnowledge::NotSent; // 仅匹配读取/角色规划失败，尚未发出建行写。
    entry.callFinished = true;
    return true;
}
std::optional<DiscoveryStoreTicket> DiscoveryObservationLedger::RetryAfterKnownNoEffect(
    const DiscoveryStoreTicket &ticket, const BindingIndex &bindings)
{
    if (!MatchesStore(ticket, bindings)) { return std::nullopt; }
    auto &entry = accepted_.at({ticket.observation.session.application.slot,
        ticket.observation.session.application.applicationKey});
    if (entry.pending || !entry.callFinished || (entry.effect != EffectKnowledge::NotSent &&
        entry.effect != EffectKnowledge::Rejected && entry.effect != EffectKnowledge::ConfirmedNotApplied)) {
        return std::nullopt;
    }
    entry.admission = ++nextAdmission_;
    entry.pending = true;
    entry.effect = EffectKnowledge::NotSent;
    entry.callFinished = false;
    return DiscoveryStoreTicket {ticket.observation, entry.admission, ticket.bindingRevision};
}
DiscoveryStoreAcceptance DiscoveryObservationLedger::CompleteStored(const DiscoveryStoreTicket &ticket,
    EffectKnowledge effect, std::optional<SubscriptionId> identity, bool callFinished, BindingIndex &bindings)
{
    const Key key {ticket.observation.session.application.slot, ticket.observation.session.application.applicationKey};
    const auto found = accepted_.find(key);
    if (found != accepted_.end() && found->second.admission == ticket.admission && !found->second.pending) {
        return {false, false, true};
    }
    const bool mayHaveWritten = effect == EffectKnowledge::Unknown || effect == EffectKnowledge::ConfirmedApplied;
    if (!MatchesStore(ticket, bindings)) { return {false, mayHaveWritten, false}; }
    auto &entry = accepted_.at(key);
    entry.pending = false;
    entry.effect = effect;
    entry.callFinished = callFinished;
    if (effect != EffectKnowledge::ConfirmedApplied || !identity) { return {false, mayHaveWritten, false}; }
    const bool accepted = bindings.AcceptBinding(ticket.observation.session.application.slot, *identity,
        ticket.bindingRevision);
    return {accepted, !accepted, false};
}
} // namespace OHOS::Telephony::MultiSim
