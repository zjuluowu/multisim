/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/runtime/msisdn_runtime.h"
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
MsisdnRuntime::MsisdnRuntime(CardObservationRuntime &card, ResourceBarrier &resources, MsisdnPlatformPort &platform)
    : card_(card), resources_(resources), platform_(platform) {}

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
    const auto reject = [&] {
        sink_->AcceptMsisdnResult({request.step, request.session, EffectKnowledge::NotSent, false});
    };
    std::optional<ResourceKey> key;
    try {
        key = platform_.ResourceFor(request.session.application.slot);
    } catch (const std::exception &) {
        reject();
        return;
    }
    if (!key || !card_.StillCurrent(request.session)) { reject(); return; }
    const auto lease = resources_.Acquire({*key}, request.step);
    if (!lease) { reject(); return; }
    const MsisdnTransportTicket ticket {request.step, request.session, lease->fence};
    try {
        std::lock_guard<std::mutex> guard(mutex_);
        pending_.emplace(request.step.request, Pending {ticket, *lease, {request.session, request.number, request.alphaTag}, true, false, false, false,
            EffectKnowledge::Unknown, {}});
    } catch (const std::exception &) {
        resources_.CancelConfirmedNotSent(*lease);
        reject();
        return;
    }
    bool accepted = true;
    try {
        accepted = platform_.SubmitNumberWrite(ticket, request.alphaTag, request.number);
    } catch (const std::exception &) {
        // A throwing transport may have handed off. Preserve the job and require original evidence.
    }
    bool drain = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto &pending = pending_.at(request.step.request);
        accepted = accepted || !pending.early.empty();
        if (accepted) {
            resources_.MarkSent(*lease);
            pending.submitting = false;
            pending.delivering = drain = !pending.early.empty();
        } else {
            resources_.CancelConfirmedNotSent(*lease);
            pending_.erase(request.step.request);
        }
    }
    if (!accepted) { reject(); }
    if (drain) { DrainEvidence(request.step.request); }
}

bool MsisdnRuntime::AcceptEvidence(const MsisdnTransportEvidence &evidence)
{
    const auto request = evidence.ticket.step.request;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto found = pending_.find(request);
        if (found == pending_.end() || !(found->second.ticket == evidence.ticket) ||
            evidence.effect == EffectKnowledge::NotSent) {
            return false;
        }
        auto &pending = found->second;
        if (pending.delivered && (!evidence.commandEnded || !evidence.resourceReusable ||
            !evidence.currentStateVerified || evidence.effect == EffectKnowledge::Unknown ||
            (pending.effect != EffectKnowledge::Unknown && pending.effect != evidence.effect))) {
            return false;
        }
        pending.early.push_back(evidence);
        if (pending.submitting || pending.delivering) { return true; }
        pending.delivering = true;
    }
    DrainEvidence(request);
    return true;
}

void MsisdnRuntime::DrainEvidence(RequestId request)
{
    for (;;) {
        std::optional<MsisdnTransportEvidence> next;
        std::optional<CardNumberView> publish;
        bool ready = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            const auto found = pending_.find(request);
            if (found == pending_.end()) { return; }
            auto &pending = found->second;
            if (pending.early.empty()) {
                pending.delivering = false;
                return;
            }
            next = std::move(pending.early.front());
            pending.early.erase(pending.early.begin());
            if (pending.effect != EffectKnowledge::Unknown && pending.effect != next->effect) { continue; }
            ready = next->effect != EffectKnowledge::Unknown && next->commandEnded && next->resourceReusable &&
                (!pending.quarantined || next->currentStateVerified);
            if (ready && next->effect == EffectKnowledge::ConfirmedApplied) { publish = pending.written; }
        }
        // Keep the physical lease through acceptance into the original records cache.
        if (publish && card_.StillCurrent(publish->session)) {
            try {
                platform_.AcceptWrittenNumber(*publish);
            } catch (const std::exception &) {
                // A projection failure does not reverse the known card effect.
            }
        }
        std::optional<MsisdnWriteResult> result;
        bool first;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            const auto found = pending_.find(request);
            auto &pending = found->second;
            const auto &evidence = *next;
            const auto &ticket = evidence.ticket;
            const bool settled = ready && (pending.quarantined ? resources_.AcceptRecoveryEvidence(pending.lease,
                {ticket.step, evidence.effect, evidence.commandEnded, evidence.currentStateVerified,
                    evidence.resourceReusable}) :
                resources_.Complete(pending.lease,
                    {ticket.step, evidence.effect, evidence.commandEnded, evidence.resourceReusable}));
            first = !pending.delivered;
            if (!first && !settled) { continue; }
            result = MsisdnWriteResult {ticket.step, ticket.session,
                settled ? evidence.effect : EffectKnowledge::Unknown,
                settled && first && evidence.legacyWaitSucceeded && evidence.effect == EffectKnowledge::ConfirmedApplied};
            if (settled) {
                pending_.erase(found);
            } else {
                pending.delivered = true;
                pending.effect = evidence.effect;
                pending.quarantined = resources_.Quarantine(pending.lease, evidence.effect);
            }
        }
        try {
            if (first) { sink_->AcceptMsisdnResult(*result); }
            else { sink_->AcceptLateEvidence(*result); }
        } catch (const std::exception &) {
            // An observer exiting abnormally cannot prevent recovery of the physical lease.
        }
    }
}

bool MsisdnRuntime::HasPending() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return !pending_.empty();
}
} // namespace OHOS::Telephony::MultiSim
