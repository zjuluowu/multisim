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
#include <set>
#include <stdexcept>
#include "role_assignment/legacy_entry.h"

namespace OHOS::Telephony::MultiSim {
namespace {
bool C0EffectSettled(const C0ParameterOutcome &outcome)
{
    return outcome.call.callFinished && (outcome.effect == EffectKnowledge::ConfirmedApplied ||
        outcome.effect == EffectKnowledge::ConfirmedNotApplied);
}

bool CanPlanC1Capabilities(const PrimaryRadioObservation &observed, const RadioProtocolMember &target)
{
    if (!target.capability || target.capability->modemId <= 0 || !observed.currentModemId ||
        target.capability->modemId != *observed.currentModemId || !observed.routing) {
        return false;
    }
    std::set<int32_t> modems;
    for (const auto &member : observed.routing->members) {
        if (!member.capability || member.capability->modemId < 0 ||
            !modems.insert(member.capability->modemId).second) {
            return false;
        }
    }
    return modems.count(0) == 1;
}
} // namespace

RequestId StartLegacyPrimarySwitch(AssignmentService &entry, const SwitchPrimaryCommand &command)
{ return entry.StartPrimarySwitch(command); }

void SetLegacyDefaultCallMessage(AssignmentService &entry, const SetRoleCommand &command,
    const std::optional<BindingRef> &binding, const CompletionPlan &completion)
{
    entry.SetDefaultRole(command, binding, completion);
}

void SetLegacyDefaultData(AssignmentService &entry, const BindingRef &binding, const CompletionPlan &completion)
{
    entry.SetDefaultRole({DefaultRole::Data, binding.subscription, binding.slot}, binding, completion);
}

AssignmentService::AssignmentService(const SlotCatalog &catalog, AssignmentStore &store,
    RadioExecutionPort &radio, AssignmentOutputPort &output, PrimaryTopologyPort &topology,
    LegacyIntCompletionPort &completion, RuntimeEpoch epoch, PrimaryScope primaryScope)
    : supportsRoleProvider_(store.SupportsRoleProvider()),
      independentRoleTransactions_(store.HasIndependentRoleTransactions()), catalog_(catalog), primaryScope_(primaryScope),
      reloadEpoch_(epoch),
      operations_(epoch, RequestOwner::Assignment), store_(store), radio_(radio),
      output_(output), topology_(topology), completion_(completion) {}

void AssignmentService::CompleteOnce(RequestId request, const CompletionPlan &plan, int32_t result,
    TerminalState terminal, std::unique_lock<std::recursive_mutex> *commit)
{
    if (!operations_.TryMarkTerminal(request, terminal)) { return; }
    const bool claimed = terminal == TerminalState::Open ? operations_.ClaimEarlyCompletion(request) :
        operations_.ClaimCompletion(request);
    if (claimed) {
        if (commit) { commit->unlock(); }
        try {
            completion_.CompleteLegacyInt(plan, result);
        } catch (const std::exception &) {
            if (!commit) { throw; }
            // The attempt was claimed before calling the sink. It must not be sent again.
        }
        if (commit) { commit->lock(); }
    }
}

void AssignmentService::BindRetryScheduler(const std::shared_ptr<RetrySchedulerPort> &scheduler)
{
    retryScheduler_ = scheduler;
    retrySchedulerBound_ = static_cast<bool>(scheduler);
}

bool AssignmentService::ScheduleC1Retry(RequestId request, uint32_t attempt,
    std::unique_lock<std::recursive_mutex> &guard)
{
    auto pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end() || pending->second.retryScheduled || stopping_) { return false; }
    if (!retrySchedulerBound_) {
        pending->second.retryReady = true;
        return true;
    }
    const auto scheduler = retryScheduler_.lock();
    const std::weak_ptr<AssignmentService> self = weak_from_this();
    const std::weak_ptr<ScheduledRetrySink> sink(self);
    if (!scheduler || sink.expired()) { return false; }
    const uint64_t generation = nextRetryScheduleGeneration_++;
    const ScheduledRetryEvent event {request, generation, attempt};
    pending->second.retryReady = true;
    pending->second.retryScheduled = true;
    pending->second.retryScheduleGeneration = generation;
    std::optional<uint64_t> task;
    guard.unlock();
    try {
        task = scheduler->ScheduleRetry(event, LEGACY_PRIMARY_PROTOCOL_RETRY_MS, sink);
    } catch (const std::exception &) {
        // A still holds the original scheduling qualification.
    }
    guard.lock();
    pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end() || !pending->second.retryScheduled ||
        pending->second.retryScheduleGeneration != generation) {
        // A callback or Stop consumed the original intent while registration was in flight.
        CancelScheduledTask(task, guard);
        return true;
    }
    if (!task) {
        pending->second.retryReady = false;
        pending->second.retryScheduled = false;
        pending->second.retryScheduleGeneration = 0;
        return false;
    }
    pending->second.retrySchedulerTask = task;
    return true;
}

void AssignmentService::CancelScheduledTask(std::optional<uint64_t> task,
    std::unique_lock<std::recursive_mutex> &guard)
{
    if (!task) { return; }
    const auto scheduler = retryScheduler_.lock();
    if (!scheduler) { return; }
    guard.unlock();
    try {
        (void)scheduler->CancelRetry(*task);
    } catch (const std::exception &) {
        // The scheduler audit still owns any queued or running callback.
    }
    guard.lock();
}

bool AssignmentService::ArmC1ProtocolDeadline(const StepRef &step, std::unique_lock<std::recursive_mutex> &guard)
{
    if (primaryScope_ != PrimaryScope::TrackedHost) { return true; }
    const auto scheduler = retryScheduler_.lock();
    const std::weak_ptr<ScheduledRetrySink> sink(weak_from_this());
    if (!scheduler || sink.expired() || stopping_) { return false; }
    const auto original = pendingPrimary_.find(step.request.value);
    if (original == pendingPrimary_.end() || !(original->second.step == step)) { return false; }
    const auto generation = nextRetryScheduleGeneration_++;
    original->second.protocolDeadlineGeneration = generation;
    const ScheduledRetryEvent event {step.request, generation, step.attempt,
        ScheduledRetryEvent::Purpose::ProtocolDeadline};
    std::optional<uint64_t> task;
    guard.unlock();
    try {
        task = scheduler->ScheduleRetry(event, LEGACY_PRIMARY_PROTOCOL_DEADLINE_MS, sink);
    } catch (const std::exception &) {
        // CHECK stays outside the platform when its original deadline cannot be armed.
    }
    guard.lock();
    const auto pending = pendingPrimary_.find(step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == step) ||
        pending->second.protocolDeadlineGeneration != generation || stopping_) {
        CancelScheduledTask(task, guard);
        return false;
    }
    pending->second.protocolDeadlineTask = task;
    return task.has_value();
}

void AssignmentService::CancelC1ProtocolDeadline(const StepRef &step, std::unique_lock<std::recursive_mutex> &guard)
{
    const auto pending = pendingPrimary_.find(step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step.request == step.request) ||
        pending->second.step.attempt != step.attempt) { return; }
    pending->second.protocolDeadlineGeneration = 0;
    const auto task = std::exchange(pending->second.protocolDeadlineTask, std::nullopt);
    CancelScheduledTask(task, guard);
}

bool AssignmentService::AcceptScheduledC1Retry(const ScheduledRetryEvent &event)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    if (stopping_) { return false; }
    const auto pending = pendingPrimary_.find(event.request.value);
    if (event.purpose == ScheduledRetryEvent::Purpose::ProtocolDeadline) {
        if (pending == pendingPrimary_.end() || !(pending->second.step.request == event.request) ||
            event.scheduleGeneration == 0 || pending->second.protocolDeadlineGeneration != event.scheduleGeneration ||
            pending->second.step.attempt != event.attempt) {
            return false;
        }
        pending->second.protocolDeadlineGeneration = 0;
        pending->second.protocolDeadlineTask.reset();
        return ExpirePrimarySwitch(event.request, true, guard);
    }
    if (pending == pendingPrimary_.end() || !pending->second.retryScheduled ||
        pending->second.retryScheduleGeneration != event.scheduleGeneration ||
        pending->second.attemptCount != event.attempt || !(pending->second.step.request == event.request)) {
        return false;
    }
    pending->second.retryScheduled = false;
    pending->second.retrySchedulerTask.reset();
    return AcceptC1RetryDue(event.request, guard);
}

void AssignmentService::RequestStopAccepting()
{
    stopping_.store(true, std::memory_order_release);
}

bool AssignmentService::StopAcceptingAndCancelScheduledRetries()
{
    RequestStopAccepting();
    bool noPending = false;
    {
        std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
        for (auto pending = pendingPrimary_.begin(); pending != pendingPrimary_.end();) {
            const auto operation = operations_.Read(pending->second.step.request);
            const bool cancellableRetry = pending->second.retryReady && pending->second.phase == 4 && operation &&
                operation->terminal == TerminalState::Open;
            if (!cancellableRetry) { ++pending; continue; }
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
                pending->second.step.request.value, TransitionPhase::Reconciling, TransitionPhase::Failed);
            operations_.MarkTerminal(pending->second.step.request, TerminalState::Failed);
            assignment_.ReleaseRoleWriteSet(pending->second.step.request.value, kPrimaryRoleWriteSet);
            operations_.RetireAfterEvidenceRetention(pending->second.step.request);
            pending = pendingPrimary_.erase(pending);
        }
        noPending = pendingPrimary_.empty();
    }
    if (const auto scheduler = retryScheduler_.lock()) {
        scheduler->CancelAllForOwner(operations_.ReadRuntimeEpoch(), RequestOwner::Assignment);
    }
    return noPending;
}

bool AssignmentService::IsStoppingPrimaryRequests() const
{
    return stopping_.load(std::memory_order_acquire);
}

bool AssignmentService::HasPendingDeviceEffects() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return !pendingPrimary_.empty();
}

AssignmentService::PrimaryCloseEffects AssignmentService::ReadPrimaryCloseEffects() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return {!pendingPrimary_.empty(), c0Receipt_ && c0Receipt_->writeSet == kPrimaryRoleWriteSet};
}

