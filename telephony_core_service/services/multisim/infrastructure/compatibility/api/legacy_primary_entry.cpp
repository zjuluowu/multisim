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
    const std::function<void(const SwitchPrimaryCommand &)> &issue)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!host.SupportsPrimarySlot(slot) || !catalog.Find(slot)) {
        return host.PrimaryError(LegacyPrimaryError::InvalidSlot);
    }
    if (!hasVerifiedCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    const auto target = topology.ReadPrimaryTarget(slot);
    if (!target || !target->active) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }

    const auto waiter = completion.RetainPrimaryWaiter();
    if (!waiter) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    try {
        issue({{slot}, true, {SynchronousCompletionRoute {*waiter}, true}, std::nullopt, target->binding});
    } catch (const std::exception &) {
        // Preserve an already-recorded result if the original delivery hook unwound.
    }
    const auto result = completion.TakePrimaryResult(*waiter);
    if (!result || *result == -1) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    return *result;
}
} // namespace OHOS::Telephony::MultiSim
