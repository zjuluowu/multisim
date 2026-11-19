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
    std::lock_guard<std::mutex> guard(lock_);
    return barrier_.AcceptInitialRecoveryEvidence(key, evidence);
}

bool ResourceRadioRuntime::OwnerCallbacksDrained() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return groups_.empty() && transmissions_.empty() && submittingTransmissions_.empty() &&
        pendingSubmissionResults_.empty() && (!dispatch_ || dispatch_->RadioOwnerCallbacksDrained());
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
    uint64_t fence = 0;
    uint64_t transport = 0;
    {
        std::lock_guard<std::mutex> guard(lock_);
        lastSubmission_ = request;
        lastSubmissionAccepted_ = false;
        if (stopping_ || sink_.expired()) { return; }
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
        fence = group->lease.fence;
        transport = nextTransportToken_++;
        transmissions_[transport] = {fence, request, std::nullopt};
        submittingTransmissions_.insert(transport);
    }

    // No R state lock is held while entering the platform handoff boundary.
    const auto handoff = platform_.SubmitPrimaryRadio(request, transport);

    std::optional<RadioDeviceResult> pending;
    {
        std::lock_guard<std::mutex> guard(lock_);
        submittingTransmissions_.erase(transport);
        const auto found = groups_.find(fence);
        if (found == groups_.end()) { transmissions_.erase(transport); return; }
        Group &group = found->second;
        const bool callbackContradiction = pendingSubmissionResults_.find(transport) !=
            pendingSubmissionResults_.end();
        if (handoff.disposition == RadioSubmitDisposition::ConfirmedNotSent) {
            transmissions_.erase(transport);
            pendingSubmissionResults_.erase(transport);
            if (!callbackContradiction && group.accepted.empty() &&
                barrier_.CancelConfirmedNotSent(group.lease)) {
                groups_.erase(found);
            } else {
                Quarantine(group, EffectKnowledge::Unknown);
            }
            return;
        }
        if (!group.sent) {
            if (!barrier_.MarkSent(group.lease)) {
                Quarantine(group, EffectKnowledge::Unknown);
                return;
            }
            group.sent = true;
        }
        ++submittedCommands_;
        transmissions_.at(transport).transportSerial = handoff.transportSerial;
        group.submittedMembers.insert(request.target.legacySlotId);
        group.submittedTransmissions.insert(transport);
        if (handoff.disposition == RadioSubmitDisposition::HandoffUnknown) {
            Quarantine(group, EffectKnowledge::Unknown);
        } else {
            lastSubmissionAccepted_ = true;
        }
        const auto pendingFound = pendingSubmissionResults_.find(transport);
        if (pendingFound != pendingSubmissionResults_.end()) {
            pending = pendingFound->second;
            pendingSubmissionResults_.erase(pendingFound);
        }
    }
    if (pending) { (void)AcceptDeviceResult(*pending); }
}

bool ResourceRadioRuntime::WasLastSubmissionAccepted(const RadioSwitchRequest &request) const
{
    std::lock_guard<std::mutex> guard(lock_);
    return lastSubmission_ && SameRequest(*lastSubmission_, request) && lastSubmissionAccepted_;
}