AssignmentService::CloseReceipt AssignmentService::ReadCloseReceipt() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    CloseReceipt receipt;
    receipt.primary.reserve(pendingPrimary_.size());
    for (const auto &entry : pendingPrimary_) {
        const auto &pending = entry.second;
        receipt.primary.push_back({pending.step, pending.originalC1Check, pending.protocolSession,
            pending.retrySchedulerTask, pending.protocolDeadlineTask});
    }
    receipt.parameterWrites = c0Receipt_;
    for (const auto &provider : providerReceipts_) {
        if (provider) { receipt.providerWrites.push_back(provider->evidence); }
    }
    receipt.discoveryRoles = accountRoles_.ReadPendingDiscoveryRoles();
    return receipt;
}

std::optional<AssignmentService::C0Receipt> AssignmentService::ReadC0Receipt() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return c0Receipt_ && c0Receipt_->writeSet == kPrimaryRoleWriteSet ? c0Receipt_ : std::nullopt;
}

std::optional<C0ParameterEvidence> AssignmentService::ReadDataParameterReceipt() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    if (!c0Receipt_ || c0Receipt_->writeSet != kDefaultDataWriteSet) { return std::nullopt; }
    return c0Receipt_->writes[static_cast<size_t>(C0ParameterKey::DefaultData)];
}

RequestId AssignmentService::StartPrimarySwitch(const SwitchPrimaryCommand &command)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const bool c0Only = primaryScope_ == PrimaryScope::C0Only;
    const bool trackedHost = primaryScope_ != PrimaryScope::ExistingBranches;
    const RequestId request = operations_.Begin(command.target, command.completion, command.parent);
    const auto fail = [&] {
        CompleteOnce(request, command.completion, -1, TerminalState::Failed, trackedHost ? &guard : nullptr);
        if (trackedHost) { operations_.RetireAfterEvidenceRetention(request); }
    };
    if (stopping_) {
        fail();
        return request;
    }
    if (command.parent && command.parent->request.epoch != request.epoch) {
        fail();
        return request;
    }
    if (!operations_.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt)) {
        fail();
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
        fail();
        return request;
    }

    std::optional<PrimaryRadioObservation> observation;
    guard.unlock();
    try {
        if (!c0Only || store_.SupportsTrackedC0()) {
            observation = radio_.ReadPrimaryRadioObservation(command.target.slot);
        }
    } catch (const std::exception &) {
        // No effect has been issued. Reject if the required external observation is unavailable.
    }
    guard.lock();
    if (!observation || stopping_) {
        fail();
        return request;
    }
    const auto &observed = *observation;
    const auto refreshedFacts = topology_.ReadPrimaryTarget(command.target.slot);
    if (!refreshedFacts || !(refreshedFacts->binding.subscription == facts->binding.subscription) ||
        !(refreshedFacts->binding.slot == facts->binding.slot) ||
        refreshedFacts->binding.bindingRevision != facts->binding.bindingRevision ||
        refreshedFacts->iccid != facts->iccid || refreshedFacts->active != facts->active) {
        fail();
        return request;
    }
    const bool validObservation = observed.slot == command.target.slot && observed.observationRevision != 0 &&
        observed.currentModemId && *observed.currentModemId >= 0;
    if (!validObservation) {
        // An unknown current modem does not describe M0.
        fail();
        return request;
    }
    const bool alreadyOnPrimaryModem = *observed.currentModemId == 0;
    const auto plan = PlanPrimarySwitch({*target, command.userSet, observed.dedicatedPrimaryRilSupported,
        alreadyOnPrimaryModem, observed.vendorMappingRequired});
    if (!plan || (c0Only && plan->mode != PrimarySwitchMode::AlreadyPrimary) ||
        (!observed.ordinarySwitchAllowed && plan->mode != PrimarySwitchMode::WithoutModemRestart) ||
        (plan->mode != PrimarySwitchMode::AlreadyPrimary &&
        plan->mode != PrimarySwitchMode::WithoutModemRestart &&
        plan->mode != PrimarySwitchMode::ThreePhaseProtocol)) {
        fail();
        return request;
    }
    std::optional<RadioProtocolMember> targetMember;
    if (plan->mode != PrimarySwitchMode::AlreadyPrimary) {
        const auto &routing = observed.routing;
        if (routing) {
            const auto member = std::find_if(routing->members.begin(), routing->members.end(),
                [&](const auto &entry) { return entry.nativeMember == routing->targetMember; });
            if (member != routing->members.end()) { targetMember = *member; }
        }
        if (!targetMember || (plan->mode == PrimarySwitchMode::ThreePhaseProtocol &&
            !CanPlanC1Capabilities(observed, *targetMember))) {
            fail();
            return request;
        }
    }
    if (assignment_.AdmitTransitionWithRoleWriteSet(*target, request.value, kPrimaryRoleWriteSet) !=
        TransitionAdmission::Admitted) {
        fail();
        return request;
    }
    if (trackedHost && plan->mode == PrimarySwitchMode::AlreadyPrimary) {
        RunTrackedPrimaryParameters({request, 1, 0}, TransitionPhase::Admitted, command, *target, *facts, guard);
        return request;
    }
    if (plan->mode == PrimarySwitchMode::WithoutModemRestart) {
        StepRef step {request, 1, 0};
        bool preferenceSettled = false;
        if (store_.SupportsTrackedC0()) {
            c0Receipt_.emplace(C0Receipt {request, C0Stage::Preference, facts->binding});
            const bool returned = WriteTrackedParameter(C0ParameterKey::Preference, step, &*facts, command.userSet, guard);
            const auto &preference = c0Receipt_->writes[static_cast<size_t>(C0ParameterKey::Preference)];
            if (!preference) {
                c0Receipt_.reset();
                assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                    TransitionPhase::Admitted, TransitionPhase::Failed);
                CompleteOnce(request, command.completion, -1, TerminalState::Failed, &guard);
                assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
                operations_.RetireAfterEvidenceRetention(request, true);
                return request;
            }
            if (!returned || !C0EffectSettled(preference->outcome)) {
                assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                    TransitionPhase::Admitted, TransitionPhase::Reconciling);
                operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
                c0Receipt_->outstanding = C0Stage::Completion;
                CompleteOnce(request, command.completion, 83886087, TerminalState::NeedsReconciliation, &guard);
                c0Receipt_->outstanding = C0Stage::Preference;
                return request;
            }
            preferenceSettled = true;
            c0Receipt_.reset();
        } else {
            const auto preference = store_.WriteUserPrimaryPreference(command.userSet);
            if (!preference.callFinished) {
                assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                    TransitionPhase::Admitted, TransitionPhase::Reconciling);
                operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
                return request;
            }
            preferenceSettled = preference.parameterResult == 0;
        }
        const StepRef sentStep {request, step.step + 1, step.attempt};
        if (!operations_.AdvanceStep(step, sentStep.step, sentStep.attempt, std::nullopt) ||
            !operations_.RecordRadioStep(sentStep) ||
            !assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::Admitted, TransitionPhase::DevicePending)) {
            fail();
            assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
            return request;
        }
        pendingPrimary_.emplace(request.value, PendingPrimarySwitch {sentStep, *facts, *target,
            facts->iccid, command.userSet, command.completion, plan->mode, 0,
            nextProtocolSession_++, targetMember->nativeMember, {*targetMember}, {}, {},
            false, 0, false, false, false, 0, std::nullopt, std::nullopt});
        const RadioSwitchRequest device {sentStep, targetMember->route, *target->radioDomain, plan->mode,
            facts->binding, pendingPrimary_.at(request.value).protocolSession, 0, 1, targetMember};
        guard.unlock();
        try {
            output_.PublishPrimaryPhase(false, command.userSet);
        } catch (const std::exception &) {
            // The original Radio admission and result remain independently owned.
        }
        guard.lock();
        const auto handoff = SubmitRadioOutsideCommit(device, *facts, guard);
        if (handoff != RadioSubmitDisposition::HandoffAccepted) {
            const auto pending = pendingPrimary_.find(request.value);
            const auto operation = operations_.Read(request);
            if (pending == pendingPrimary_.end() || !(pending->second.step == sentStep) || !operation) {
                return request;
            }
            const bool waiting = operation->terminal == TerminalState::Open;
            const bool timedOut = operation->terminal == TerminalState::NeedsReconciliation &&
                operation->wait == WaitState::TimedOut;
            if (handoff == RadioSubmitDisposition::ConfirmedNotSent && preferenceSettled &&
                (waiting || timedOut)) {
                if (waiting) {
                    assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                        TransitionPhase::DevicePending, TransitionPhase::Failed);
                    operations_.AcceptResult(sentStep, EffectKnowledge::NotSent);
                    operations_.EndWait(request, WaitState::Delivered);
                    operations_.MarkTerminal(request, TerminalState::Failed);
                    guard.unlock();
                    try {
                        output_.PublishPrimaryPhase(true, command.userSet);
                    } catch (const std::exception &) {
                        // The original completion remains deliverable.
                    }
                    guard.lock();
                    CompleteOnce(request, command.completion, 83886087, TerminalState::Failed, &guard);
                }
                FinishPrimaryWithNoDeviceEffect(sentStep);
            } else if (waiting) {
                assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                    TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
                operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
            }
        }
        return request;
    }
    if (plan->mode == PrimarySwitchMode::ThreePhaseProtocol) {
        std::optional<PrimaryParameterSnapshot> oldParameters;
        guard.unlock();
        try {
            oldParameters = store_.ReadPrimaryParameters();
        } catch (const std::exception &) {
            // No parameter or Radio effect was admitted for this request yet.
        }
        guard.lock();
        if (!oldParameters || !oldParameters->confirmed || stopping_ ||
            !topology_.WithCurrentPrimaryTarget(*facts, [] {})) {
            assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::Admitted, TransitionPhase::Failed);
            CompleteOnce(request, command.completion, -1, TerminalState::Failed, &guard);
            assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
            operations_.RetireAfterEvidenceRetention(request, true);
            return request;
        }
        StepRef step {request, 1, 0};
        if (store_.SupportsTrackedC0()) {
            if (!PrepareTrackedC1Parameters(step, *facts, *oldParameters, command.userSet, guard)) {
                FailC1Preparation(request, *target->radioDomain, TransitionPhase::Admitted, command.completion, guard);
                return request;
            }
        } else {
            const auto preference = store_.WriteUserPrimaryPreference(command.userSet);
            const auto primary = store_.WritePrimaryParameters(target->slot, facts->iccid);
            if (!preference.callFinished || !primary.callFinished) {
                assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                    TransitionPhase::Admitted, TransitionPhase::Reconciling);
                operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
                return request;
            }
            assignment_.AcceptPrimaryParameterProjection(target->slot);
        }
        assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
            TransitionPhase::Admitted, TransitionPhase::ParametersWritten);
        const StepRef sentStep {request, step.step + 1, step.attempt};
        if (!operations_.AdvanceStep(step, sentStep.step, sentStep.attempt, std::nullopt) ||
            !operations_.RecordRadioStep(sentStep) ||
            !assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
                TransitionPhase::ParametersWritten, TransitionPhase::DevicePending)) {
            fail();
            assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
            return request;
        }
        PendingPrimarySwitch pending {sentStep, *facts, *target, facts->iccid, command.userSet,
            command.completion, plan->mode, 1, nextProtocolSession_++, targetMember->nativeMember,
            observed.routing->members, {}, *oldParameters, false, 0, false, false, false, 0,
            std::nullopt, std::nullopt};
        pendingPrimary_.emplace(request.value, std::move(pending));
        guard.unlock();
        try {
            output_.PublishPrimaryPhase(false, false); // Legacy C1 status ignores the input flag.
        } catch (const std::exception &) {
            // A still owns the original parameters and Radio intent.
        }
        guard.lock();
        (void)SubmitC1Check(sentStep, guard);
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
    const auto primary = store_.WritePrimaryParameters(target->slot, facts->iccid);
    if (!primary.callFinished) {
        retainUnknownEffect(TransitionPhase::Admitted);
        return request;
    }
    assignment_.AcceptPrimaryParameterProjection(target->slot);
    assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
        TransitionPhase::Admitted, TransitionPhase::ParametersWritten);
    output_.PublishMainCardChanged(target->slot);

    const auto data = store_.WriteDefaultData(target->slot);
    if (!data.callFinished) {
        retainUnknownEffect(TransitionPhase::ParametersWritten);
        return request;
    }
    assignment_.AcceptDataParameterProjection(target->slot, facts->binding.subscription);
    output_.PublishDefaultDataChanged(target->slot, facts->binding.subscription);
    assignment_.AcceptTransitionPhase(*target->radioDomain, request.value,
        TransitionPhase::ParametersWritten, TransitionPhase::Completed);
    CompleteOnce(request, command.completion, 0, TerminalState::Succeeded);
    assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
    return request;
}

