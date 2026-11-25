/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "infrastructure/compatibility/api/legacy_sim_facade.h"

#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
namespace {
int32_t MetadataError(AccountMetadataResult result, LegacyPrimaryHostPort &host, bool slotTarget)
{
    switch (result) {
        case AccountMetadataResult::Success: return 0;
        case AccountMetadataResult::NoAccount:
            return host.PrimaryError(slotTarget ? LegacyPrimaryError::NoCard : LegacyPrimaryError::InvalidArgument);
        case AccountMetadataResult::InvalidLabel: return host.PrimaryError(LegacyPrimaryError::InvalidArgument);
        case AccountMetadataResult::Conflict: return host.PrimaryError(LegacyPrimaryError::Failed);
        case AccountMetadataResult::DatabaseWriteFailed: return host.PrimaryError(LegacyPrimaryError::DatabaseWriteFailed);
        case AccountMetadataResult::HistoricalMissing: return host.PrimaryError(LegacyPrimaryError::HistoricalMissing);
    }
    return host.PrimaryError(LegacyPrimaryError::Failed);
}

int32_t CompleteAccountWrite(LegacyCompletion &completion, const std::function<int32_t(const CompletionPlan &)> &issue,
    bool notify, int32_t unavailable)
{
    const auto waiter = completion.RetainRoleWaiter();
    if (!waiter) { return unavailable; }
    const CompletionPlan plan {SynchronousCompletionRoute {*waiter}, notify};
    try {
        completion.CompleteLegacyInt(plan, issue(plan));
    } catch (const std::exception &) {
        // A result delivered before an account observer unwound stays authoritative.
    }
    const auto result = completion.TakeRoleResult(*waiter);
    return result ? *result : unavailable;
}
} // namespace

int32_t LegacySimFacade::SaveImsSwitch(SlotId slot, const SlotCatalog &catalog, LegacyPrimaryHostPort &host,
    LegacyCompletion &completion, const std::function<int32_t(const CompletionPlan &)> &issue)
{
    if (!catalog.Find(slot)) { return host.PrimaryError(LegacyPrimaryError::InvalidArgument); }
    return CompleteAccountWrite(completion, issue, false, -1);
}

int32_t LegacySimFacade::QueryImsSwitch(SlotId slot, const SlotCatalog &catalog, LegacyPrimaryHostPort &host,
    const std::function<int32_t()> &read)
{
    if (!catalog.Find(slot)) { return host.PrimaryError(LegacyPrimaryError::InvalidArgument); }
    return read();
}

int32_t LegacySimFacade::SetShowName(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
    LegacyPrimaryHostPort &host, LegacyCompletion &completion, const std::function<bool(SlotId)> &hasCard,
    const std::function<AccountMetadataResult(const CompletionPlan &)> &issue)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!host.SupportsPrimarySlot(slot)) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }
    if (!catalog.Find(slot)) { return host.PrimaryError(LegacyPrimaryError::Unavailable); }
    if (!hasCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    return CompleteAccountWrite(completion,
        [&](const CompletionPlan &plan) { return MetadataError(issue(plan), host, true); },
        true, host.PrimaryError(LegacyPrimaryError::Failed));
}

int32_t LegacySimFacade::SetSimLabelIndexByIccId(LegacyPrimaryHostPort &host, LegacyCompletion &completion,
    const std::function<AccountMetadataResult(const CompletionPlan &)> &issue)
{
    // The ICCID entry represents the existing in-process CMI route.
    return CompleteAccountWrite(completion,
        [&](const CompletionPlan &plan) { return MetadataError(issue(plan), host, false); },
        true, host.PrimaryError(LegacyPrimaryError::Failed));
}

int32_t LegacySimFacade::GetShowName(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
    LegacyPrimaryHostPort &host, std::u16string &name, const std::function<bool(SlotId)> &hasCard,
    const std::function<std::optional<AccountView>()> &read)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanGetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!host.SupportsPrimarySlot(slot)) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }
    if (!catalog.Find(slot)) { return host.PrimaryError(LegacyPrimaryError::Unavailable); }
    if (!hasCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    const auto account = read();
    if (!account) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    name = account->displayName;
    return 0;
}
} // namespace OHOS::Telephony::MultiSim
