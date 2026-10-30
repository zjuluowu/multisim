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

#include "topology/application/activation_progress.h"
namespace OHOS::Telephony::MultiSim {
namespace {
bool SameTicket(const ActivationProgressTicket &a, const ActivationProgressTicket &b)
{
    return a.admission == b.admission && a.request.value == b.request.value && a.slot == b.slot &&
        a.sentSession.epoch == b.sentSession.epoch &&
        a.sentSession.application.slot == b.sentSession.application.slot &&
        a.sentSession.application.applicationKey == b.sentSession.application.applicationKey &&
        a.sentSession.bindingGeneration == b.sentSession.bindingGeneration;
}
}
std::optional<ActivationProgressTicket> ActivationProgress::Prepare(RequestId request, SlotId slot,
    const SessionRef &session)
{
    if (!(slot == session.application.slot)) { return std::nullopt; }
    ActivationProgressTicket ticket {request, slot, session, ++admissions_};
    issued_.insert_or_assign(slot, ticket);
    return ticket;
}
bool ActivationProgress::Begin(const ActivationProgressTicket &ticket)
{
    const auto issued = issued_.find(ticket.slot);
    if (issued == issued_.end() || !SameTicket(issued->second, ticket) ||
        ticket.admission <= lastStarted_[ticket.slot]) { return false; }
    lastStarted_[ticket.slot] = ticket.admission;
    transitions_.insert_or_assign(ticket.slot, ticket);
    return true;
}
bool ActivationProgress::Clear(const ActivationProgressTicket &ticket)
{
    const auto found = transitions_.find(ticket.slot);
    if (found == transitions_.end() || !SameTicket(found->second, ticket)) { return false; }
    transitions_.erase(found);
    return true;
}
bool ActivationProgress::IsInProgress(SlotId slot) const { return transitions_.count(slot) != 0; }
} // namespace OHOS::Telephony::MultiSim
