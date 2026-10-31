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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §3.3、§4.2、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_BINDING_INDEX_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_BINDING_INDEX_H

#include <map>
#include <optional>
#include <vector>
#include "foundation/slot_catalog.h"

namespace OHOS::Telephony::MultiSim {
struct SlotRuntimeEntry {
    SlotId slot;
    bool instanceAssembled {false};
    uint64_t bindingRevision {0}; // 空位置也保存；移除后不得归零，防同卡再插ABA。
    std::optional<BindingRef> acceptedBinding;
};
// T 维护已接受绑定/空位置版本；R 维护原始设备会话。目录不证明对象已装配。
// 一位置一当前绑定沿旧契约；Port映到不同逻辑槽不自动要求新schema。
class BindingIndex final {
public:
    explicit BindingIndex(const SlotCatalog &catalog);
    std::optional<BindingRef> Find(SlotId slot) const;
    std::optional<uint64_t> ReadPositionRevision(SlotId slot) const;
    bool StillMatches(const BindingRef &expected) const;
    bool AcceptAssemblyState(SlotId slot, bool assembled);
    bool AcceptBinding(SlotId slot, SubscriptionId subscription, uint64_t expectedRevision);
    bool RemoveBinding(const BindingRef &expected);
    std::vector<SlotRuntimeEntry> ReadRuntimeSlots() const;
private:
    const SlotCatalog &catalog_;
    std::map<SlotId, SlotRuntimeEntry> slots_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_BINDING_INDEX_H
