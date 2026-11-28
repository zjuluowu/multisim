/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/runtime/card_command_transport.h"

#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
CardCommandTransport::CardCommandTransport(CardObservationRuntime &card, ResourceBarrier &resources,
    RuntimeEpoch epoch) : card_(card), resources_(resources), localRequests_(epoch, RequestOwner::LocalCard) {}

std::optional<StepRef> CardCommandTransport::BeginLocalRequest(const SessionRef &session,
    CompletionPlan completion)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!acceptingLocal_) { return std::nullopt; }
    const auto request = localRequests_.Begin(session.application, std::move(completion));
    if (!localRequests_.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt)) { return std::nullopt; }
    return StepRef {request, 1, 0};
}

std::optional<StepRef> CardCommandTransport::AdvanceLocalRequest(const StepRef &original)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!acceptingLocal_ || localRequests_.AcceptResult(original, EffectKnowledge::ConfirmedApplied) !=
        RuntimeOperationStore::ResultAdmission::Accepted) { return std::nullopt; }
    const auto next = original.step + 1;
    if (!localRequests_.AdvanceStep(original, next, original.attempt, std::nullopt)) { return std::nullopt; }
    return StepRef {original.request, next, original.attempt};
}

void CardCommandTransport::CloseLocalRequest(const StepRef &original, EffectKnowledge evidence, bool transportEnded)
{
    std::lock_guard<std::mutex> guard(mutex_);
    const auto operation = localRequests_.Read(original.request);
    if (!operation || !(operation->context.step == original)) { return; }
    if (operation->terminal == TerminalState::Open) {
        if (localRequests_.AcceptResult(original, evidence) != RuntimeOperationStore::ResultAdmission::Accepted) {
            return;
        }
        const auto terminal = transportEnded ?
            (evidence == EffectKnowledge::ConfirmedApplied ? TerminalState::Succeeded : TerminalState::Failed) :
            TerminalState::NeedsReconciliation;
        localRequests_.MarkTerminal(original.request, terminal);
    } else if (!transportEnded) {
        return;
    }
    localRequests_.RetireAfterEvidenceRetention(original.request, transportEnded);
}

void CardCommandTransport::SealLocalCommands()
{
    std::lock_guard<std::mutex> guard(mutex_);
    acceptingLocal_ = false;
}

void CardCommandTransport::Submit(const CardCommandExecution &execution, const Handoff &handoff,
    CardCommandHandlers handlers)
{
    const auto reject = [&](const CardCommandHandlers &callbacks) {
        callbacks.deliver({{execution.step, execution.session, 0, execution.kind}, EffectKnowledge::NotSent, false, false,
            false}, EffectKnowledge::NotSent, true, true);
    };
    const auto callbacks = std::make_shared<const CardCommandHandlers>(std::move(handlers));
    const auto lease = resources_.Acquire(execution.resources, execution.step, execution.effectRequirement);
    if (!lease) { reject(*callbacks); return; }
    const CardCommandTicket ticket {execution.step, execution.session, lease->fence, execution.kind};
    bool registered = false;
    bool duplicate = false;
    try {
        registered = card_.WithCurrentApplication(execution.session, [&] {
            if (execution.scopeCurrent && !execution.scopeCurrent()) { return false; }
            std::lock_guard<std::mutex> guard(mutex_);
            duplicate = pending_.count(execution.step.request) != 0;
            if (duplicate) { return false; }
            if (execution.step.request.owner == RequestOwner::LocalCard &&
                (!acceptingLocal_ || !localRequests_.RecordSent(execution.step, execution.session))) { return false; }
            const auto inserted = pending_.try_emplace(execution.step.request,
                Pending {ticket, *lease, callbacks, true, false, false, false, EffectKnowledge::Unknown, {}});
            return inserted.second;
        });
    } catch (const std::exception &) {
        // Permission was never granted.
    }
    if (!registered) {
        resources_.CancelConfirmedNotSent(*lease);
        if (!duplicate) { reject(*callbacks); }
        return;
    }

    bool accepted = true;
    try {
        accepted = handoff(ticket);
    } catch (const std::exception &) {
        // A throwing transport may have handed off. Require original evidence.
    }
    bool drain = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto &pending = pending_.at(execution.step.request);
        const bool contradicted = pending.contradictoryEvidence || (!accepted && !pending.evidence.empty());
        accepted = accepted || contradicted;
        if (accepted) {
            resources_.MarkSent(*lease);
            if (contradicted) {
                pending.quarantined = resources_.Quarantine(*lease, EffectKnowledge::Unknown);
                pending.evidence.clear();
                pending.evidence.push_back({ticket, EffectKnowledge::Unknown, false, false, false});
            }
            pending.submitting = false;
            pending.delivering = drain = !pending.evidence.empty();
        } else {
            resources_.CancelConfirmedNotSent(*lease);
            pending_.erase(execution.step.request);
        }
    }
    if (!accepted) { reject(*callbacks); }
    if (drain) { DrainEvidence(execution.step.request); }
}

