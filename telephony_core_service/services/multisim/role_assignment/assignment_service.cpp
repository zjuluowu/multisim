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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// C0最小组件闭环已实现；非C0异步/激活方法保持显式拒绝，未提供伪成功或设备路径。

#include "role_assignment/assignment_service.h"
#include "role_assignment/primary/primary_switch_plan.h"

#include <algorithm>
#include <stdexcept>
#include "role_assignment/legacy_entry.h"

namespace OHOS::Telephony::MultiSim {
RequestId StartLegacyPrimarySwitch(AssignmentService &entry, const SwitchPrimaryCommand &command)
{ return entry.StartPrimarySwitch(command); }

AssignmentService::AssignmentService(const SlotCatalog &catalog, AssignmentStore &store,
    RadioExecutionPort &radio, AssignmentOutputPort &output, PrimaryTopologyPort &topology,
    LegacyIntCompletionPort &completion, RuntimeEpoch epoch)
    : catalog_(catalog), operations_(epoch, RequestOwner::Assignment), store_(store), radio_(radio),
      output_(output), topology_(topology), completion_(completion) {}

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

void AssignmentService::CompleteOnce(RequestId request, const CompletionPlan &plan, int32_t result,
    TerminalState terminal)
{
    if (!operations_.TryMarkTerminal(request, terminal)) { return; }
    const bool claimed = terminal == TerminalState::Open ? operations_.ClaimEarlyCompletion(request) :
        operations_.ClaimCompletion(request);
    if (claimed) { completion_.CompleteLegacyInt(plan, result); }
}

RequestId AssignmentService::StartPrimarySwitch(const SwitchPrimaryCommand &command)
{
    const RequestId request = operations_.Begin(command.target, command.completion, command.parent);
    if (command.parent && command.parent->request.epoch != request.epoch) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    if (!operations_.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt)) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    const SlotDescriptor *target = catalog_.Find(command.target.slot);
    const auto facts = topology_.ReadPrimaryTarget(command.target.slot);
    const bool expectedBindingUnchanged = facts && command.expectedBinding &&
        command.expectedBinding->subscription == facts->binding.subscription &&
        command.expectedBinding->slot == facts->binding.slot &&
        command.expectedBinding->bindingRevision == facts->binding.bindingRevision;
    if (!target || !target->radioDomain || !facts || !facts->active ||
        !(facts->binding.slot == command.target.slot) || !expectedBindingUnchanged || facts->iccid.empty()) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }

    const PrimaryRadioObservation observed = radio_.ReadPrimaryRadioObservation(command.target.slot);
    const auto refreshedFacts = topology_.ReadPrimaryTarget(command.target.slot);
    if (!refreshedFacts || !(refreshedFacts->binding.subscription == facts->binding.subscription) ||
        !(refreshedFacts->binding.slot == facts->binding.slot) ||
        refreshedFacts->binding.bindingRevision != facts->binding.bindingRevision ||
        refreshedFacts->iccid != facts->iccid || refreshedFacts->active != facts->active) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    const bool validObservation = observed.slot == command.target.slot && observed.observationRevision != 0 &&
        observed.currentModemId.has_value() && target->rilInstance.has_value();
    if (!validObservation) {
        // Unknown is not M0. Other primary paths remain outside this C0 host closure.
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    const bool alreadyOnPrimaryModem = *observed.currentModemId == 0 &&
        *target->rilInstance == *observed.currentModemId;
    const auto plan = PlanPrimarySwitch({*target, command.userSet, observed.dedicatedPrimaryRilSupported,
        alreadyOnPrimaryModem, observed.vendorMappingRequired});
    if (!plan || (plan->mode != PrimarySwitchMode::AlreadyPrimary &&
        plan->mode != PrimarySwitchMode::WithoutModemRestart &&
        plan->mode != PrimarySwitchMode::ThreePhaseProtocol)) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    if (assignment_.AdmitTransitionWithRoleWriteSet(*target, request.value, kPrimaryRoleWriteSet) !=
        TransitionAdmission::Admitted) {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed);
        return request;
    }
    if (plan->mode == PrimarySwitchMode::WithoutModemRestart) {
        const auto preference = store_.WriteUserPrimaryPreference(command.userSet);
        if (!preference.callFinished) {
            assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::Admitted, TransitionPhase::Reconciling);
            operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
            return request;
        }
        const SessionRef session {request.epoch,
            CardApplicationTarget {target->slot, "USIM"}, facts->binding.bindingRevision};
        const StepRef sentStep {request, 2, 0};
        if (!operations_.AdvanceStep({request, 1, 0}, 2, 0, std::nullopt) ||
            !operations_.RecordSent(sentStep, session) ||
            !assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::Admitted, TransitionPhase::DevicePending)) {
            CompleteOnce(request, command.completion, -1, TerminalState::Failed);
            assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
            return request;
        }
        pendingPrimary_.emplace(request.value, PendingPrimarySwitch {sentStep, *facts, *target,
            facts->iccid, command.userSet, command.completion, plan->mode, 0,
            nextProtocolSession_++, {}, {}, {}, false, 0, false, false});
        output_.PublishPrimaryPhase(false, command.userSet);
        radio_.SwitchPrimaryRadio({sentStep, target->slot, *target->radioDomain, plan->mode, facts->binding,
            pendingPrimary_.at(request.value).protocolSession, 0});
        return request;
    }
    if (plan->mode == PrimarySwitchMode::ThreePhaseProtocol) {
        const PrimaryParameterSnapshot oldParameters = store_.ReadPrimaryParameters();
        const auto preference = store_.WriteUserPrimaryPreference(command.userSet);
        const auto primary = store_.WritePrimaryParameters(target->slot, facts->iccid);
        if (!preference.callFinished || !primary.callFinished) {
            assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::Admitted, TransitionPhase::Reconciling);
            operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
            return request;
        }
        assignment_.AcceptPrimaryParameterProjection(target->slot);
        assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
            TransitionPhase::Admitted, TransitionPhase::ParametersWritten);
        output_.PublishPrimaryPhase(false, false); // Legacy C1 status event ignores the public input flag.
        const SessionRef session {request.epoch,
            CardApplicationTarget {target->slot, "USIM"}, facts->binding.bindingRevision};
        const StepRef sentStep {request, 2, 0};
        if (!operations_.AdvanceStep({request, 1, 0}, 2, 0, std::nullopt) ||
            !operations_.RecordSent(sentStep, session) ||
            !assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::ParametersWritten, TransitionPhase::DevicePending)) {
            CompleteOnce(request, command.completion, -1, TerminalState::Failed);
            assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
            return request;
        }
        PendingPrimarySwitch pending {sentStep, *facts, *target, facts->iccid, command.userSet,
            command.completion, plan->mode, 1, nextProtocolSession_++, {}, {}, oldParameters,
            false, 0, false, false};
        for (const auto &slot : catalog_.Slots()) { pending.members.push_back(slot.slot); }
        pendingPrimary_.emplace(request.value, std::move(pending));
        for (const auto &member : pendingPrimary_.at(request.value).members) {
            radio_.SwitchPrimaryRadio({sentStep, member, *target->radioDomain, plan->mode,
                member == target->slot ? std::optional<BindingRef>{facts->binding} : std::nullopt,
                pendingPrimary_.at(request.value).protocolSession, 1});
        }
        return request;
    }

    const auto retainUnknownEffect = [&](TransitionPhase expected) {
        assignment_.AcceptTransitionPhase(*target->radioDomain, request.value, expected,
            TransitionPhase::Reconciling);
        operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
    };

    const auto preference = store_.WriteUserPrimaryPreference(command.userSet);
    if (!preference.callFinished) {
        retainUnknownEffect(TransitionPhase::Admitted);
        return request;
    }
    // Legacy Core ignores completed SetParameter failure for this user preference hint.
    const auto primary = store_.WritePrimaryParameters(target->slot, facts->iccid);
    if (!primary.callFinished) {
        retainUnknownEffect(TransitionPhase::Admitted);
        return request;
    }
    // Legacy SavePrimaryCardInfo records in-memory/output role despite completed parameter failure.
    assignment_.AcceptPrimaryParameterProjection(target->slot);
    assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
        TransitionPhase::Admitted, TransitionPhase::ParametersWritten);
    output_.PublishMainCardChanged(target->slot);

    const auto data = store_.WriteDefaultData(target->slot);
    if (!data.callFinished) {
        retainUnknownEffect(TransitionPhase::ParametersWritten);
        return request;
    }
    // Legacy SetDefaultCellularDataSlotId also returns success after a void parameter write.
    assignment_.AcceptDataParameterProjection(target->slot);
    output_.PublishDefaultDataChanged(target->slot, facts->binding.subscription);
    assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
        TransitionPhase::ParametersWritten, TransitionPhase::Completed);
    CompleteOnce(request, command.completion, 0, TerminalState::Succeeded);
    assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
    return request;
}

