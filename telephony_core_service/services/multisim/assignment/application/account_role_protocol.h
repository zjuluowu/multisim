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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.1、§11～12。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_ACCOUNT_ROLE_PROTOCOL_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_ACCOUNT_ROLE_PROTOCOL_H
#include <map>
#include "assignment/api/assignment_requests.h"
namespace OHOS::Telephony::MultiSim {
// A内部联合建行/重载接纳协议。所有调用在A受控域串行，非provider CAS。
class AccountRoleProtocol final {
public:
    AccountRoleFragment Prepare(const DiscoveryRoleContext &context, bool legacySingleSlot) const
    {
        const bool initial = legacySingleSlot;
        return {context.matchedSubscription, revision_, context.observationSequence,
            !context.matchedSubscription || legacySingleSlot, initial, initial, initial, initial};
    }
    bool IsCurrent(const AccountRoleFragment &fragment) const
    { return fragment.assignmentRevision == revision_; }
    RoleFragmentAcceptance AcceptStorage(const AccountRoleFragment &fragment,
        std::optional<SubscriptionId> identity, EffectKnowledge effect) const
    {
        if (effect == EffectKnowledge::NotSent || effect == EffectKnowledge::Rejected ||
            effect == EffectKnowledge::ConfirmedNotApplied) { return RoleFragmentAcceptance::NoEffect; }
        if (effect != EffectKnowledge::ConfirmedApplied || !identity || !IsCurrent(fragment) ||
            (fragment.subscription && !(*fragment.subscription == *identity))) {
            return RoleFragmentAcceptance::NeedsReconciliation;
        }
        // 原建行并非缓存直接发布点；独立current/all重载再接纳角色。
        return RoleFragmentAcceptance::ReadyForOwnerReload;
    }
    RoleReloadTicket BeginReload(AccountReadView view, uint64_t attempt)
    { attempts_[view] = attempt; return {view, attempt, revision_}; }
    bool AcceptReload(const RoleReloadTicket &ticket, const std::vector<AccountRoleFragment> &rows,
        bool readSucceeded)
    {
        const auto attempt = attempts_.find(ticket.view);
        if (!readSucceeded || attempt == attempts_.end() || attempt->second != ticket.attempt ||
            ticket.assignmentRevision != revision_) { return false; }
        std::map<SubscriptionId, AccountRoleFragment> next;
        for (const auto &row : rows) {
            // I先按原投影资格筛选；Owner不能静默合并重复身份或丢弃缺身份片段。
            if (!row.subscription || !next.emplace(*row.subscription, row).second) { return false; }
        }
        projections_[ticket.view] = std::move(next);
        attempts_.erase(attempt); // 成员资格一次接纳；失败保留旧投影。
        return true;
    }
    void NoteLocalRoleMutation() { ++revision_; }
    uint64_t Revision() const { return revision_; }
    const std::map<SubscriptionId, AccountRoleFragment> &Projection(AccountReadView view) const
    {
        const auto found = projections_.find(view);
        static const std::map<SubscriptionId, AccountRoleFragment> empty;
        return found == projections_.end() ? empty : found->second;
    }
private:
    uint64_t revision_ {0};
    std::map<AccountReadView, uint64_t> attempts_;
    std::map<AccountReadView, std::map<SubscriptionId, AccountRoleFragment>> projections_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
