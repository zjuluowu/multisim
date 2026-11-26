/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/runtime/msisdn_runtime.h"
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
MsisdnRuntime::MsisdnRuntime(CardObservationRuntime &card, CardCommandTransport &transport,
    MsisdnPlatformPort &platform) : card_(card), transport_(transport), platform_(platform) {}

std::optional<CardNumberView> MsisdnRuntime::ReadMsisdnCache(const SessionRef &session) const
{
    if (!card_.StillCurrent(session)) { return std::nullopt; }
    try {
        const auto view = platform_.ReadCachedNumber(session);
        return view && view->session == session && card_.StillCurrent(session) ? view : std::nullopt;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void MsisdnRuntime::WriteMsisdn(const MsisdnWriteRequest &request)
{
    std::vector<ResourceKey> resources;
    try {
        if (const auto key = platform_.ResourceFor(request.session.application.slot)) {
            resources.push_back(*key);
        }
    } catch (const std::exception &) {
        // No verified physical resource is available for the original command.
    }
    transport_.Submit({request.step, request.session, std::move(resources), EffectRequirement::VerifyWriteEffect},
        [&](const CardCommandTicket &ticket) {
            return platform_.SubmitNumberWrite(ticket, request.alphaTag, request.number);
        }, {
            [this, written = CardNumberView {request.session, request.number, request.alphaTag}] {
                if (card_.StillCurrent(written.session)) { platform_.AcceptWrittenNumber(written); }
            },
            [this](const CardCommandEvidence &evidence, EffectKnowledge effect, bool first) {
                const auto &ticket = evidence.ticket;
                const MsisdnWriteResult result {ticket.step, ticket.session, effect,
                    first && evidence.legacyWaitSucceeded && effect == EffectKnowledge::ConfirmedApplied};
                if (first) { sink_->AcceptMsisdnResult(result); }
                else { sink_->AcceptLateEvidence(result); }
            }
        });
}
} // namespace OHOS::Telephony::MultiSim
