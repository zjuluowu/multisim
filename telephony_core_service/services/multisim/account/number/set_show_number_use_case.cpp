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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.3、§9.3～9.5、§11.1～11.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "account/number/set_show_number_use_case.h"

namespace OHOS::Telephony::MultiSim {
SetShowNumberUseCase::SetShowNumberUseCase(TopologyService &topology, ShowNumberRuntimePort &runtime,
    ShowNumberCompletionPort &completion, RuntimeEpoch epoch)
    : topology_(topology), runtime_(runtime), completion_(completion), operations_(epoch, RequestOwner::ShowNumber) {}

void SetShowNumberUseCase::StartAtExecutionPoint(const SetShowNumberCommand &command, const NumberLegacyErrors &errors)
{
    const auto session = topology_.ReadNumberSession(command.origin.slot);
    if (!session) {
        completion_.CompleteShowNumber({command.completion, errors.noCard, EffectKnowledge::NotSent, {}});
        return;
    }
    const auto card = runtime_.ReadMsisdnCache(*session);
    const auto target = topology_.PrepareNumberTarget(command.origin, *session);
    if (!card || !target) {
        completion_.CompleteShowNumber({command.completion, card ? errors.noCard : errors.unavailable,
            EffectKnowledge::NotSent, {}});
        return;
    }
    StepRef step;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto request = operations_.Begin(command.origin, command.completion);
        step = {request, 1, 0};
        operations_.AdvanceStep({request, 0, 0}, step.step, step.attempt, *session);
        pending_.emplace(request.value, PendingNumber {*target, command.number, errors});
    }
    runtime_.WriteMsisdn({step, *session, card->alphaTag, command.number});
}

void SetShowNumberUseCase::AcceptMsisdnResult(const MsisdnWriteResult &result)
{
    std::optional<PendingNumber> pending;
    std::optional<CompletionPlan> completion;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto entry = pending_.find(result.step.request.value);
        const auto operation = operations_.Read(result.step.request);
        if (entry == pending_.end() || !operation || !(entry->second.target.session == result.sentSession) ||
            operations_.AcceptResult(result.step, result.effect) != RuntimeOperationStore::ResultAdmission::Accepted) {
            return;
        }
        pending = std::move(entry->second);
        pending_.erase(entry);
        completion = operation->context.completion;
    }
    int32_t legacyResult = pending->errors.failed;
    std::optional<NumberAcceptance> account;
    if (result.legacyWaitSucceeded && result.effect == EffectKnowledge::ConfirmedApplied) {
        account = topology_.AcceptNumberAfterCardWrite(pending->target, pending->number, result.step);
        switch (account->acceptance) {
            case AccountMetadataResult::Success: legacyResult = 0; break;
            case AccountMetadataResult::DatabaseWriteFailed: legacyResult = pending->errors.databaseWriteFailed; break;
            case AccountMetadataResult::HistoricalMissing: legacyResult = pending->errors.historicalMissing; break;
            default: break;
        }
    }
    {
        std::lock_guard<std::mutex> guard(mutex_);
        operations_.MarkTerminal(result.step.request, result.effect == EffectKnowledge::Unknown ?
            TerminalState::NeedsReconciliation : (legacyResult == 0 ? TerminalState::Succeeded : TerminalState::Failed));
        if (!operations_.ClaimCompletion(result.step.request)) { return; }
        operations_.RetireAfterEvidenceRetention(result.step.request);
    }
    completion_.CompleteShowNumber({*completion, legacyResult, result.effect, account});
}

void SetShowNumberUseCase::AcceptLateEvidence(const MsisdnWriteResult &result)
{
    std::lock_guard<std::mutex> guard(mutex_);
    const auto operation = operations_.Read(result.step.request);
    if (operation && operation->context.sentSession == result.sentSession &&
        operations_.AcceptLateEvidence(result.step, result.effect)) {
        operations_.RetireAfterEvidenceRetention(result.step.request);
    }
}
} // namespace OHOS::Telephony::MultiSim
