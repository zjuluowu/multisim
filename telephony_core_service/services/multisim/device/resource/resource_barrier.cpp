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

#include "device/resource/resource_barrier.h"
#include <algorithm>
#include <limits>
#include <new>
#include <type_traits>

namespace OHOS::Telephony::MultiSim {
namespace {
bool EffectAllowsRelease(EffectRequirement requirement, EffectKnowledge effect)
{
    // 只读命令结束且资源可复用时，不因查询载荷丢失永久隔离；写命令仍需效果核对。
    return requirement == EffectRequirement::NoBusinessWrite || effect != EffectKnowledge::Unknown;
}
} // namespace

bool ResourceBarrier::RegisterForInitialCheck(const ResourceKey &key)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (key.verifiedPhysicalDomain.empty()) {
        return false;
    }
    return resources_.emplace(key,
        ResourceRecord {ResourceState::Checking, 0, nullptr, EffectKnowledge::Unknown, false}).second;
}

bool ResourceBarrier::AcceptInitialRecoveryEvidence(const ResourceKey &key,
    const InitialRecoveryEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto record = resources_.find(key);
    if (record == resources_.end() || record->second.state != ResourceState::Checking ||
        evidence.epoch != epoch_ || !evidence.noPriorExecutionCanContinue || !evidence.resourceReusable) {
        return false;
    }
    record->second.state = ResourceState::Available;
    return true;
}

std::optional<ResourceLease> ResourceBarrier::Acquire(const std::vector<ResourceKey> &keys, StepRef requester,
    EffectRequirement requirement)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (keys.empty() || requester.request.value == 0 || requester.request.owner == RequestOwner::Unspecified ||
        requester.request.epoch != epoch_ || nextFence_ == std::numeric_limits<uint64_t>::max()) {
        return std::nullopt;
    }
    static_assert(std::is_nothrow_move_constructible_v<std::optional<ResourceLease>>);
    try {
        std::optional<ResourceLease> lease {ResourceLease {epoch_, keys, requester, nextFence_, requirement}};
        auto &ordered = lease->resources;
        std::sort(ordered.begin(), ordered.end());
        std::vector<ResourceRecord *> records;
        records.reserve(ordered.size());
        for (size_t i = 0; i < ordered.size(); ++i) {
            const auto &key = ordered[i];
            if (i > 0 && ordered[i - 1].verifiedPhysicalDomain == key.verifiedPhysicalDomain) {
                return std::nullopt;
            }
            auto record = resources_.find(key);
            if (record == resources_.end() || record->second.state != ResourceState::Available) {
                return std::nullopt;
            }
            records.push_back(&record->second);
        }
        // The return value and the one immutable record shared by all keys are ready before publication.
        const auto registration = std::make_shared<const ResourceLease>(*lease);
        for (auto *record : records) {
            *record = {ResourceState::InFlight, nextFence_, registration, EffectKnowledge::NotSent, false};
        }
        ++nextFence_;
        return lease;
    } catch (const std::bad_alloc &) {
        return std::nullopt;
    }
}

bool ResourceBarrier::MatchesAll(const ResourceLease &lease, ResourceState state) const
{
    if (lease.epoch != epoch_ || lease.resources.empty()) {
        return false;
    }
    for (const auto &key : lease.resources) {
        auto record = resources_.find(key);
        if (record == resources_.end() || record->second.state != state || !record->second.lease ||
            record->second.fence != lease.fence || !(record->second.lease->holder == lease.holder) ||
            record->second.lease->effectRequirement != lease.effectRequirement ||
            record->second.lease->resources.size() != lease.resources.size()) {
            return false;
        }
        const auto &registeredKeys = record->second.lease->resources;
        for (size_t i = 0; i < registeredKeys.size(); ++i) {
            if (registeredKeys[i].verifiedPhysicalDomain != lease.resources[i].verifiedPhysicalDomain) {
                return false; // 不能用同长度的重复/替换键伪造租约的全资源集合。
            }
        }
    }
    return true;
}

bool ResourceBarrier::MarkSent(const ResourceLease &lease)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!MatchesAll(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        if (resources_.at(key).sent) {
            return false;
        }
    }
    for (const auto &key : lease.resources) {
        resources_.at(key).sent = true;
        resources_.at(key).effect = EffectKnowledge::Unknown;
    }
    return true;
}

void ResourceBarrier::Release(const ResourceLease &lease, EffectKnowledge effect)
{
    for (const auto &key : lease.resources) {
        auto &record = resources_.at(key);
        record.state = ResourceState::Available;
        record.lease.reset();
        record.effect = effect;
        record.sent = false;
    }
}

bool ResourceBarrier::CancelConfirmedNotSent(const ResourceLease &lease)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!MatchesAll(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        if (resources_.at(key).sent) {
            return false;
        }
    }
    Release(lease, EffectKnowledge::NotSent);
    return true;
}

bool ResourceBarrier::Complete(const ResourceLease &lease, const CompletionEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!MatchesAll(lease, ResourceState::InFlight) || !(evidence.original == lease.holder) ||
        !evidence.commandEnded || !evidence.resourceReusable || evidence.effect == EffectKnowledge::NotSent ||
        !EffectAllowsRelease(lease.effectRequirement, evidence.effect)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        if (!resources_.at(key).sent) {
            return false;
        }
    }
    Release(lease, evidence.effect);
    return true;
}

bool ResourceBarrier::Quarantine(const ResourceLease &lease, EffectKnowledge effect)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!MatchesAll(lease, ResourceState::InFlight)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        auto &record = resources_.at(key);
        record.state = ResourceState::Quarantined;
        record.effect = effect;
    }
    return true;
}

bool ResourceBarrier::AcceptRecoveryEvidence(const ResourceLease &lease, const RecoveryEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!MatchesAll(lease, ResourceState::Quarantined) || !(evidence.original == lease.holder) ||
        !evidence.oldCommandEnded || !evidence.currentStateVerified || !evidence.resourceReusable ||
        !EffectAllowsRelease(lease.effectRequirement, evidence.effect)) {
        return false;
    }
    for (const auto &key : lease.resources) {
        if (resources_.at(key).sent && evidence.effect == EffectKnowledge::NotSent) {
            return false; // 已登记发出，恢复只能用ConfirmedNotApplied等效果，不能改写成未发送。
        }
    }
    Release(lease, evidence.effect);
    return true;
}

std::optional<ResourceState> ResourceBarrier::ReadState(const ResourceKey &key) const
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto record = resources_.find(key);
    return record == resources_.end() ? std::nullopt : std::optional<ResourceState>(record->second.state);
}
} // namespace OHOS::Telephony::MultiSim
