/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/records/iccid_records_runtime.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace OHOS::Telephony::MultiSim {
namespace {
constexpr uint16_t ICCID_FILE_ID = 0x2fe2;
constexpr uint16_t ICCID_GET_RESPONSE_BYTES = 15;
constexpr auto ICCID_SCOPE = "ICCID";

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

IccidRecords DecodeIccid(const std::string &raw)
{
    // Preserve the separate full and decimal views from IccFile.
    IccidRecords result;
    if (raw.size() < 2) { return {raw, raw}; }
    bool decimalFinished = false;
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        const auto high = raw[i + 1];
        const auto low = raw[i];
        result.full += high;
        result.full += low;
        if (decimalFinished || high > '9') {
            decimalFinished = true;
            continue;
        }
        result.decimal += high;
        if (low == 'F') { continue; }
        if (low > '9') {
            decimalFinished = true;
        } else {
            result.decimal += low;
        }
    }
    return result;
}

bool TrustedIdentity(const IccidRecords &records)
{
    const auto padding = records.full.find_first_not_of("0123456789");
    return !records.decimal.empty() && records.full.compare(0, records.decimal.size(), records.decimal) == 0 &&
        (padding == std::string::npos || (padding == records.decimal.size() &&
            records.full.find_first_not_of('F', padding) == std::string::npos));
}
} // namespace

IccidRecordsRuntime::IccidRecordsRuntime(CardObservationRuntime &card, CardCommandTransport &transport, IccidIoPort &io)
    : card_(card), transport_(transport), io_(io) {}

IccidRecordsRuntime::Key IccidRecordsRuntime::ApplicationKey(const SessionRef &session)
{
    return {session.application.slot, session.application.applicationKey};
}

bool IccidRecordsRuntime::IsCurrentLoad(const Scope &scope, const LoadToken &token) const
{
    return scope.session == token.session && scope.inFlight && scope.inFlight->member == token.member &&
        scope.generation == token.recordsGeneration;
}

std::optional<RequestId> IccidRecordsRuntime::Load(const SessionRef &session)
{
    std::optional<IccidIoRoute> route;
    try {
        route = io_.ResolveIccidRoute(session);
    } catch (const std::exception &) {
        return std::nullopt;
    }
    if (!route || route->resources.empty() || route->path.empty()) { return std::nullopt; }

    std::optional<LoadToken> token;
    std::optional<StepRef> step;
    if (!card_.WithCurrentApplication(session, [&] {
        auto &scope = scopes_.try_emplace(ApplicationKey(session), Scope {session}).first->second;
        if (!(scope.session == session)) { scope = Scope {session}; }
        if (scope.inFlight || scope.generation == std::numeric_limits<uint64_t>::max()) { return false; }
        step = transport_.BeginLocalRequest(session);
        if (!step) { return false; }
        token.emplace(LoadToken {session, ICCID_SCOPE, ++scope.generation, step->request.value});
        scope.loading.emplace(session, ICCID_SCOPE, scope.generation);
        scope.loading->AddMember({token->member, std::nullopt, true, LoadAccess::Read, LoadMemberState::Pending});
        scope.inFlight = token;
        return true;
    })) { return std::nullopt; }

    Submit(*step, *token, *route, IccidIoCommand::Code::GetResponse, ICCID_GET_RESPONSE_BYTES);
    return step->request;
}

void IccidRecordsRuntime::Submit(const StepRef &step, const LoadToken &token, const IccidIoRoute &route,
    IccidIoCommand::Code code, uint16_t size)
{
    const IccidIoCommand command {code, ICCID_FILE_ID, route.path, size};
    transport_.Submit({step, token.session, route.resources, EffectRequirement::NoBusinessWrite, [this, &token] {
        const auto found = scopes_.find(ApplicationKey(token.session));
        return found != scopes_.end() && IsCurrentLoad(found->second, token);
    }, CardCommandKind::ReadIccid}, [&](const CardCommandTicket &ticket) { return io_.SubmitIccidIo(ticket, command); },
    {{}, [this, step, token, route, code, size](const CardCommandEvidence &evidence,
        EffectKnowledge effect, bool first, bool ended) {
        if (!first) {
            transport_.CloseLocalRequest(step, effect, ended);
            return;
        }
        const auto *raw = std::get_if<std::string>(&evidence.response);
        const bool validResponse = effect == EffectKnowledge::ConfirmedApplied && evidence.legacyWaitSucceeded && raw;
        if (validResponse && code == IccidIoCommand::Code::GetResponse) {
            const auto length = FileSize(*raw);
            const auto next = length ? transport_.AdvanceLocalRequest(step) : std::nullopt;
            if (next) {
                Submit(*next, token, route, IccidIoCommand::Code::ReadBinary, *length);
                return;
            }
        }

        std::optional<IccidRecords> data;
        if (validResponse && code == IccidIoCommand::Code::ReadBinary &&
            raw->size() == static_cast<size_t>(size) * 2 &&
            std::all_of(raw->begin(), raw->end(),
                [](char value) { return DecodeNibble(value) >= 0; })) {
            data = DecodeIccid(*raw);
        }
        AcceptRead(token, data);
        transport_.CloseLocalRequest(step, effect, ended);
    }});
}

void IccidRecordsRuntime::AcceptRead(const LoadToken &token, const std::optional<IccidRecords> &data)
{
    const auto commit = [&](bool accepted, std::optional<uint64_t> sequence) {
        const auto found = scopes_.find(ApplicationKey(token.session));
        if (found == scopes_.end() || !IsCurrentLoad(found->second, token)) { return false; }
        auto &scope = found->second;
        if (!scope.loading->TryConsumeRead(token, accepted)) { return false; }
        scope.inFlight.reset();
        if (accepted) {
            scope.accepted = data;
            if (sequence) { scope.identityObservation = sequence; }
        }
        return true;
    };
    if (data && TrustedIdentity(*data)) {
        card_.PublishRecordIdentity(token.session, data->full, data->decimal, commit);
    } else {
        card_.WithCurrentApplication(token.session, [&] { return commit(false, std::nullopt); });
    }
}

bool IccidRecordsRuntime::Refresh(const SessionRef &session)
{
    return card_.RefreshRecordIdentity(session, [&](std::optional<uint64_t> &sequence) {
        const auto found = scopes_.find(ApplicationKey(session));
        if (found == scopes_.end() || !(found->second.session == session)) { return false; }
        auto &scope = found->second;
        if (scope.loading) { scope.loading->InvalidateAffectedReadMembers(); }
        scope.inFlight.reset();
        scope.accepted.reset();
        sequence = std::exchange(scope.identityObservation, std::nullopt);
        return true;
    });
}

std::optional<IccidRecords> IccidRecordsRuntime::Read(const SessionRef &session)
{
    std::optional<IccidRecords> data;
    card_.WithCurrentApplication(session, [&] {
        const auto found = scopes_.find(ApplicationKey(session));
        if (found == scopes_.end() || !(found->second.session == session)) { return false; }
        data = found->second.accepted;
        return true;
    });
    return data;
}
} // namespace OHOS::Telephony::MultiSim