AssignmentView AssignmentService::ReadLegacyAssignment() const
{
    AssignmentView view {};
    const auto &slots = catalog_.Slots();
    for (const auto &slot : slots) {
        if (slot.radioDomain && assignment_.IsSwitchInProgress(*slot.radioDomain)) {
            view.primarySwitchInProgress = true;
        }
    }
    view.lastPrimary = assignment_.ReadLastPrimary();
    view.lastData = assignment_.ReadLastData();
    view.assignmentRevision = assignment_.Revision();
    return view;
}

int32_t AssignmentService::SetDefaultRole(const SetRoleCommand &command)
{
    const uint32_t members = command.role == DefaultRole::Data ? kDefaultDataWriteSet :
        command.role == DefaultRole::Voice ? kVoiceParameters : kSmsParameters;
    const uint64_t ownerToken = ~uint64_t {0} - nextConflictToken_++;
    if (!assignment_.AdmitRoleWriteSet(ownerToken, members)) { return -1; }
    const auto result = store_.WriteDefaultRole(command);
    if (result.callFinished && result.writeConfirmed && result.legacyResult == 0) {
        assignment_.AcceptRoleSelection({command.role, command.subscription, command.subscription},
            assignment_.Revision());
    }
    if (result.callFinished) { assignment_.ReleaseRoleWriteSet(ownerToken, members); }
    return result.legacyResult;
}

