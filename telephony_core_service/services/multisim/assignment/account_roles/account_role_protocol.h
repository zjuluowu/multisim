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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ACCOUNT_ROLES_ACCOUNT_ROLE_PROTOCOL_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ACCOUNT_ROLES_ACCOUNT_ROLE_PROTOCOL_H
#include <map>
#include "assignment/api/assignment_requests.h"
namespace OHOS::Telephony::MultiSim {
// A受控域内的唯一建行资格及角色投影；不是provider CAS，未知效果不释放资格。
class AccountRoleProtocol final {
public:
    std::optional<AccountRoleFragment> Prepare(const AccountRolePlanningInput &input)
    {
        if (!reservations_.empty()) { return std::nullopt; }
        const bool main = input.useLegacySingleSlotRoles;
        AccountRoleFragment plan {input.existingSubscription, {++nextToken_, roleRevision_},
            main, main, main, main, !input.existingSubscription || main};
        reservations_.emplace(plan.reservation.token, ReservedPlan {plan, std::nullopt});
        return plan;
    }
    bool StillOwns(const AccountRoleFragment &plan) const
    {
        const auto found = reservations_.find(plan.reservation.token);
        return found != reservations_.end() && SamePlan(found->second.plan, plan) &&
            plan.reservation.assignmentRevision == roleRevision_;
    }
    bool AcceptDiscovered(const AccountRoleFragment &plan, SubscriptionId identity)
    {
        if (!StillOwns(plan) || (plan.subscription && !(*plan.subscription == identity))) { return false; }
        reservations_.at(plan.reservation.token).resolvedIdentity = identity;
        return true; // 原Insert不是current/all发布点；后续原重载才替换角色缓存。
    }
    bool Finalize(const AccountRoleFragment &plan, const InitialRoleFinalizationEvidence &evidence)
    {
        const auto found = reservations_.find(plan.reservation.token);
        if (found == reservations_.end() || !SamePlan(found->second.plan, plan) || !MayFinalizeInitialRoles(evidence)) {
            return false;
        }
        reservations_.erase(found); // 旧计划仅在已接纳/核对后结束，不必强求仍是当前角色版本。
        return true;
    }
    uint64_t Revision(bool historical) const { return historical ? historical_.revision : current_.revision; }
    bool AcceptReloadSet(const std::vector<StoredAccountRoleFragment> &rows, uint64_t attempt,
        uint64_t expectedRevision, bool historical)
    {
        auto &projection = historical ? historical_ : current_;
        if (projection.revision != expectedRevision || attempt <= projection.lastAttempt) { return false; }
        std::map<SubscriptionId, StoredAccountRoleFragment> next;
        for (const auto &row : rows) {
            if (!next.emplace(row.subscription, row).second) { return false; }
        }
        projection.rows = std::move(next);
        projection.lastAttempt = attempt;
        ++projection.revision;
        ++roleRevision_;
        return true;
    }
    bool AcceptSingleProjection(const StoredAccountRoleFragment &row, uint64_t expectedRevision, bool historical)
    {
        auto &projection = historical ? historical_ : current_;
        if (projection.revision != expectedRevision) { return false; }
        projection.rows.insert_or_assign(row.subscription, row);
        ++projection.revision;
        ++roleRevision_;
        return true;
    }
    void NoteLocalRoleMutation()
    {
        ++roleRevision_;
        ++current_.revision; // 原setter仅更新current；historical保留原重载节点。
    }
    const std::map<SubscriptionId, StoredAccountRoleFragment> &Projection(bool historical) const
    { return historical ? historical_.rows : current_.rows; }
private:
    static bool SamePlan(const AccountRoleFragment &a, const AccountRoleFragment &b)
    {
        return a.subscription == b.subscription && a.reservation.token == b.reservation.token &&
            a.reservation.assignmentRevision == b.reservation.assignmentRevision && a.voice == b.voice &&
            a.sms == b.sms && a.mainCardColumn == b.mainCardColumn && a.dataCardColumn == b.dataCardColumn &&
            a.includeInStorage == b.includeInStorage;
    }
    struct ReservedPlan { AccountRoleFragment plan; std::optional<SubscriptionId> resolvedIdentity; };
    struct RoleProjection {
        std::map<SubscriptionId, StoredAccountRoleFragment> rows;
        uint64_t revision {0};
        uint64_t lastAttempt {0};
    };
    RoleProjection current_;
    RoleProjection historical_;
    uint64_t nextToken_ {0};
    uint64_t roleRevision_ {0};
    std::map<uint64_t, ReservedPlan> reservations_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
