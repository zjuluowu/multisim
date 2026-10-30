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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 尚未接入生产或构建，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §3.3、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_FOUNDATION_SLOT_CATALOG_H
#define OHOS_TELEPHONY_MULTISIM_FOUNDATION_SLOT_CATALOG_H

#include <optional>
#include <vector>
#include "foundation/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class EndpointKind { Physical, EuiccCapable, Virtual, RestrictedPhysical, Distributed, Unknown };
enum class VisibilityClass { Ordinary, CallerCapabilityRequired, Internal, Unknown };
struct SlotDescriptor {
    SlotId slot;
    EndpointKind kind;
    std::optional<int32_t> rilInstance;
    std::optional<RadioDomainId> radioDomain;
    VisibilityClass visibility;
    bool mayJoinAtRuntime;
};
struct LegacyCapacityMeasures {
    std::optional<uint32_t> configured;
    std::optional<uint32_t> routable;
    std::optional<uint32_t> assembled;
    std::optional<uint32_t> externallyVisible;
    std::optional<uint32_t> radioStackSlots;
};
// H 构建后以 const 引用注入 T/A/R/C；不维护在位/绑定，不是新 Manager。
// 来源：multi_sim_controller.h / sim_utils / capability_mgr / EXT 槽与容量规则。
// 各调用点旧容量不一致的产品组合需单列裁决，不能静默统一。
class SlotCatalog final {
public:
    SlotCatalog(std::vector<SlotDescriptor> slots, LegacyCapacityMeasures capacities);
    const SlotDescriptor *Find(SlotId slot) const;
    const std::vector<SlotDescriptor> &Slots() const { return slots_; }
    const LegacyCapacityMeasures &Capacities() const { return capacities_; }
private:
    const std::vector<SlotDescriptor> slots_;
    const LegacyCapacityMeasures capacities_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_FOUNDATION_SLOT_CATALOG_H
