/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_PIN_UNLOCK_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_PIN_UNLOCK_RUNTIME_H

#include "card/runtime/card_command_transport.h"

namespace OHOS::Telephony::MultiSim {
struct PinIoRoute {
    CardApplicationTarget application;
    std::vector<ResourceKey> resources;
};

class PinIoPort {
public:
    virtual ~PinIoPort() = default;
    virtual std::vector<ResourceKey> InitialResources() const = 0;
    virtual std::optional<PinIoRoute> ResolvePinRoute(SlotId slot) = 0;
    // Reply evidence (including wait expiration) is delivered to the original ticket.
    // False proves the command did not enter transport.
    virtual bool SubmitUnlockPin(const CardCommandTicket &ticket, const std::u16string &pin) = 0;
};

class PinUnlockRuntime final {
public:
    PinUnlockRuntime(CardObservationRuntime &card, CardCommandTransport &transport,
        PinIoPort &io, PinCompletionPort &completion);
    void Unlock(SlotId slot, const std::u16string &pin, CompletionId callback);
private:
    CardObservationRuntime &card_;
    CardCommandTransport &transport_;
    PinIoPort &io_;
    PinCompletionPort &completion_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
