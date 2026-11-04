// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
namespace SimGuard {
// Wire delivery controls only. They never read or repair an SUT observation.
class RilFaultControls {
public:
    enum class StatusFault { None, Malformed, Drop };
    enum class PinFault { None, GenericTransportError, Hold };
    void NextStatus(StatusFault value) { std::lock_guard<std::mutex> g(lock_); statusFault_ = value; }
    StatusFault TakeStatusFault()
    {
        std::lock_guard<std::mutex> g(lock_);
        auto result = statusFault_; statusFault_ = StatusFault::None;
        return result;
    }
    void StatusRequested()
    {
        std::lock_guard<std::mutex> g(lock_); ++statusRequests_; changed_.notify_all();
    }
    unsigned StatusRevision() { std::lock_guard<std::mutex> g(lock_); return statusRequests_; }
    bool AwaitStatusAfter(unsigned revision)
    {
        std::unique_lock<std::mutex> g(lock_);
        return changed_.wait_for(g, std::chrono::seconds(2), [&] { return statusRequests_ > revision; });
    }
    void NextPin(PinFault value) { std::lock_guard<std::mutex> g(lock_); pinFault_ = value; }
    PinFault TakePinFault()
    {
        std::lock_guard<std::mutex> g(lock_); auto result = pinFault_; pinFault_ = PinFault::None; return result;
    }
    void HoldPin(std::function<void()> response)
    {
        std::lock_guard<std::mutex> g(lock_);
        if (heldPin_) throw std::logic_error("PIN response already held");
        heldPin_ = std::move(response); changed_.notify_all();
    }
    bool AwaitHeldPin()
    {
        std::unique_lock<std::mutex> g(lock_);
        return changed_.wait_for(g, std::chrono::seconds(2), [&] { return static_cast<bool>(heldPin_); });
    }
    void ReleasePin()
    {
        std::function<void()> response;
        { std::lock_guard<std::mutex> g(lock_); response = std::move(heldPin_); }
        if (response) response();
    }
    void SetPukRemaining(int remaining)
    {
        std::lock_guard<std::mutex> g(lock_); pukRemaining_ = remaining;
    }
    UnlockResult PukReply(const std::string &puk, const std::string &newPin)
    {
        std::lock_guard<std::mutex> g(lock_);
        if (newPin.size() != 4 || newPin.find_first_not_of("0123456789") != std::string::npos)
            return {-1, Unlock::Failure, pukRemaining_};
        return {0, puk == "12345678" ? Unlock::Ok : Unlock::Incorrect, pukRemaining_};
    }
private:
    std::mutex lock_;
    std::condition_variable changed_;
    StatusFault statusFault_ = StatusFault::None;
    PinFault pinFault_ = PinFault::None;
    std::function<void()> heldPin_;
    unsigned statusRequests_ = 0;
    int pukRemaining_ = 7; // fixture-supplied device data, not a product attempt limit
};
}
