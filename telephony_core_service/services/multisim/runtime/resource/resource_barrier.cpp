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
// PROPOSED: 仅资源协议纯逻辑，尚未启用生产准入或转移设备执行权。
#include "runtime/resource/resource_barrier.h"
#include <algorithm>

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameStep(const StepRef &left, const StepRef &right)
{
    return left.request.value == right.request.value && left.step == right.step && left.attempt == right.attempt;
}
bool SameKeys(const std::vector<ResourceKey> &left, const std::vector<ResourceKey> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t index = 0; index < left.size(); ++index) {
        if (left[index].verifiedPhysicalDomain != right[index].verifiedPhysicalDomain) {
            return false;
        }
    }
    return true;
}
}

CheckingTicket ResourceBarrier::TrackResource(const ResourceKey &key)
{
    auto found = resources_.find(key);
    if (found == resources_.end()) {
        found = resources_.emplace(key, ResourceRecord {ResourceState::Checking, ++nextFence_, std::nullopt,
            EffectKnowledge::Unknown}).first;
    }
    return {key, found->second.fence};
}

bool ResourceBarrier::AcceptInitialEvidence(const CheckingTicket &ticket, const InitialResourceEvidence &evidence)
{
    auto found = resources_.find(ticket.resource);
    if (found == resources_.end() || found->second.state != ResourceState::Checking ||
        found->second.fence != ticket.fence || !evidence.oldCommandEnded || !evidence.routeVerified ||
        !evidence.resourceReusable) {
        return false;
    }
    found->second.state = ResourceState::Available;
    return true;
}

std::optional<ResourceLease> ResourceBarrier::Acquire(const std::vector<ResourceKey> &keys, StepRef requester, ResourceUse use)
{
    auto ordered = keys;
    std::sort(ordered.begin(), ordered.end());
    ordered.erase(std::unique(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
        return left.verifiedPhysicalDomain == right.verifiedPhysicalDomain;
    }), ordered.end());
    if (ordered.empty()) {
        return std::nullopt;
    }
    for (const auto &key : ordered) {
        const auto found = resources_.find(key);
        if (found == resources_.end() || found->second.state != ResourceState::Available) {
            return std::nullopt;
        }
    }
    ResourceLease lease {ordered, requester, ++nextFence_, use};
    for (const auto &key : ordered) {
        resources_.at(key) = {ResourceState::InFlight, lease.fence, lease, EffectKnowledge::NotSent};
    }
    return lease;
}

bool ResourceBarrier::Matches(const ResourceLease &lease, ResourceState required) const
{
    if (lease.resources.empty()) {
        return false;
    }
    for (const auto &key : lease.resources) {
        const auto found = resources_.find(key);
        if (found == resources_.end() || found->second.state != required || found->second.fence != lease.fence ||
            !found->second.lease || found->second.lease->use != lease.use || !SameStep(found->second.lease->holder, lease.holder) ||
            !SameKeys(found->second.lease->resources, lease.resources)) {
            return false;
        }
    }
    return true;
}

void ResourceBarrier::Release(const ResourceLease &lease)
{
    const uint64_t releasedFence = ++nextFence_;
    for (const auto &key : lease.resources) {
        resources_.at(key) = {ResourceState::Available, releasedFence, std::nullopt, EffectKnowledge::NotSent};
    }
}

bool ResourceBarrier::MarkSent(const ResourceLease &lease)
{
    if (!Matches(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        resources_.at(key).effect = EffectKnowledge::Unknown;
    }
    return true;
}

bool ResourceBarrier::CancelNotSent(const ResourceLease &lease)
{
    if (!Matches(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        if (resources_.at(key).effect != EffectKnowledge::NotSent) {
            return false;
        }
    }
    Release(lease);
    return true;
}

bool ResourceBarrier::Complete(const ResourceLease &lease, const CompletionEvidence &evidence)
{
    if (!Matches(lease, ResourceState::InFlight) || !SameStep(lease.holder, evidence.original) ||
        !evidence.commandEnded || !evidence.requiredChannelClosed || !evidence.resourceReusable ||
        (lease.use == ResourceUse::Mutation && evidence.effect == EffectKnowledge::Unknown) ||
        evidence.effect == EffectKnowledge::NotSent) {
        return false;
    }
    Release(lease);
    return true;
}

bool ResourceBarrier::Quarantine(const ResourceLease &lease, EffectKnowledge effect)
{
    if (!Matches(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        resources_.at(key).state = ResourceState::Quarantined;
        resources_.at(key).effect = effect;
    }
    return true;
}

bool ResourceBarrier::AcceptRecoveryEvidence(const ResourceLease &lease, const RecoveryEvidence &evidence)
{
    if (!Matches(lease, ResourceState::Quarantined) || !SameStep(lease.holder, evidence.original) ||
        !evidence.oldCommandEnded || (lease.use == ResourceUse::Mutation && !evidence.currentStateVerified) ||
        !evidence.resourceReusable) {
        return false;
    }
    Release(lease);
    return true;
}

ResourceState ResourceBarrier::ReadState(const ResourceKey &key) const
{
    const auto found = resources_.find(key);
    return found == resources_.end() ? ResourceState::Checking : found->second.state;
}
} // namespace OHOS::Telephony::MultiSim
