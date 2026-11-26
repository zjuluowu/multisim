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
#include "device/resource/resource_barrier.h"
#include "infrastructure/request/runtime_operation_store.h"

namespace OHOS::Telephony::MultiSim {
struct CardCommandTicket {
    StepRef step;
    SessionRef session;
    uint64_t fence;
    bool operator==(const CardCommandTicket &other) const
    { return step == other.step && session == other.session && fence == other.fence; }
};

struct CardCommandEvidence {
    CardCommandTicket ticket;
    EffectKnowledge effect;
    bool commandEnded;
    bool resourceReusable;
    bool legacyWaitSucceeded;
    bool currentStateVerified {false};
    std::optional<std::string> rawResponse {};
};

struct CardCommandExecution {
    StepRef step;
    SessionRef session;
    std::vector<ResourceKey> resources;
    EffectRequirement effectRequirement;
    std::function<bool()> scopeCurrent {};
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
    std::optional<StepRef> BeginLocalRequest(const SessionRef &session);
    std::optional<StepRef> AdvanceLocalRequest(const StepRef &original);
    void CloseLocalRequest(const StepRef &original, EffectKnowledge evidence, bool transportEnded);
    void SealLocalCommands();
    using Handoff = std::function<bool(const CardCommandTicket &)>;
    void Submit(const CardCommandExecution &execution, const Handoff &handoff, CardCommandHandlers handlers);
    bool AcceptEvidence(const CardCommandEvidence &evidence);
    bool HasPending() const;
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
