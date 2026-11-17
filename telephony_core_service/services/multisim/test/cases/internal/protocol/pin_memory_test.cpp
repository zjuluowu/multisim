#include "sim_protocol_test.h"
#include "internal/host/pin_memory_spy.h"
namespace SimGuard {
class PinMemoryContract : public SimTest {
protected:
    StateProtocol protocol{*this};
    std::unique_ptr<PinMemorySpy> spy;
    void SetUp() override
    {
        SimTest::SetUp();
        spy = std::make_unique<PinMemorySpy>();
    }
    void TearDown() override
    {
        // Drain/destroy the real SUT before restoring its external callback pointer.
        SimTest::TearDown();
        spy.reset();
    }
};

TEST_F(PinMemoryContract, PinSuccessUsesOriginalSlotIdentityAndCredential)
{
    protocol.Start(2);
    protocol.Change(0, {State::Ready, CardKind::Sim, "synthetic-other-card"});
    protocol.Change(1, {State::Locked, CardKind::Usim, "synthetic-pin-card", Lock::Pin});
    spy->Expect(1, "synthetic-pin-card", PinOperationType::PIN_ENABLE, "1234");
    const auto wrong = protocol.UnlockPin(1, "0000");
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    spy->Assert(0);
    const auto correct = protocol.UnlockPin(1, "1234");
    EXPECT_EQ(correct.error, 0);
    EXPECT_EQ(correct.result, Unlock::Ok);
    spy->Assert(1);
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Sim);
}

TEST_F(PinMemoryContract, PukSuccessUsesNewPinAndPinAlterOperation)
{
    protocol.Start(1);
    protocol.Change(0, {State::Locked, CardKind::Usim, "synthetic-puk-card", Lock::Puk});
    spy->Expect(0, "synthetic-puk-card", PinOperationType::PIN_ALTER, "2468");
    const auto wrong = protocol.UnlockPuk(0, "2468", "00000000");
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    spy->Assert(0);
    const auto correct = protocol.UnlockPuk(0, "2468", "12345678");
    EXPECT_EQ(correct.error, 0);
    EXPECT_EQ(correct.result, Unlock::Ok);
    spy->Assert(1); // Neither the PUK nor a prior PIN may be forwarded as the new PIN.
}

TEST_F(PinMemoryContract, FailedPinDoesNotPublishMemoryHook)
{
    protocol.Start(1);
    protocol.Change(0, {State::Locked, CardKind::Usim, "synthetic-failed-card", Lock::Pin});
    spy->Expect(0, "synthetic-failed-card", PinOperationType::PIN_ENABLE, "1234");
    protocol.FailPin(true);
    const auto failed = protocol.UnlockPin(0, "1234");
    EXPECT_NE(failed.error, 0);
    EXPECT_EQ(failed.result, Unlock::Failure);
    spy->Assert(0);
}

TEST_F(PinMemoryContract, RemovalUsesAbsentResponseIdentityWithoutCredential)
{
    protocol.Start(2);
    protocol.Change(1, {State::Locked, CardKind::Usim, "synthetic-removed-card", Lock::Pin});
    // Current production updates iccid_ from the absent RIL response before the
    // hook. This characterizes its empty identity; it does not approve an asset
    // cleanup policy or claim the previous card identity survives removal.
    spy->Expect(1, "", PinOperationType::SIM_ABSENT, "");
    protocol.Change(1, {});
    spy->Assert(1);
    EXPECT_TRUE(protocol.Observe().cards.at(1).iccid.empty());
    ExpectCard(protocol.Observe().cards.at(1), 1, State::Absent, CardKind::Unknown);
}
}