namespace {
using C0Stage = AssignmentService::C0Stage;
C0Stage ParameterStage(C0ParameterKey key)
{
    if (key == C0ParameterKey::Preference) { return C0Stage::Preference; }
    if (key == C0ParameterKey::DefaultData) { return C0Stage::DataParameters; }
    return C0Stage::PrimaryParameters;
}

std::optional<C0Stage> FirstUnresolved(const AssignmentService::C0Receipt &receipt)
{
    for (const auto &write : receipt.writes) {
        if (write && !C0EffectSettled(write->outcome)) { return ParameterStage(write->operation.key); }
    }
    return std::nullopt;
}

bool AnyPrimaryParameterApplied(const AssignmentService::C0Receipt &receipt)
{
    for (const auto key : {C0ParameterKey::PrimarySlot, C0ParameterKey::PrimaryIccid}) {
        const auto &original = receipt.writes[static_cast<size_t>(key)];
        if (original && original->outcome.effect == EffectKnowledge::ConfirmedApplied) { return true; }
    }
    return false;
}

bool AnyPrimaryParameterRegistered(const AssignmentService::C0Receipt &receipt)
{
    return receipt.writes[static_cast<size_t>(C0ParameterKey::PrimarySlot)] ||
        receipt.writes[static_cast<size_t>(C0ParameterKey::PrimaryIccid)];
}
} // namespace

bool AssignmentService::WriteTrackedParameter(C0ParameterKey key, StepRef &step, const PrimaryTargetFacts *facts,
    bool userSet, std::unique_lock<std::recursive_mutex> &guard, const PrimaryParameterSnapshot *restore,
    bool requireOpenAdmission)
{
    auto &receipt = *c0Receipt_;
    auto &original = receipt.writes[static_cast<size_t>(key)];
    const auto registerWrite = [&] {
        if (requireOpenAdmission && stopping_) { return; }
        const bool registered = operations_.AdvanceStep(step, step.step + 1, step.attempt, std::nullopt) ||
            (restore && operations_.AdvanceReconciliationStep(step, step.step + 1));
        if (registered) {
            ++step.step;
            original.emplace(C0ParameterEvidence {{step, key}, {}});
            receipt.outstanding = ParameterStage(key);
        }
    };
    if (restore) {
        if (assignment_.OwnsRoleWriteSet(step.request.value, kPrimaryRoleWriteSet)) { registerWrite(); }
    } else if (facts) {
        topology_.WithCurrentPrimaryTarget(*facts, registerWrite);
    }
    if (!original) { return false; }

    receipt.storeCallInFlight = true;
    guard.unlock();
    C0ParameterOutcome outcome;
    try {
        outcome = restore ? store_.RestorePrimaryParameter(original->operation, *restore) :
            store_.WriteC0Parameter(original->operation, facts->binding.slot, facts->iccid, userSet);
    } catch (const std::exception &) {
        // The original call may have crossed the write point.
    }
    guard.lock();
    original->outcome = outcome;
    receipt.storeCallInFlight = false;
    return outcome.call.callFinished;
}

bool AssignmentService::RestoreTrackedPrimary(StepRef &step, const PrimaryTargetFacts &facts,
    const PrimaryParameterSnapshot &old, std::unique_lock<std::recursive_mutex> &guard)
{
    if (!old.CanRestore()) { return true; }
    if (!store_.SupportsTrackedC0()) {
        bool returned = false;
        guard.unlock();
        try {
            returned = store_.RestorePrimaryIfLegacyValid(old).callFinished;
        } catch (const std::exception &) {
            // The original Store effect still requires reconciliation.
        }
        guard.lock();
        return returned;
    }
    c0Receipt_.emplace(C0Receipt {step.request, C0Stage::PrimaryParameters, facts.binding});
    c0Receipt_->restore = old;
    if (!ContinuePrimaryRestore(step, guard)) { return false; }
    c0Receipt_.reset();
    return true;
}

bool AssignmentService::ContinuePrimaryRestore(StepRef &step, std::unique_lock<std::recursive_mutex> &guard)
{
    auto &receipt = *c0Receipt_;
    for (const auto key : {C0ParameterKey::PrimarySlot, C0ParameterKey::PrimaryIccid}) {
        const auto &write = receipt.writes[static_cast<size_t>(key)];
        if (!write) { (void)WriteTrackedParameter(key, step, nullptr, false, guard, &*receipt.restore); }
        if (!write || !C0EffectSettled(write->outcome) ||
            write->outcome.effect != EffectKnowledge::ConfirmedApplied) { return false; }
    }
    return true;
}

void AssignmentService::AcceptRestoredPrimaryProjection(const PrimaryParameterSnapshot &old)
{
    for (const auto &slot : catalog_.Slots()) {
        if (std::to_string(slot.slot.legacySlotId) == old.slotText) {
            assignment_.AcceptPrimaryParameterProjection(slot.slot);
            return;
        }
    }
}

bool AssignmentService::PrepareTrackedC1Parameters(StepRef &step, const PrimaryTargetFacts &facts,
    const PrimaryParameterSnapshot &old, std::optional<bool> preference, std::unique_lock<std::recursive_mutex> &guard)
{
    c0Receipt_.emplace(C0Receipt {step.request, preference ? C0Stage::Preference : C0Stage::PrimaryParameters,
        facts.binding});
    const auto write = [&](C0ParameterKey key) {
        if (stopping_) { return false; }
        (void)WriteTrackedParameter(key, step, &facts, preference.value_or(false), guard, nullptr, true);
        const auto &original = c0Receipt_->writes[static_cast<size_t>(key)];
        // Legacy SavePrimaryCardInfo proceeds after completed parameter calls regardless of their return codes.
        return original && C0EffectSettled(original->outcome);
    };
    const bool ready = (!preference || write(C0ParameterKey::Preference)) &&
        write(C0ParameterKey::PrimarySlot) && write(C0ParameterKey::PrimaryIccid) &&
        !stopping_ && topology_.WithCurrentPrimaryTarget(facts, [&] {
            assignment_.AcceptPrimaryParameterProjection(facts.binding.slot);
        });
    if (ready) {
        c0Receipt_.reset();
        return true;
    }
    if (const auto unresolved = FirstUnresolved(*c0Receipt_)) {
        c0Receipt_->outstanding = *unresolved;
        if (old.CanRestore() && AnyPrimaryParameterRegistered(*c0Receipt_)) {
            c0Receipt_->restore = old;
            c0Receipt_->restoreAfterPrewrite = true;
        }
        return false;
    }
    const bool mustRestore = AnyPrimaryParameterApplied(*c0Receipt_);
    c0Receipt_.reset();
    if (mustRestore && RestoreTrackedPrimary(step, facts, old, guard)) { AcceptRestoredPrimaryProjection(old); }
    return false;
}

void AssignmentService::FailC1Preparation(RequestId request, const RadioDomainId &domain, TransitionPhase phase,
    const CompletionPlan &completion, std::unique_lock<std::recursive_mutex> &guard)
{
    if (c0Receipt_) {
        assignment_.AcceptTransitionPhase(domain, request.value, phase, TransitionPhase::Reconciling);
        const auto outstanding = c0Receipt_->outstanding;
        c0Receipt_->outstanding = C0Stage::Completion;
        CompleteOnce(request, completion, 83886087, TerminalState::NeedsReconciliation, &guard);
        c0Receipt_->outstanding = outstanding;
        return;
    }
    assignment_.AcceptTransitionPhase(domain, request.value, phase, TransitionPhase::Failed);
    CompleteOnce(request, completion, 83886087, TerminalState::Failed, &guard);
    assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
    operations_.RetireAfterEvidenceRetention(request, true);
}

