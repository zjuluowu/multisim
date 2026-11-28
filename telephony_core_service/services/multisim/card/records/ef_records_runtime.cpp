/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/records/ef_records_runtime.h"
#include "card/runtime/card_observation_runtime.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace OHOS::Telephony::MultiSim {
namespace {
constexpr uint16_t GET_RESPONSE_BYTES = 15;

const char *ScopeName(EfField field)
{
    return field == EfField::Iccid ? "ICCID" : "GID1";
}

int DecodeNibble(char value)
{
    if (value >= '0' && value <= '9') { return value - '0'; }
    if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
    if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
    return -1;
}

std::optional<uint16_t> FileSize(const std::string &hex)
{
    // Same transparent EF metadata offsets as IccFileController::ProcessBinarySize.
    constexpr size_t sizeHigh = 2;
    constexpr size_t sizeLow = 3;
    constexpr size_t fileType = 6;
    constexpr size_t fileStructure = 13;
    if (hex.size() % 2 != 0 || hex.size() / 2 <= fileStructure ||
        std::any_of(hex.begin(), hex.end(), [](char digit) { return DecodeNibble(digit) < 0; })) {
        return std::nullopt;
    }
    const auto at = [&](size_t index) {
        return (DecodeNibble(hex[index * 2]) << 4) | DecodeNibble(hex[index * 2 + 1]);
    };
    if (at(fileType) != 4 || at(fileStructure) != 0) { return std::nullopt; }
    const int size = (at(sizeHigh) << 8) | at(sizeLow);
    return size > 0 && size <= 255 ? std::make_optional(static_cast<uint16_t>(size)) : std::nullopt;
}

} // namespace

EfRecordsRuntime::EfRecordsRuntime(CardObservationRuntime &card, CardCommandTransport &transport, EfIoPort &io)
    : card_(card), transport_(transport), io_(io) {}

EfRecordsRuntime::Key EfRecordsRuntime::ApplicationKey(const SessionRef &session, EfField field)
{
    return {session.application.slot, session.application.applicationKey, field};
}

bool EfRecordsRuntime::IsCurrentLoad(const Scope &scope, const LoadToken &token) const
{
    return scope.session == token.session && scope.inFlight && scope.inFlight->member == token.member &&
        scope.generation == token.recordsGeneration;
}

std::optional<RequestId> EfRecordsRuntime::Load(const SessionRef &session, EfField field)
{
    std::optional<EfIoRoute> route;
    try {
        route = io_.ResolveEfRoute(session, field);
    } catch (const std::exception &) {
        return std::nullopt;
    }
    if (!route || route->resources.empty() || route->path.empty() || route->revision == 0) { return std::nullopt; }

    std::optional<LoadToken> token;
    std::optional<StepRef> step;
    if (!card_.WithCurrentApplication(session, [&] {
        auto &scope = scopes_.try_emplace(ApplicationKey(session, field), Scope {session}).first->second;
        if (!(scope.session == session)) { scope = Scope {session}; }
        if (scope.inFlight || scope.generation == std::numeric_limits<uint64_t>::max()) { return false; }
        step = transport_.BeginLocalRequest(session);
        if (!step) { return false; }
        token.emplace(LoadToken {session, ScopeName(field), ++scope.generation, step->request.value});
        scope.loading.emplace(session, ScopeName(field), scope.generation);
        scope.loading->AddMember({token->member, std::nullopt, true, LoadAccess::Read, LoadMemberState::Pending});
        scope.inFlight = token;
        return true;
    })) { return std::nullopt; }

    Submit(*step, *token, field, *route, EfIoCommand::Code::GetResponse, GET_RESPONSE_BYTES);
    return step->request;
}

