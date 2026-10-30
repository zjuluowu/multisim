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
};
// Core/CMI的同步等待与应用接纳执行域分开；不得在完成所需串行域内等待。
// 实际宿主等待用既有FFRT设施；CompletionCell只表达串行域内一次完成，不内置同步原语。
enum class LegacySynchronousMethod { PrimarySwitch, OrdinaryActivation };
class CompletionHostPort {
public:
    virtual ~CompletionHostPort() = default;
    // 创建唯一活跃CompletionId与SynchronousReturn出口，先建立再启动，避免先完成丢失。
    virtual CompletionPlan OpenSynchronousResult(LegacySynchronousMethod method) = 0;
    // 保持各旧分支预算/错误：第三卡早返回不等到后续自动Primary完成。
    virtual int32_t AwaitOriginalSynchronousResult(const CompletionPlan &plan) = 0;
    // 按plan.kind交原同步cell、callback或内部receipt；一次终结，晚到仅供核对。
    virtual void DeliverOriginalResult(const CompletionPlan &plan, int32_t legacyResult) = 0;
};
// C拥有原完成与通知格式，I适配现有Monitor/平台；没有可写账户或角色缓存。
class LegacyOutputPort {
public:
    virtual ~LegacyOutputPort() = default;
    virtual void NotifyOriginalAccountChanged() = 0;
    virtual void TransferOriginalShowNumberResult(CompletionId completion, int32_t legacyResult) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_BOUNDARY_PORTS_H
