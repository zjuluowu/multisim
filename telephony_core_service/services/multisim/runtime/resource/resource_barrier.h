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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §12.3～12.5、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_RESOURCE_RESOURCE_BARRIER_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_RESOURCE_RESOURCE_BARRIER_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
enum class ResourceState { Checking, Available, InFlight, Quarantined };
struct ResourceKey {
    std::string verifiedPhysicalDomain; // 真实共享Modem/eUICC；普通/低优先级对象共域。
    bool operator<(const ResourceKey &other) const { return verifiedPhysicalDomain < other.verifiedPhysicalDomain; }
};
enum class ResourceUse { Query, Mutation };
struct ResourceLease { std::vector<ResourceKey> resources; StepRef holder; uint64_t fence; ResourceUse use; };
struct CheckingTicket { ResourceKey resource; uint64_t fence; };
struct InitialResourceEvidence {
    bool oldCommandEnded;
    bool routeVerified;
    bool resourceReusable;
};
struct CompletionEvidence {
    StepRef original;
    bool commandEnded;
    bool requiredChannelClosed;
    bool resourceReusable;
    EffectKnowledge effect;
};
struct RecoveryEvidence {
    StepRef original;
    bool oldCommandEnded;
    bool currentStateVerified; // 写命令须核对相关事实；查询不要求恢复旧响应载荷。
    bool resourceReusable;
};
struct ResourceRecord {
    ResourceState state;
    uint64_t fence;
    std::optional<ResourceLease> lease;
    EffectKnowledge effect;
};
// R 唯一资源准入者；纯状态机可在主机验证，不提供设备已结束的生产证据。
// 全部入口由R原短执行域串行保护；本类不另建同步域。离开短域后才发令或等待。
class ResourceBarrier final {
public:
    CheckingTicket TrackResource(const ResourceKey &key); // 首见Checking，重复调用不重置活动租约。
    bool AcceptInitialEvidence(const CheckingTicket &ticket, const InitialResourceEvidence &evidence);
    std::optional<ResourceLease> Acquire(const std::vector<ResourceKey> &keys, StepRef requester, ResourceUse use);
    bool MarkSent(const ResourceLease &lease);
    bool CancelNotSent(const ResourceLease &lease);
    bool Complete(const ResourceLease &lease, const CompletionEvidence &evidence);
    bool Quarantine(const ResourceLease &lease, EffectKnowledge effect);
    bool AcceptRecoveryEvidence(const ResourceLease &lease, const RecoveryEvidence &evidence);
    ResourceState ReadState(const ResourceKey &key) const;
private:
    bool Matches(const ResourceLease &lease, ResourceState required) const;
    void Release(const ResourceLease &lease);
    uint64_t nextFence_ {0};
    std::map<ResourceKey, ResourceRecord> resources_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_RESOURCE_RESOURCE_BARRIER_H
