/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "infrastructure/assembly/primary_host_lifecycle.h"

namespace OHOS::Telephony::MultiSim {
PrimaryHostLifecycle::PrimaryHostLifecycle(AssignmentService &assignment, ResourceRadioRuntime &radio,
    RetrySchedulerPort &scheduler)
    : assignment_(assignment), radio_(radio), scheduler_(scheduler)
{}

PrimaryHostLifecycle::DrainResult PrimaryHostLifecycle::StopAcceptingAndDrain()
{
    bool initiate = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (stopped_) { return DrainResult::Drained; }
        if (!stopping_) { stopping_ = true; initiate = true; }
    }
    if (initiate) {
        // H closes every producer in a fixed order; no external callback is invoked
        // while this gate holds its own mutex.
        (void)assignment_.StopAcceptingAndCancelScheduledRetries();
        scheduler_.StopAcceptingAndCancel();
        (void)radio_.StopAccepting();
    }

    const auto scheduler = scheduler_.Audit();
    if (scheduler.pending != 0 || scheduler.running != 0 || !radio_.OwnerCallbacksDrained()) {
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