void AssignmentService::RunTrackedPrimaryParameters(StepRef step, TransitionPhase phase, const SwitchPrimaryCommand &command,
    const SlotDescriptor &target, const PrimaryTargetFacts &facts, std::unique_lock<std::recursive_mutex> &guard,
    const PrimaryParameterSnapshot *c1Prewrite)
{
    const auto request = step.request;
    const auto deviceStep = step;
    const bool prewritten = c1Prewrite != nullptr;
    const bool completedRadio = phase == TransitionPhase::DevicePending;
    c0Receipt_.emplace(C0Receipt {request, completedRadio ? C0Stage::PrimaryParameters : C0Stage::Preference,
        facts.binding});
    auto &receipt = *c0Receipt_;
    bool callDidNotReturn = false;
    const auto write = [&](C0ParameterKey key) {
        const bool returned = WriteTrackedParameter(key, step, &facts, command.userSet, guard);
        const auto &original = receipt.writes[static_cast<size_t>(key)];
        callDidNotReturn = original && !returned;
        return returned && (!prewritten || (original && C0EffectSettled(original->outcome)));
    };
    const auto publish = [&](C0Stage stage, const auto &commit, const auto &notify) {
        if (!topology_.WithCurrentPrimaryTarget(facts, [&] {
            commit();
            receipt.outstanding = stage;
        })) { return false; }
        guard.unlock();
        bool delivered = true;
        try {
            notify();
        } catch (const std::exception &) {
            delivered = false;
        }
        guard.lock();
        return delivered;
    };

    bool announcePrimary = false;
    bool mainAdmitted = false;
    // Preserve the original treatment of completed parameter return codes.
    const bool success = (completedRadio || write(C0ParameterKey::Preference)) &&
        (prewritten || (write(C0ParameterKey::PrimarySlot) && write(C0ParameterKey::PrimaryIccid))) &&
        publish(C0Stage::MainNotification, [&] {
            mainAdmitted = true;
            assignment_.AcceptPrimaryParameterProjection(target.slot);
            announcePrimary = assignment_.ClaimPrimaryAnnouncement(target.slot, facts.binding.subscription);
            assignment_.AcceptTransitionPhase(*target.radioDomain, request.value, phase,
                TransitionPhase::ParametersWritten);
            phase = TransitionPhase::ParametersWritten;
        }, [&] {
            if (announcePrimary) { output_.PublishMainCardChanged(target.slot, facts.binding.subscription); }
        }) &&
        write(C0ParameterKey::DefaultData) && publish(C0Stage::DataNotification,
            [&] { assignment_.AcceptDataParameterProjection(target.slot, facts.binding.subscription); },
            [&] {
                output_.PublishDefaultDataChanged(target.slot, facts.binding.subscription);
                if (prewritten) { output_.ReportDefaultDataSelection(target.slot); }
            });
    if (prewritten && !mainAdmitted) {
        c0Receipt_.reset();
        if (RestoreTrackedPrimary(step, facts, *c1Prewrite, guard)) {
            AcceptRestoredPrimaryProjection(*c1Prewrite);
        }
        FinishC1Phase(deviceStep, *target.radioDomain, guard);
        FailC1Preparation(request, *target.radioDomain, phase, command.completion, guard);
        return;
    }
    receipt.businessSucceeded = success;
    const auto unsettled = FirstUnresolved(receipt);
    assignment_.AcceptTransitionPhase(*target.radioDomain, request.value, phase, unsettled ?
        TransitionPhase::Reconciling : (success ? TransitionPhase::Completed : TransitionPhase::Failed));
    const auto terminal = unsettled ? TerminalState::NeedsReconciliation :
        (success ? TerminalState::Succeeded : TerminalState::Failed);

    if (completedRadio) {
        operations_.MarkTerminal(request, terminal);
        receipt.outstanding = C0Stage::Completion;
        if (prewritten) {
            FinishC1Phase(deviceStep, *target.radioDomain, guard);
        } else {
            guard.unlock();
            try {
                output_.PublishPrimaryPhase(true, command.userSet);
            } catch (const std::exception &) {
                // A failure to notify must not prevent delivery of the original result.
            }
            guard.lock();
        }
    }
    const auto *sync = std::get_if<SynchronousCompletionRoute>(&command.completion.route);
    if (completedRadio || !callDidNotReturn || (sync && sync->waiter.origin == SynchronousWaitRef::Origin::ModuleRole)) {
        receipt.outstanding = C0Stage::Completion;
        const int32_t failure = completedRadio ? 83886087 : -1;
        if (prewritten && success && deviceStep.attempt != 0 && command.completion.accountNotificationOnWholeSuccess) {
            guard.unlock();
            try {
                completion_.NotifyRetriedPrimarySuccess();
            } catch (const std::exception &) {
                // The successful background attempt's original notification was attempted once.
            }
            guard.lock();
        }
        CompleteOnce(request, command.completion, success ? 0 : failure, terminal, &guard);
    } else {
        operations_.MarkTerminal(request, terminal);
    }
    if (unsettled) {
        receipt.outstanding = *unsettled;
        return;
    }
    assignment_.ReleaseRoleWriteSet(request.value, kPrimaryRoleWriteSet);
    operations_.RetireAfterEvidenceRetention(request, completedRadio);
    c0Receipt_.reset();
}

void AssignmentService::FinishC1Phase(const StepRef &step, const RadioDomainId &domain,
    std::unique_lock<std::recursive_mutex> &guard)
{
    if (const auto pending = pendingPrimary_.find(step.request.value);
        pending != pendingPrimary_.end() && pending->second.step.request == step.request &&
        pending->second.step.attempt == step.attempt) {
        if (pending->second.c1PhaseEndStarted) { return; }
        pending->second.c1PhaseEndStarted = true;
    }
    CancelC1ProtocolDeadline(step, guard);
    guard.unlock();
    try {
        output_.PublishPrimaryPhase(true, false);
    } catch (const std::exception &) {
        // The original phase still owns the SenseSwitch end.
    }
    try {
        radio_.EndLegacySenseSwitch(step, domain);
    } catch (const std::exception &) {
        // The reporting failure does not replace the original result.
    }
    guard.lock();
}

bool AssignmentService::ReconcileC0Parameter(const C0ParameterOperation &operation)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const auto index = static_cast<size_t>(operation.key);
    if (!c0Receipt_ || c0Receipt_->outstanding == C0Stage::Completion || c0Receipt_->storeCallInFlight ||
        index >= c0Receipt_->writes.size()) { return false; }
    const auto context = operations_.Read(operation.original.request);
    if (!context || context->terminal != TerminalState::NeedsReconciliation) { return false; }
    auto original = c0Receipt_->writes[index];
    if (!original || !(original->operation == operation) || C0EffectSettled(original->outcome)) { return false; }
    guard.unlock();
    std::optional<C0ParameterEvidence> evidence;
    try {
        evidence = store_.ReadC0ParameterEvidence(operation);
    } catch (const std::exception &) {
        return false;
    }
    guard.lock();
    if (!c0Receipt_ || c0Receipt_->outstanding == C0Stage::Completion || c0Receipt_->storeCallInFlight) { return false; }
    original = c0Receipt_->writes[index];
    if (!original || !(original->operation == operation) || C0EffectSettled(original->outcome)) { return false; }
    if (!evidence || !(evidence->operation == operation) || !C0EffectSettled(evidence->outcome)) { return false; }
    if (original->outcome.effect != EffectKnowledge::Unknown && original->outcome.effect != evidence->outcome.effect) {
        return false;
    }
    c0Receipt_->writes[index]->outcome = evidence->outcome;
    if (c0Receipt_->restore && !c0Receipt_->restoreAfterPrewrite) {
        auto step = context->context.step;
        if (!ContinuePrimaryRestore(step, guard)) { return true; }
        AcceptRestoredPrimaryProjection(*c0Receipt_->restore);
    }
    const auto unsettled = FirstUnresolved(*c0Receipt_);
    if (unsettled) {
        c0Receipt_->outstanding = *unsettled;
        return true;
    }
    if (c0Receipt_->restoreAfterPrewrite && AnyPrimaryParameterApplied(*c0Receipt_)) {
        c0Receipt_->writes = {};
        c0Receipt_->restoreAfterPrewrite = false;
        c0Receipt_->outstanding = C0Stage::PrimaryParameters;
        auto step = context->context.step;
        if (!ContinuePrimaryRestore(step, guard)) { return true; }
        AcceptRestoredPrimaryProjection(*c0Receipt_->restore);
    }
    const auto request = c0Receipt_->request;
    if (c0Receipt_->writeSet == kPrimaryRoleWriteSet) {
        const auto *target = catalog_.Find(c0Receipt_->binding.slot);
        assignment_.AcceptTransitionPhase(*target->radioDomain, request.value, TransitionPhase::Reconciling,
            c0Receipt_->businessSucceeded ? TransitionPhase::Completed : TransitionPhase::Failed);
    }
    assignment_.ReleaseRoleWriteSet(request.value, c0Receipt_->writeSet);
    operations_.RetireAfterEvidenceRetention(request);
    c0Receipt_.reset();
    return true;
}


AssignmentView AssignmentService::ReadLegacyAssignment() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    AssignmentView view {};
    const auto &slots = catalog_.Slots();
    for (const auto &slot : slots) {
        if (slot.radioDomain && assignment_.IsSwitchInProgress(*slot.radioDomain)) {
            view.primarySwitchInProgress = true;
        }
    }
    view.lastPrimary = assignment_.ReadLastPrimary();
    view.lastData = assignment_.ReadLastData();
    if (const auto data = assignment_.ReadRoleSelection(DefaultRole::Data)) {
        view.defaultData = data->effective;
    }
    const auto voice = assignment_.ReadRoleSelection(DefaultRole::Voice);
    const auto sms = assignment_.ReadRoleSelection(DefaultRole::Sms);
    if (voice) { view.voicePreference = voice->preference; }
    if (sms) { view.smsPreference = sms->preference; }
    if (supportsRoleProvider_) {
        std::optional<std::pair<SlotId, SubscriptionId>> firstActive;
        for (const auto &slot : slots) {
            const auto facts = topology_.ReadPrimaryTarget(slot.slot);
            if (!facts || !facts->active) { continue; }
            const auto subscription = facts->binding.subscription;
            if (view.voicePreference == subscription) { view.defaultVoice = subscription; }
            if (view.smsPreference == subscription) { view.defaultSms = subscription; }
            if (!firstActive || slot.slot < firstActive->first) { firstActive.emplace(slot.slot, subscription); }
        }
        if (!view.defaultSms && firstActive) { view.defaultSms = firstActive->second; }
    } else {
        view.defaultVoice = view.voicePreference;
        view.defaultSms = view.smsPreference;
    }
    view.assignmentRevision = assignment_.Revision();
    return view;
}

