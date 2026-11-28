/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_CARD_COMMAND_TRANSPORT_H
#define OHOS_TELEPHONY_MULTISIM_CARD_COMMAND_TRANSPORT_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "card/runtime/card_observation_runtime.h"
#include "card/security/pin_requests.h"
#include "device/resource/resource_barrier.h"
#include "infrastructure/request/runtime_operation_store.h"

namespace OHOS::Telephony::MultiSim {
enum class CardCommandKind { WriteMsisdn, ReadIccid, UnlockPin, ReadGid1 };

struct CardCommandTicket {
    StepRef step;
    SessionRef session;
    uint64_t fence;
    CardCommandKind kind {CardCommandKind::WriteMsisdn};
    bool operator==(const CardCommandTicket &other) const
    { return step == other.step && session == other.session && fence == other.fence && kind == other.kind; }
};

struct CardCommandEvidence {
    CardCommandTicket ticket;
    EffectKnowledge effect;
    bool commandEnded;
    bool resourceReusable;
    bool legacyWaitSucceeded;
    bool currentStateVerified {false};
    std::variant<std::monostate, std::string, PinReply> response {};
};

struct CardCommandExecution {
    StepRef step;
    SessionRef session;
    std::vector<ResourceKey> resources;
    EffectRequirement effectRequirement;
    std::function<bool()> scopeCurrent {};
    CardCommandKind kind {CardCommandKind::WriteMsisdn};
};

struct CardCommandHandlers {
    std::function<void()> projectApplied;
    std::function<void(const CardCommandEvidence &, EffectKnowledge, bool first, bool transportEnded)> deliver;
};

// One card transport owns association and resource responsibility. Each capability
// supplies its own business projection and completion for the original command.
class CardCommandTransport final {
public:
    CardCommandTransport(CardObservationRuntime &card, ResourceBarrier &resources, RuntimeEpoch epoch);
    std::optional<StepRef> BeginLocalRequest(const SessionRef &session,
        CompletionPlan completion = {InternalCompletionRoute {{0}}, false});
    std::optional<StepRef> AdvanceLocalRequest(const StepRef &original);
    void CloseLocalRequest(const StepRef &original, EffectKnowledge evidence, bool transportEnded);
    void SealLocalCommands();
    using Handoff = std::function<bool(const CardCommandTicket &)>;
    void Submit(const CardCommandExecution &execution, const Handoff &handoff, CardCommandHandlers handlers);
    bool AcceptEvidence(const CardCommandEvidence &evidence);
    bool HasPending() const;
    struct CloseEffect {
        CardCommandTicket original;
        std::vector<ResourceKey> resources;
        EffectKnowledge effect;
        bool submitting;
        bool delivering;
        bool businessResultOffered;
        bool quarantined;
    };
    std::vector<CloseEffect> ReadCloseEffects() const;
private:
    struct Pending {
        CardCommandTicket ticket;
        ResourceLease lease;
        std::shared_ptr<const CardCommandHandlers> handlers;
        bool submitting {true};
        bool delivering {false};
        bool delivered {false};
        bool quarantined {false};
        EffectKnowledge effect {EffectKnowledge::Unknown};
        std::vector<CardCommandEvidence> evidence;
        bool contradictoryEvidence {false};
    };
    void DrainEvidence(RequestId request);
    CardObservationRuntime &card_;
    ResourceBarrier &resources_;
    mutable std::mutex mutex_;
    std::map<RequestId, Pending> pending_;
    RuntimeOperationStore localRequests_;
    bool acceptingLocal_ {true};
};
} // namespace OHOS::Telephony::MultiSim
#endif
