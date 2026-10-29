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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H
#define OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H

#include <optional>
#include <tuple>
#include <variant>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class RequestOwner { Unspecified, ShowNumber, Activation, Assignment, LocalCard, LegacyBridge };
struct RequestId {
    uint64_t value;
    RuntimeEpoch epoch {};
    RequestOwner owner {RequestOwner::Unspecified};
    bool operator==(const RequestId &other) const
    {
        return value == other.value && epoch == other.epoch && owner == other.owner;
    }
    bool operator<(const RequestId &other) const
    {
        return std::tie(epoch, owner, value) < std::tie(other.epoch, other.owner, other.value);
    }
}; // 序号只在实例+用例命名空间有效；R共享屏障不能仅按value识别Owner。
struct CompletionId { uint64_t value; }; // 旧callback索引；不改变IPC。
struct SynchronousWaitRef { uint64_t value; };
struct InternalReplyRef { uint64_t value; };
struct CallbackCompletionRoute { CompletionId callback; };
struct SynchronousCompletionRoute { SynchronousWaitRef waiter; };
struct InternalCompletionRoute { InternalReplyRef receiver; };
using CompletionRoute = std::variant<CallbackCompletionRoute, SynchronousCompletionRoute, InternalCompletionRoute>;
struct StepRef {
    RequestId request;
    uint32_t step;
    uint32_t attempt;
    bool operator==(const StepRef &other) const
    {
        return request == other.request && step == other.step && attempt == other.attempt;
    }
};
enum class EffectKnowledge { NotSent, Rejected, ConfirmedApplied, ConfirmedNotApplied, Unknown };
enum class WaitState { Waiting, Delivered, TimedOut, CancelledWait, CallerDied };
enum class TerminalState { Open, Succeeded, Failed, NeedsReconciliation };
struct CompletionPlan {
    CompletionRoute route;
    bool accountNotificationOnWholeSuccess;
};
struct RequestContext {
    StepRef step;
    std::optional<StepRef> parent; // 跨Owner子请求；原调用可早返回，不等于子请求或设备完成。
    ResolvedTarget originTarget;
    std::optional<SessionRef> sentSession;
    CompletionPlan completion;
    // R/I保存原serial/InnerEvent关联；不得在回包时读取当前会话补造。
};
struct RuntimeOperation {
    RequestContext context;
    EffectKnowledge deviceEffect;
    WaitState wait;
    TerminalState terminal;
};
// 具名int结果出口：A切换与T激活消费，C适配旧同步/异步/内部出口。
// 不含等待、通知重试或通用Execute；等待只在原宿主入口适配层。
class LegacyIntCompletionPort {
public:
    virtual ~LegacyIntCompletionPort() = default;
    virtual void CompleteLegacyInt(const CompletionPlan &plan, int32_t legacyResult) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H
