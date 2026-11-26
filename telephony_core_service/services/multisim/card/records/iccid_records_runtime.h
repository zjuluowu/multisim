/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_ICCID_RECORDS_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_ICCID_RECORDS_RUNTIME_H

#include "card/records/load_session.h"
#include "card/runtime/card_command_transport.h"

namespace OHOS::Telephony::MultiSim {
struct IccidIoRoute {
    std::vector<ResourceKey> resources;
    std::string path;
};

struct IccidIoCommand {
    enum class Code : uint8_t { GetResponse = 0xc0, ReadBinary = 0xb0 };
    Code code;
    uint16_t fileId;
    std::string path;
    uint16_t size;
};

class IccidIoPort {
public:
    virtual ~IccidIoPort() = default;
    virtual std::vector<ResourceKey> InitialResources() const = 0;
    virtual std::optional<IccidIoRoute> ResolveIccidRoute(const SessionRef &session) = 0;
    // Dispatch only to the original application in the ticket. False proves the command did not enter transport.
    virtual bool SubmitIccidIo(const CardCommandTicket &ticket, const IccidIoCommand &command) = 0;
};

struct IccidRecords {
    std::string full;
    std::string decimal;
};

class IccidRecordsRuntime final {
public:
    IccidRecordsRuntime(CardObservationRuntime &card, CardCommandTransport &transport, IccidIoPort &io);
    std::optional<RequestId> Load(const SessionRef &session);
    bool Refresh(const SessionRef &session);
    std::optional<IccidRecords> Read(const SessionRef &session);
private:
    struct Scope {
        SessionRef session;
        uint64_t generation {0};
        std::optional<LoadSession> loading {};
        std::optional<LoadToken> inFlight {};
        std::optional<IccidRecords> accepted {};
        std::optional<uint64_t> identityObservation {};
    };
    using Key = std::pair<SlotId, std::string>;
    static Key ApplicationKey(const SessionRef &session);
    bool IsCurrentLoad(const Scope &scope, const LoadToken &token) const;
    void Submit(const StepRef &step, const LoadToken &token, const IccidIoRoute &route,
        IccidIoCommand::Code code, uint16_t size);
    void AcceptRead(const LoadToken &token, const std::optional<IccidRecords> &data);

    CardObservationRuntime &card_;
    CardCommandTransport &transport_;
    IccidIoPort &io_;
    // All views and load generations are guarded by the card application's commit domain.
    std::map<Key, Scope> scopes_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