bool CardCommandTransport::AcceptEvidence(const CardCommandEvidence &evidence)
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
            !evidence.currentStateVerified || (evidence.effect == EffectKnowledge::Unknown &&
                pending.lease.effectRequirement != EffectRequirement::NoBusinessWrite) ||
            (pending.effect != EffectKnowledge::Unknown && pending.effect != evidence.effect))) {
            return false;
        }
        if (pending.submitting && !pending.evidence.empty()) {
            const auto &first = pending.evidence.front();
            pending.contradictoryEvidence |= first.effect != evidence.effect ||
                first.commandEnded != evidence.commandEnded || first.resourceReusable != evidence.resourceReusable ||
                first.legacyWaitSucceeded != evidence.legacyWaitSucceeded ||
                first.currentStateVerified != evidence.currentStateVerified || !(first.response == evidence.response);
            return true;
        }
        pending.evidence.push_back(evidence);
        if (pending.submitting || pending.delivering) { return true; }
        pending.delivering = true;
    }
    DrainEvidence(request);
    return true;
}

void CardCommandTransport::DrainEvidence(RequestId request)
{
    for (;;) {
        std::optional<CardCommandEvidence> next;
        std::shared_ptr<const CardCommandHandlers> callbacks;
        bool ready = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            const auto found = pending_.find(request);
            if (found == pending_.end()) { return; }
            auto &pending = found->second;
            if (pending.evidence.empty()) {
                pending.delivering = false;
                return;
            }
            next = std::move(pending.evidence.front());
            pending.evidence.erase(pending.evidence.begin());
            if (pending.effect != EffectKnowledge::Unknown && pending.effect != next->effect) { continue; }
            ready = next->commandEnded && next->resourceReusable &&
                (next->effect != EffectKnowledge::Unknown ||
                    pending.lease.effectRequirement == EffectRequirement::NoBusinessWrite) &&
                (!pending.quarantined || next->currentStateVerified);
            callbacks = pending.handlers;
        }
        if (ready && next->effect == EffectKnowledge::ConfirmedApplied && callbacks->projectApplied) {
            try {
                callbacks->projectApplied();
            } catch (const std::exception &) {
                // A projection failure cannot change the known device effect.
            }
        }

        bool settled;
        bool first;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            const auto found = pending_.find(request);
            auto &pending = found->second;
            const auto &reply = *next;
            settled = ready && (pending.quarantined ? resources_.AcceptRecoveryEvidence(pending.lease,
                {reply.ticket.step, reply.effect, reply.commandEnded, reply.currentStateVerified,
                    reply.resourceReusable}) :
                resources_.Complete(pending.lease,
                    {reply.ticket.step, reply.effect, reply.commandEnded, reply.resourceReusable}));
            first = !pending.delivered;
            if (!first && !settled) { continue; }
            if (settled) {
                pending_.erase(found);
            } else {
                pending.delivered = true;
                pending.effect = reply.effect;
                if (!pending.quarantined) {
                    pending.quarantined = resources_.Quarantine(pending.lease, reply.effect);
                }
            }
        }
        try {
            callbacks->deliver(*next, settled ? next->effect : EffectKnowledge::Unknown, first, settled);
        } catch (const std::exception &) {
            // Observer failure cannot prevent recovery of the physical lease.
        }
        if (settled) { return; }
    }
}

bool CardCommandTransport::HasPending() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return !pending_.empty();
}

std::vector<CardCommandTransport::CloseEffect> CardCommandTransport::ReadCloseEffects() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<CloseEffect> effects;
    effects.reserve(pending_.size());
    for (const auto &entry : pending_) {
        const auto &pending = entry.second;
        effects.push_back({pending.ticket, pending.lease.resources, pending.effect, pending.submitting,
            pending.delivering, pending.delivered, pending.quarantined});
    }
    return effects;
}
} // namespace OHOS::Telephony::MultiSim
