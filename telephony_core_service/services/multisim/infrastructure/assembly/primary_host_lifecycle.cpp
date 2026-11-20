/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "infrastructure/assembly/primary_host_lifecycle.h"

namespace OHOS::Telephony::MultiSim {
PrimaryHostLifecycle::PrimaryHostLifecycle(AssignmentService &assignment, ResourceRadioRuntime &radio,
    RetrySchedulerPort &scheduler, const std::shared_ptr<RadioOwnerDispatchPort> &ownerDispatch)
    : assignment_(assignment), radio_(radio), scheduler_(scheduler), ownerDispatch_(ownerDispatch)
{}

PrimaryHostLifecycle::DrainResult PrimaryHostLifecycle::StopAcceptingAndDrain()
{
    bool sealGates = false;
    bool postClose = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (stopped_) { return DrainResult::Drained; }
        stopping_ = true;
        if (!closePosted_) { closePosted_ = true; postClose = true; }
        if (!gateSealStarted_ && !gatesSealed_) { gateSealStarted_ = true; sealGates = true; }
    }
    if (sealGates) {
        // New admission is closed on caller-safe gates before any Owner mutation
        // is posted. The Owner task below is only the serial A commit/cancel step.
        assignment_.RequestStopAccepting();
        scheduler_.StopAcceptingAndCancel();
        (void)radio_.StopAccepting();
        std::lock_guard<std::mutex> guard(mutex_);
        gatesSealed_ = true;
    }
    if (postClose) {
        const auto dispatch = ownerDispatch_.lock();
        if (!dispatch || !dispatch->PostRadioOwnerTask([this] {
            (void)assignment_.StopAcceptingAndCancelScheduledRetries();
            std::lock_guard<std::mutex> guard(mutex_);
            sealsComplete_ = true;
        })) {
            std::lock_guard<std::mutex> guard(mutex_);
            closePosted_ = false;
            return DrainResult::CallbacksStillInFlight;
        }
    }
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!gatesSealed_ || !sealsComplete_) { return DrainResult::CallbacksStillInFlight; }
    }
    if (!radio_.OwnerCallbacksDrained()) {
        return DrainResult::CallbacksStillInFlight;
    }
    const auto scheduler = scheduler_.Audit();
    if (scheduler.pending != 0 || scheduler.running != 0) {
        return DrainResult::CallbacksStillInFlight;
    }
    if (!radio_.DeviceResourcesDrained() || assignment_.HasPendingDeviceEffects()) {
        return DrainResult::DeviceEffectsUnresolved;
    }
    {
        std::lock_guard<std::mutex> guard(mutex_);
        stopped_ = true;
    }
    return DrainResult::Drained;
}

bool PrimaryHostLifecycle::IsStopped() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return stopped_;
}
} // namespace OHOS::Telephony::MultiSim
