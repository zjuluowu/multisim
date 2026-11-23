// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core_service_sim.h"
#include "sim_manager.h"
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Sim028Host {
// Narrow Host-only bridge for the real CoreServiceSim and real Legacy SimManager.
// It is not a production Core coordinator and deliberately has no RIL stop right.
class CoreLegacyHostStop final {
public:
    enum class Result { Drained, CoreCallbacksInFlight, ManagerStopFailed, AlreadyStopping };
    CoreLegacyHostStop(OHOS::Telephony::CoreServiceSim &core,
        const std::shared_ptr<OHOS::Telephony::SimManager> &manager)
        : core_(core), manager_(manager) {}

    bool WaitUntilStopping(std::chrono::milliseconds timeout = std::chrono::seconds(3))
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, timeout, [this] { return stopping_ || drained_; });
    }

    Result StopAndDrain(std::chrono::milliseconds timeout = std::chrono::seconds(3))
    {
        std::shared_ptr<OHOS::Telephony::SimManager> manager;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (drained_) { return Result::Drained; }
            if (stopping_) { return Result::AlreadyStopping; }
            stopping_ = true;
            changed_.notify_all();
            manager = manager_;
        }
        core_.StopAccepting();
        if (!core_.WaitForAcceptedCallsDrained(timeout)) {
            FinishAttempt(false);
            return Result::CoreCallbacksInFlight;
        }
        if (!manager || !manager->Stop()) {
            FinishAttempt(false);
            return Result::ManagerStopFailed;
        }
        FinishAttempt(true);
        return Result::Drained;
    }
private:
    void FinishAttempt(bool drained)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        drained_ = drained;
        stopping_ = false;
    }
    OHOS::Telephony::CoreServiceSim &core_;
    std::shared_ptr<OHOS::Telephony::SimManager> manager_;
    std::mutex mutex_;
    std::condition_variable changed_;
    bool stopping_ {false};
    bool drained_ {false};
};
} // namespace Sim028Host