void EfRecordsRuntime::Submit(const StepRef &step, const LoadToken &token, EfField field, const EfIoRoute &route,
    EfIoCommand::Code code, uint16_t size)
{
    const EfIoCommand command {code, static_cast<uint16_t>(field), route.path, size};
    transport_.Submit({step, token.session, route.resources, EffectRequirement::NoBusinessWrite, [this, &token, field] {
        const auto found = scopes_.find(ApplicationKey(token.session, field));
        return found != scopes_.end() && IsCurrentLoad(found->second, token);
    }, field == EfField::Iccid ? CardCommandKind::ReadIccid : CardCommandKind::ReadGid1},
    [&](const CardCommandTicket &ticket) { return io_.SubmitEfIo(ticket, route, command); },
    {{}, [this, step, token, field, route, code, size](const CardCommandEvidence &evidence,
        EffectKnowledge effect, bool first, bool ended) {
        if (!first) {
            transport_.CloseLocalRequest(step, effect, ended);
            return;
        }
        const auto *raw = std::get_if<std::string>(&evidence.response);
        const bool validResponse = effect == EffectKnowledge::ConfirmedApplied && evidence.legacyWaitSucceeded && raw;
        if (validResponse && code == EfIoCommand::Code::GetResponse) {
            const auto length = FileSize(*raw);
            const auto next = length ? transport_.AdvanceLocalRequest(step) : std::nullopt;
            if (next) {
                Submit(*next, token, field, route, EfIoCommand::Code::ReadBinary, *length);
                return;
            }
        }

        std::optional<std::string> data;
        if (validResponse && code == EfIoCommand::Code::ReadBinary &&
            raw->size() == static_cast<size_t>(size) * 2 &&
            std::all_of(raw->begin(), raw->end(),
                [](char value) { return DecodeNibble(value) >= 0; })) {
            data = *raw;
        }
        AcceptRead(token, field, data);
        transport_.CloseLocalRequest(step, effect, ended);
    }});
}

void EfRecordsRuntime::AcceptRead(const LoadToken &token, EfField field, const std::optional<std::string> &raw)
{
    const auto commit = [&](const std::optional<Record> &record, std::optional<uint64_t> sequence) {
        const auto found = scopes_.find(ApplicationKey(token.session, field));
        if (found == scopes_.end() || !IsCurrentLoad(found->second, token)) { return false; }
        auto &scope = found->second;
        if (!scope.loading->TryConsumeRead(token, record.has_value())) { return false; }
        scope.inFlight.reset();
        if (record || field == EfField::Gid1) { scope.accepted = record; }
        if (sequence) { scope.identityObservation = sequence; }
        return true;
    };
    if (field == EfField::Iccid && raw) {
        const auto records = DecodeIccid(*raw);
        if (TrustedIdentity(records)) {
            card_.PublishRecordIdentity(token.session, records.full, records.decimal, [&](bool accepted, auto sequence) {
                return commit(accepted ? std::make_optional(Record {records}) : std::nullopt, sequence);
            });
            return;
        }
    }
    card_.WithCurrentApplication(token.session, [&] {
        return commit(field == EfField::Gid1 && raw ? std::make_optional(Record {*raw}) : std::nullopt, std::nullopt);
    });
}

bool EfRecordsRuntime::Refresh(const SessionRef &session, EfField field)
{
    const auto invalidate = [&](std::optional<uint64_t> &sequence) {
        const auto found = scopes_.find(ApplicationKey(session, field));
        if (found == scopes_.end() || !(found->second.session == session)) { return false; }
        auto &scope = found->second;
        if (scope.loading) { scope.loading->InvalidateAffectedReadMembers(); }
        scope.inFlight.reset();
        scope.accepted.reset();
        sequence = std::exchange(scope.identityObservation, std::nullopt);
        return true;
    };
    if (field == EfField::Iccid) { return card_.RefreshRecordIdentity(session, invalidate); }
    return card_.WithCurrentApplication(session, [&] {
        std::optional<uint64_t> unused;
        return invalidate(unused);
    });
}

std::optional<EfRecordsRuntime::Record> EfRecordsRuntime::Read(const SessionRef &session, EfField field)
{
    std::optional<Record> data;
    card_.WithCurrentApplication(session, [&] {
        const auto found = scopes_.find(ApplicationKey(session, field));
        if (found == scopes_.end() || !(found->second.session == session)) { return false; }
        data = found->second.accepted;
        return true;
    });
    return data;
}

std::optional<IccidRecords> EfRecordsRuntime::ReadIccid(const SessionRef &session)
{
    auto data = Read(session, EfField::Iccid);
    return data ? std::make_optional(std::get<IccidRecords>(std::move(*data))) : std::nullopt;
}

std::optional<std::string> EfRecordsRuntime::ReadGid1(const SessionRef &session)
{
    auto data = Read(session, EfField::Gid1);
    return data ? std::make_optional(std::get<std::string>(std::move(*data))) : std::nullopt;
}
} // namespace OHOS::Telephony::MultiSim
