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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// C0参数/转换投影已实现；设备切换及业务重试未由本文件执行。

#include "role_assignment/assignment_aggregate.h"

namespace OHOS::Telephony::MultiSim {
bool AssignmentAggregate::AcceptRoleSelection(const RoleSelection &selection, uint64_t expectedAssignmentRevision)
{
    if (expectedAssignmentRevision != revision_) { return false; }
    roles_[selection.role] = selection;
    ++revision_;
    return true;
}

std::optional<RoleSelection> AssignmentAggregate::ReadRoleSelection(DefaultRole role) const
{
    const auto found = roles_.find(role);
    return found == roles_.end() ? std::nullopt : std::make_optional(found->second);
}

bool AssignmentAggregate::AdmitRoleWriteSet(uint64_t ownerToken, uint32_t members)
{
    if (members == 0) { return false; }
    for (const auto &[owner, held] : roleWriteOwners_) {
        if (owner != ownerToken && (held & members) != 0) { return false; }
    }
    auto found = roleWriteOwners_.find(ownerToken);
    if (found != roleWriteOwners_.end() && (found->second & members) != members) { return false; }
    roleWriteOwners_[ownerToken] = members;
    return true;
}

bool AssignmentAggregate::ReleaseRoleWriteSet(uint64_t ownerToken, uint32_t members)
{
    const auto found = roleWriteOwners_.find(ownerToken);
    if (found == roleWriteOwners_.end() || (found->second & members) != members) { return false; }
    found->second &= ~members;
    if (found->second == 0) { roleWriteOwners_.erase(found); }
    return true;
}

bool AssignmentAggregate::OwnsRoleWriteSet(uint64_t ownerToken, uint32_t members) const
{
    const auto found = roleWriteOwners_.find(ownerToken);
    return found != roleWriteOwners_.end() && (found->second & members) == members;
}

TransitionAdmission AssignmentAggregate::AdmitTransitionWithRoleWriteSet(const SlotDescriptor &target,
    uint64_t transitionToken, uint32_t members)
{
    if (!target.radioDomain) { return TransitionAdmission::UnknownResourceDomain; }
    const auto found = transitions_.find(*target.radioDomain);
    if (found != transitions_.end() && found->second.phase != TransitionPhase::Completed &&
        found->second.phase != TransitionPhase::Failed) {
        return TransitionAdmission::DomainBusy;
    }
    if (!AdmitRoleWriteSet(transitionToken, members)) { return TransitionAdmission::DomainBusy; }
    transitions_[*target.radioDomain] = AssignmentTransition {*target.radioDomain, target.slot,
        transitionToken, TransitionPhase::Admitted, 0};
    ++revision_;
    return TransitionAdmission::Admitted;
}

TransitionAdmission AssignmentAggregate::AdmitTransition(const SlotDescriptor &target, uint64_t transitionToken)
{
    if (!target.radioDomain) { return TransitionAdmission::UnknownResourceDomain; }
    const auto found = transitions_.find(*target.radioDomain);
    if (found != transitions_.end() && found->second.phase != TransitionPhase::Completed &&
        found->second.phase != TransitionPhase::Failed) {
        return TransitionAdmission::DomainBusy;
    }
    transitions_[*target.radioDomain] = AssignmentTransition {*target.radioDomain, target.slot,
        transitionToken, TransitionPhase::Admitted, 0};
    ++revision_;
    return TransitionAdmission::Admitted;
}

bool AssignmentAggregate::AcceptTransitionPhase(const RadioDomainId &domain, uint64_t transitionToken,
    TransitionPhase expected, TransitionPhase next)
{
    auto found = transitions_.find(domain);
    if (found == transitions_.end() || found->second.transitionToken != transitionToken ||
        found->second.phase != expected) {
        return false;
    }
    found->second.phase = next;
    ++revision_;
    return true;
}

bool AssignmentAggregate::IsSwitchInProgress(const RadioDomainId &domain) const
{
    const auto found = transitions_.find(domain);
    return found != transitions_.end() && found->second.phase != TransitionPhase::Completed &&
        found->second.phase != TransitionPhase::Failed && found->second.phase != TransitionPhase::NeedsReconciliation;
}

void AssignmentAggregate::AcceptPrimaryParameterProjection(SlotId lastPrimary)
{
    lastPrimary_ = lastPrimary;
    ++revision_;
}

void AssignmentAggregate::AcceptDataParameterProjection(SlotId lastData)
{
    lastData_ = lastData;
    ++revision_;
}
} // namespace OHOS::Telephony::MultiSim