void AssignmentService::AcceptRadioStep(const RadioSwitchResult &result)
{
    const auto pending = pendingPrimary_.find(result.step.request.value);
    if (pending == pendingPrimary_.end()) { return; }
    const auto operation = operations_.Read(result.step.request);
    if (!assignment_.OwnsRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet) ||
        !operation || operation->terminal != TerminalState::Open ||
        !(pending->second.step == result.step) || !operation->context.sentSession ||
        result.protocolSession != pending->second.protocolSession) {
        return;
    }
    if (pending->second.mode == PrimarySwitchMode::WithoutModemRestart) {
        if (!(result.memberSlot == pending->second.target.slot) || !result.expectedBinding ||
            !(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
            result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision) {
            return;
        }
    } else if (result.phase != pending->second.phase ||
        (pending->second.phase == 2 && !result.notify) ||
        (result.expectedBinding &&
         (!(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
          result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision)) ||
        std::find(pending->second.members.begin(), pending->second.members.end(), result.memberSlot) ==
            pending->second.members.end() ||
        std::find(pending->second.completedMembers.begin(), pending->second.completedMembers.end(), result.memberSlot) !=
            pending->second.completedMembers.end()) {
        return;
    }
    if (pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        if (result.effect == EffectKnowledge::Unknown) {
            operations_.AcceptResult(result.step, result.effect);
            operations_.EndWait(result.step.request, WaitState::Delivered);
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
            output_.PublishPrimaryPhase(true, false);
            CompleteOnce(result.step.request, pending->second.completion, 83886087,
                TerminalState::NeedsReconciliation);
            return;
        }
        pending->second.roundFailed |= result.effect != EffectKnowledge::ConfirmedApplied || result.legacyResult != 0;
        pending->second.completedMembers.push_back(result.memberSlot);
        if (pending->second.completedMembers.size() != pending->second.members.size()) { return; }
    }
    if (pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        pending->second.completedMembers.clear();
        if (pending->second.phase != 4) {
            const uint32_t nextPhase = pending->second.roundFailed ? 4 :
                pending->second.phase == 1 ? 2 : 4;
            const uint32_t nextStep = pending->second.step.step + 1;
            if (!operations_.AdvanceStep(pending->second.step, nextStep,
                pending->second.step.attempt, std::nullopt)) { return; }
            pending->second.step.step = nextStep;
            const SessionRef session {pending->second.step.request.epoch,
                CardApplicationTarget {pending->second.target.slot, "USIM"},
                pending->second.facts.binding.bindingRevision};
            if (!operations_.RecordSent(pending->second.step, session)) { return; }
            pending->second.phase = nextPhase;
            for (const auto &member : pending->second.members) {
                radio_.SwitchPrimaryRadio({pending->second.step, member, pending->second.target.radioDomain.value(),
                    pending->second.mode,
                    member == pending->second.target.slot ?
                        std::optional<BindingRef>{pending->second.facts.binding} : std::nullopt,
                    pending->second.protocolSession, pending->second.phase});
            }
            return;
        }
    }
    const auto admission = operations_.AcceptResult(result.step, result.effect);
    if (admission != RuntimeOperationStore::ResultAdmission::Accepted) { return; }
    operations_.EndWait(result.step.request, WaitState::Delivered);
    radio_.EndLegacySenseSwitch(pending->second.step, pending->second.target.radioDomain.value());
    if (pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol && pending->second.roundFailed) {
        const auto restored = store_.RestorePrimaryIfLegacyValid(pending->second.oldParameters);
        if (!restored.callFinished) {
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
            operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
            return;
        }
        for (const auto &slot : catalog_.Slots()) {
            if (std::to_string(slot.slot.legacySlotId) == pending->second.oldParameters.slotText) {
                assignment_.AcceptPrimaryParameterProjection(slot.slot);
                break;
            }
        }
        output_.PublishPrimaryPhase(true, false);
        if (pending->second.attemptCount == 0) {
            CompleteOnce(result.step.request, pending->second.completion, 83886087, TerminalState::Open);
            pending->second.completionDelivered = true;
            operations_.EndWait(result.step.request, WaitState::Delivered);
        }
        if (pending->second.attemptCount < 5) {
            ++pending->second.attemptCount;
            pending->second.retryReady = true;
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::Reconciling);
            operations_.MarkTerminal(result.step.request, TerminalState::Open);
            return;
        }
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
            TransitionPhase::DevicePending, TransitionPhase::Failed);
        operations_.MarkTerminal(result.step.request, TerminalState::Failed);
        assignment_.ReleaseRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet);
        pendingPrimary_.erase(pending);
        return;
    }
    const auto currentFacts = topology_.ReadPrimaryTarget(pending->second.target.slot);
    const bool bindingCurrent = currentFacts &&
        currentFacts->binding.subscription == pending->second.facts.binding.subscription &&
        currentFacts->binding.slot == pending->second.facts.binding.slot &&
        currentFacts->binding.bindingRevision == pending->second.facts.binding.bindingRevision &&
        currentFacts->iccid == pending->second.facts.iccid;
    if (result.effect == EffectKnowledge::ConfirmedApplied && result.legacyResult == 0 && bindingCurrent) {
        const bool prewritten = pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol;
        const auto primary = prewritten ? AssignmentParameterResult {0, true} :
            store_.WritePrimaryParameters(pending->second.target.slot, pending->second.iccid);
        if (!primary.callFinished) {
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
                result.step.request.value, TransitionPhase::DevicePending, TransitionPhase::Reconciling);
            operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
            pendingPrimary_.erase(pending);
            return;
        }
        assignment_.AcceptPrimaryParameterProjection(pending->second.target.slot);
        output_.PublishMainCardChanged(pending->second.target.slot);
        const auto data = store_.WriteDefaultData(pending->second.target.slot);
        if (!data.callFinished) {
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
                result.step.request.value, TransitionPhase::DevicePending, TransitionPhase::Reconciling);
            operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
            pendingPrimary_.erase(pending);
            return;
        }
        assignment_.AcceptDataParameterProjection(pending->second.target.slot);
        output_.PublishDefaultDataChanged(pending->second.target.slot,
            pending->second.facts.binding.subscription);
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
            result.step.request.value, TransitionPhase::DevicePending, TransitionPhase::Completed);
        output_.PublishPrimaryPhase(true,
            pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol ? false : pending->second.userSet);
        CompleteOnce(result.step.request, pending->second.completion, 0, TerminalState::Succeeded);
        assignment_.ReleaseRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet);
        pendingPrimary_.erase(pending);
        operations_.RetireAfterEvidenceRetention(result.step.request);
        return;
    }
    const bool needsReconciliation = result.effect == EffectKnowledge::Unknown ||
        (result.effect == EffectKnowledge::ConfirmedApplied && !bindingCurrent) ||
        (result.effect == EffectKnowledge::ConfirmedApplied && result.legacyResult != 0);
    if (needsReconciliation && !bindingCurrent &&
        pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        const auto restored = store_.RestorePrimaryIfLegacyValid(pending->second.oldParameters);
        if (restored.callFinished) {
            for (const auto &slot : catalog_.Slots()) {
                if (std::to_string(slot.slot.legacySlotId) == pending->second.oldParameters.slotText) {
                    assignment_.AcceptPrimaryParameterProjection(slot.slot);
                    break;
                }
            }
        }
    }
    assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
        TransitionPhase::DevicePending, needsReconciliation ? TransitionPhase::NeedsReconciliation :
        TransitionPhase::Failed);
    output_.PublishPrimaryPhase(true, pending->second.userSet);
    const int32_t legacyResult = result.effect == EffectKnowledge::Unknown ? 83886087 : result.legacyResult;
    CompleteOnce(result.step.request, pending->second.completion, legacyResult,
        needsReconciliation ? TerminalState::NeedsReconciliation : TerminalState::Failed);
    if (!needsReconciliation) {
        assignment_.ReleaseRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet);
        pendingPrimary_.erase(pending);
        operations_.RetireAfterEvidenceRetention(result.step.request);
    } else {
        // Keep the original StepRef/binding/effect qualification for late reconciliation;
        // no new same-domain transition may bypass this record.
    }
}

