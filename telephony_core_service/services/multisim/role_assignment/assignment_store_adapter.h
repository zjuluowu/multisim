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
// TARGET ARCHITECTURE SKELETON / PROPOSED: only the C0 parameter boundary is implemented.
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3 WU-03/04、§9.3、§13.2.
#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_STORE_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_STORE_ADAPTER_H

#include <array>
#include <string>
#include <string_view>
#include "role_assignment/assignment_store.h"

namespace OHOS::Telephony::MultiSim {
// Narrow platform boundary for the A-owned system-parameter fields. SIM account
// database responsibilities remain outside this C0 adapter.
class AssignmentParameterPort {
public:
    virtual ~AssignmentParameterPort() = default;
    virtual AssignmentParameterResult Read(const std::string &key, const std::string &fallback,
        std::string &value) = 0;
    virtual AssignmentParameterResult Write(const std::string &key, const std::string &value) = 0;
    virtual C0ParameterOutcome WriteC0Parameter(const C0ParameterOperation &, const std::string &key,
        const std::string &value);
    virtual std::optional<C0ParameterEvidence> ReadC0ParameterEvidence(const C0ParameterOperation &)
    { return std::nullopt; }
};

using RoleProviderValues = std::array<std::pair<std::string_view, int32_t>, 2>;
class AssignmentRoleProviderPort {
public:
    virtual ~AssignmentRoleProviderPort() = default;
    virtual bool HasIndependentRoleTransactions() const = 0;
    virtual RoleProviderOutcome UpdateDefaultCard(const RoleProviderOperation &, std::string_view uri,
        const RoleProviderValues &values) = 0;
    virtual std::optional<RoleProviderEvidence> ReadRoleProviderEvidence(const RoleProviderOperation &) = 0;
};

class AssignmentStoreAdapter final : public AssignmentStore {
public:
    explicit AssignmentStoreAdapter(AssignmentParameterPort &parameters, AssignmentRoleProviderPort *provider = nullptr)
        : parameters_(parameters), provider_(provider) {}
    bool SupportsRoleProvider() const override { return provider_ != nullptr; }
    bool HasIndependentRoleTransactions() const override
    { return provider_ && provider_->HasIndependentRoleTransactions(); }
    RoleProviderOutcome WriteDefaultRole(const RoleProviderOperation &operation) override;
    std::optional<RoleProviderEvidence> ReadRoleProviderEvidence(const RoleProviderOperation &operation) override;
    bool SupportsTrackedC0() const override { return true; }
    C0ParameterOutcome WriteC0Parameter(const C0ParameterOperation &operation, SlotId slot,
        const std::string &rawIccid, bool userSet) override;
    std::optional<C0ParameterEvidence> ReadC0ParameterEvidence(const C0ParameterOperation &operation) override;
    RoleStoreResult WriteDefaultRole(const SetRoleCommand &command) override;
    PrimaryParameterSnapshot ReadPrimaryParameters() override;
    AssignmentParameterResult WritePrimaryParameters(SlotId slot, const std::string &rawIccid) override;
    AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &old) override;
    AssignmentParameterResult WriteDefaultData(SlotId slot) override;
    AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) override;
private:
    AssignmentParameterPort &parameters_;
    AssignmentRoleProviderPort *provider_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_STORE_ADAPTER_H
