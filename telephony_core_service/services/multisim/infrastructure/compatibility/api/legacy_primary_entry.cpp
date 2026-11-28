/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "infrastructure/compatibility/api/legacy_sim_facade.h"
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
int32_t LegacySimFacade::SetPrimarySlotId(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
    LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology, LegacyCompletion &completion,
    const std::function<bool(SlotId)> &hasVerifiedCard,
    const std::function<PrimaryStart(const SwitchPrimaryCommand &)> &issue,
    const std::function<bool(RequestId, uint64_t)> &timeout)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!host.SupportsPrimarySlot(slot) || !catalog.Find(slot)) {
        return host.PrimaryError(LegacyPrimaryError::InvalidSlot);
    }
    if (!hasVerifiedCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    const auto target = topology.ReadPrimaryTarget(slot);
    if (!target || !target->active) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }

    const auto waiter = completion.RetainRoleWaiter();
    if (!waiter) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    try {
        const auto started = issue({{slot}, true, {SynchronousCompletionRoute {*waiter}, true},
            std::nullopt, target->binding});
        if (timeout && started.waitMilliseconds &&
            !completion.AwaitPrimaryResult(*waiter, started.waitMilliseconds) &&
            !timeout(started.request, *started.waitMilliseconds)) {
            // The device or Store result won the original deadline. Its publication can still be running.
            (void)completion.AwaitPrimaryResult(*waiter, std::nullopt);
        }
    } catch (const std::exception &) {
        // Preserve an already-recorded result if the original delivery hook unwound.
    }
    const auto result = completion.TakeRoleResult(*waiter);
    if (!result || *result == -1) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    return *result;
}

int32_t LegacySimFacade::SetDefaultCellularDataSlotId(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
    LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology, LegacyCompletion &completion,
    AssignmentService &assignment, const std::function<bool(SlotId)> &hasVerifiedCard)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!catalog.Find(slot) || !hasVerifiedCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    const auto target = topology.ReadPrimaryTarget(slot);
    if (!target || !target->active) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }

    const auto waiter = completion.RetainRoleWaiter();
    if (!waiter) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    try {
        SetLegacyDefaultData(assignment, target->binding,
            {SynchronousCompletionRoute {*waiter}, true});
    } catch (const std::exception &) {
        // Take the retained result even if dispatch unwinds.
    }
    const auto result = completion.TakeRoleResult(*waiter);
    return !result || *result == -1 ? host.PrimaryError(LegacyPrimaryError::Failed) : *result;
}

int32_t LegacySimFacade::SetDefaultCallMessageRole(DefaultRole role, SlotId slot, LegacyCallerRef caller,
    const SlotCatalog &catalog, LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology,
    LegacyCompletion &completion, AssignmentService &assignment, const std::function<bool(SlotId)> &hasVerifiedCard)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    std::optional<BindingRef> binding;
    if (slot.legacySlotId != -1) {
        if (!catalog.Find(slot) || (role == DefaultRole::Voice && !host.SupportsPrimarySlot(slot))) {
            return host.PrimaryError(LegacyPrimaryError::InvalidSlot);
        }
        if (!hasVerifiedCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
        const auto facts = topology.ReadPrimaryTarget(slot);
        if (!facts || !facts->active) { return host.PrimaryError(LegacyPrimaryError::InactiveCard); }
        binding = facts->binding;
    }

    const auto waiter = completion.RetainRoleWaiter();
    if (!waiter) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    try {
        SetLegacyDefaultCallMessage(assignment, {role,
            binding ? std::make_optional(binding->subscription) : std::nullopt, slot}, binding,
            {SynchronousCompletionRoute {*waiter}, true});
    } catch (const std::exception &) {
        // Consume the original waiter even if validation or dispatch unwinds.
    }
    const auto result = static_cast<LegacyRoleResult>(completion.TakeRoleResult(*waiter).value_or(-1));
    switch (result) {
        case LegacyRoleResult::Success: return 0;
        case LegacyRoleResult::StoreFailed: return host.PrimaryError(LegacyPrimaryError::DatabaseWriteFailed);
        case LegacyRoleResult::PublicationFailed: return host.PrimaryError(LegacyPrimaryError::BroadcastFailed);
        case LegacyRoleResult::Failed: return host.PrimaryError(LegacyPrimaryError::Failed);
    }
    return host.PrimaryError(LegacyPrimaryError::Failed);
}
} // namespace OHOS::Telephony::MultiSim
