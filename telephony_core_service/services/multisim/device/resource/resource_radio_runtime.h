/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#ifndef OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H

#include <map>
#include <optional>
#include <set>
#include <vector>
#include "device/resource/resource_barrier.h"
#include "role_assignment/primary/radio_execution_port.h"

namespace OHOS::Telephony::MultiSim {
// R-owned adapter. A decides business; this class only acquires the complete
// physical-resource set, associates one transport token per command, and accepts
// device end/reuse evidence before forwarding a business result.
class ResourceRadioRuntime final : public RadioExecutionPort {
public:
    ResourceRadioRuntime(RuntimeEpoch epoch, RadioPlatformPort &platform,
        std::vector<ResourceKey> primaryResources);
    void BindResultSink(RadioResultSink &sink) { sink_ = &sink; }
    bool Initialize(const ResourceKey &key, const InitialRecoveryEvidence &evidence);
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override;
    void SwitchPrimaryRadio(const RadioSwitchRequest &request) override;
    void EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain) override;
    bool AcceptDeviceResult(const RadioDeviceResult &deviceResult);
    bool Recover(const StepRef &original, const RecoveryEvidence &evidence);
    bool StopAccepting();
    bool OwnerCallbacksDrained() const { return groups_.empty() && transmissions_.empty(); }
    bool DeviceResourcesDrained() const;
    std::optional<ResourceState> ReadResourceState(const ResourceKey &key) const;
    size_t SubmittedCommands() const { return submittedCommands_; }
    size_t RejectedStaleResults() const { return rejectedStaleResults_; }
    bool WasLastSubmissionAccepted(const RadioSwitchRequest &request) const override;
private:
    struct Group {
        ResourceLease lease;
        RadioSwitchRequest first;
        uint32_t expected;
        std::vector<RadioSwitchResult> accepted;
        std::vector<RadioSwitchResult> recovered;
        std::set<int32_t> submittedMembers;
        std::set<int32_t> acceptedMembers;
        std::set<uint64_t> submittedTransmissions;
        std::set<uint64_t> recoveredTransmissions;
        bool quarantined {false};
        bool sent {false};
    };
    struct Transmission {
        uint64_t fence;
        RadioSwitchRequest request;
    };
    Group *FindGroup(const RadioSwitchRequest &request);
    static bool SameGroup(const RadioSwitchRequest &left, const RadioSwitchRequest &right);
    static bool SameBinding(const RadioSwitchRequest &request, const RadioSwitchResult &result);
    static bool SameCommand(const RadioSwitchRequest &request, const RadioSwitchResult &result);
    static bool SameRequest(const RadioSwitchRequest &left, const RadioSwitchRequest &right);
    void Quarantine(Group &group, EffectKnowledge effect);
    void ForwardCompleted(Group &group);

    RadioPlatformPort &platform_;
    std::vector<ResourceKey> resources_;
    ResourceBarrier barrier_;
    RadioResultSink *sink_ {nullptr};
    bool stopping_ {false};
    uint64_t nextTransportToken_ {1};
    size_t submittedCommands_ {0};
    size_t rejectedStaleResults_ {0};
    std::map<uint64_t, Group> groups_;
    std::map<uint64_t, Transmission> transmissions_;
    std::optional<RadioSwitchRequest> lastSubmission_;
    bool lastSubmissionAccepted_ {false};
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H