void ResourceRadioRuntime::EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!stopping_) {
            for (auto &entry : groups_) {
                if (entry.second.lease.holder == owner && !entry.second.quarantined) {
                    Quarantine(entry.second, EffectKnowledge::Unknown);
                }
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

bool ResourceRadioRuntime::AcceptDeviceResult(const RadioDeviceResult &deviceResult)
{
    std::vector<RadioSwitchResult> normal;
    std::vector<RadioSwitchResult> late;
    bool accepted = false;
    std::shared_ptr<RadioResultSink> sink;
    RadioOwnerDispatchPort *dispatch = nullptr;
    {
        std::lock_guard<std::mutex> guard(lock_);
        sink = sink_.lock();
        dispatch = dispatch_;
        if (submittingTransmissions_.find(deviceResult.transportToken) != submittingTransmissions_.end()) {
            pendingSubmissionResults_[deviceResult.transportToken] = deviceResult;
            return true;
        }
        const auto transmission = transmissions_.find(deviceResult.transportToken);
        if (transmission == transmissions_.end()) { ++rejectedStaleResults_; return false; }
        const RadioSwitchRequest request = transmission->second.request;
        if (deviceResult.transportSerial && transmission->second.transportSerial &&
            *deviceResult.transportSerial != *transmission->second.transportSerial) {
            ++rejectedStaleResults_;
            return false;
        }
        const uint64_t fence = transmission->second.fence;
        const auto groupFound = groups_.find(fence);
        if (groupFound == groups_.end()) {
            transmissions_.erase(transmission);
            ++rejectedStaleResults_;
            return false;
        }
        Group &group = groupFound->second;
        const RadioSwitchResult &result = deviceResult.result;
        if (!SameCommand(request, result)) { ++rejectedStaleResults_; return false; }
        if (group.quarantined) {
            if (group.recoveredTransmissions.find(deviceResult.transportToken) !=
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
            late.insert(late.end(), group.accepted.begin(), group.accepted.end());
            late.insert(late.end(), group.recovered.begin(), group.recovered.end());
            groups_.erase(groupFound);
            accepted = true;
        } else {
            if (!deviceResult.commandEnded || !deviceResult.resourceReusable ||
                result.effect == EffectKnowledge::Unknown) {
                ++rejectedStaleResults_;
                Quarantine(group, result.effect);
                auto unresolved = result;
                unresolved.effect = EffectKnowledge::Unknown;
                normal.push_back(unresolved);
                accepted = false;
            } else {
                if (group.acceptedMembers.find(result.memberSlot.legacySlotId) != group.acceptedMembers.end()) {
                    ++rejectedStaleResults_;
                    return false;
                }
                transmissions_.erase(transmission);
                group.submittedTransmissions.erase(deviceResult.transportToken);
                group.acceptedMembers.insert(result.memberSlot.legacySlotId);
                group.accepted.push_back(result);
                if (group.acceptedMembers.size() < group.expected) { accepted = true; }
                else {
                    EffectKnowledge effect = EffectKnowledge::ConfirmedApplied;
                    for (const auto &row : group.accepted) {
                        if (row.effect == EffectKnowledge::ConfirmedNotApplied ||
                            row.effect == EffectKnowledge::Rejected) {
                            effect = row.effect;
                            break;
                        }
                    }
                    if (!barrier_.Complete(group.lease, {group.lease.holder, effect, true, true})) {
                        Quarantine(group, effect);
                        return false;
                    }
                    normal.assign(group.accepted.begin(), group.accepted.end());
                    groups_.erase(groupFound);
                    accepted = true;
                }
            }
        }
    }
    if (!sink) { return accepted; }
    if (dispatch) {
        for (const auto &result : normal) {
            if (!dispatch->PostRadioOwnerTask([sink, result] { sink->AcceptRadioStep(result); })) {
                return false;
            }
        }
        for (const auto &result : late) {
            if (!dispatch->PostRadioOwnerTask([sink, result] { sink->AcceptLateRadioEvidence(result); })) {
                return false;
            }
        }
    } else {
        for (const auto &result : normal) { sink->AcceptRadioStep(result); }
        for (const auto &result : late) { sink->AcceptLateRadioEvidence(result); }
    }
    return accepted;
}

bool ResourceRadioRuntime::Recover(const StepRef &original, const RecoveryEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(lock_);
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
    std::lock_guard<std::mutex> guard(lock_);
    stopping_ = true;
    if (!groups_.empty() || !transmissions_.empty() || !submittingTransmissions_.empty() ||
        !pendingSubmissionResults_.empty()) { return false; }
    for (const auto &resource : resources_) {
        const auto state = barrier_.ReadState(resource);
        if (!state || *state != ResourceState::Available) { return false; }
    }
    return !dispatch_ || dispatch_->RadioOwnerCallbacksDrained();
}

bool ResourceRadioRuntime::DeviceResourcesDrained() const
{
    std::lock_guard<std::mutex> guard(lock_);
    for (const auto &resource : resources_) {
        const auto state = barrier_.ReadState(resource);
        if (!state || *state != ResourceState::Available) { return false; }
    }
    return groups_.empty() && transmissions_.empty() && submittingTransmissions_.empty() &&
        pendingSubmissionResults_.empty();
}

std::optional<int32_t> ResourceRadioRuntime::ReadTransportSerial(uint64_t token) const
{
    std::lock_guard<std::mutex> guard(lock_);
    const auto found = transmissions_.find(token);
    return found == transmissions_.end() ? std::nullopt : found->second.transportSerial;
}

std::optional<ResourceState> ResourceRadioRuntime::ReadResourceState(const ResourceKey &key) const
{
    std::lock_guard<std::mutex> guard(lock_);
    return barrier_.ReadState(key);
}
} // namespace OHOS::Telephony::MultiSim
