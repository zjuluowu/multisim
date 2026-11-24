/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_CARD_OBSERVATION_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_CARD_OBSERVATION_RUNTIME_H

#include <mutex>
#include "account/discovery/card_observation_port.h"
#include "card/state/session_registry.h"
#include "infrastructure/assembly/slot_catalog.h"

namespace OHOS::Telephony::MultiSim {
// Verified input comes from the card source. R translates its application generation to an instance session.
class CardObservationRuntime final {
public:
    CardObservationRuntime(const SlotCatalog &catalog, SessionRegistry &sessions,
        CardObservationSink &sink, RuntimeEpoch epoch);
    std::optional<SessionRef> ObserveVerifiedCard(const CardApplicationTarget &application, uint64_t generation,
        const std::string &iccid);
    bool HasVerifiedCard(SlotId slot);
    bool InvalidateVerifiedCard(const SessionRef &session);
private:
    const SlotCatalog &catalog_;
    SessionRegistry &sessions_;
    CardObservationSink &sink_;
    const RuntimeEpoch epoch_;
    std::mutex mutex_;
    uint64_t sequence_ {1}; // Startup seed uses sequence 1.
};
} // namespace OHOS::Telephony::MultiSim
#endif
