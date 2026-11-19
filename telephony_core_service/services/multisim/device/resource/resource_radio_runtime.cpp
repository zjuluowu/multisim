/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "device/resource/resource_radio_runtime.h"

#include <algorithm>

namespace OHOS::Telephony::MultiSim {
ResourceRadioRuntime::ResourceRadioRuntime(RuntimeEpoch epoch, RadioPlatformPort &platform,
    std::vector<ResourceKey> primaryResources)
    : platform_(platform), resources_(std::move(primaryResources)), barrier_(epoch)
{
    for (const auto &resource : resources_) { (void)barrier_.RegisterForInitialCheck(resource); }
}

bool ResourceRadioRuntime::Initialize(const ResourceKey &key, const InitialRecoveryEvidence &evidence)
{
    return barrier_.AcceptInitialRecoveryEvidence(key, evidence);
}

PrimaryRadioObservation ResourceRadioRuntime::ReadPrimaryRadioObservation(SlotId slot) const
{
    return platform_.ReadPrimaryRadioObservation(slot);
}

bool ResourceRadioRuntime::SameGroup(const RadioSwitchRequest &left, const RadioSwitchRequest &right)
{
    return left.step == right.step && left.protocolSession == right.protocolSession &&
        left.phase == right.phase && left.mode == right.mode && left.domain.key == right.domain.key &&
        left.memberCount == right.memberCount;
}

bool ResourceRadioRuntime::SameBinding(const RadioSwitchRequest &request, const RadioSwitchResult &result)
{
    return (!request.expectedBinding && !result.expectedBinding) ||
        (request.expectedBinding && result.expectedBinding &&
         request.expectedBinding->subscription == result.expectedBinding->subscription &&
         request.expectedBinding->slot == result.expectedBinding->slot &&
         request.expectedBinding->bindingRevision == result.expectedBinding->bindingRevision);
}

bool ResourceRadioRuntime::SameCommand(const RadioSwitchRequest &request, const RadioSwitchResult &result)
{
    const bool notifyExpected = request.mode == PrimarySwitchMode::ThreePhaseProtocol && request.phase == 2;
    return result.step == request.step && result.memberSlot == request.target && SameBinding(request, result) &&
        result.protocolSession == request.protocolSession && result.phase == request.phase &&
        result.notify == notifyExpected;
}

bool ResourceRadioRuntime::SameRequest(const RadioSwitchRequest &left, const RadioSwitchRequest &right)
{
    const bool binding = (!left.expectedBinding && !right.expectedBinding) ||
        (left.expectedBinding && right.expectedBinding &&
         left.expectedBinding->subscription == right.expectedBinding->subscription &&
         left.expectedBinding->slot == right.expectedBinding->slot &&
         left.expectedBinding->bindingRevision == right.expectedBinding->bindingRevision);
    return SameGroup(left, right) && left.target == right.target && binding;
}

ResourceRadioRuntime::Group *ResourceRadioRuntime::FindGroup(const RadioSwitchRequest &request)
{
    for (auto &[fence, group] : groups_) {
        if (SameGroup(group.first, request)) { return &group; }
        (void)fence;
    }
    return nullptr;
}

void ResourceRadioRuntime::SwitchPrimaryRadio(const RadioSwitchRequest &request)
{
    lastSubmission_ = request;
    lastSubmissionAccepted_ = false;
    if (stopping_ || !sink_) { return; }
    Group *group = FindGroup(request);
    if (!group) {
        const auto lease = barrier_.Acquire(resources_, request.step);
        if (!lease) { return; }
        auto inserted = groups_.emplace(lease->fence,
            Group {*lease, request, request.memberCount == 0 ? 1u : request.memberCount,
                {}, {}, {}, {}, {}, {}, false, false});
        group = &inserted.first->second;
    }
    if (group->quarantined || group->submittedMembers.size() >= group->expected ||
        group->submittedMembers.find(request.target.legacySlotId) != group->submittedMembers.end()) {
        return;
    }
    const uint64_t transport = nextTransportToken_++;
    transmissions_[transport] = {group->lease.fence, request};
    if (!platform_.SubmitPrimaryRadio(request, transport)) {
        transmissions_.erase(transport);
        const uint64_t fence = group->lease.fence;
        if (group->accepted.empty() && barrier_.CancelConfirmedNotSent(group->lease)) {
            groups_.erase(fence);
        } else {
            Quarantine(*group, EffectKnowledge::Unknown);
        }
        return;
    }
    ++submittedCommands_;
    group->submittedMembers.insert(request.target.legacySlotId);
    group->submittedTransmissions.insert(transport);
    if (!group->sent) {
        if (!barrier_.MarkSent(group->lease)) {
            Quarantine(*group, EffectKnowledge::Unknown);
            return;
        }
        group->sent = true;
    }
    lastSubmissionAccepted_ = true;
}

bool ResourceRadioRuntime::WasLastSubmissionAccepted(const RadioSwitchRequest &request) const
{
    return lastSubmission_ && SameRequest(*lastSubmission_, request) && lastSubmissionAccepted_;
}

void ResourceRadioRuntime::EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain)
{
    if (!stopping_) {
        for (auto &entry : groups_) {
            if (entry.second.lease.holder == owner && !entry.second.quarantined) {
                Quarantine(entry.second, EffectKnowledge::Unknown);
            }
        }
    }
    platform_.EndLegacySenseSwitch(owner, domain);
}

