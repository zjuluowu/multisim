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
#include "infrastructure/request/runtime_operation_store.h"

namespace OHOS::Telephony::MultiSim {
bool RuntimeOperationStore::TryMarkTerminal(RequestId request, TerminalState terminal)
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

RuntimeOperationStore::ResultAdmission RuntimeOperationStore::AcceptResult(
    StepRef step, EffectKnowledge evidence)
{
    if (step.request.epoch != epoch_ || step.request.owner != owner_) {
        return ResultAdmission::UnknownRequest;
    }
    const auto found = operations_.find(step.request.value);
    if (found == operations_.end()) { return ResultAdmission::UnknownRequest; }
    if (found->second.terminal != TerminalState::Open) { return ResultAdmission::AlreadyTerminal; }
    if (!(found->second.context.step == step)) { return ResultAdmission::WrongStep; }
    if (stepConsumed_[step.request.value]) { return ResultAdmission::Duplicate; }
    stepConsumed_[step.request.value] = true;
    found->second.deviceEffect = evidence;
    return ResultAdmission::Accepted;
}

bool RuntimeOperationStore::AcceptLateEvidence(StepRef step, EffectKnowledge evidence)
{
    if (step.request.epoch != epoch_ || step.request.owner != owner_) { return false; }
    const auto found = operations_.find(step.request.value);
    if (evidence == EffectKnowledge::Unknown || found == operations_.end() ||
        !(found->second.context.step == step) || found->second.terminal != TerminalState::NeedsReconciliation) {
        return false;
    }
    const auto late = lateEffects_.find(step.request.value);
    if (late != lateEffects_.end()) { return false; }
    lateEffects_[step.request.value] = evidence;
    return true;
}

std::optional<EffectKnowledge> RuntimeOperationStore::ReadLateEvidence(RequestId request) const
{
    if (request.epoch != epoch_ || request.owner != owner_) { return std::nullopt; }
    const auto found = lateEffects_.find(request.value);
    return found == lateEffects_.end() ? std::nullopt : std::make_optional(found->second);
}

bool RuntimeOperationStore::ClaimCompletion(RequestId request)
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

bool RuntimeOperationStore::ClaimEarlyCompletion(RequestId request)
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
