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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§9.3、§14.4、§16.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_BOUNDARY_PORTS_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_BOUNDARY_PORTS_H

#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
struct LegacyCallerRef { uint64_t token; }; // Core 保存真实调用者上下文，不在异步线程重新取服务身份。
struct LegacyCallbackRef { uint64_t token; };
struct EntryCheck { bool accepted; int32_t legacyResult; };
enum class LegacySynchronousCall { PrimarySwitch, SimActivation };
// C 所需的Core宿主接缝；Core提供具体实现。不是公开InnerKit API。
class LegacyIngressPort {
public:
    virtual ~LegacyIngressPort() = default;
    virtual EntryCheck CheckSetShowNumberEntry(LegacyCallerRef caller, SlotId slot, LegacyCallbackRef callback) = 0;
    virtual void QueueSetShowNumberTask(const SetShowNumberCommand &command) = 0;
};
class CallbackHostPort {
public:
    virtual ~CallbackHostPort() = default;
    virtual CompletionId RetainCallback(LegacyCallbackRef callback) = 0;
    virtual void TransferOriginalIntResult(CompletionId completion, int32_t legacyResult) = 0;
    virtual SynchronousWaitRef RetainSynchronousWaiter(LegacySynchronousCall call) = 0;
    virtual int32_t AwaitOriginalIntResult(SynchronousWaitRef waiter) = 0;
    virtual void DeliverOriginalInt(const CompletionRoute &route, int32_t legacyResult) = 0;
    // Retain在发起前；可在Await前完成。宿主用原FFRT设施保存各方法原预算/线程，不能阻塞Owner接纳域。
    // 同一受控域内以CompletionCell一次终结，超时/调用者死亡后的结果只核对，不重复交付。
};
// C拥有原完成与通知格式，I适配现有Monitor/平台；没有可写账户或角色缓存。
class LegacyOutputPort {
public:
    virtual ~LegacyOutputPort() = default;
    virtual void NotifyOriginalAccountChanged() = 0;
    virtual void TransferOriginalShowNumberResult(CompletionId completion, int32_t legacyResult) = 0;
    virtual void DeliverOriginalInt(const CompletionRoute &route, int32_t legacyResult) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_BOUNDARY_PORTS_H
