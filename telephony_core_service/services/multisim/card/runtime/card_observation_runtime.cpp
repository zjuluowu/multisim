/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/runtime/card_observation_runtime.h"
#include <limits>

namespace OHOS::Telephony::MultiSim {
CardObservationRuntime::CardObservationRuntime(const SlotCatalog &catalog, SessionRegistry &sessions,
    CardObservationSink &sink, RuntimeEpoch epoch)
    : catalog_(catalog), sessions_(sessions), sink_(sink), epoch_(epoch) {}

bool CardObservationRuntime::AcceptApplication(const SessionRef &session)
{
    const auto &application = session.application;
    const auto *slot = catalog_.Find(application.slot);
    if (!slot || slot->kind != EndpointKind::Physical || application.applicationKey.empty() ||
        !sessions_.MayPublishVerifiedSession(session) || sequence_ >= std::numeric_limits<uint64_t>::max() - 1) {
        return false;
    }
    if (sessions_.StillCurrent(session)) { return true; }
    if (const auto previous = sessions_.Current(application)) {
        sink_.AcceptCardSessionInvalidation({*previous, ++sequence_});
        sessions_.InvalidateForVerifiedReplacement(*previous);
    }
    return sessions_.PublishApplicationSession(session);
}

std::optional<SessionRef> CardObservationRuntime::ObserveCardApplication(const CardApplicationTarget &application,
    uint64_t generation)
{
    const SessionRef session {epoch_, application, generation};
    std::lock_guard<std::mutex> guard(mutex_);
    return AcceptApplication(session) ? std::make_optional(session) : std::nullopt;
}

std::optional<SessionRef> CardObservationRuntime::ObserveVerifiedCard(const CardApplicationTarget &application,
    uint64_t generation, const std::string &iccid)
{
    const SessionRef session {epoch_, application, generation};
    if (iccid.empty()) { return std::nullopt; }
    CardDiscoveryObservation observation {session, 0, iccid};
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!AcceptApplication(session) || sessions_.IdentityPublished(session)) { return std::nullopt; }
        observation.observationSequence = ++sequence_;
        if (!sink_.RegisterCardDiscoveryObservation(observation)) { return std::nullopt; }
        if (!sessions_.PublishVerifiedSession(session)) { return std::nullopt; }
    }
    (void)sink_.DiscoverAccount(observation);
    return session;
}

bool CardObservationRuntime::StillCurrent(const SessionRef &session)
{
    std::lock_guard<std::mutex> guard(mutex_);
    return sessions_.StillCurrent(session);
}

bool CardObservationRuntime::WithCurrentApplication(const SessionRef &session,
    const std::function<bool()> &commit)
{
    std::lock_guard<std::mutex> guard(mutex_);
    return sessions_.StillCurrent(session) && commit();
}

bool CardObservationRuntime::PublishRecordIdentity(const SessionRef &session, const std::string &full,
    const std::string &decimal,
    const std::function<bool(bool accepted, std::optional<uint64_t> sequence)> &commit)
{
    CardDiscoveryObservation observation {session, 0, full};
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!sessions_.StillCurrent(session)) { return false; }
        auto matched = sink_.MatchesObservedIdentity(session, full);
        if (matched && !*matched && full != decimal) {
            matched = sink_.MatchesObservedIdentity(session, decimal);
            if (matched.value_or(false)) { observation.iccid = decimal; }
        }
        if ((matched && !*matched) || full.empty()) {
            return commit(false, std::nullopt);
        }
        if (sessions_.IdentityPublished(session)) { return commit(matched.value_or(false), std::nullopt); }
        if (sequence_ == std::numeric_limits<uint64_t>::max()) { return commit(false, std::nullopt); }
        observation.observationSequence = ++sequence_;
        if (!commit(true, observation.observationSequence) ||
            !sink_.RegisterCardDiscoveryObservation(observation) || !sessions_.PublishVerifiedSession(session)) {
            return false;
        }
    }
    if (!sink_.DiscoverAccount(observation).observationAdmitted) {
        // A previous original reservation can still own this identity. Retire only
        // our unadmitted observation so a fresh Records load can try after it settles.
        std::lock_guard<std::mutex> guard(mutex_);
        if (sessions_.StillCurrent(session) && sink_.RevokePendingIdentity(session, observation.observationSequence)) {
            sessions_.WithdrawPublishedIdentity(session);
        }
    }
    return true;
}

bool CardObservationRuntime::RefreshRecordIdentity(const SessionRef &session,
    const std::function<bool(std::optional<uint64_t> &sequence)> &refresh)
{
    std::lock_guard<std::mutex> guard(mutex_);
    std::optional<uint64_t> sequence;
    if (!sessions_.StillCurrent(session) || !refresh(sequence)) { return false; }
    if (sequence && sink_.RevokePendingIdentity(session, *sequence)) {
        sessions_.WithdrawPublishedIdentity(session);
    }
    return true;
}

bool CardObservationRuntime::HasVerifiedCard(SlotId slot)
{
    std::lock_guard<std::mutex> guard(mutex_);
    return sessions_.HasVerifiedSession(slot);
}

bool CardObservationRuntime::InvalidateVerifiedCard(const SessionRef &session)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!sessions_.StillCurrent(session) || sequence_ == std::numeric_limits<uint64_t>::max()) { return false; }
    sink_.AcceptCardSessionInvalidation({session, ++sequence_});
    sessions_.InvalidateForVerifiedReplacement(session);
    return true;
}
} // namespace OHOS::Telephony::MultiSim
