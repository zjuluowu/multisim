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
#include "model/slot_catalog.h"

namespace OHOS::Telephony::MultiSim {
struct SlotRuntimeEntry {
    SlotId slot;
    bool instanceAssembled;
    std::optional<BindingRef> acceptedBinding;
};
// T 拥有槽运行清单/位置绑定/局部 revision；R 拥有原始在位状态和 SessionRef。
// 近期每位置至多一个当前绑定；Port 感知模型仅 G4 有证据后演进。
class BindingIndex final {
public:
    explicit BindingIndex(const SlotCatalog &catalog);
    std::optional<BindingRef> Find(SlotId slot) const;
    bool StillMatches(const BindingRef &expected) const;
    bool AcceptBinding(SlotId slot, SubscriptionId subscription, uint64_t expectedRevision);
    bool RemoveBinding(const BindingRef &expected);
    std::vector<SlotRuntimeEntry> ReadRuntimeSlots() const;
private:
    const SlotCatalog &catalog_;
    std::map<SlotId, SlotRuntimeEntry> slots_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_BINDING_INDEX_H