void AssignmentService::AcceptLateRadioEvidence(const RadioSwitchResult &result)
{
    const auto pending = pendingPrimary_.find(result.step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == result.step)) { return; }
    const auto operation = operations_.Read(result.step.request);
    if (!assignment_.OwnsRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet) ||
        !operation || operation->terminal != TerminalState::NeedsReconciliation) { return; }
    if (!result.expectedBinding ||
        !(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
        result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision ||
        result.protocolSession != pending->second.protocolSession ||
        !(result.memberSlot == pending->second.target.slot)) {
        return;
    }
    // Late evidence is recorded separately from the original completion/step.
    // It does not replay completion, role submission, notification, or commands.
    operations_.AcceptLateEvidence(result.step, result.effect);
}

bool AssignmentService::AcceptC1RetryDue(RequestId request)
{
    const auto pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end() || !pending->second.retryReady || pending->second.phase != 4 ||
        pending->second.mode != PrimarySwitchMode::ThreePhaseProtocol) {
        return false;
    }
    const auto operation = operations_.Read(request);
    if (!assignment_.OwnsRoleWriteSet(request.value, kPrimaryRoleWriteSet) ||
        !operation || operation->terminal != TerminalState::Open || pending->second.attemptCount > 5) {
        return false;
    }
    pending->second.retryReady = false;
    const auto current = topology_.ReadPrimaryTarget(pending->second.target.slot);
    if (!current || current->binding.bindingRevision != pending->second.facts.binding.bindingRevision ||
        current->iccid != pending->second.facts.iccid || !current->active) {
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), request.value,
            TransitionPhase::Reconciling, TransitionPhase::NeedsReconciliation);
        operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
        return true;
    }
    const auto primary = store_.WritePrimaryParameters(pending->second.target.slot, pending->second.facts.iccid);
    if (!primary.callFinished) {
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), request.value,
            TransitionPhase::Reconciling, TransitionPhase::NeedsReconciliation);
        operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
        return true;
    }
    const uint32_t nextStep = pending->second.step.step + 1;
    if (!operations_.AdvanceStep(pending->second.step, nextStep, pending->second.attemptCount, std::nullopt)) {
        return false;
    }
    pending->second.step.step = nextStep;
    pending->second.step.attempt = pending->second.attemptCount;
    pending->second.protocolSession = nextProtocolSession_++;
    const SessionRef session {request.epoch, CardApplicationTarget {pending->second.target.slot, "USIM"},
        pending->second.facts.binding.bindingRevision};
    if (!operations_.RecordSent(pending->second.step, session) ||
        !assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), request.value,
            TransitionPhase::Reconciling, TransitionPhase::DevicePending)) {
        return false;
    }
    pending->second.phase = 1;
    pending->second.roundFailed = false;
    pending->second.completedMembers.clear();
    output_.PublishPrimaryPhase(false, false);
    for (const auto &member : pending->second.members) {
        radio_.SwitchPrimaryRadio({pending->second.step, member, pending->second.target.radioDomain.value(),
            PrimarySwitchMode::ThreePhaseProtocol,
            member == pending->second.target.slot ?
                std::optional<BindingRef>{pending->second.facts.binding} : std::nullopt,
            pending->second.protocolSession, 1});
    }
    return true;
}