int32_t AssignmentService::SetDefaultRole(const SetRoleCommand &command)
{
    // Retain the old recording-store component tests. Formal and Provider graphs use original operations.
    if (command.role == DefaultRole::Data || primaryScope_ != PrimaryScope::ExistingBranches ||
        supportsRoleProvider_) { return -1; }
    const uint32_t members = command.role == DefaultRole::Voice ? kVoiceParameters : kSmsParameters;
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    if (stopping_) { return -1; }
    const uint64_t ownerToken = ~uint64_t {0} - nextConflictToken_++;
    if (!assignment_.AdmitRoleWriteSet(ownerToken, members)) { return -1; }
    guard.unlock();
    RoleStoreResult result {-1, false, false};
    try {
        result = store_.WriteDefaultRole(command);
    } catch (const std::exception &) {
        // This component entry has no evidence port for a possibly issued write.
    }
    guard.lock();
    if (result.callFinished && result.writeConfirmed && result.legacyResult == 0) {
        assignment_.AcceptRoleSelection({command.role, command.subscription, command.subscription},
            assignment_.Revision());
    }
    if (result.callFinished) { assignment_.ReleaseRoleWriteSet(ownerToken, members); }
    return result.legacyResult;
}

void AssignmentService::SetDefaultRole(const SetRoleCommand &command, const BindingRef &binding,
    const CompletionPlan &completion)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    if (command.role != DefaultRole::Data || !(command.subscription == binding.subscription) ||
        !(command.legacySlot == binding.slot)) { return; }

    const auto request = operations_.Begin(SubscriptionTarget {binding.subscription}, completion, std::nullopt);
    auto step = StepRef {request, 0, 0};
    const auto facts = topology_.ReadPrimaryTarget(binding.slot);
    if (stopping_ || !store_.SupportsTrackedC0() || !facts ||
        !(facts->binding.subscription == binding.subscription) ||
        facts->binding.bindingRevision != binding.bindingRevision ||
        !assignment_.AdmitRoleWriteSet(request.value, kDefaultDataWriteSet)) {
        CompleteOnce(request, completion, -1, TerminalState::Failed, &guard);
        operations_.RetireAfterEvidenceRetention(request);
        return;
    }

    c0Receipt_.emplace(C0Receipt {request, C0Stage::DataParameters, binding, {}, false, kDefaultDataWriteSet});
    auto &receipt = *c0Receipt_;
    const bool returned = WriteTrackedParameter(C0ParameterKey::DefaultData, step, &*facts, false, guard);
    bool success = returned && topology_.WithCurrentPrimaryTarget(*facts, [&] {
        assignment_.AcceptDataParameterProjection(binding.slot, binding.subscription);
        receipt.outstanding = C0Stage::DataNotification;
    });
    if (success) {
        guard.unlock();
        try {
            output_.PublishDefaultDataChanged(binding.slot, binding.subscription);
            output_.ReportDefaultDataSelection(binding.slot);
        } catch (const std::exception &) {
            success = false;
        }
        guard.lock();
    }

    receipt.businessSucceeded = success;
    const auto unsettled = FirstUnresolved(receipt);
    receipt.outstanding = C0Stage::Completion;
    CompleteOnce(request, completion, success ? 0 : -1, unsettled ? TerminalState::NeedsReconciliation :
        (success ? TerminalState::Succeeded : TerminalState::Failed), &guard);
    if (unsettled) {
        receipt.outstanding = *unsettled;
        return;
    }
    assignment_.ReleaseRoleWriteSet(request.value, kDefaultDataWriteSet);
    operations_.RetireAfterEvidenceRetention(request);
    c0Receipt_.reset();
}

void AssignmentService::SetDefaultRole(const SetRoleCommand &command, const std::optional<BindingRef> &binding,
    const CompletionPlan &completion)
{
    const auto role = command.role;
    if (role == DefaultRole::Data) { return; }
    const auto index = static_cast<size_t>(role);
    const uint32_t members = independentRoleTransactions_ ?
        (role == DefaultRole::Voice ? kVoiceParameters : kSmsParameters) : kVoiceParameters | kSmsParameters;
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const auto facts = binding ? topology_.ReadPrimaryTarget(binding->slot) : std::nullopt;
    const auto request = operations_.Begin(command.subscription ?
        ResolvedTarget {SubscriptionTarget {*command.subscription}} : ResolvedTarget {SlotTarget {command.legacySlot}},
        completion, std::nullopt);
    auto &receipt = providerReceipts_[index];
    const auto finish = [&](LegacyRoleResult result, bool unresolved) {
        CompleteOnce(request, completion, static_cast<int32_t>(result), unresolved ? TerminalState::NeedsReconciliation :
            (result == LegacyRoleResult::Success ? TerminalState::Succeeded : TerminalState::Failed), &guard);
        if (unresolved) {
            receipt->reconcilable = true;
        } else {
            operations_.RetireAfterEvidenceRetention(request);
        }
    };
    const bool validTarget = binding ?
        facts && binding->subscription.legacySimId > 0 && command.subscription == binding->subscription &&
            command.legacySlot == binding->slot &&
            facts->binding.subscription == binding->subscription &&
            facts->binding.bindingRevision == binding->bindingRevision :
        !command.subscription && command.legacySlot.legacySlotId == -1;
    if (stopping_ || !supportsRoleProvider_ || !validTarget || accountRoles_.HasPendingStorageRoles()) {
        finish(LegacyRoleResult::Failed, false);
        return;
    }
    const auto admit = [&] {
        if (!assignment_.AdmitRoleWriteSet(request.value, members)) { return; }
        if (!operations_.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt)) {
            assignment_.ReleaseRoleWriteSet(request.value, members);
            return;
        }
        receipt.emplace(RoleProviderReceipt {{{{request, 1, 0}, role, command.subscription}, {}}, members});
    };
    try {
        if (facts) { topology_.WithCurrentPrimaryTarget(*facts, admit); }
        else { admit(); }
    } catch (const std::exception &) {
        assignment_.ReleaseRoleWriteSet(request.value, members);
        if (receipt && receipt->evidence.operation.original.request == request) { receipt.reset(); }
    }
    if (!receipt || !(receipt->evidence.operation.original.request == request)) {
        finish(LegacyRoleResult::Failed, false);
        return;
    }
    const auto original = receipt->evidence.operation;
    guard.unlock();
    RoleProviderOutcome outcome;
    try {
        outcome = store_.WriteDefaultRole(original);
    } catch (const std::exception &) {
        // The Provider may have accepted the original update before the call unwound.
    }
    guard.lock();
    receipt->evidence.outcome = outcome;
    auto result = LegacyRoleResult::StoreFailed;
    if (outcome.callFinished && (outcome.effect == RoleProviderEffect::Applied ||
        outcome.effect == RoleProviderEffect::Partial)) {
        accountRoles_.ProtectProviderRole(role, outcome.position);
    }
    if (outcome.callFinished && outcome.result != -1 && outcome.effect == RoleProviderEffect::Applied) {
        const auto previous = assignment_.ReadRoleSelection(role);
        const bool announce = !((previous ? previous->preference : std::nullopt) == command.subscription);
        const auto commit = [&] {
            assignment_.AcceptRoleSelection({role, command.subscription, command.subscription}, assignment_.Revision());
            accountRoles_.NoteLocalRoleMutation(role, command.subscription, outcome.position);
        };
        bool accepted = true;
        try {
            if (facts) { accepted = topology_.WithCurrentPrimaryTarget(*facts, commit); }
            else { commit(); }
        } catch (const std::exception &) {
            accepted = false;
        }
        result = accepted ? LegacyRoleResult::Success : LegacyRoleResult::Failed;
        if (accepted && announce) {
            guard.unlock();
            bool published = false;
            try {
                published = output_.PublishDefaultRoleChanged(role, command.subscription);
            } catch (const std::exception &) {
                // The committed role and deduplication survive failure to publish.
            }
            guard.lock();
            if (!published) { result = LegacyRoleResult::PublicationFailed; }
        }
    }
    const bool unresolved = !outcome.Settled();
    finish(result, unresolved);
    if (!unresolved) {
        assignment_.ReleaseRoleWriteSet(request.value, members);
        receipt.reset();
    }
}

std::optional<RoleProviderEvidence> AssignmentService::ReadRoleProviderReceipt(DefaultRole role) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    if (role == DefaultRole::Data) { return std::nullopt; }
    const auto &receipt = providerReceipts_[static_cast<size_t>(role)];
    return receipt ? std::make_optional(receipt->evidence) : std::nullopt;
}

bool AssignmentService::ReconcileRoleProvider(const RoleProviderOperation &operation)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    if (operation.role == DefaultRole::Data) { return false; }
    auto &receipt = providerReceipts_[static_cast<size_t>(operation.role)];
    const auto matches = [&] {
        return receipt && receipt->reconcilable && receipt->evidence.operation == operation;
    };
    if (!matches()) { return false; }
    guard.unlock();
    std::optional<RoleProviderEvidence> evidence;
    try {
        evidence = store_.ReadRoleProviderEvidence(operation);
    } catch (const std::exception &) {
        return false;
    }
    guard.lock();
    if (!matches() || !evidence || !(evidence->operation == operation) || !evidence->outcome.Settled()) { return false; }
    const auto previousEffect = receipt->evidence.outcome.effect;
    if (previousEffect != RoleProviderEffect::Unknown && previousEffect != evidence->outcome.effect) { return false; }
    if (evidence->outcome.effect == RoleProviderEffect::Applied || evidence->outcome.effect == RoleProviderEffect::Partial) {
        accountRoles_.ProtectProviderRole(operation.role, evidence->outcome.position);
    }
    assignment_.ReleaseRoleWriteSet(operation.original.request.value, receipt->writeSet);
    operations_.RetireAfterEvidenceRetention(operation.original.request);
    receipt.reset();
    return true;
}

