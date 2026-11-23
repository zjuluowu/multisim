/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "infrastructure/assembly/primary_host_lifecycle.h"

namespace OHOS::Telephony::MultiSim {
PrimaryHostLifecycle::PrimaryHostLifecycle(const std::shared_ptr<AssignmentService> &assignment,
    ResourceRadioRuntime &radio, RetrySchedulerPort &scheduler,
    const std::shared_ptr<RadioOwnerDispatchPort> &ownerDispatch)
    : assignment_(assignment), radio_(radio), scheduler_(scheduler), ownerDispatch_(ownerDispatch),
      state_(std::make_shared<State>())
{}

PrimaryHostLifecycle::DrainResult PrimaryHostLifecycle::StopAcceptingAndDrain()
{
    bool sealGates = false;
    bool postClose = false;
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (state_->stopped) { return DrainResult::Drained; }
        state_->stopping = true;
        if (!state_->closePosted) { state_->closePosted = true; postClose = true; }
        if (!state_->gateSealStarted && !state_->gatesSealed) {
            state_->gateSealStarted = true; sealGates = true;
        }
    }
    if (sealGates) {
        // New admission is closed on caller-safe gates before any Owner mutation
        // is posted. The Owner task below is only the serial A commit/cancel step.
        const auto assignment = assignment_.lock();
        if (!assignment) { return DrainResult::CallbacksStillInFlight; }
        assignment->RequestStopAccepting();
        scheduler_.StopAcceptingAndCancel();
        (void)radio_.StopAccepting();
        std::lock_guard<std::mutex> guard(state_->mutex);
        state_->gatesSealed = true;
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
            return DrainResult::CallbacksStillInFlight;
        }
    }
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        if (!state_->gatesSealed || !state_->sealsComplete) {
            return DrainResult::CallbacksStillInFlight;
        }
    }
    if (!radio_.OwnerCallbacksDrained()) {
        return DrainResult::CallbacksStillInFlight;
    }
    const auto scheduler = scheduler_.Audit();
    if (scheduler.pending != 0 || scheduler.running != 0) {
        return DrainResult::CallbacksStillInFlight;
    }
    const auto assignment = assignment_.lock();
    if (!assignment || !radio_.DeviceResourcesDrained() || assignment->HasPendingDeviceEffects()) {
        return DrainResult::DeviceEffectsUnresolved;
    }
    {
        std::lock_guard<std::mutex> guard(state_->mutex);
        state_->stopped = true;
    }
    return DrainResult::Drained;
}

bool PrimaryHostLifecycle::IsStopped() const
{
    std::lock_guard<std::mutex> guard(state_->mutex);
    return state_->stopped;
}
} // namespace OHOS::Telephony::MultiSim
