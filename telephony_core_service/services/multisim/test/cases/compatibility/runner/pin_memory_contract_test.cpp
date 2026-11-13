// SPDX-License-Identifier: Apache-2.0
// Independent SIM-OC-013 suite. Never changes the frozen compatibility Oracle.
#include "runner/contract_driver.h"
#include "contracts/sim_session.h"
#include "fakes/ril_fault_controls.h"
#include "telephony_ext_wrapper.h"
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

class PinMemoryContract : public ContractDriver {
protected:
    RilFaultControls faults;
    std::unique_ptr<PinMemorySpy> spy;
    void SetUp() override
    {
        sut = CreateSimSessionSut(ril, faults);
        spy = std::make_unique<PinMemorySpy>();
    }
    void TearDown() override
    {
        // Drain/destroy the real SUT before restoring its external callback pointer.
        ContractDriver::TearDown();
        sut.reset();
        spy.reset();
    }
    ISimSessionAdapter &Session() { return static_cast<ISimSessionAdapter &>(*sut); }
};

TEST_F(PinMemoryContract, PinSuccessUsesOriginalSlotIdentityAndCredential)
{
    sut->Start(2);
    Change(0, {State::Ready, CardKind::Sim, "synthetic-other-card"});
    Change(1, {State::Locked, CardKind::Usim, "synthetic-pin-card", Lock::Pin});
    spy->Expect(1, "synthetic-pin-card", PinOperationType::PIN_ENABLE, "1234");
    const auto wrong = sut->UnlockPin(1, "0000");
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    spy->Assert(0);
    const auto correct = sut->UnlockPin(1, "1234");
    EXPECT_EQ(correct.error, 0);
    EXPECT_EQ(correct.result, Unlock::Ok);
    spy->Assert(1);
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Sim);
}

TEST_F(PinMemoryContract, PukSuccessUsesNewPinAndPinAlterOperation)
{
    sut->Start(1);
    Change(0, {State::Locked, CardKind::Usim, "synthetic-puk-card", Lock::Puk});
    spy->Expect(0, "synthetic-puk-card", PinOperationType::PIN_ALTER, "2468");
    const auto wrong = Session().UnlockPuk(0, "2468", "00000000");
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    spy->Assert(0);
    const auto correct = Session().UnlockPuk(0, "2468", "12345678");
    EXPECT_EQ(correct.error, 0);
    EXPECT_EQ(correct.result, Unlock::Ok);
    spy->Assert(1); // Neither the PUK nor a prior PIN may be forwarded as the new PIN.
}

TEST_F(PinMemoryContract, FailedPinDoesNotPublishMemoryHook)
{
    sut->Start(1);
    Change(0, {State::Locked, CardKind::Usim, "synthetic-failed-card", Lock::Pin});
    spy->Expect(0, "synthetic-failed-card", PinOperationType::PIN_ENABLE, "1234");
    ril.FailPin(true);
    const auto failed = sut->UnlockPin(0, "1234");
    EXPECT_NE(failed.error, 0);
    EXPECT_EQ(failed.result, Unlock::Failure);
    spy->Assert(0);
}

TEST_F(PinMemoryContract, RemovalUsesAbsentResponseIdentityWithoutCredential)
{
    sut->Start(2);
    Change(1, {State::Locked, CardKind::Usim, "synthetic-removed-card", Lock::Pin});
    // Current production updates iccid_ from the absent RIL response before the
    // hook. This characterizes its empty identity; it does not approve an asset
    // cleanup policy or claim the previous card identity survives removal.
    spy->Expect(1, "", PinOperationType::SIM_ABSENT, "");
    Change(1, {});
    spy->Assert(1);
    EXPECT_TRUE(sut->Observe().cards.at(1).iccid.empty());
    ExpectCard(sut->Observe().cards.at(1), 1, State::Absent, CardKind::Unknown);
}
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
