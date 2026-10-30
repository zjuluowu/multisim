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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "foundation/slot_catalog.h"
#include <utility>

namespace OHOS::Telephony::MultiSim {
SlotCatalog::SlotCatalog(std::vector<SlotDescriptor> slots, LegacyCapacityMeasures capacities)
    : slots_(std::move(slots)), capacities_(std::move(capacities)) {}

const SlotDescriptor *SlotCatalog::Find(SlotId slot) const
{
    for (const auto &entry : slots_) {
        if (entry.slot == slot) {
            return &entry;
        }
    }
    return nullptr; // 真实的目录缺项；不把 Unknown 映射为槽0或 Modem0。
}
} // namespace OHOS::Telephony::MultiSim
