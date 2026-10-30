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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.1、§11、§12.1～12.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H
#define OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H

#include <map>
#include <optional>
#include "foundation/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
// 消费方：T SetShowNumber/ActivateSim，A AssignmentService。每个 Owner 自有容器。
// 不是业务聚合/持久日志，不存账户、角色、进行中标志，不安排重试或调用端口。
class RuntimeOperationStore final {
public:
    enum class ResultAdmission { Accepted, Duplicate, WrongStep, AlreadyTerminal, UnknownRequest };
    RuntimeOperationStore(RuntimeEpoch epoch, RequestOwner owner) : epoch_(epoch), owner_(owner) {}
    RequestId Begin(ResolvedTarget origin, CompletionPlan completion, std::optional<StepRef> parent = std::nullopt);
    bool AdvanceStep(StepRef expected, uint32_t nextStep, uint32_t attempt,
        std::optional<SessionRef> sentSession); // 同域推进，禁止外部直接改Context。
    bool RecordSent(StepRef expected, const SessionRef &sentSession);
    std::optional<RuntimeOperation> Read(RequestId request) const;
    ResultAdmission AcceptResult(StepRef step, EffectKnowledge evidence);
    void EndWait(RequestId request, WaitState reason);
    void MarkTerminal(RequestId request, TerminalState terminal);
    void RetireAfterEvidenceRetention(RequestId request);
private:
    // 由所属 Owner 的短提交域保护；未来实现使用 FFRT，不持锁跨 I/O。
    RuntimeEpoch epoch_;
    RequestOwner owner_;
    std::map<uint64_t, RuntimeOperation> operations_;
    uint64_t nextId_ {1}; // Begin/Read/Accept均核验epoch+owner；无跨重启恢复。
    // Parent等待不占Owner短提交域；同一冲突集合须在唯一准入处一次登记，不能跨Owner持资格嵌套等。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H
