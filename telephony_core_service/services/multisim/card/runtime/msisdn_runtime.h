/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_MSISDN_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_MSISDN_RUNTIME_H

#include "account/number/show_number_runtime_port.h"
#include "card/runtime/card_observation_runtime.h"
#include "device/resource/resource_barrier.h"

namespace OHOS::Telephony::MultiSim {
struct MsisdnTransportTicket {
    StepRef step;
    SessionRef session;
    uint64_t fence;
    bool operator==(const MsisdnTransportTicket &other) const
    { return step == other.step && session == other.session && fence == other.fence; }
};
struct MsisdnTransportEvidence {
    MsisdnTransportTicket ticket;
    EffectKnowledge effect;
    bool commandEnded;
    bool resourceReusable;
    bool legacyWaitSucceeded;
    bool currentStateVerified {false};
};
class MsisdnPlatformPort {
public:
    virtual ~MsisdnPlatformPort() = default;
    // Physical domain and card protocol are supplied by the installed records/transport bridge.
    virtual std::optional<ResourceKey> ResourceFor(SlotId slot) const = 0;
    virtual std::optional<CardNumberView> ReadCachedNumber(const SessionRef &session) const = 0;
    // The bridge hands off only to this original session and returns false only if it proved no send.
    // It returns evidence through the module ingress, including the original ticket on timeout.
    // Publish into the one records cache for the original session. The bridge must reject
    // an invalidated session at its cache commit point.
    virtual void AcceptWrittenNumber(const CardNumberView &view) = 0;
    virtual bool SubmitNumberWrite(const MsisdnTransportTicket &, const std::u16string &alphaTag,
        const std::u16string &number) = 0;
};
// The number slice of CardRuntime. Uses H's session and physical-resource authorities.
class MsisdnRuntime final : public ShowNumberRuntimePort {
public:
    MsisdnRuntime(CardObservationRuntime &card, ResourceBarrier &resources, MsisdnPlatformPort &platform);
    void BindNumberSink(MsisdnResultSink &sink) { sink_ = &sink; }
    std::optional<CardNumberView> ReadMsisdnCache(const SessionRef &session) const override;
    void WriteMsisdn(const MsisdnWriteRequest &request) override;
    bool AcceptEvidence(const MsisdnTransportEvidence &evidence);
    bool HasPending() const;
private:
    struct Pending {
        MsisdnTransportTicket ticket;
        ResourceLease lease;
        CardNumberView written;
        bool submitting {true};
        bool delivering {false};
        bool delivered {false};
        bool quarantined {false};
        EffectKnowledge effect {EffectKnowledge::Unknown};
        std::vector<MsisdnTransportEvidence> early;
    };
    void DrainEvidence(RequestId request);
    CardObservationRuntime &card_;
    ResourceBarrier &resources_;
    MsisdnPlatformPort &platform_;
    MsisdnResultSink *sink_ {nullptr};
    mutable std::mutex mutex_;
    std::map<RequestId, Pending> pending_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
