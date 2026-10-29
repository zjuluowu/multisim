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
enum class EffectRequirement { NoBusinessWrite, VerifyWriteEffect };
struct ResourceKey {
    std::string verifiedPhysicalDomain; // 真实共享Modem/eUICC；普通/低优先级对象共域。
    bool operator<(const ResourceKey &other) const { return verifiedPhysicalDomain < other.verifiedPhysicalDomain; }
};
struct ResourceLease {
    RuntimeEpoch epoch;
    std::vector<ResourceKey> resources;
    StepRef holder;
    uint64_t fence;
    EffectRequirement effectRequirement;
};
struct InitialRecoveryEvidence {
    RuntimeEpoch epoch;
    bool noPriorExecutionCanContinue;
    bool resourceReusable;
};
struct CompletionEvidence {
    StepRef original;
    EffectKnowledge effect;
    bool commandEnded;
    bool resourceReusable; // 包括该命令所需的真实通道关闭证据，不是本地channel=0。
};
struct RecoveryEvidence {
    StepRef original;
    EffectKnowledge effect;
    bool oldCommandEnded;
    bool currentStateVerified;
};
struct ResourceRecord {
    ResourceState state;
    uint64_t fence;
    std::optional<ResourceLease> lease;
    EffectKnowledge effect;
    bool sent;
};
// R CardRuntime 使用，R 统一维护真实资源租约，A 的仲裁规则不替代设备屏障。
// 尚未启用。需证明资源键、成功/错误/投递失败关联及可达恢复证据后才实施。
class ResourceBarrier final {
public:
    explicit ResourceBarrier(RuntimeEpoch epoch) : epoch_(epoch) {}
    bool RegisterForInitialCheck(const ResourceKey &key);
    bool AcceptInitialRecoveryEvidence(const ResourceKey &key, const InitialRecoveryEvidence &evidence);
    std::optional<ResourceLease> Acquire(const std::vector<ResourceKey> &keys, StepRef requester,
        EffectRequirement requirement = EffectRequirement::VerifyWriteEffect);
    bool MarkSent(const ResourceLease &lease);
    bool CancelConfirmedNotSent(const ResourceLease &lease);
    bool Complete(const ResourceLease &lease, const CompletionEvidence &evidence);
    bool Quarantine(const ResourceLease &lease, EffectKnowledge effect);
    bool AcceptRecoveryEvidence(const ResourceLease &lease, const RecoveryEvidence &evidence);
    std::optional<ResourceState> ReadState(const ResourceKey &key) const;
private:
    bool MatchesAll(const ResourceLease &lease, ResourceState state) const;
    void Release(const ResourceLease &lease, EffectKnowledge effect);
    RuntimeEpoch epoch_;
    uint64_t nextFence_ {1}; // 本R实例内唯一；模块重建还须匹配epoch。
    std::map<ResourceKey, ResourceRecord> resources_; // 新实例先Checking；超时不自动Available。
    // 纯转移示例；调用方须在同一R短提交域中执行，锁外才可发设备请求。
    // 证据由命令适配提供；这些bool本身不证明硬件能力，未闭合命令不得启用屏障。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_RESOURCE_RESOURCE_BARRIER_H