void AssignmentService::AcceptRadioStep(const RadioSwitchResult &result)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const auto pending = pendingPrimary_.find(result.step.request.value);
    if (pending == pendingPrimary_.end()) { return; }
    const auto operation = operations_.Read(result.step.request);
    if (!assignment_.OwnsRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet) ||
        !operation || !(pending->second.step == result.step) || !operation->context.deviceStepRegistered ||
        result.protocolSession != pending->second.protocolSession) {
        return;
    }
    if (operation->terminal == TerminalState::NeedsReconciliation && operation->wait == WaitState::TimedOut) {
        // R may have queued normal, already verified device evidence just before A consumed the timeout.
        AcceptLateRadioEvidenceLocked(result, guard);
        return;
    }
    if (operation->terminal != TerminalState::Open) { return; }
    if (pending->second.mode == PrimarySwitchMode::WithoutModemRestart) {
        if (!(result.memberSlot == pending->second.members.front().route) || !result.expectedBinding ||
            !(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
            result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision) {
            return;
        }
    } else if (result.phase != pending->second.phase ||
        (pending->second.phase == 2 && !result.notify) ||
        (result.expectedBinding &&
         (!(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
          result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision)) ||
        std::none_of(pending->second.members.begin(), pending->second.members.end(),
            [&](const auto &member) { return member.route == result.memberSlot; }) ||
        std::find(pending->second.completedMembers.begin(), pending->second.completedMembers.end(), result.memberSlot) !=
            pending->second.completedMembers.end()) {
        return;
    }
    if (pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        if (result.effect == EffectKnowledge::Unknown) {
            if (operations_.AcceptResult(result.step, result.effect) !=
                RuntimeOperationStore::ResultAdmission::Accepted) { return; }
            operations_.EndWait(result.step.request, WaitState::Delivered);
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
            operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
            FinishC1Phase(result.step, *pending->second.target.radioDomain, guard);
            CompleteOnce(result.step.request, pending->second.completion, 83886087,
                TerminalState::NeedsReconciliation, &guard);
            return;
        }
        pending->second.roundFailed |= result.effect != EffectKnowledge::ConfirmedApplied || result.legacyResult != 0;
        pending->second.completedMembers.push_back(result.memberSlot);
        if (pending->second.completedMembers.size() != pending->second.members.size()) { return; }
    }
    if (pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        pending->second.completedMembers.clear();
        if (pending->second.phase != 4) {
            if (IsStoppingPrimaryRequests()) {
                assignment_.AcceptTransitionPhase(*pending->second.target.radioDomain, result.step.request.value,
                    TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
                operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
                return;
            }
            const uint32_t nextPhase = pending->second.roundFailed ? 4 :
                pending->second.phase == 1 ? 2 : 4;
            const uint32_t nextStep = pending->second.step.step + 1;
            if (!operations_.AdvanceStep(pending->second.step, nextStep,
                pending->second.step.attempt, std::nullopt)) { return; }
            pending->second.step.step = nextStep;
            if (!operations_.RecordRadioStep(pending->second.step)) { return; }
            pending->second.phase = nextPhase;
            (void)SubmitC1Round(pending->second, guard);
            return;
        }
    }
    const auto admission = operations_.AcceptResult(result.step, result.effect);
    if (admission != RuntimeOperationStore::ResultAdmission::Accepted) { return; }
    operations_.EndWait(result.step.request, WaitState::Delivered);
    const auto domain = pending->second.target.radioDomain.value();
    const bool c1 = pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol;
    if (c1) {
        CancelC1ProtocolDeadline(result.step, guard);
    } else {
        guard.unlock();
        try {
            radio_.EndLegacySenseSwitch(result.step, domain);
        } catch (const std::exception &) {
            // The original result owns this step. A failed platform notification is not a new result.
        }
        guard.lock();
    }
    if (c1 && pending->second.roundFailed) {
        if (!RestoreTrackedPrimary(pending->second.step, pending->second.facts, pending->second.oldParameters, guard)) {
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::Reconciling);
            operations_.MarkTerminal(result.step.request, TerminalState::NeedsReconciliation);
            if (c0Receipt_) {
                const auto outstanding = c0Receipt_->outstanding;
                c0Receipt_->outstanding = C0Stage::Completion;
                FinishC1Phase(result.step, domain, guard);
                CompleteOnce(result.step.request, pending->second.completion, 83886087,
                    TerminalState::NeedsReconciliation, &guard);
                c0Receipt_->outstanding = outstanding;
                pendingPrimary_.erase(pending);
            }
            return;
        }
        AcceptRestoredPrimaryProjection(pending->second.oldParameters);
        FinishC1Phase(result.step, domain, guard);
        if (pending->second.attemptCount == 0) {
            CompleteOnce(result.step.request, pending->second.completion, 83886087, TerminalState::Open, &guard);
            pending->second.completionDelivered = true;
            operations_.EndWait(result.step.request, WaitState::Delivered);
        }
        if (pending->second.attemptCount < 5) {
            ++pending->second.attemptCount;
            assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
                TransitionPhase::DevicePending, TransitionPhase::Reconciling);
            operations_.MarkTerminal(result.step.request, TerminalState::Open);
            if (!ScheduleC1Retry(result.step.request, pending->second.attemptCount, guard)) {
                assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
                    result.step.request.value, TransitionPhase::Reconciling, TransitionPhase::Failed);
                operations_.MarkTerminal(result.step.request, TerminalState::Failed);
                assignment_.ReleaseRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet);
                operations_.RetireAfterEvidenceRetention(result.step.request);
                pendingPrimary_.erase(pending);
            }
            return;
        }
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), result.step.request.value,
            TransitionPhase::DevicePending, TransitionPhase::Failed);
        operations_.MarkTerminal(result.step.request, TerminalState::Failed);
        assignment_.ReleaseRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet);
        pendingPrimary_.erase(pending);
        return;
    }
    const bool deviceSucceeded = result.effect == EffectKnowledge::ConfirmedApplied && result.legacyResult == 0;
    if (c1 && deviceSucceeded && store_.SupportsTrackedC0()) {
        const SwitchPrimaryCommand command {{pending->second.target.slot}, pending->second.userSet,
            pending->second.completion, std::nullopt, pending->second.facts.binding};
        RunTrackedPrimaryParameters(result.step, TransitionPhase::DevicePending, command, pending->second.target,
            pending->second.facts, guard, &pending->second.oldParameters);
        pendingPrimary_.erase(pending);
        return;
    }
    const auto currentFacts = topology_.ReadPrimaryTarget(pending->second.target.slot);
    const bool bindingCurrent = currentFacts && currentFacts->active &&
        currentFacts->binding.subscription == pending->second.facts.binding.subscription &&
        currentFacts->binding.slot == pending->second.facts.binding.slot &&
        currentFacts->binding.bindingRevision == pending->second.facts.binding.bindingRevision &&
        currentFacts->iccid == pending->second.facts.iccid;
    if (deviceSucceeded && bindingCurrent) {
        if (store_.SupportsTrackedC0()) {
            const SwitchPrimaryCommand command {{pending->second.target.slot}, pending->second.userSet,
                pending->second.completion, std::nullopt, pending->second.facts.binding};
            RunTrackedPrimaryParameters(result.step, TransitionPhase::DevicePending, command, pending->second.target,
                pending->second.facts, guard);
            pendingPrimary_.erase(pending);
            return;
        }
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
        assignment_.AcceptDataParameterProjection(pending->second.target.slot, pending->second.facts.binding.subscription);
        output_.PublishDefaultDataChanged(pending->second.target.slot,
            pending->second.facts.binding.subscription);
        assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(),
            result.step.request.value, TransitionPhase::DevicePending, TransitionPhase::Completed);
        if (c1) {
            FinishC1Phase(result.step, domain, guard);
        } else {
            output_.PublishPrimaryPhase(true, pending->second.userSet);
        }
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
    const bool dedicated = pending->second.mode == PrimarySwitchMode::WithoutModemRestart;
    const bool userSet = pending->second.userSet;
    const auto terminal = needsReconciliation ? TerminalState::NeedsReconciliation : TerminalState::Failed;
    if (dedicated) {
        if (needsReconciliation && result.effect != EffectKnowledge::Unknown && !pending->second.lateDeviceEffect) {
            pending->second.lateDeviceEffect = result.effect;
        }
        operations_.MarkTerminal(result.step.request, terminal);
        guard.unlock();
        try {
            output_.PublishPrimaryPhase(true, userSet);
        } catch (const std::exception &) {
            // Reporting the original result still owns its Completion.
        }
        guard.lock();
    } else if (c1) {
        operations_.MarkTerminal(result.step.request, terminal);
        FinishC1Phase(result.step, domain, guard);
    } else {
        output_.PublishPrimaryPhase(true, userSet);
    }
    const int32_t legacyResult = result.effect == EffectKnowledge::Unknown ||
        result.legacyResult == 0 ? 83886087 : result.legacyResult;
    CompleteOnce(result.step.request, pending->second.completion, legacyResult,
        terminal, dedicated || c1 ? &guard : nullptr);
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
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    AcceptLateRadioEvidenceLocked(result, guard);
}

void AssignmentService::AcceptC1RoundRecovery(const RadioC1RoundRecovery &evidence)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const auto request = evidence.originalCheck.request;
    const auto pending = pendingPrimary_.find(request.value);
    const auto operation = operations_.Read(request);
    if (pending == pendingPrimary_.end() || !operation || !operation->context.deviceStepRegistered ||
        (operation->terminal != TerminalState::Open && operation->terminal != TerminalState::NeedsReconciliation) ||
        !assignment_.OwnsRoleWriteSet(request.value, kPrimaryRoleWriteSet)) { return; }
    auto &round = pending->second;
    if (round.mode != PrimarySwitchMode::ThreePhaseProtocol ||
        !round.originalC1Check || !(*round.originalC1Check == evidence.originalCheck) ||
        round.protocolSession != evidence.protocolSession || round.members.size() != evidence.members.size() ||
        !std::is_permutation(round.members.begin(), round.members.end(), evidence.members.begin()) ||
        evidence.effect != EffectKnowledge::ConfirmedNotApplied || !evidence.oldRoundEnded ||
        !evidence.currentStateVerified || !evidence.resourcesReusable) { return; }
    FinishC1WithNoDeviceEffect(round, guard, true);
}

