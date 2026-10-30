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

#include "topology/application/discovery_admission.h"

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameSession(const SessionRef &a, const SessionRef &b)
{
    return a.epoch == b.epoch && a.application.slot == b.application.slot &&
        a.application.applicationKey == b.application.applicationKey && a.bindingGeneration == b.bindingGeneration;
}
}
bool DiscoveryAdmission::Fence(const SessionRef &session, uint64_t sequence)
{
    if (session.epoch != epoch_) { return false; }
    const auto found = entries_.find(session.application.slot);
    if (found != entries_.end() && sequence <= found->second.sequence) { return false; }
    entries_.insert_or_assign(session.application.slot, Entry {session, sequence, 0, false, EffectKnowledge::NotSent, false, {}});
    return true;
}
std::optional<DiscoveryTicket> DiscoveryAdmission::Begin(const CardDiscoveryObservation &observation,
    const BindingIndex &bindings)
{
    auto found = entries_.find(observation.session.application.slot);
    if (found == entries_.end() || !bindings.HasSlot(observation.session.application.slot) || observation.iccid.empty() ||
        found->second.sequence != observation.observationSequence ||
        !SameSession(found->second.session, observation.session) || found->second.admission != 0) {
        return std::nullopt;
    }
    found->second.admission = ++admissions_;
    found->second.pending = true;
    found->second.iccid = observation.iccid;
    return DiscoveryTicket {observation, found->second.admission, bindings.Revision(observation.session.application.slot)};
}
bool DiscoveryAdmission::MatchesFence(const DiscoveryTicket &ticket, const BindingIndex &bindings) const
{
    const auto found = entries_.find(ticket.observation.session.application.slot);
    return found != entries_.end() && found->second.admission == ticket.admission &&
        found->second.sequence == ticket.observation.observationSequence && found->second.iccid == ticket.observation.iccid &&
        SameSession(found->second.session, ticket.observation.session) &&
        bindings.Revision(ticket.observation.session.application.slot) == ticket.bindingRevision;
}
bool DiscoveryAdmission::IsCurrent(const DiscoveryTicket &ticket, const BindingIndex &bindings) const
{
    return MatchesFence(ticket, bindings) && entries_.at(ticket.observation.session.application.slot).pending;
}
bool DiscoveryAdmission::EndPreparationFailure(const DiscoveryTicket &ticket, bool callFinished,
    const BindingIndex &bindings)
{
    if (!callFinished || !IsCurrent(ticket, bindings)) { return false; }
    auto &entry = entries_.at(ticket.observation.session.application.slot);
    entry.pending = false;
    entry.lastEffect = EffectKnowledge::NotSent; // 仅匹配查询/纯规划失败，尚未调用建行写入。
    entry.callFinished = true;
    return true;
}
std::optional<DiscoveryTicket> DiscoveryAdmission::RetryAfterKnownNoEffect(const DiscoveryTicket &ticket,
    const BindingIndex &bindings)
{
    if (!MatchesFence(ticket, bindings)) { return std::nullopt; }
    auto &entry = entries_.at(ticket.observation.session.application.slot);
    const bool noEffect = entry.lastEffect == EffectKnowledge::NotSent ||
        entry.lastEffect == EffectKnowledge::Rejected || entry.lastEffect == EffectKnowledge::ConfirmedNotApplied;
    if (entry.pending || !entry.callFinished || !noEffect) { return std::nullopt; }
    entry.admission = ++admissions_;
    entry.pending = true;
    entry.lastEffect = EffectKnowledge::NotSent;
    entry.callFinished = false;
    return DiscoveryTicket {ticket.observation, entry.admission, ticket.bindingRevision};
}
bool DiscoveryAdmission::Finish(const DiscoveryTicket &ticket, const BindingIndex &bindings)
{
    if (!IsCurrent(ticket, bindings)) { return false; }
    auto &entry = entries_.at(ticket.observation.session.application.slot);
    entry.pending = false;
    entry.lastEffect = EffectKnowledge::Unknown; // 无存储证据的通用关闭不授权重试。
    entry.callFinished = false;
    return true;
}
DiscoveryStoreAcceptance DiscoveryAdmission::CompleteStored(const DiscoveryTicket &ticket, EffectKnowledge effect,
    std::optional<SubscriptionId> identity, bool callFinished, BindingIndex &bindings)
{
    const auto found = entries_.find(ticket.observation.session.application.slot);
    if (found != entries_.end() && found->second.admission == ticket.admission && !found->second.pending) {
        return {false, false, true}; // 同一终结结果不重复建绑定或触发下一步。
    }
    const bool mayHaveWritten = effect == EffectKnowledge::ConfirmedApplied || effect == EffectKnowledge::Unknown;
    if (!IsCurrent(ticket, bindings)) { return {false, mayHaveWritten, false}; }
    // 终结一次票据：Unknown留字段核对；已知无效果且调用结束才允许原用例显式重试。
    auto &entry = entries_.at(ticket.observation.session.application.slot);
    entry.pending = false;
    entry.lastEffect = effect;
    entry.callFinished = callFinished;
    if (effect != EffectKnowledge::ConfirmedApplied || !identity) { return {false, mayHaveWritten, false}; }
    const bool accepted = bindings.AcceptBinding(ticket.observation.session.application.slot, *identity,
        ticket.bindingRevision);
    return {accepted, !accepted, false};
}
} // namespace OHOS::Telephony::MultiSim
