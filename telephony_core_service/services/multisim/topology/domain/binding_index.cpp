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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/domain/binding_index.h"
#include <limits>

namespace OHOS::Telephony::MultiSim {
BindingIndex::BindingIndex(const SlotCatalog &catalog) : catalog_(catalog)
{
    for (const auto &descriptor : catalog_.Slots()) {
        slots_.emplace(descriptor.slot, SlotRuntimeEntry {descriptor.slot, false, 0, std::nullopt});
    }
}

std::optional<BindingRef> BindingIndex::Find(SlotId slot) const
{
    auto entry = slots_.find(slot);
    return entry == slots_.end() ? std::nullopt : entry->second.acceptedBinding;
}

std::optional<uint64_t> BindingIndex::ReadPositionRevision(SlotId slot) const
{
    auto entry = slots_.find(slot);
    return entry == slots_.end() ? std::nullopt : std::optional<uint64_t>(entry->second.bindingRevision);
}

bool BindingIndex::StillMatches(const BindingRef &expected) const
{
    auto current = Find(expected.slot);
    return current && current->subscription == expected.subscription &&
        current->bindingRevision == expected.bindingRevision;
}

bool BindingIndex::AcceptAssemblyState(SlotId slot, bool assembled)
{
    auto entry = slots_.find(slot);
    if (entry == slots_.end() || (!assembled && entry->second.acceptedBinding)) {
        return false; // 先经T正式解绑；不能靠销毁对象抹掉业务绑定。
    }
    entry->second.instanceAssembled = assembled;
    return true;
}

bool BindingIndex::AcceptBinding(SlotId slot, SubscriptionId subscription, uint64_t expectedRevision)
{
    auto entry = slots_.find(slot);
    if (entry == slots_.end() || !entry->second.instanceAssembled ||
        entry->second.bindingRevision != expectedRevision) {
        return false;
    }
    if (entry->second.acceptedBinding) {
        return entry->second.acceptedBinding->subscription == subscription;
    }
    for (const auto &[otherSlot, other] : slots_) {
        if (!(otherSlot == slot) && other.acceptedBinding &&
            other.acceptedBinding->subscription == subscription) {
            return false; // 同一已接受账户不暗中迁移；跨键移动先走T计划。
        }
    }
    if (entry->second.bindingRevision == std::numeric_limits<uint64_t>::max()) {
        return false; // 不允许回绕产生ABA。
    }
    ++entry->second.bindingRevision;
    entry->second.acceptedBinding = BindingRef {subscription, slot, entry->second.bindingRevision};
    return true;
}

bool BindingIndex::RemoveBinding(const BindingRef &expected)
{
    if (!StillMatches(expected)) {
        return false;
    }
    auto &entry = slots_.find(expected.slot)->second;
    if (entry.bindingRevision == std::numeric_limits<uint64_t>::max()) {
        return false;
    }
    ++entry.bindingRevision;
    entry.acceptedBinding.reset(); // 空位置继续持有递增后的版本。
    return true;
}

std::vector<SlotRuntimeEntry> BindingIndex::ReadRuntimeSlots() const
{
    std::vector<SlotRuntimeEntry> result;
    for (const auto &[slot, entry] : slots_) {
        (void)slot;
        result.push_back(entry);
    }
    return result;
}
} // namespace OHOS::Telephony::MultiSim
