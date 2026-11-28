/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_EF_RECORDS_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_EF_RECORDS_RUNTIME_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>
#include "card/records/iccid_records_runtime.h"
#include "card/records/load_session.h"
#include "card/runtime/card_command_transport.h"

namespace OHOS::Telephony::MultiSim {
enum class EfField : uint16_t { Iccid = 0x2fe2, Gid1 = 0x6f3e };

struct EfIoRoute {
    std::vector<ResourceKey> resources;
    std::string path;
    uint64_t revision;
};
struct EfIoCommand {
    enum class Code { GetResponse, ReadBinary };
    Code code;
    uint16_t fileId;
    std::string path;
    uint16_t size;
};
class EfIoPort {
public:
    virtual ~EfIoPort() = default;
    virtual std::vector<ResourceKey> InitialResources() const = 0;
    virtual std::optional<EfIoRoute> ResolveEfRoute(const SessionRef &session, EfField field) = 0;
    // Compare the original application and route at irreversible transport admission.
    // False proves the original command did not enter transport.
    virtual bool SubmitEfIo(const CardCommandTicket &ticket, const EfIoRoute &route, const EfIoCommand &command) = 0;
};

class EfRecordsRuntime final {
public:
    EfRecordsRuntime(CardObservationRuntime &card, CardCommandTransport &transport, EfIoPort &io);
    std::optional<RequestId> Load(const SessionRef &session, EfField field);
    bool Refresh(const SessionRef &session, EfField field);
    std::optional<IccidRecords> ReadIccid(const SessionRef &session);
    std::optional<std::string> ReadGid1(const SessionRef &session);
private:
    using Record = std::variant<IccidRecords, std::string>;
    struct Scope {
        SessionRef session;
        uint64_t generation {0};
        std::optional<LoadSession> loading {};
        std::optional<LoadToken> inFlight {};
        std::optional<Record> accepted {};
        std::optional<uint64_t> identityObservation {};
    };
    using Key = std::tuple<SlotId, std::string, EfField>;
    static Key ApplicationKey(const SessionRef &session, EfField field);
    bool IsCurrentLoad(const Scope &scope, const LoadToken &token) const;
    void Submit(const StepRef &step, const LoadToken &token, EfField field, const EfIoRoute &route,
        EfIoCommand::Code code, uint16_t size);
    void AcceptRead(const LoadToken &token, EfField field, const std::optional<std::string> &raw);
    std::optional<Record> Read(const SessionRef &session, EfField field);

    CardObservationRuntime &card_;
    CardCommandTransport &transport_;
    EfIoPort &io_;
    // Scope generation and the accepted field are guarded by the application's commit domain.
    std::map<Key, Scope> scopes_;
};
} // namespace OHOS::Telephony::MultiSim
#endif
