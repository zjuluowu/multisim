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

int32_t CompleteMetadata(LegacyPrimaryHostPort &host, LegacyCompletion &completion,
    const std::function<AccountMetadataResult(const CompletionPlan &)> &issue, bool slotTarget)
{
    const auto waiter = completion.RetainRoleWaiter();
    if (!waiter) { return host.PrimaryError(LegacyPrimaryError::Failed); }
    const CompletionPlan plan {SynchronousCompletionRoute {*waiter}, true};
    try {
        const auto result = issue(plan);
        completion.CompleteLegacyInt(plan, MetadataError(result, host, slotTarget));
    } catch (const std::exception &) {
        // A result delivered before an account observer unwound stays authoritative.
    }
    const auto result = completion.TakeRoleResult(*waiter);
    return result ? *result : host.PrimaryError(LegacyPrimaryError::Failed);
}
} // namespace

int32_t LegacySimFacade::SetShowName(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
    LegacyPrimaryHostPort &host, LegacyCompletion &completion, const std::function<bool(SlotId)> &hasCard,
    const std::function<AccountMetadataResult(const CompletionPlan &)> &issue)
{
    if (!host.IsSystemCaller(caller)) { return host.PrimaryError(LegacyPrimaryError::NonSystemCaller); }
    if (!host.CanSetTelephonyState(caller)) { return host.PrimaryError(LegacyPrimaryError::PermissionDenied); }
    if (!host.SupportsPrimarySlot(slot)) { return host.PrimaryError(LegacyPrimaryError::InvalidSlot); }
    if (!catalog.Find(slot)) { return host.PrimaryError(LegacyPrimaryError::Unavailable); }
    if (!hasCard(slot)) { return host.PrimaryError(LegacyPrimaryError::NoCard); }
    return CompleteMetadata(host, completion, issue, true);
}

int32_t LegacySimFacade::SetSimLabelIndexByIccId(LegacyPrimaryHostPort &host, LegacyCompletion &completion,
    const std::function<AccountMetadataResult(const CompletionPlan &)> &issue)
{
    // The ICCID entry represents the existing in-process CMI route.
    return CompleteMetadata(host, completion, issue, false);
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
