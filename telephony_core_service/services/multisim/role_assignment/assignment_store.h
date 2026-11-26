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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3 WU-03/04、§9.3、§13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_STORE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_STORE_H

#include <string>
#include "role_assignment/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
struct PrimaryParameterSnapshot { std::string slotText; std::string mainIccidText; };
struct AssignmentParameterResult {
    int32_t parameterResult;
    bool callFinished; // false: effect is unknown/unconfirmed; 0 must not be translated to success.
};
enum class C0ParameterKey { Preference, PrimarySlot, PrimaryIccid, DefaultData, Count };
struct C0ParameterOperation {
    StepRef original;
    C0ParameterKey key;
    bool operator==(const C0ParameterOperation &other) const
    { return original == other.original && key == other.key; }
};
struct C0ParameterOutcome {
    AssignmentParameterResult call {-1, false};
    EffectKnowledge effect {EffectKnowledge::Unknown};
};
struct C0ParameterEvidence {
    C0ParameterOperation operation;
    C0ParameterOutcome outcome;
};
enum class RoleProviderEffect { Unknown, NotWritten, Applied, Partial };
struct RoleProviderOperation {
    StepRef original;
    DefaultRole role;
    std::optional<SubscriptionId> subscription;
    bool operator==(const RoleProviderOperation &other) const
    { return original == other.original && role == other.role && subscription == other.subscription; }
};
struct RoleProviderOutcome {
    int32_t result {-1};
    bool callFinished {false};
    RoleProviderEffect effect {RoleProviderEffect::Unknown};
    std::optional<StorePosition> position {};
    bool Settled() const { return callFinished && effect != RoleProviderEffect::Unknown; }
};
struct RoleProviderEvidence {
    RoleProviderOperation operation;
    RoleProviderOutcome outcome;
};
struct RoleStoreResult { int32_t legacyResult; bool writeConfirmed; bool callFinished; };
// A 专属字段/参数协议。A专属 AssignmentStoreAdapter 实现，保持原 URI/键，非新 schema。
class AssignmentStore {
public:
    virtual ~AssignmentStore() = default;
    virtual bool SupportsTrackedC0() const { return false; }
    virtual C0ParameterOutcome WriteC0Parameter(const C0ParameterOperation &, SlotId, const std::string &, bool)
    { return {}; }
    virtual std::optional<C0ParameterEvidence> ReadC0ParameterEvidence(const C0ParameterOperation &)
    { return std::nullopt; }
    virtual bool SupportsRoleProvider() const { return false; }
    virtual bool HasIndependentRoleTransactions() const { return false; }
    virtual RoleProviderOutcome WriteDefaultRole(const RoleProviderOperation &) { return {}; }
    virtual std::optional<RoleProviderEvidence> ReadRoleProviderEvidence(const RoleProviderOperation &)
    { return std::nullopt; }
    virtual RoleStoreResult WriteDefaultRole(const SetRoleCommand &command) = 0;
    virtual PrimaryParameterSnapshot ReadPrimaryParameters() = 0;
    // Despite the historical argument name, input is raw ICCID; the Store adapter owns Legacy-compatible encoding.
    virtual AssignmentParameterResult WritePrimaryParameters(SlotId slot, const std::string &encodedIccid) = 0;
    virtual AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &old) = 0;
    virtual AssignmentParameterResult WriteDefaultData(SlotId slot) = 0;
    virtual AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_ASSIGNMENT_STORE_H