void AssignmentService::AcceptLateRadioEvidenceLocked(const RadioSwitchResult &result,
    std::unique_lock<std::recursive_mutex> &guard)
{
    const auto pending = pendingPrimary_.find(result.step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == result.step)) { return; }
    const auto operation = operations_.Read(result.step.request);
    if (!assignment_.OwnsRoleWriteSet(result.step.request.value, kPrimaryRoleWriteSet) ||
        !operation || !operation->context.deviceStepRegistered || result.effect == EffectKnowledge::Unknown) { return; }
    const bool earlyHandoffEvidence = operation->terminal == TerminalState::Open &&
        pending->second.mode == PrimarySwitchMode::WithoutModemRestart;
    if (!earlyHandoffEvidence && operation->terminal != TerminalState::NeedsReconciliation) { return; }
    auto &round = pending->second;
    if (round.mode == PrimarySwitchMode::ThreePhaseProtocol) {
        const auto member = std::find_if(round.members.begin(), round.members.end(),
            [&](const auto &entry) { return entry.route == result.memberSlot; });
        if (member == round.members.end() || result.phase != round.phase ||
            result.notify != (round.phase == 2) || result.protocolSession != round.protocolSession) {
            return;
        }
        const auto &binding = result.expectedBinding;
        const auto &expected = round.facts.binding;
        if (member->nativeMember == round.targetMember ? (!binding || !(binding->subscription == expected.subscription) ||
            !(binding->slot == expected.slot) || binding->bindingRevision != expected.bindingRevision) : binding.has_value()) {
            return;
        }
        const auto [saved, inserted] = round.lateC1Evidence.emplace(result.memberSlot, result);
        if (!inserted && result.recoveryVerified && !saved->second.recoveryVerified) {
            saved->second = result;
        }
        if (round.phase == 1 && operation->wait == WaitState::TimedOut &&
            round.lateC1Evidence.size() == round.members.size() &&
            std::all_of(round.lateC1Evidence.begin(), round.lateC1Evidence.end(), [](const auto &entry) {
                return entry.second.recoveryVerified && entry.second.effect == EffectKnowledge::ConfirmedNotApplied;
            })) {
            FinishC1WithNoDeviceEffect(round, guard);
        }
        return;
    }
    if (!result.expectedBinding ||
        !(result.expectedBinding->subscription == pending->second.facts.binding.subscription) ||
        !(result.expectedBinding->slot == pending->second.facts.binding.slot) ||
        result.expectedBinding->bindingRevision != pending->second.facts.binding.bindingRevision ||
        result.phase != pending->second.phase || result.notify != (pending->second.phase == 2) ||
        result.protocolSession != pending->second.protocolSession ||
        std::none_of(pending->second.members.begin(), pending->second.members.end(), [&](const auto &member) {
            return member.nativeMember == pending->second.targetMember && member.route == result.memberSlot;
        })) {
        return;
    }
    // The pending original retains the evidence even if R recovered the transport
    // before the original Submit returned. Store and role responsibility remain with A.
    if (!pending->second.lateDeviceEffect.has_value()) {
        pending->second.lateDeviceEffect = result.effect;
    }
    if (operation->wait == WaitState::TimedOut && pending->second.mode == PrimarySwitchMode::WithoutModemRestart &&
        store_.SupportsTrackedC0() && pending->second.lateDeviceEffect == EffectKnowledge::ConfirmedNotApplied) {
        FinishPrimaryWithNoDeviceEffect(result.step);
    }
}

std::optional<EffectKnowledge> AssignmentService::ReadPrimaryDeviceEvidence(const StepRef &original) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    const auto pending = pendingPrimary_.find(original.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == original)) { return std::nullopt; }
    return pending->second.lateDeviceEffect;
}

bool AssignmentService::AcceptC1RetryDue(RequestId request)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    return AcceptC1RetryDue(request, guard);
}

bool AssignmentService::AcceptC1RetryDue(RequestId request, std::unique_lock<std::recursive_mutex> &guard)
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
    if (retrySchedulerBound_ && pending->second.retryScheduled) { return false; }
    const auto previousStep = pending->second.step;
    const auto target = pending->second.target;
    const auto facts = pending->second.facts;
    pending->second.retryReady = false;
    std::optional<PrimaryRadioObservation> observed;
    guard.unlock();
    try {
        observed = radio_.ReadPrimaryRadioObservation(target.slot);
    } catch (const std::exception &) {
    }
    guard.lock();
    const auto original = pendingPrimary_.find(request.value);
    if (original == pendingPrimary_.end() || !(original->second.step == previousStep)) { return false; }
    auto &round = original->second;
    const auto stopBeforeNewRound = [&] {
        FailC1Preparation(request, *target.radioDomain, TransitionPhase::Reconciling, round.completion, guard);
        pendingPrimary_.erase(original);
        return true;
    };
    if (stopping_ || !observed || !(observed->slot == target.slot) || observed->observationRevision == 0 ||
        !observed->currentModemId || *observed->currentModemId <= 0 || observed->vendorMappingRequired ||
        !observed->ordinarySwitchAllowed ||
        !observed->routing || !topology_.WithCurrentPrimaryTarget(facts, [] {})) {
        return stopBeforeNewRound();
    }
    const auto &routing = *observed->routing;
    const auto targetMember = std::find_if(routing.members.begin(), routing.members.end(),
        [&](const auto &member) { return member.nativeMember == routing.targetMember; });
    if (targetMember == routing.members.end() || !CanPlanC1Capabilities(*observed, *targetMember)) {
        return stopBeforeNewRound();
    }

    StepRef step {request, previousStep.step + 1, round.attemptCount};
    if (!operations_.AdvanceStep(previousStep, step.step, step.attempt, std::nullopt)) { return false; }
    if (store_.SupportsTrackedC0()) {
        if (!PrepareTrackedC1Parameters(step, facts, round.oldParameters, std::nullopt, guard)) {
            return stopBeforeNewRound();
        }
    } else {
        const auto primary = store_.WritePrimaryParameters(target.slot, facts.iccid);
        if (!primary.callFinished) {
            assignment_.AcceptTransitionPhase(*target.radioDomain, request.value,
                TransitionPhase::Reconciling, TransitionPhase::NeedsReconciliation);
            operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
            return true;
        }
    }
    if (!operations_.AdvanceStep(step, step.step + 1, step.attempt, std::nullopt)) { return false; }
    ++step.step;
    round.step = step;
    round.protocolSession = nextProtocolSession_++;
    round.targetMember = targetMember->nativeMember;
    round.members = routing.members;
    if (!operations_.RecordRadioStep(step) ||
        !assignment_.AcceptTransitionPhase(*target.radioDomain, request.value,
            TransitionPhase::Reconciling, TransitionPhase::DevicePending)) {
        return false;
    }
    round.phase = 1;
    round.roundFailed = false;
    round.completedMembers.clear();
    round.lateDeviceEffect.reset();
    round.lateC1Evidence.clear();
    guard.unlock();
    try {
        output_.PublishPrimaryPhase(false, false);
    } catch (const std::exception &) {
        // The prepared round remains owned while the original phase notification unwinds.
    }
    guard.lock();
    return SubmitC1Check(step, guard);
}

RadioSubmitDisposition AssignmentService::SubmitRadioOutsideCommit(const RadioSwitchRequest &device,
    const PrimaryTargetFacts &facts,
    std::unique_lock<std::recursive_mutex> &guard)
{
    // The original step and its write qualification are already registered.
    guard.unlock();
    auto handoff = RadioSubmitDisposition::HandoffUnknown;
    try {
        handoff = radio_.SwitchPrimaryRadio(device, [&](const RadioPermissionRegistration &registerPermission) {
            std::lock_guard<std::recursive_mutex> commit(primaryCommitLock_);
            const auto pending = pendingPrimary_.find(device.step.request.value);
            const auto operation = operations_.Read(device.step.request);
            if (stopping_ || pending == pendingPrimary_.end() || !(pending->second.step == device.step) ||
                pending->second.protocolSession != device.protocolSession || pending->second.phase != device.phase ||
                !operation || operation->terminal != TerminalState::Open ||
                !assignment_.OwnsRoleWriteSet(device.step.request.value, kPrimaryRoleWriteSet)) {
                return false;
            }
            bool permitted = false;
            return topology_.WithCurrentPrimaryTarget(facts, [&] {
                permitted = registerPermission([this] { return !IsStoppingPrimaryRequests(); });
            }) && permitted;
        });
    } catch (const std::exception &) {
        // Crossing the platform boundary may leave an unknown effect.
    }
    guard.lock();
    return handoff;
}

bool AssignmentService::SubmitC1Round(const PendingPrimarySwitch &round,
    std::unique_lock<std::recursive_mutex> &guard)
{
    // A synchronous result may advance or retire this round before Submit returns.
    const StepRef step = round.step;
    const auto domain = round.target.radioDomain.value();
    const auto targetMember = round.targetMember;
    const auto facts = round.facts;
    const uint32_t protocolSession = round.protocolSession;
    const uint32_t phase = round.phase;
    const auto members = round.members;
    const auto primaryCapability = std::find_if(members.begin(), members.end(),
        [](const RadioProtocolMember &member) { return member.capability->modemId == 0; })->capability.value();
    const auto targetCapability = std::find_if(members.begin(), members.end(),
        [&](const RadioProtocolMember &member) { return member.nativeMember == targetMember; })->capability.value();
    const auto status = phase == 4 ? (round.roundFailed ? RadioProtocolStatus::Fail :
        RadioProtocolStatus::Success) : RadioProtocolStatus::None;
    const auto stillCurrent = [&] {
        const auto active = pendingPrimary_.find(step.request.value);
        if (active == pendingPrimary_.end() || !(active->second.step == step)) { return false; }
        const auto operation = operations_.Read(step.request);
        return operation && operation->terminal == TerminalState::Open;
    };
    bool issued = false;
    for (const auto &member : members) {
        if (!stillCurrent()) { return true; }
        auto capability = *member.capability;
        if (phase == 2 || status == RadioProtocolStatus::Success) {
            if (member.nativeMember == targetMember) { capability = primaryCapability; }
            if (member.capability->modemId == 0) { capability = targetCapability; }
        }
        const RadioSwitchRequest device {step, member.route, domain, PrimarySwitchMode::ThreePhaseProtocol,
            member.nativeMember == targetMember ? std::optional<BindingRef>{facts.binding} : std::nullopt,
            protocolSession, phase, static_cast<uint32_t>(members.size()), member, RadioProtocolPayload {capability, status},
            members};
        const auto handoff = SubmitRadioOutsideCommit(device, facts, guard);
        if (handoff != RadioSubmitDisposition::HandoffAccepted) {
            if (!issued && phase == 1 && handoff == RadioSubmitDisposition::ConfirmedNotSent) {
                const auto original = pendingPrimary_.find(step.request.value);
                if (original != pendingPrimary_.end() && original->second.step == step) {
                    FinishC1WithNoDeviceEffect(original->second, guard);
                }
                return false;
            }
            if (!stillCurrent()) { return true; }
            assignment_.AcceptTransitionPhase(domain, step.request.value, TransitionPhase::DevicePending,
                TransitionPhase::NeedsReconciliation);
            operations_.MarkTerminal(step.request, TerminalState::NeedsReconciliation);
            return false;
        }
        issued = true;
    }
    return true;
}

