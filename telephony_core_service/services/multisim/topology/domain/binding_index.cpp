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

namespace OHOS::Telephony::MultiSim {
BindingIndex::BindingIndex(const SlotCatalog &catalog)
{
    for (const auto &descriptor : catalog.Slots()) {
        slots_.emplace(descriptor.slot, SlotRuntimeEntry {descriptor.slot, false, 0, std::nullopt});
    }
}
std::optional<BindingRef> BindingIndex::Find(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it == slots_.end() ? std::nullopt : it->second.acceptedBinding;
}
bool BindingIndex::HasSlot(SlotId slot) const { return slots_.count(slot) != 0; }
uint64_t BindingIndex::Revision(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it == slots_.end() ? 0 : it->second.revision;
}
bool BindingIndex::StillMatches(const BindingRef &expected) const
{
    const auto current = Find(expected.slot);
    return current && current->subscription == expected.subscription &&
        current->bindingRevision == expected.bindingRevision;
}
bool BindingIndex::AcceptBinding(SlotId slot, SubscriptionId subscription, uint64_t expectedRevision)
{
    auto it = slots_.find(slot);
    if (it == slots_.end() || it->second.revision != expectedRevision) { return false; }
    // 一个当前账户不能同时绑定两个位置；移动须在同一 T 短提交中先撤销旧位置。
    for (const auto &[otherSlot, entry] : slots_) {
        if (!(otherSlot == slot) && entry.acceptedBinding && entry.acceptedBinding->subscription == subscription) {
            return false;
        }
    }
    auto &entry = it->second;
    entry.acceptedBinding = BindingRef {subscription, slot, ++entry.revision};
    return true;
}
bool BindingIndex::RemoveBinding(const BindingRef &expected)
{
    if (!StillMatches(expected)) { return false; }
    auto &entry = slots_.at(expected.slot);
    entry.acceptedBinding.reset();
    ++entry.revision;
    return true;
}
std::vector<SlotRuntimeEntry> BindingIndex::ReadRuntimeSlots() const
{
    std::vector<SlotRuntimeEntry> result;
    for (const auto &[slot, entry] : slots_) { result.push_back(entry); }
    return result;
}
} // namespace OHOS::Telephony::MultiSim