bool AssignmentService::AcceptPrimarySwitchTimeout(RequestId request, uint64_t elapsedMilliseconds)
{
    static constexpr uint64_t kLegacyPrimaryTimeoutMs = 45000;
    if (elapsedMilliseconds < kLegacyPrimaryTimeoutMs) { return false; }
    const auto pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end()) { return false; }
    const auto operation = operations_.Read(request);
    if (!assignment_.OwnsRoleWriteSet(request.value, kPrimaryRoleWriteSet) ||
        !operation || operation->terminal != TerminalState::Open ||
        !(pending->second.step.request == request)) { return false; }
    operations_.EndWait(request, WaitState::TimedOut);
    operations_.AcceptResult(pending->second.step, EffectKnowledge::Unknown);
    assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), request.value,
        TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
    radio_.EndLegacySenseSwitch(pending->second.step, pending->second.target.radioDomain.value());
    output_.PublishPrimaryPhase(true, pending->second.userSet);
    CompleteOnce(request, pending->second.completion, 83886087, TerminalState::NeedsReconciliation);
    return true;
}
void AssignmentService::RetryPrimaryAfterReconciliation(RequestId)
{ throw std::logic_error("C0 Target component slice does not schedule retries"); }
LegacyActivationWaitResult AssignmentService::WaitForLegacyPrimaryBeforeActivation(SlotId)
{ throw std::logic_error("C0 Target component slice does not implement activation waiting"); }
std::optional<ActivationConflictLease> AssignmentService::AdmitActivationBeforeDevice(const ActivationConflictRequest &)
{ throw std::logic_error("C0 Target component slice does not implement activation admission"); }
bool AssignmentService::ReleaseActivationConflict(const ActivationConflictLease &, const ActivationConflictEndEvidence &)
{ throw std::logic_error("C0 Target component slice does not implement activation release"); }
bool AssignmentService::TransferActivationConflictToReconciliation(const ActivationConflictLease &, EffectKnowledge)
{ throw std::logic_error("C0 Target component slice does not implement activation reconciliation"); }
void AssignmentService::AcceptEntryPrimaryPreference(bool userSet) { store_.WriteUserPrimaryPreference(userSet); }
void AssignmentService::RecheckPrimaryAfterAccountChange(const PrimaryRecheck &)
{ throw std::logic_error("C0 Target component slice does not implement account rechecks"); }
void AssignmentService::AcceptLocalProfilePreference(const StepRef &, SlotId)
{
    store_.WriteUserPrimaryPreference(true); // 原File返回后写入；参数失败不覆盖旧双结果。
}
std::optional<AccountRoleFragment> AssignmentService::PrepareAccountRoleFragment(const AccountRolePlanningInput &input)
{ return accountRoles_.Prepare(input); }
bool AssignmentService::AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned)
{ return accountRoles_.AcceptDiscovered(planned, assigned); }
bool AssignmentService::StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const
{ return accountRoles_.StillOwns(planned); }
bool AssignmentService::FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
    const InitialRoleFinalizationEvidence &evidence)
{ return accountRoles_.Finalize(planned, evidence); }
uint64_t AssignmentService::ReadRoleProjectionRevision(bool historicalView) const
{ return accountRoles_.Revision(historicalView); }
bool AssignmentService::AcceptReloadedRoleProjectionSet(const std::vector<StoredAccountRoleFragment> &loaded,
    uint64_t loadAttempt, uint64_t expectedRevision, bool historicalView)
{ return accountRoles_.AcceptReloadSet(loaded, loadAttempt, expectedRevision, historicalView); }
bool AssignmentService::AcceptReloadedRoleProjection(const StoredAccountRoleFragment &loaded,
    uint64_t, uint64_t expectedRevision, bool historicalView)
{ return accountRoles_.AcceptSingleProjection(loaded, expectedRevision, historicalView); }
/* 仅声明方法的流程契约：
 * StartPrimarySwitch在原执行点读取T窄事实与R动态PrimaryRadioObservation，不从目录推断当前分配。
 * 同一请求的全局角色/Primary参数域与真实Radio冲突集在唯一准入处一次登记；跨Owner子请求带parent。
 * 不在A持资格同步等T；第三卡映射返回子请求/原早返回轨迹，后续事件再推进，不经CMI自调用。
 * PlanPrimarySwitch按原分支优先级选择：第三卡映射 -> 用户专用RIL -> 已M0 -> 普通三阶段。
 * ThreePhaseProtocol: 进度/SenseSwitch -> 参数/last预写 -> R三阶段 -> 原广播 -> Data -> Done。
 * WithoutModemRestart: 原开始事件 -> R共享RIL专用SetPrimarySlot -> 成功才保存/联动Data；失败仍Done(true)。
 * AlreadyPrimary: 原保存/联动Data，不发送设备协议；VendorRouteMapping经T子请求与原厂商端口。
 * 成功/错误都先按epoch/owner/request/step/attempt、原session/phase/member准入，再修改阶段/计数。
 * Done与EndLegacySenseSwitch核对原请求资格，旧完成不能清新transition；阶段结束不是设备成功。
 * completion_.CompleteLegacyInt在各旧返回点交C；同步等待在宿主适配层，不占A提交域。
 * 失败按旧参数条件恢复；恢复不等于设备回滚；Unknown交R，唯一业务重试由A授权。
 * G2保留原入口/预算/重试；新准入和隔离是G3c行为增强，不由本骨架宣称已启用。
 *
 * WaitForLegacyPrimaryBeforeActivation仅保留G2原有界等待/超时继续，不创建G3资格。
 * G3 AdmitActivationBeforeDevice以原step+sentSession在assignmentConflicts_和Primary同处核验并登记
 * 完整冲突集，返回ActivationConflictLease；等待/拒绝需方法级行为验收，不隐含新增生产策略。
 * ReleaseActivationConflict只对匹配lease且MayReleaseActivationConflict证据成立的登记释放；
 * Unknown或结束未证时TransferActivationConflictToReconciliation保留同一token/原归属，T进度清零不释放。
 *
 * PrepareAccountRoleFragment按发现上下文预留角色域，未知simId保持optional；原Insert返回id后
 * AcceptDiscoveredRoleProjection将计划绑定该id。T发I/O前及返回后经StillOwnsInitialRoleReservation复核。
 * FinalizeInitialRoleReservation只在MayFinalizeInitialRoles成立且token/角色域资格匹配时释放；
 * DB已写但观察失效、Unknown或投影尚未接纳/核对均保留原资格，不能仅凭callFinished解锁。
 * current/all角色载荷来自原两读源，成功整次读取用AcceptReloadedRoleProjectionSet独立替换；
 * 失败不调用/不清空，原setter只改current并同步accountRoles_.NoteLocalRoleMutation。
 * SetDefaultRole等实际提交均更新这个唯一角色资格；不以当前A状态反填DB结果。
 * 入口/eSIM偏好均在原指定节点交A；迁移关闭旧参数直写、恢复、重试事件及detached CMI自调用。
 */
} // namespace OHOS::Telephony::MultiSim