bool AssignmentService::SubmitC1Check(const StepRef &step, std::unique_lock<std::recursive_mutex> &guard)
{
    const auto pending = pendingPrimary_.find(step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == step)) { return false; }
    pending->second.originalC1Check = step;
    pending->second.c1PhaseEndStarted = false;
    const bool armed = ArmC1ProtocolDeadline(step, guard);
    const auto original = pendingPrimary_.find(step.request.value);
    if (original == pendingPrimary_.end() || !(original->second.step == step)) { return false; }
    if (!armed) {
        FinishC1WithNoDeviceEffect(original->second, guard);
        return false;
    }
    return SubmitC1Round(original->second, guard);
}

void AssignmentService::FinishC1WithNoDeviceEffect(PendingPrimarySwitch &round,
    std::unique_lock<std::recursive_mutex> &guard, bool wholeRoundVerified)
{
    const auto original = round.step;
    const auto request = original.request;
    const auto domain = *round.target.radioDomain;
    const auto operation = operations_.Read(request);
    const bool reconciling = operation && operation->terminal == TerminalState::NeedsReconciliation;
    const bool timedOut = reconciling && operation->wait == WaitState::TimedOut;
    if (timedOut && round.timeoutDeliveryInFlight) {
        round.noDeviceEffect = true;
        return;
    }
    if (reconciling && !timedOut) {
        if (!wholeRoundVerified || !operations_.AcceptLateEvidence(original, EffectKnowledge::ConfirmedNotApplied)) {
            return;
        }
        operations_.EndWait(request, WaitState::Delivered);
    } else if (!reconciling) {
        if (operations_.AcceptResult(round.step, wholeRoundVerified ? EffectKnowledge::ConfirmedNotApplied :
            EffectKnowledge::NotSent) !=
            RuntimeOperationStore::ResultAdmission::Accepted) { return; }
        operations_.EndWait(request, WaitState::Delivered);
    }
    const bool restored = RestoreTrackedPrimary(round.step, round.facts, round.oldParameters, guard);
    if (restored) { AcceptRestoredPrimaryProjection(round.oldParameters); }
    std::optional<C0Stage> outstanding;
    if (!restored && c0Receipt_ && c0Receipt_->request == request) {
        outstanding = c0Receipt_->outstanding;
        c0Receipt_->outstanding = C0Stage::Completion;
    }
    if (!timedOut) { FinishC1Phase(original, domain, guard); }
    if (outstanding) { c0Receipt_->outstanding = *outstanding; }
    if (!restored && !c0Receipt_) {
        assignment_.AcceptTransitionPhase(domain, request.value, TransitionPhase::DevicePending,
            TransitionPhase::NeedsReconciliation);
        CompleteOnce(request, round.completion, 83886087, TerminalState::NeedsReconciliation, &guard);
        return;
    }
    if (reconciling && !timedOut && !c0Receipt_) {
        CompleteOnce(request, round.completion, 83886087, TerminalState::NeedsReconciliation, &guard);
    }
    FailC1Preparation(request, domain, reconciling ? TransitionPhase::NeedsReconciliation :
        TransitionPhase::DevicePending, round.completion, guard);
    pendingPrimary_.erase(request.value);
}

std::optional<uint64_t> AssignmentService::ReadLegacyPrimaryWaitBudget(RequestId request) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    const auto pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step.request == request)) { return std::nullopt; }
    return primaryScope_ == PrimaryScope::TrackedHost && pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol ?
        LEGACY_ORDINARY_PRIMARY_WAIT_MS : LEGACY_DEDICATED_PRIMARY_WAIT_MS;
}

bool AssignmentService::AcceptPrimarySwitchTimeout(RequestId request, uint64_t elapsedMilliseconds)
{
    std::unique_lock<std::recursive_mutex> guard(primaryCommitLock_);
    const auto budget = ReadLegacyPrimaryWaitBudget(request);
    return budget && elapsedMilliseconds >= *budget && ExpirePrimarySwitch(request, false, guard);
}

bool AssignmentService::ExpirePrimarySwitch(RequestId request, bool protocolDeadline,
    std::unique_lock<std::recursive_mutex> &guard)
{
    const auto pending = pendingPrimary_.find(request.value);
    if (pending == pendingPrimary_.end()) { return false; }
    const auto operation = operations_.Read(request);
    const auto step = pending->second.step;
    const bool c1 = pending->second.mode == PrimarySwitchMode::ThreePhaseProtocol;
    const bool retryDeadline = c1 && protocolDeadline && step.attempt != 0;
    if (!assignment_.OwnsRoleWriteSet(request.value, kPrimaryRoleWriteSet) ||
        !operation || !operation->context.deviceStepRegistered || !(step.request == request) ||
        (operation->wait != WaitState::Waiting && !(retryDeadline && operation->wait == WaitState::Delivered))) {
        return false;
    }
    if (operation->terminal == TerminalState::Open) {
        if (operations_.AcceptResult(step, EffectKnowledge::Unknown) !=
            RuntimeOperationStore::ResultAdmission::Accepted) { return false; }
    } else if (operation->terminal != TerminalState::NeedsReconciliation) {
        return false;
    }
    const auto domain = pending->second.target.radioDomain.value();
    const auto completion = pending->second.completion;
    const bool userSet = pending->second.userSet;
    operations_.EndWait(request, WaitState::TimedOut);
    assignment_.AcceptTransitionPhase(domain, request.value,
        TransitionPhase::DevicePending, TransitionPhase::NeedsReconciliation);
    operations_.MarkTerminal(request, TerminalState::NeedsReconciliation);
    const bool deliver = operations_.ClaimCompletion(request);
    pending->second.timeoutDeliveryInFlight = true;
    if (c1) { FinishC1Phase(step, domain, guard); }
    guard.unlock();
    if (!c1) {
        try {
            radio_.EndLegacySenseSwitch(step, domain);
        } catch (const std::exception &) {
            // R retains the original resource/effect responsibility.
        }
        try {
            output_.PublishPrimaryPhase(true, userSet);
        } catch (const std::exception &) {
            // Failure to publish the phase does not revoke the original timeout result.
        }
    }
    if (deliver) {
        try {
            completion_.CompleteLegacyInt(completion, 83886087);
        } catch (const std::exception &) {
            // The original delivery was claimed already. A later call must not repeat it.
        }
    }
    guard.lock();
    const auto original = pendingPrimary_.find(request.value);
    if (original != pendingPrimary_.end() && original->second.step == step) {
        original->second.timeoutDeliveryInFlight = false;
        if (c1 && original->second.noDeviceEffect) {
            FinishC1WithNoDeviceEffect(original->second, guard);
            return true;
        }
        const bool verifiedNoEffect = original->second.mode == PrimarySwitchMode::WithoutModemRestart &&
            store_.SupportsTrackedC0() && original->second.lateDeviceEffect == EffectKnowledge::ConfirmedNotApplied;
        if (original->second.noDeviceEffect || verifiedNoEffect) { FinishPrimaryWithNoDeviceEffect(step); }
    }
    return true;
}

void AssignmentService::FinishPrimaryWithNoDeviceEffect(const StepRef &step)
{
    const auto pending = pendingPrimary_.find(step.request.value);
    if (pending == pendingPrimary_.end() || !(pending->second.step == step)) { return; }
    if (pending->second.timeoutDeliveryInFlight) {
        pending->second.noDeviceEffect = true;
        return;
    }
    assignment_.AcceptTransitionPhase(pending->second.target.radioDomain.value(), step.request.value,
        TransitionPhase::NeedsReconciliation, TransitionPhase::Failed);
    assignment_.ReleaseRoleWriteSet(step.request.value, kPrimaryRoleWriteSet);
    operations_.RetireAfterEvidenceRetention(step.request, true);
    pendingPrimary_.erase(pending);
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
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    if ((!input.existingSubscription || input.useLegacySingleSlotRoles) &&
        (providerReceipts_[0] || providerReceipts_[1])) { return std::nullopt; }
    return accountRoles_.Prepare(input);
}
bool AssignmentService::AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned)
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.AcceptDiscovered(planned, assigned);
}
bool AssignmentService::StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.StillOwns(planned);
}
void AssignmentService::ConfirmInitialRoleReceipt(const AccountRoleFragment &planned,
    const InitialRoleFinalizationEvidence &evidence)
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    accountRoles_.ConfirmReceipt(planned, evidence);
}

std::optional<ConfirmedDiscoveryRoles> AssignmentService::ReadConfirmedDiscoveryRoles(SubscriptionId id) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.ReadConfirmedDiscoveryRoles(id);
}

size_t AssignmentService::PendingDiscoveryRoles() const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.PendingDiscoveryRoles();
}

bool AssignmentService::FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
    const InitialRoleFinalizationEvidence &evidence)
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.Finalize(planned, evidence);
}
std::optional<StoredAccountRoleFragment> AssignmentService::ReadStoredAccountRoles(SubscriptionId id,
    bool historical) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    const auto &projection = accountRoles_.Projection(historical);
    const auto row = projection.find(id);
    return row == projection.end() ? std::nullopt : std::make_optional(row->second);
}

uint64_t AssignmentService::ReadRoleProjectionRevision(bool historicalView) const
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    return accountRoles_.Revision(historicalView);
}
void AssignmentService::AcceptReloadedRoleProjectionSet(const RoleReloadDelivery &delivery)
{
    std::lock_guard<std::recursive_mutex> guard(primaryCommitLock_);
    if (delivery.epoch != reloadEpoch_ || providerReceipts_[0] || providerReceipts_[1] ||
        accountRoles_.HasPendingStorageRoles()) {
        auto expected = RoleReloadDisposition::Available;
        (void)delivery.receipt.compare_exchange_strong(expected, RoleReloadDisposition::Rejected);
        return;
    }
    accountRoles_.AcceptReloadSet(delivery);
}

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
