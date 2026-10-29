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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §3、§6.1、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_MODEL_SIM_IDENTITY_H
#define OHOS_TELEPHONY_MULTISIM_MODEL_SIM_IDENTITY_H

#include <array>
#include <cstdint>
#include <string>
#include <variant>

namespace OHOS::Telephony::MultiSim {
// 最小纯值叶子；不是公共 Subscription API，也不是所有端口的 common 容器。
struct SubscriptionId {
    int32_t legacySimId; // 包装旧 simId；不分配新身份，不承诺跨删除/重装稳定。
    bool operator<(const SubscriptionId &other) const { return legacySimId < other.legacySimId; }
    bool operator==(const SubscriptionId &other) const { return legacySimId == other.legacySimId; }
};
struct SlotId {
    int32_t legacySlotId;
    bool operator<(const SlotId &other) const { return legacySlotId < other.legacySlotId; }
    bool operator==(const SlotId &other) const { return legacySlotId == other.legacySlotId; }
};
struct RadioDomainId {
    std::string key; // 真实资源域经 H 提供；不能直接由槽数推导独立性。
    bool operator<(const RadioDomainId &other) const { return key < other.key; }
};
struct SlotTarget { SlotId slot; }; // 空槽也合法。
struct CardApplicationTarget {
    SlotId slot;
    std::string applicationKey; // 已知卡应用路由；不凭空填默认 Port=0。
};
struct SubscriptionTarget { SubscriptionId subscription; }; // 允许离线账户资料。
using ResolvedTarget = std::variant<SlotTarget, CardApplicationTarget, SubscriptionTarget>;
using RuntimeEpoch = std::array<uint64_t, 2>; // H 每次实例创建提供新令牌，非从零重置计数。

// R 拥有代次；纯值契约可跨端口传递，Domain 不拥有运行会话。
struct SessionRef {
    RuntimeEpoch epoch;
    CardApplicationTarget application;
    uint64_t bindingGeneration;
};
struct BindingRef {
    SubscriptionId subscription;
    SlotId slot;
    uint64_t bindingRevision; // T 按绑定键维护；不是全局 TopologyRevision。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_MODEL_SIM_IDENTITY_H
