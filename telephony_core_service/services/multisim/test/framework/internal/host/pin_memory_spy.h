#pragma once
#include "telephony_ext_wrapper.h"
#include <gtest/gtest.h>
#include <atomic>
#include <mutex>
namespace SimGuard {
namespace {
using OHOS::Telephony::PinOperationType;
using OHOS::Telephony::TelephonyExtWrapper;
using Hook = OHOS::Telephony::TelephonyExtWrapper::CacheAssetPinForUpgrade;

// Expected credentials are synthetic and remain in memory. Only boolean comparison
// results and call counts reach GoogleTest; never print either actual or expected PIN.
class PinMemorySpy {
public:
    PinMemorySpy() : previous_(TELEPHONY_EXT_WRAPPER.cacheAssetPinForUpgrade_)
    {
        PinMemorySpy *empty = nullptr;
        if (!active_.compare_exchange_strong(empty, this))
            throw std::logic_error("PIN memory observer already installed");
        TELEPHONY_EXT_WRAPPER.cacheAssetPinForUpgrade_ = Receive;
    }
    ~PinMemorySpy()
    {
        TELEPHONY_EXT_WRAPPER.cacheAssetPinForUpgrade_ = previous_;
        active_.store(nullptr);
    }
    void Expect(int slot, std::string identity, PinOperationType operation, std::string credential)
    {
        std::lock_guard<std::mutex> guard(lock_);
        expectedSlot_ = slot;
        expectedIdentity_ = std::move(identity);
        expectedOperation_ = operation;
        expectedCredential_ = std::move(credential);
        armed_ = true;
        matched_ = true;
        calls_ = 0;
    }
    void Assert(unsigned calls)
    {
        std::lock_guard<std::mutex> guard(lock_);
        EXPECT_EQ(calls_, calls);
        EXPECT_TRUE(matched_) << "PIN memory hook contract mismatch (payload suppressed)";
    }
private:
    static void Receive(int32_t slot, const std::string &identity,
        PinOperationType operation, const std::string &credential)
    {
        auto *self = active_.load();
        if (!self) throw std::logic_error("PIN memory observer unavailable");
        std::lock_guard<std::mutex> guard(self->lock_);
        ++self->calls_;
        self->matched_ = self->matched_ && self->armed_ && slot == self->expectedSlot_ &&
            identity == self->expectedIdentity_ && operation == self->expectedOperation_ &&
            credential == self->expectedCredential_;
    }
    static std::atomic<PinMemorySpy *> active_;
    Hook previous_;
    std::mutex lock_;
    int expectedSlot_ = -1;
    std::string expectedIdentity_;
    PinOperationType expectedOperation_ = PinOperationType::SIM_ABSENT;
    std::string expectedCredential_;
    bool armed_ = false;
    bool matched_ = true;
    unsigned calls_ = 0;
};
std::atomic<PinMemorySpy *> PinMemorySpy::active_{nullptr};
}

}
