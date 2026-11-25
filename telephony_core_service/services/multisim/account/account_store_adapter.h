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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3、§9.2、§13.1～13.2。
// Match/Write discovery 已定义；其余方法保持显式失败，不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACCOUNT_STORE_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACCOUNT_STORE_ADAPTER_H

#include <string_view>
#include "account/account_store.h"

namespace OHOS::Telephony { class SimRdbHelper; }
namespace OHOS::Telephony::MultiSim {
// Narrow legacy-provider bridge. The production implementation calls the existing
// SimRdbHelper; it is linked by the future owning assembly, not by the closed skeleton.
AccountLoadResult LoadLegacyAccounts(OHOS::Telephony::SimRdbHelper &store,
    const AccountLoadRequest &request);
AccountIdentityMatch MatchLegacyAccountByIccid(OHOS::Telephony::SimRdbHelper &store,
    const std::string &iccid);
AccountDiscoveryStoreResult WriteLegacyDiscoveredAccount(OHOS::Telephony::SimRdbHelper &store,
    const AccountDiscoveryWritePlan &plan);
using AccountMetadataValue = std::variant<std::u16string, int32_t>;
class AccountMetadataProviderPort {
public:
    virtual ~AccountMetadataProviderPort() = default;
    virtual bool HasIndependentMetadataFields() const = 0;
    virtual AccountMetadataOutcome UpdateAccountField(const AccountMetadataOperation &, std::string_view uri,
        std::string_view predicateColumn, const std::string &predicateValue, std::string_view field,
        const AccountMetadataValue &value) = 0;
    // A found row can carry the original UNKNOWN value. Query results never attribute a write.
    virtual ImsPreferenceRead QueryAccountField(std::string_view, std::string_view,
        const std::string &, std::string_view) { return {}; }
    virtual std::optional<AccountMetadataEvidence> ReadMetadataEvidence(const AccountMetadataOperation &) = 0;
};
// The platform port encodes the typed value in the original DataShare values bucket.

class AccountStoreAdapter final : public AccountStore {
public:
    explicit AccountStoreAdapter(SimRdbHelper &legacyStore)
        : legacy_ {&legacyStore, LoadLegacyAccounts, MatchLegacyAccountByIccid, WriteLegacyDiscoveredAccount} {}
    AccountStoreAdapter(AccountStore &discovery, AccountMetadataProviderPort &metadata)
        : discovery_(&discovery), metadata_(&metadata) {}
    bool SupportsMetadata() const override { return metadata_ != nullptr; }
    bool HasIndependentMetadataFields() const override
    { return metadata_ && metadata_->HasIndependentMetadataFields(); }
    AccountMetadataOutcome WriteDisplayName(const AccountMetadataOperation &, const AccountNamePatch &) override;
    AccountMetadataOutcome WriteLabel(const AccountMetadataOperation &, const AccountLabelPatch &) override;
    std::optional<AccountMetadataEvidence> ReadMetadataEvidence(const AccountMetadataOperation &) override;
    AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) override;
    AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) override;
    AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) override;
    AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) override;
    AccountMetadataOutcome WriteImsPreference(const AccountMetadataOperation &operation,
        const AccountImsPreferencePatch &patch) override;
    AccountLoadResult LoadAccounts(const AccountLoadRequest &request) override;
    AccountIdentityMatch MatchAccountByIccid(const std::string &iccid) override;
    AccountIdentityMatch MatchDiscoveryAccount(DiscoveryCallRef call, const std::string &iccid) override
    { return discovery_ ? discovery_->MatchDiscoveryAccount(call, iccid) : MatchAccountByIccid(iccid); }
    bool DiscoveryMatchReturned(DiscoveryCallRef call) override
    { return discovery_ && discovery_->DiscoveryMatchReturned(call); }
    std::optional<AccountDiscoveryStoreResult> ReadDiscoveryEvidence(DiscoveryCallRef call) override
    { return discovery_ ? discovery_->ReadDiscoveryEvidence(call) : std::nullopt; }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &plan) override;
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &patch) override;
    ImsPreferenceRead QueryImsPreferenceByIccid(const std::string &iccid) override;
private:
    struct LegacyDiscoveryBridge {
        SimRdbHelper *store {nullptr};
        decltype(&LoadLegacyAccounts) load {nullptr};
        decltype(&MatchLegacyAccountByIccid) match {nullptr};
        decltype(&WriteLegacyDiscoveredAccount) write {nullptr};
    };
    LegacyDiscoveryBridge legacy_;
    AccountStore *discovery_ {nullptr};
    AccountMetadataProviderPort *metadata_ {nullptr};
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACCOUNT_STORE_ADAPTER_H
