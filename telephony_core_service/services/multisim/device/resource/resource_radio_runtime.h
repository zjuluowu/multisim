/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#ifndef OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <tuple>
#include <variant>
#include <vector>
#include "device/resource/resource_barrier.h"
#include "role_assignment/primary/radio_execution_port.h"

namespace OHOS::Telephony::MultiSim {
// R-owned adapter. A decides business; this class only acquires the complete
// physical-resource set, associates one transport token per command, and accepts
// device end/reuse evidence before forwarding a business result.
class ResourceRadioRuntime final : public RadioExecutionPort {
public:
    enum class Mode { ObserveOnly, Execute };
    ResourceRadioRuntime(ResourceBarrier &barrier, RadioPlatformPort &platform,
        std::vector<ResourceKey> primaryResources, Mode mode);
    void BindResultSink(const std::shared_ptr<RadioResultSink> &sink) { sink_ = sink; }
    void BindOwnerDispatch(const std::shared_ptr<RadioOwnerDispatchPort> &dispatch) { dispatch_ = dispatch; }
    bool Initialize(const ResourceKey &key, const InitialRecoveryEvidence &evidence);
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override;
    RadioSubmitDisposition SwitchPrimaryRadio(const RadioSwitchRequest &request,
        const RadioSendQualification &qualify) override;
    void EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain) override;
    bool AcceptDeviceResult(const RadioDeviceResult &deviceResult);
    bool Recover(const StepRef &original, const RecoveryEvidence &evidence);
    bool RecoverC1Round(const RadioC1RoundRecovery &evidence);
    bool StopAccepting();
    bool OwnerCallbacksDrained() const;
    bool FlushOwnerDeliveries();
    bool DeviceResourcesDrained() const;
    std::optional<ResourceState> ReadResourceState(const ResourceKey &key) const;
    size_t SubmittedCommands() const;
    size_t RejectedStaleResults() const;
    std::optional<int32_t> ReadTransportSerial(uint64_t token) const;
private:
    // Holds the receipt responsibility across the R-to-Owner transfer, including
    // the interval before the dispatcher accepts the callback.
    class OwnerDeliveryFrame {
    public:
        explicit OwnerDeliveryFrame(ResourceRadioRuntime &owner) : owner_(owner) {}
        ~OwnerDeliveryFrame();
        void Begin(); // Called under R's lock, before the current receipt source is retired.
        OwnerDeliveryFrame(const OwnerDeliveryFrame &) = delete;
        OwnerDeliveryFrame &operator=(const OwnerDeliveryFrame &) = delete;
    private:
        ResourceRadioRuntime &owner_;
        bool active_ {false};
    };
    struct Group {
        ResourceLease lease;
        RadioSwitchRequest first;
        uint32_t expected;
        std::vector<RadioSwitchResult> accepted;
        std::vector<RadioSwitchResult> recovered;
        std::set<int32_t> admittedMembers;
        std::set<int32_t> acceptedMembers;
        std::set<uint64_t> submittedTransmissions;
        std::set<uint64_t> recoveredTransmissions;
        bool quarantined {false};
        bool sent {false};
        uint32_t submitting {0};
        bool hasNotSentMember {false};
        bool endRequested {false};
        std::vector<uint64_t> closedUpdateTransmissions {};
        bool awaitingC1Phase {false};
    };
    struct Transmission {
        uint64_t fence;
        RadioSwitchRequest request;
        std::optional<int32_t> transportSerial;
        bool sendPermitted {false};
        std::optional<int32_t> updateError {};
        bool updateGroupEnded {false};
    };
    struct BufferedSubmissionEvidence {
        RadioDeviceResult first;
        uint64_t order;
        bool conflict {false};
    };
    struct OwnerDelivery {
        std::variant<RadioSwitchResult, RadioC1RoundRecovery> evidence;
        bool late {false};
        void Deliver(RadioResultSink &sink) const;
    };
    struct OwnerDeliveryReceipt {
        explicit OwnerDeliveryReceipt(const OwnerDelivery &value) : delivery(value) {}
        OwnerDelivery delivery;
        std::atomic<bool> delivered {false};
    };
    Group *FindGroup(const RadioSwitchRequest &request);
    static bool AdvanceC1Phase(Group &group, const RadioSwitchRequest &request);
    static bool SameGroup(const RadioSwitchRequest &left, const RadioSwitchRequest &right);
    static bool SameBinding(const RadioSwitchRequest &request, const RadioSwitchResult &result);
    static bool SameCommand(const RadioSwitchRequest &request, const RadioSwitchResult &result);
    std::optional<int32_t> CloseUpdateAssociations(const StepRef &owner);
    bool AcceptDeviceResultLocked(const RadioDeviceResult &deviceResult, std::vector<RadioSwitchResult> &normal,
        std::vector<RadioSwitchResult> &late);
    bool DeliverDeviceResults(const std::vector<RadioSwitchResult> &normal, const std::vector<RadioSwitchResult> &late,
        const std::shared_ptr<RadioResultSink> &sink, const std::shared_ptr<RadioOwnerDispatchPort> &dispatch);
    bool DeliverOwnerResults(const std::vector<OwnerDelivery> &deliveries, const std::shared_ptr<RadioResultSink> &sink,
        const std::shared_ptr<RadioOwnerDispatchPort> &dispatch);
    bool LocalOwnerCallbacksDrained() const;
    void Quarantine(Group &group, EffectKnowledge effect);

    mutable std::mutex lock_;
    RadioPlatformPort &platform_;
    std::vector<ResourceKey> resources_;
    ResourceBarrier &barrier_;
    const Mode mode_;
    std::weak_ptr<RadioResultSink> sink_;
    std::weak_ptr<RadioOwnerDispatchPort> dispatch_;
    bool stopping_ {false};
    uint64_t nextTransportToken_ {1};
    uint64_t nextBufferedOrder_ {1};
    size_t submittedCommands_ {0};
    size_t rejectedStaleResults_ {0};
    std::map<uint64_t, Group> groups_;
    std::map<uint64_t, Transmission> transmissions_;
    std::set<uint64_t> submittingTransmissions_;
    using BufferedKey = std::tuple<uint64_t, bool, std::optional<int32_t>>;
    std::map<BufferedKey, BufferedSubmissionEvidence> pendingSubmissionResults_;
    std::vector<OwnerDelivery> pendingOwnerDeliveries_;
    mutable std::vector<std::shared_ptr<OwnerDeliveryReceipt>> ownerDeliveryReceipts_;
    size_t ownerDeliveryFrames_ {0};
    uint64_t ownerDeliveryRevision_ {0};
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_DEVICE_RESOURCE_RESOURCE_RADIO_RUNTIME_H