void ResourceRadioRuntime::Quarantine(Group &group, EffectKnowledge effect)
{
    group.quarantined = true;
    (void)barrier_.Quarantine(group.lease, effect);
}

void ResourceRadioRuntime::ForwardCompleted(Group &group)
{
    if (!sink_) { return; }
    for (const auto &result : group.accepted) { sink_->AcceptRadioStep(result); }
}

bool ResourceRadioRuntime::AcceptDeviceResult(const RadioDeviceResult &deviceResult)
{
    if (!sink_) { return false; }
    const auto transmission = transmissions_.find(deviceResult.transportToken);
    if (transmission == transmissions_.end()) { ++rejectedStaleResults_; return false; }
    const RadioSwitchRequest request = transmission->second.request;
    const uint64_t fence = transmission->second.fence;
    const auto groupFound = groups_.find(fence);
    if (groupFound == groups_.end()) {
        transmissions_.erase(transmission);
        ++rejectedStaleResults_;
        return false;
    }
    Group &group = groupFound->second;
    const RadioSwitchResult &result = deviceResult.result;
    if (group.quarantined) {
        if (!SameCommand(request, result) ||
            group.recoveredTransmissions.find(deviceResult.transportToken) !=
                group.recoveredTransmissions.end() ||
            !deviceResult.commandEnded || !deviceResult.resourceReusable ||
            !deviceResult.currentStateVerified || result.effect == EffectKnowledge::Unknown) {
            ++rejectedStaleResults_;
            return false;
        }
        group.recoveredTransmissions.insert(deviceResult.transportToken);
        group.recovered.push_back(result);
        transmissions_.erase(transmission);
        if (group.recoveredTransmissions.size() < group.submittedTransmissions.size()) { return true; }
        EffectKnowledge effect = EffectKnowledge::ConfirmedApplied;
        for (const auto &row : group.recovered) {
            if (row.effect == EffectKnowledge::ConfirmedNotApplied || row.effect == EffectKnowledge::Rejected) {
                effect = row.effect;
                break;
            }
        }
        if (!barrier_.AcceptRecoveryEvidence(group.lease,
            {group.lease.holder, effect, true, true, true})) {
            return false;
        }
        if (sink_) {
            for (const auto &late : group.accepted) { sink_->AcceptLateRadioEvidence(late); }
            for (const auto &late : group.recovered) { sink_->AcceptLateRadioEvidence(late); }
        }
        groups_.erase(groupFound);
        return true;
    }
    if (!SameCommand(request, result)) {
        ++rejectedStaleResults_;
        return false;
    }
    if (!deviceResult.commandEnded || !deviceResult.resourceReusable ||
        result.effect == EffectKnowledge::Unknown) {
        ++rejectedStaleResults_;
        Quarantine(group, result.effect);
        auto unresolved = result;
        unresolved.effect = EffectKnowledge::Unknown;
        if (sink_) { sink_->AcceptRadioStep(unresolved); }
        return false;
    }
    if (group.acceptedMembers.find(result.memberSlot.legacySlotId) != group.acceptedMembers.end()) {
        ++rejectedStaleResults_;
        return false;
    }
    transmissions_.erase(transmission);
    group.submittedTransmissions.erase(deviceResult.transportToken);
    group.acceptedMembers.insert(result.memberSlot.legacySlotId);
    group.accepted.push_back(result);
    if (group.acceptedMembers.size() < group.expected) { return true; }
    bool reusable = std::all_of(group.accepted.begin(), group.accepted.end(), [](const RadioSwitchResult &row) {
        return row.effect != EffectKnowledge::Unknown;
    });
    EffectKnowledge effect = EffectKnowledge::ConfirmedApplied;
    for (const auto &row : group.accepted) {
        if (row.effect == EffectKnowledge::ConfirmedNotApplied || row.effect == EffectKnowledge::Rejected) {
            effect = row.effect;
            break;
        }
    }
    if (!reusable || !barrier_.Complete(group.lease,
        {group.lease.holder, effect, true, true})) {
        Quarantine(group, effect);
        return false;
    }
    ForwardCompleted(group);
    groups_.erase(groupFound);
    return true;
}

bool ResourceRadioRuntime::Recover(const StepRef &original, const RecoveryEvidence &evidence)
{
    if (!(original == evidence.original)) { return false; }
    for (auto found = groups_.begin(); found != groups_.end();) {
        if (found->second.lease.holder == original && found->second.submittedTransmissions.empty() &&
            barrier_.AcceptRecoveryEvidence(found->second.lease, evidence)) {
            for (auto tx = transmissions_.begin(); tx != transmissions_.end();) {
                if (tx->second.fence == found->first) { tx = transmissions_.erase(tx); }
                else { ++tx; }
            }
            found = groups_.erase(found);
            return true;
        }
        ++found;
    }
    return false;
}

bool ResourceRadioRuntime::StopAccepting()
{
    stopping_ = true;
    return OwnerCallbacksDrained() && DeviceResourcesDrained();
}

bool ResourceRadioRuntime::DeviceResourcesDrained() const
{
    for (const auto &resource : resources_) {
        const auto state = barrier_.ReadState(resource);
        if (!state || *state != ResourceState::Available) { return false; }
    }
    return groups_.empty() && transmissions_.empty();
}

std::optional<ResourceState> ResourceRadioRuntime::ReadResourceState(const ResourceKey &key) const
{
    return barrier_.ReadState(key);
}
} // namespace OHOS::Telephony::MultiSim
