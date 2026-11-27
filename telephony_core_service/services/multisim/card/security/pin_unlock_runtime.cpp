/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/security/pin_unlock_runtime.h"

#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
PinUnlockRuntime::PinUnlockRuntime(CardObservationRuntime &card, CardCommandTransport &transport,
    PinIoPort &io, PinCompletionPort &completion)
    : card_(card), transport_(transport), io_(io), completion_(completion) {}

void PinUnlockRuntime::Unlock(SlotId slot, const std::u16string &pin, CompletionId callback)
{
    std::optional<PinIoRoute> route;
    try {
        route = io_.ResolvePinRoute(slot);
    } catch (const std::exception &) {
        // No verified application or physical route was obtained.
    }
    if (!route || !(route->application.slot == slot)) {
        completion_.CompletePin(callback, {PinReplyStatus::Unavailable, 0});
        return;
    }
    const auto session = card_.ReadCurrentApplication(route->application);
    const auto step = session ? transport_.BeginLocalRequest(*session, {CallbackCompletionRoute {callback}, false}) :
        std::nullopt;
    if (!step) {
        completion_.CompletePin(callback, {PinReplyStatus::Unavailable, 0});
        return;
    }
    transport_.Submit({*step, *session, std::move(route->resources), EffectRequirement::VerifyWriteEffect,
        {}, CardCommandKind::UnlockPin}, [&](const CardCommandTicket &ticket) {
            return io_.SubmitUnlockPin(ticket, pin);
        }, {{}, [this, step = *step, session = *session, callback](const CardCommandEvidence &reply,
            EffectKnowledge effect, bool first, bool ended) {
            transport_.CloseLocalRequest(step, effect, ended);
            if (!first) { return; }
            PinReply result {PinReplyStatus::Failed, 0};
            const auto *device = std::get_if<PinReply>(&reply.response);
            if (device && device->status == PinReplyStatus::TimedOut) {
                result.status = PinReplyStatus::TimedOut;
            } else if (ended && device) {
                if (device->status == PinReplyStatus::Unlocked && effect == EffectKnowledge::ConfirmedApplied) {
                    if (const auto changed = card_.AcceptPinUnlock(session)) {
                        result = *device;
                        if (*changed) { completion_.PublishPinStateChange(session); }
                    }
                } else if (device->status == PinReplyStatus::Incorrect &&
                    effect == EffectKnowledge::ConfirmedNotApplied && card_.StillCurrent(session)) {
                    result = *device;
                }
            }
            completion_.CompletePin(callback, result);
        }});
}
} // namespace OHOS::Telephony::MultiSim
