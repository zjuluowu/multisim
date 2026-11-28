/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "infrastructure/assembly/primary_host_lifecycle.h"

#include <tuple>

namespace OHOS::Telephony::MultiSim {
PrimaryHostLifecycle::PrimaryHostLifecycle(const std::shared_ptr<AssignmentService> &assignment,
    ResourceRadioRuntime &radio, RetrySchedulerPort &scheduler,
    const std::shared_ptr<RadioOwnerDispatchPort> &ownerDispatch)
    : assignment_(assignment), radio_(radio), scheduler_(scheduler), ownerDispatch_(ownerDispatch),
      state_(std::make_shared<State>())
{}

PrimaryHostLifecycle::DrainResult PrimaryHostLifecycle::StopAcceptingAndDrain()
{
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (state_->stopped) { return DrainResult::Drained; }
    }
    const auto observation = ObserveClose();
    if (observation.result == DrainResult::Drained) {
        std::lock_guard<std::mutex> guard(state_->mutex);
        state_->stopped = true;
    }
    return observation.result;
}

bool PrimaryHostLifecycle::CloseObservation::SameSchedulerActivity(const CloseObservation &other) const
{
    return std::tie(scheduler.scheduled, scheduler.delivered, scheduler.cancelled, scheduler.rejected) ==
        std::tie(other.scheduler.scheduled, other.scheduler.delivered, other.scheduler.cancelled,
            other.scheduler.rejected);
}

PrimaryHostLifecycle::CloseObservation PrimaryHostLifecycle::ObserveClose()
{
    bool sealGates = false;
    bool sealScheduler = false;
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (state_->gateSealStarted) { return {DrainResult::CallbacksStillInFlight}; }
        if (!state_->gatesSealed) {
            state_->gateSealStarted = true;
            sealGates = true;
            sealScheduler = !state_->schedulerSealed;
        }
    }
    if (sealGates) {
        bool sealed = false;
        try {
            if (const auto assignment = assignment_.lock()) {
                assignment->RequestStopAccepting();
                if (sealScheduler) {
                    scheduler_.StopAcceptingAndCancel();
                    std::lock_guard<std::mutex> guard(state_->mutex);
                    state_->schedulerSealed = true;
                }
                (void)radio_.StopAccepting();
                sealed = true;
            }
        } catch (const std::exception &) {
            // The module's new admission remains closed. A later observer retries
            // the unacknowledged gate before offering any work to the Owner.
        }
        std::lock_guard<std::mutex> guard(state_->mutex);
        state_->gateSealStarted = false;
        state_->gatesSealed = sealed;
        if (!sealed) { return {DrainResult::CallbacksStillInFlight}; }
    }

    bool postClose = false;
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (!state_->closePosted) {
            state_->closePosted = true;
            postClose = true;
        }
    }
    if (postClose) {
        const auto dispatch = ownerDispatch_.lock();
        const auto assignment = assignment_.lock();
        const auto state = state_;
        if (!dispatch || !assignment || !dispatch->PostRadioOwnerTask([assignment, state] {
            (void)assignment->StopAcceptingAndCancelScheduledRetries();
            std::lock_guard<std::mutex> guard(state->mutex);
            state->sealsComplete = true;
        })) {
            std::lock_guard<std::mutex> guard(state_->mutex);
            state_->closePosted = false;
            return {DrainResult::CallbacksStillInFlight};
        }
    }
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (!state_->gatesSealed || !state_->sealsComplete) {
            return {DrainResult::CallbacksStillInFlight};
        }
    }
    if (!radio_.OwnerCallbacksDrained()) {
        return {DrainResult::CallbacksStillInFlight};
    }
    const auto scheduler = scheduler_.Audit();
    if (scheduler.accepting || scheduler.pending != 0 || scheduler.running != 0) {
        return {DrainResult::CallbacksStillInFlight, scheduler};
    }
    const auto assignment = assignment_.lock();
    if (!assignment || !radio_.DeviceResourcesDrained()) {
        return {DrainResult::DeviceEffectsUnresolved, scheduler};
    }
    const auto effects = assignment->ReadPrimaryCloseEffects();
    if (effects.device) { return {DrainResult::DeviceEffectsUnresolved, scheduler}; }
    if (effects.store) { return {DrainResult::BusinessEffectsUnresolved, scheduler}; }
    return {DrainResult::Drained, scheduler};
}

bool PrimaryHostLifecycle::IsStopped() const
{
    std::lock_guard<std::mutex> guard(state_->mutex);
    return state_->stopped;
}
} // namespace OHOS::Telephony::MultiSim
