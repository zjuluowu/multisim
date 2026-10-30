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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.1、§11～12。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H
#define OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H

#include <optional>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
struct RequestId { uint64_t value; };
struct CompletionId { uint64_t value; }; // C 管理真实 callback 的索引；不改变 IPC。
struct StepRef {
    RequestId request;
    uint32_t step;
    uint32_t attempt;
};
enum class EffectKnowledge { NotSent, Rejected, ConfirmedApplied, ConfirmedNotApplied, Unknown };
enum class WaitState { Waiting, Delivered, TimedOut, CancelledWait, CallerDied };
enum class TerminalState { Open, Succeeded, Failed, NeedsReconciliation };
enum class CompletionKind { SynchronousReturn, AsynchronousCallback, InternalReceipt };
struct CompletionPlan {
    CompletionId completion;
    bool accountNotificationOnWholeSuccess;
    CompletionKind kind {CompletionKind::AsynchronousCallback};
};
struct RequestContext {
    StepRef step;
    ResolvedTarget originTarget;
    std::optional<SessionRef> sentSession;
    CompletionPlan completion;
    // R/I 保留实际 serial / InnerEvent；不得在回包时用当前会话补造关联。
};
// 仅用例私有过程状态，T/A 各自持有；没有跨业务查询入口或全局 Coordinator。
struct RuntimeOperation {
    RequestContext context;
    EffectKnowledge deviceEffect;
    WaitState wait;
    TerminalState terminal;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_APPLICATION_ORCHESTRATION_REQUEST_CONTEXT_H
