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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.1、§11、§12.1～12.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H
#define OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H

#include <map>
#include <optional>
#include <utility>
#include "infrastructure/request/request_context.h"

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
    bool RecordSent(StepRef expected, const SessionRef &sentSession)
    {
        const auto found = operations_.find(expected.request.value);
        if (expected.request.epoch != epoch_ || expected.request.owner != owner_ || found == operations_.end() ||
            !(found->second.context.step == expected) || found->second.terminal != TerminalState::Open ||
            found->second.context.sentSession) {
            return false;
        }
        found->second.context.sentSession = sentSession;
        return true;
    }
    std::optional<RuntimeOperation> Read(RequestId request) const;
    ResultAdmission AcceptResult(StepRef step, EffectKnowledge evidence);
    bool AcceptLateEvidence(StepRef step, EffectKnowledge evidence);
    std::optional<EffectKnowledge> ReadLateEvidence(RequestId request) const;
    bool ClaimCompletion(RequestId request);
    bool ClaimEarlyCompletion(RequestId request);
    void EndWait(RequestId request, WaitState reason)
    {
        if (request.epoch != epoch_ || request.owner != owner_) { return; }
        const auto found = operations_.find(request.value);
        if (found != operations_.end()) { found->second.wait = reason; }
    }
    void MarkTerminal(RequestId request, TerminalState terminal) { (void)TryMarkTerminal(request, terminal); }
    bool TryMarkTerminal(RequestId request, TerminalState terminal);
    void RetireAfterEvidenceRetention(RequestId request)
    {
        if (request.epoch != epoch_ || request.owner != owner_) { return; }
        const auto found = operations_.find(request.value);
        if (found != operations_.end() && found->second.terminal != TerminalState::Open &&
            found->second.deviceEffect != EffectKnowledge::Unknown) {
            stepConsumed_.erase(request.value);
            completionClaimed_.erase(request.value);
            lateEffects_.erase(request.value);
            operations_.erase(found);
        }
    }
private:
    // 由所属 Owner 的短提交域保护；未来实现使用 FFRT，不持锁跨 I/O。
    RuntimeEpoch epoch_;
    RequestOwner owner_;
    std::map<uint64_t, RuntimeOperation> operations_;
    std::map<uint64_t, bool> stepConsumed_;
    std::map<uint64_t, bool> completionClaimed_;
    std::map<uint64_t, EffectKnowledge> lateEffects_;
    uint64_t nextId_ {1}; // Begin/Read/Accept均核验epoch+owner；无跨重启恢复。
    // Parent等待不占Owner短提交域；同一冲突集合须在唯一准入处一次登记，不能跨Owner持资格嵌套等。
};

inline RequestId RuntimeOperationStore::Begin(ResolvedTarget origin, CompletionPlan completion,
    std::optional<StepRef> parent)
{
    const RequestId request {nextId_++, epoch_, owner_};
    operations_[request.value] = {{StepRef {request, 0, 0}, parent, std::move(origin), std::nullopt,
        std::move(completion)}, EffectKnowledge::NotSent, WaitState::Waiting, TerminalState::Open};
    return request;
}

inline bool RuntimeOperationStore::AdvanceStep(StepRef expected, uint32_t nextStep, uint32_t attempt,
    std::optional<SessionRef> sentSession)
{
    if (expected.request.epoch != epoch_ || expected.request.owner != owner_ || nextStep <= expected.step) {
        return false;
    }
    const auto found = operations_.find(expected.request.value);
    if (found == operations_.end() || !(found->second.context.step == expected) ||
        found->second.terminal != TerminalState::Open) {
        return false;
    }
    found->second.context.step = StepRef {expected.request, nextStep, attempt};
    found->second.context.sentSession = std::move(sentSession);
    found->second.deviceEffect = EffectKnowledge::NotSent;
    stepConsumed_[expected.request.value] = false;
    return true;
}

inline bool RuntimeOperationStore::TryMarkTerminal(RequestId request, TerminalState terminal)
{
    if (request.epoch != epoch_ || request.owner != owner_) { return false; }
    const auto found = operations_.find(request.value);
    if (found == operations_.end()) { return false; }
    const auto current = found->second.terminal;
    if (terminal == TerminalState::Open) { return current == TerminalState::Open; }
    if (current != TerminalState::Open) { return current == terminal; }
    found->second.terminal = terminal;
    return true;
}

inline std::optional<RuntimeOperation> RuntimeOperationStore::Read(RequestId request) const
{
    if (request.epoch != epoch_ || request.owner != owner_) { return std::nullopt; }
    const auto found = operations_.find(request.value);
    return found == operations_.end() ? std::nullopt : std::make_optional(found->second);
}

inline bool RuntimeOperationStore::AcceptLateEvidence(StepRef step, EffectKnowledge evidence)
{
    if (step.request.epoch != epoch_ || step.request.owner != owner_) { return false; }
    const auto found = operations_.find(step.request.value);
    if (found == operations_.end() || !(found->second.context.step == step) ||
        found->second.terminal != TerminalState::NeedsReconciliation) {
        return false;
    }
    lateEffects_[step.request.value] = evidence;
    return true;
}

inline std::optional<EffectKnowledge> RuntimeOperationStore::ReadLateEvidence(RequestId request) const
{
    if (request.epoch != epoch_ || request.owner != owner_) { return std::nullopt; }
    const auto found = lateEffects_.find(request.value);
    return found == lateEffects_.end() ? std::nullopt : std::make_optional(found->second);
}

inline bool RuntimeOperationStore::ClaimCompletion(RequestId request)
{
    if (request.epoch != epoch_ || request.owner != owner_) { return false; }
    const auto found = operations_.find(request.value);
    if (found == operations_.end() || found->second.terminal == TerminalState::Open ||
        completionClaimed_[request.value]) {
        return false;
    }
    completionClaimed_[request.value] = true;
    return true;
}

inline bool RuntimeOperationStore::ClaimEarlyCompletion(RequestId request)
{
    if (request.epoch != epoch_ || request.owner != owner_) { return false; }
    const auto found = operations_.find(request.value);
    if (found == operations_.end() || found->second.terminal != TerminalState::Open ||
        completionClaimed_[request.value]) {
        return false;
    }
    completionClaimed_[request.value] = true;
    return true;
}
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_FOUNDATION_ORCHESTRATION_RUNTIME_OPERATION_STORE_H
