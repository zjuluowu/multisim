/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "device/resource/resource_radio_runtime.h"

#include <algorithm>
#include <stdexcept>
#include <limits>

namespace OHOS::Telephony::MultiSim {
ResourceRadioRuntime::ResourceRadioRuntime(ResourceBarrier &barrier, RadioPlatformPort &platform,
    std::vector<ResourceKey> primaryResources, Mode mode)
    : platform_(platform), resources_(std::move(primaryResources)), barrier_(barrier), mode_(mode)
{}

bool ResourceRadioRuntime::Initialize(const ResourceKey &key, const InitialRecoveryEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(lock_);
    return barrier_.AcceptInitialRecoveryEvidence(key, evidence);
}

void ResourceRadioRuntime::OwnerDeliveryFrame::Begin()
{
    ++owner_.ownerDeliveryFrames_;
    active_ = true;
}

ResourceRadioRuntime::OwnerDeliveryFrame::~OwnerDeliveryFrame()
{
    if (active_) {
        std::lock_guard<std::mutex> guard(owner_.lock_);
        --owner_.ownerDeliveryFrames_;
    }
}

bool ResourceRadioRuntime::OwnerCallbacksDrained() const
{
    std::unique_lock<std::mutex> guard(lock_);
    ownerDeliveryReceipts_.erase(std::remove_if(ownerDeliveryReceipts_.begin(), ownerDeliveryReceipts_.end(),
        [](const auto &receipt) { return receipt->delivered.load(std::memory_order_acquire); }),
        ownerDeliveryReceipts_.end());
    if (ownerDeliveryFrames_ || !pendingNormalDeliveries_.empty() || !pendingLateDeliveries_.empty() ||
        !ownerDeliveryReceipts_.empty()) {
        return false;
    }
    const auto dispatch = dispatch_.lock();
    // ObserveOnly cannot produce a new Radio delivery after this check.
    if (mode_ == Mode::ObserveOnly) { guard.unlock(); }
    return dispatch && dispatch->RadioOwnerCallbacksDrained();
}

PrimaryRadioObservation ResourceRadioRuntime::ReadPrimaryRadioObservation(SlotId slot) const
{
    auto observed = platform_.ReadPrimaryRadioObservation(slot);
    if (!observed.routing) { return observed; }
    std::map<uint32_t, RadioProtocolMember> byNativeMember;
    std::set<SlotId> routes;
    std::vector<RadioProtocolMember> unique;
    for (const auto &member : observed.routing->members) {
        const auto [existing, inserted] = byNativeMember.emplace(member.nativeMember, member);
        if (member.routeRevision == 0 || member.route.legacySlotId < 0 ||
            (inserted && !routes.insert(member.route).second) || !(existing->second == member)) {
            observed.routing.reset();
            return observed;
        }
        if (inserted) { unique.push_back(member); }
    }
    if (!byNativeMember.count(observed.routing->targetMember)) {
        observed.routing.reset();
    } else {
        observed.routing->members = std::move(unique);
    }
    return observed;
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

ResourceRadioRuntime::Group *ResourceRadioRuntime::FindGroup(const RadioSwitchRequest &request)
{
    for (auto &[fence, group] : groups_) {
        if (SameGroup(group.first, request)) { return &group; }
        (void)fence;
    }
    return nullptr;
}

RadioSubmitDisposition ResourceRadioRuntime::SwitchPrimaryRadio(const RadioSwitchRequest &request,
    const RadioSendQualification &qualify)
{
    uint64_t fence = 0;
    uint64_t transport = 0;
    {
        std::lock_guard<std::mutex> guard(lock_);
        Group *group = FindGroup(request);
        if (mode_ != Mode::Execute || stopping_ || !qualify || sink_.expired() || dispatch_.expired() ||
            nextTransportToken_ == std::numeric_limits<uint64_t>::max() ||
            (request.member && !(request.member->route == request.target))) {
            return group ? RadioSubmitDisposition::HandoffUnknown : RadioSubmitDisposition::ConfirmedNotSent;
        }
        if (!group) {
            const auto lease = barrier_.Acquire(resources_, request.step);
            if (!lease) { return RadioSubmitDisposition::ConfirmedNotSent; }
            auto inserted = groups_.emplace(lease->fence,
                Group {*lease, request, request.memberCount == 0 ? 1u : request.memberCount,
                    {}, {}, {}, {}, {}, {}, false, false});
            group = &inserted.first->second;
        }
        if (group->endRequested || group->quarantined || group->admittedMembers.size() >= group->expected ||
            group->admittedMembers.find(request.target.legacySlotId) != group->admittedMembers.end()) {
            return RadioSubmitDisposition::HandoffUnknown;
        }
        fence = group->lease.fence;
        transport = nextTransportToken_++;
        transmissions_[transport] = {fence, request, std::nullopt};
        submittingTransmissions_.insert(transport);
        group->admittedMembers.insert(request.target.legacySlotId);
        ++group->submitting;
    }

    // A throwing platform call does not prove the original handoff was never entered.
    RadioSubmitResult handoff {RadioSubmitDisposition::HandoffUnknown, std::nullopt};
    try {
        handoff = platform_.SubmitPrimaryRadio(request, transport, [&] {
            return qualify([&](const RadioSendPermit &stillAccepting) {
                std::lock_guard<std::mutex> guard(lock_);
                const auto original = transmissions_.find(transport);
                const auto group = groups_.find(fence);
                if (stopping_ || original == transmissions_.end() || group == groups_.end() ||
                    !submittingTransmissions_.count(transport) || original->second.sendPermitted ||
                    group->second.endRequested || group->second.quarantined || group->second.hasNotSentMember ||
                    !stillAccepting()) {
                    return false;
                }
                original->second.sendPermitted = true;
                return true;
            });
        });
    } catch (const std::exception &) {
    }

    bool accepted = false;
    std::vector<RadioDeviceResult> pending;
    {
        std::lock_guard<std::mutex> guard(lock_);
        submittingTransmissions_.erase(transport);
        const auto found = groups_.find(fence);
        if (found == groups_.end()) {
            transmissions_.erase(transport);
            return RadioSubmitDisposition::HandoffUnknown;
        }
        Group &group = found->second;
        --group.submitting;
        const auto buffered = pendingSubmissionResults_.find(transport);
        const bool callbackContradiction = buffered != pendingSubmissionResults_.end();
        if (handoff.disposition == RadioSubmitDisposition::ConfirmedNotSent && !callbackContradiction) {
            transmissions_.erase(transport);
            group.hasNotSentMember = true;
            if (group.sent) {
                Quarantine(group, EffectKnowledge::Unknown);
            } else if (group.submitting == 0 && barrier_.CancelConfirmedNotSent(group.lease)) {
                groups_.erase(found);
                return RadioSubmitDisposition::ConfirmedNotSent;
            }
        } else {
            if (!group.sent) {
                if (!barrier_.MarkSent(group.lease)) { Quarantine(group, EffectKnowledge::Unknown); }
                group.sent = true;
            }
            ++submittedCommands_;
            auto &transmission = transmissions_.at(transport);
            transmission.transportSerial = handoff.transportSerial;
            group.submittedTransmissions.insert(transport);
            accepted = handoff.disposition == RadioSubmitDisposition::HandoffAccepted &&
                transmission.sendPermitted && handoff.transportSerial.has_value();
            if (group.endRequested || group.hasNotSentMember || !accepted) {
                Quarantine(group, EffectKnowledge::Unknown);
            }
            if (callbackContradiction &&
                (buffered->second.conflict || handoff.disposition == RadioSubmitDisposition::ConfirmedNotSent)) {
                Quarantine(group, EffectKnowledge::Unknown);
                pendingSubmissionResults_.erase(buffered);
            }
        }
        if (group.submitting == 0) {
            for (const auto token : group.submittedTransmissions) {
                const auto evidence = pendingSubmissionResults_.find(token);
                if (evidence == pendingSubmissionResults_.end()) { continue; }
                if (evidence->second.conflict) { Quarantine(group, EffectKnowledge::Unknown); }
                else { pending.push_back(evidence->second.first); }
                pendingSubmissionResults_.erase(evidence);
            }
        }
    }
    for (const auto &evidence : pending) { (void)AcceptDeviceResult(evidence); }
    return accepted ? RadioSubmitDisposition::HandoffAccepted : RadioSubmitDisposition::HandoffUnknown;
}

void ResourceRadioRuntime::EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain)
{
    if (mode_ == Mode::ObserveOnly) { return; }
    {
        std::lock_guard<std::mutex> guard(lock_);
        for (auto &[fence, group] : groups_) {
            (void)fence;
            if (!(group.lease.holder == owner)) { continue; }
            // Close further admission immediately. While the original Submit is
            // still being classified, a lease with no issued member must remain
            // cancellable if all of its handoffs prove ConfirmedNotSent.
            group.endRequested = true;
            if (group.sent && !group.quarantined) { Quarantine(group, EffectKnowledge::Unknown); }
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
    OwnerDeliveryFrame delivery(*this);
    std::shared_ptr<RadioResultSink> sink;
    std::shared_ptr<RadioOwnerDispatchPort> dispatch;
    {
        std::lock_guard<std::mutex> guard(lock_);
        sink = sink_.lock();
        dispatch = dispatch_.lock();
        const auto original = transmissions_.find(deviceResult.transportToken);
        const auto originalGroup = original == transmissions_.end() ? groups_.end() : groups_.find(original->second.fence);
        if (submittingTransmissions_.count(deviceResult.transportToken) ||
            (originalGroup != groups_.end() && originalGroup->second.quarantined && originalGroup->second.submitting)) {
            auto found = pendingSubmissionResults_.find(deviceResult.transportToken);
            if (found == pendingSubmissionResults_.end()) {
                pendingSubmissionResults_[deviceResult.transportToken] = {deviceResult, false};
                return true;
            }
            const auto &old = found->second.first;
            const bool bindingEqual = (!old.result.expectedBinding && !deviceResult.result.expectedBinding) ||
                (old.result.expectedBinding && deviceResult.result.expectedBinding &&
                 old.result.expectedBinding->subscription == deviceResult.result.expectedBinding->subscription &&
                 old.result.expectedBinding->slot == deviceResult.result.expectedBinding->slot &&
                 old.result.expectedBinding->bindingRevision == deviceResult.result.expectedBinding->bindingRevision);
            const bool identical = old.transportToken == deviceResult.transportToken &&
                old.transportSerial == deviceResult.transportSerial && old.result.step == deviceResult.result.step &&
                old.result.memberSlot == deviceResult.result.memberSlot && bindingEqual &&
                old.result.protocolSession == deviceResult.result.protocolSession &&
                old.result.phase == deviceResult.result.phase && old.result.notify == deviceResult.result.notify &&
                old.result.effect == deviceResult.result.effect && old.result.legacyResult == deviceResult.result.legacyResult &&
                old.commandEnded == deviceResult.commandEnded && old.resourceReusable == deviceResult.resourceReusable &&
                old.currentStateVerified == deviceResult.currentStateVerified;
            if (!identical) { found->second.conflict = true; }
            return true;
        }
        const auto transmission = transmissions_.find(deviceResult.transportToken);
        if (transmission == transmissions_.end()) { ++rejectedStaleResults_; return false; }
        const RadioSwitchRequest request = transmission->second.request;
        if (!deviceResult.result.notify && transmission->second.transportSerial &&
            (!deviceResult.transportSerial ||
             *deviceResult.transportSerial != *transmission->second.transportSerial)) {
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
            if (group.submitting != 0) { return false; }
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
        if (!normal.empty() || !late.empty()) { delivery.Begin(); }
    }
    if (!sink) {
        std::lock_guard<std::mutex> guard(lock_);
        pendingNormalDeliveries_.insert(pendingNormalDeliveries_.end(), normal.begin(), normal.end());
        pendingLateDeliveries_.insert(pendingLateDeliveries_.end(), late.begin(), late.end());
        return accepted;
    }
    if (!dispatch) {
        std::lock_guard<std::mutex> guard(lock_);
        pendingNormalDeliveries_.insert(pendingNormalDeliveries_.end(), normal.begin(), normal.end());
        pendingLateDeliveries_.insert(pendingLateDeliveries_.end(), late.begin(), late.end());
        return false;
    }
    for (size_t i = 0; i < normal.size(); ++i) {
        auto receipt = std::make_shared<OwnerDeliveryReceipt>();
        receipt->result = normal[i];
        {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.push_back(receipt);
        }
        if (!dispatch->PostRadioOwnerTask([sink, result = normal[i], receipt] {
            sink->AcceptRadioStep(result);
            receipt->delivered.store(true, std::memory_order_release);
        })) {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.erase(std::remove(ownerDeliveryReceipts_.begin(),
                ownerDeliveryReceipts_.end(), receipt), ownerDeliveryReceipts_.end());
            pendingNormalDeliveries_.insert(pendingNormalDeliveries_.end(), normal.begin() + i, normal.end());
            pendingLateDeliveries_.insert(pendingLateDeliveries_.end(), late.begin(), late.end());
            return false;
        }
    }
    for (size_t i = 0; i < late.size(); ++i) {
        auto receipt = std::make_shared<OwnerDeliveryReceipt>();
        receipt->result = late[i];
        receipt->late = true;
        {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.push_back(receipt);
        }
        if (!dispatch->PostRadioOwnerTask([sink, result = late[i], receipt] {
            sink->AcceptLateRadioEvidence(result);
            receipt->delivered.store(true, std::memory_order_release);
        })) {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.erase(std::remove(ownerDeliveryReceipts_.begin(),
                ownerDeliveryReceipts_.end(), receipt), ownerDeliveryReceipts_.end());
            pendingLateDeliveries_.insert(pendingLateDeliveries_.end(), late.begin() + i, late.end());
            return false;
        }
    }
    return accepted;
}

bool ResourceRadioRuntime::FlushOwnerDeliveries()
{
    OwnerDeliveryFrame delivery(*this);
    std::shared_ptr<RadioResultSink> sink;
    std::shared_ptr<RadioOwnerDispatchPort> dispatch;
    std::vector<RadioSwitchResult> normal;
    std::vector<RadioSwitchResult> late;
    {
        std::lock_guard<std::mutex> guard(lock_);
        sink = sink_.lock();
        dispatch = dispatch_.lock();
        if (!sink || !dispatch || ownerDeliveryFrames_) { return false; }
        if (!dispatch->RadioOwnerCallbacksDrained()) { return false; }
        for (const auto &receipt : ownerDeliveryReceipts_) {
            if (receipt->delivered.load(std::memory_order_acquire)) { continue; }
            if (receipt->late) { late.push_back(receipt->result); }
            else { normal.push_back(receipt->result); }
        }
        ownerDeliveryReceipts_.clear();
        normal.insert(normal.end(), pendingNormalDeliveries_.begin(), pendingNormalDeliveries_.end());
        late.insert(late.end(), pendingLateDeliveries_.begin(), pendingLateDeliveries_.end());
        if (!normal.empty() || !late.empty()) { delivery.Begin(); }
        pendingNormalDeliveries_.clear();
        pendingLateDeliveries_.clear();
    }
    for (size_t i = 0; i < normal.size(); ++i) {
        auto receipt = std::make_shared<OwnerDeliveryReceipt>();
        receipt->result = normal[i];
        {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.push_back(receipt);
        }
        if (!dispatch->PostRadioOwnerTask([sink, result = normal[i], receipt] {
            sink->AcceptRadioStep(result);
            receipt->delivered.store(true, std::memory_order_release);
        })) {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.erase(std::remove(ownerDeliveryReceipts_.begin(),
                ownerDeliveryReceipts_.end(), receipt), ownerDeliveryReceipts_.end());
            pendingNormalDeliveries_.insert(pendingNormalDeliveries_.begin(), normal.begin() + i, normal.end());
            pendingLateDeliveries_.insert(pendingLateDeliveries_.begin(), late.begin(), late.end());
            return false;
        }
    }
    for (size_t i = 0; i < late.size(); ++i) {
        auto receipt = std::make_shared<OwnerDeliveryReceipt>();
        receipt->result = late[i];
        receipt->late = true;
        {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.push_back(receipt);
        }
        if (!dispatch->PostRadioOwnerTask([sink, result = late[i], receipt] {
            sink->AcceptLateRadioEvidence(result);
            receipt->delivered.store(true, std::memory_order_release);
        })) {
            std::lock_guard<std::mutex> guard(lock_);
            ownerDeliveryReceipts_.erase(std::remove(ownerDeliveryReceipts_.begin(),
                ownerDeliveryReceipts_.end(), receipt), ownerDeliveryReceipts_.end());
            pendingLateDeliveries_.insert(pendingLateDeliveries_.begin(), late.begin() + i, late.end());
            return false;
        }
    }
    return true;
}

bool ResourceRadioRuntime::Recover(const StepRef &original, const RecoveryEvidence &evidence)
{
    std::lock_guard<std::mutex> guard(lock_);
    if (!(original == evidence.original)) { return false; }
    for (auto found = groups_.begin(); found != groups_.end();) {
        if (found->second.lease.holder == original && found->second.submitting == 0 &&
            found->second.submittedTransmissions.empty() &&
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
    std::unique_lock<std::mutex> guard(lock_);
    stopping_ = true;
    for (auto &[fence, group] : groups_) {
        (void)fence;
        if (group.admittedMembers.size() < group.expected) {
            // No remaining member can cross the gate. Already admitted transmissions
            // keep their original responsibility, including calls still in Submit.
            group.hasNotSentMember = true;
            if (group.sent) { Quarantine(group, EffectKnowledge::Unknown); }
        }
    }
    ownerDeliveryReceipts_.erase(std::remove_if(ownerDeliveryReceipts_.begin(), ownerDeliveryReceipts_.end(),
        [](const auto &receipt) { return receipt->delivered.load(std::memory_order_acquire); }),
        ownerDeliveryReceipts_.end());
    if (!groups_.empty() || !transmissions_.empty() || !submittingTransmissions_.empty() ||
        !pendingSubmissionResults_.empty() || !pendingNormalDeliveries_.empty() ||
        !pendingLateDeliveries_.empty() || !ownerDeliveryReceipts_.empty() || ownerDeliveryFrames_) { return false; }
    for (const auto &resource : resources_) {
        const auto state = barrier_.ReadState(resource);
        if (!state || *state != ResourceState::Available) { return false; }
    }
    const auto dispatch = dispatch_.lock();
    if (mode_ == Mode::ObserveOnly) { guard.unlock(); }
    return dispatch && dispatch->RadioOwnerCallbacksDrained();
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

size_t ResourceRadioRuntime::SubmittedCommands() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return submittedCommands_;
}

size_t ResourceRadioRuntime::RejectedStaleResults() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return rejectedStaleResults_;
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
