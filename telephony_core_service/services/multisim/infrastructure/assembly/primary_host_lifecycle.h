/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_PRIMARY_HOST_LIFECYCLE_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_PRIMARY_HOST_LIFECYCLE_H

#include <mutex>
#include "device/resource/resource_radio_runtime.h"
#include "infrastructure/request/runtime_operation_store.h"
#include "role_assignment/assignment_service.h"

namespace OHOS::Telephony::MultiSim {
// Narrow H-side Host coordinator for the already-implemented Primary A/R slice.
// It does not own Core, shared RIL, public Manager API, or device shutdown.
class PrimaryHostLifecycle final {
public:
    enum class DrainResult { Drained, CallbacksStillInFlight, DeviceEffectsUnresolved, BusinessEffectsUnresolved };
    PrimaryHostLifecycle(const std::shared_ptr<AssignmentService> &assignment, ResourceRadioRuntime &radio,
        RetrySchedulerPort &scheduler, const std::shared_ptr<RadioOwnerDispatchPort> &ownerDispatch);
    DrainResult StopAcceptingAndDrain();
    bool IsStopped() const;
private:
    std::weak_ptr<AssignmentService> assignment_;
    ResourceRadioRuntime &radio_;
    RetrySchedulerPort &scheduler_;
    std::weak_ptr<RadioOwnerDispatchPort> ownerDispatch_;
    struct State {
        std::mutex mutex;
        bool stopping {false};
        bool sealsComplete {false};
        bool closePosted {false};
        bool gateSealStarted {false};
        bool gatesSealed {false};
        bool stopped {false};
    };
    std::shared_ptr<State> state_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_PRIMARY_HOST_LIFECYCLE_H
