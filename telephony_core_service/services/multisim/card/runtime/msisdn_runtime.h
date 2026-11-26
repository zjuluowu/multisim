/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_MSISDN_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_MSISDN_RUNTIME_H

#include "account/number/show_number_runtime_port.h"
#include "card/runtime/card_command_transport.h"

namespace OHOS::Telephony::MultiSim {
using MsisdnTransportTicket = CardCommandTicket;
using MsisdnTransportEvidence = CardCommandEvidence;

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
    MsisdnRuntime(CardObservationRuntime &card, CardCommandTransport &transport, MsisdnPlatformPort &platform);
    void BindNumberSink(MsisdnResultSink &sink) { sink_ = &sink; }
    std::optional<CardNumberView> ReadMsisdnCache(const SessionRef &session) const override;
    void WriteMsisdn(const MsisdnWriteRequest &request) override;
private:
    CardObservationRuntime &card_;
    CardCommandTransport &transport_;
    MsisdnPlatformPort &platform_;
    MsisdnResultSink *sink_ {nullptr};
};
} // namespace OHOS::Telephony::MultiSim
#endif
