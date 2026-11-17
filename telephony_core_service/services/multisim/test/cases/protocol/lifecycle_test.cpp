#include "sim_advanced_test.h"
#include "common/state_assertions.h"
#include <future>
namespace SimGuard {
class SessionLifecycle : public SimAdvancedTest {
protected:
    void ObservePending(int slots)
    {
        auto actual = Observe();
        ASSERT_EQ(actual.cards.size(), static_cast<size_t>(slots));
        RecordProperty("pending_state_policy", "C observation only; no initial/restart state policy frozen");
    }
    void RespondFor(int slot)
    {
        ASSERT_GT(Ril().RespondMatching([slot](int target, const DeviceCard &) { return target == slot; }), 0u);
        Settle();
    }
    int RefreshToLocked()
    {
        Start(1); Change(0, {State::Ready, CardKind::Usim, "refresh-A"});
        Ril().Change(0, {State::Locked, CardKind::Usim, "refresh-A", Lock::Puk}); Settle();
        auto revision = Faults().StatusRevision();
        auto refreshed = std::async(std::launch::async, [&] { return Refresh(0); });
        EXPECT_TRUE(Faults().AwaitStatusAfter(revision));
        Complete();
        const auto result = refreshed.get();
        RecordProperty("raw_refresh_return", result);
        Observe("explicit-refresh-completed");
        return result;
    }
};
struct FinishHeldPin {
    RilFaultControls &faults;
    ~FinishHeldPin() { faults.ReleasePin(); }
};
TEST_F(SessionLifecycle, INIT_001_SingleEnumeration)
{
    Start(1); ObservePending(1); Observe("initialized-without-card-response");
    Change(0, {State::Ready, CardKind::Usim, "init-A"});
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, INIT_002_MissingRilThenRecreate)
{
    StartWithRil(1, false); ObservePending(1); Observe("ril-unavailable");
    Recreate(true); ObservePending(1);
    Change(0, {State::Locked, CardKind::Sim, "init-B", Lock::Pin});
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, INIT_DUAL_002_ReversedEnumeration)
{
    Start(2); ObservePending(2);
    Ril().Change(0, {State::Ready, CardKind::Usim, "dual-A"});
    Ril().Change(1, {State::Locked, CardKind::Sim, "dual-B", Lock::Puk});
    Settle(); RespondFor(1); Observe("second-slot-response-first");
    auto partial = Observe();
    ExpectCard(partial.cards.at(1), 1, State::Locked, CardKind::Sim);
    Complete(); Observe("both-enumerated");
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_002_MalformedThenRecovery)
{
    Start(1); Faults().NextStatus(RilFaultControls::StatusFault::Malformed);
    Ril().Change(0, {State::Ready, CardKind::Usim, "callback-A"});
    Complete(); ObservePending(1); Observe("malformed-status");
    RecordProperty("injected_fault", "wrong typed RIL response payload");
    Ril().Notify(0); Complete(); Observe("valid-status-recovery");
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_003_DroppedThenRecovery)
{
    Start(1); Faults().NextStatus(RilFaultControls::StatusFault::Drop);
    Ril().Change(0, {State::Locked, CardKind::Usim, "callback-B", Lock::Puk});
    Complete(); ObservePending(1); Observe("status-response-not-delivered");
    RecordProperty("injected_fault", "RIL response dropped; no timeout-policy assertion");
    Ril().Notify(0); Complete(); Observe("new-status-response");
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, PUK_002_WrongThenSuccessfulUnlock)
{
    Start(1); Change(0, {State::Locked, CardKind::Usim, "puk-A", Lock::Puk});
    Faults().SetPukRemaining(7);
    auto wrong = UnlockPuk(0, "1234", "00000000");
    EXPECT_EQ(wrong.error, 0); EXPECT_EQ(wrong.result, Unlock::Incorrect); EXPECT_EQ(wrong.remaining, 7);
    auto ok = UnlockPuk(0, "1234", "12345678");
    EXPECT_EQ(ok.error, 0); EXPECT_EQ(ok.result, Unlock::Ok); EXPECT_EQ(ok.remaining, 7);
    Change(0, {State::Ready, CardKind::Usim, "puk-A"});
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, REFRESH_001_ExplicitStatusQuery)
{
    RefreshToLocked(); // Raw return contract conflict is retained separately in DEFECT_005.
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, RECREATE_001_DualCardReenumeration)
{
    Start(2);
    Change(0, {State::Ready, CardKind::Usim, "restart-A"});
    Change(1, {State::Locked, CardKind::Sim, "restart-B", Lock::Pin});
    Recreate(); ObservePending(2); Observe("new-real-SUT-before-enumeration");
    Ril().Notify(1); Ril().Notify(0); Complete(); Observe("reenumerated-device-facts");
    auto actual = Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Locked, CardKind::Sim);
    RecordProperty("persistence_scope", "external card facts; no Provider/DB recovery claim");
}
TEST_F(SessionLifecycle, RECREATE_002_ReplacementWhileStopped)
{
    Start(1); Change(0, {State::Ready, CardKind::Usim, "old-card"}); Stop();
    // Hardware changes while the host is down; no delivery to a dead receiver.
    Ril().BindNotify([](int) {});
    Ril().Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Puk});
    Recreate(); ObservePending(1);
    Ril().Notify(0); Complete(); Observe("new-card-after-real-recreation");
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DUAL_002_PinWhileOtherCardRemoved)
{
    Start(2);
    Change(0, {State::Locked, CardKind::Usim, "async-A", Lock::Pin});
    Change(1, {State::Ready, CardKind::Sim, "async-B"});
    Faults().NextPin(RilFaultControls::PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return UnlockPin(0, "1234"); });
    FinishHeldPin release{Faults()}; ASSERT_TRUE(Faults().AwaitHeldPin());
    Change(1, {}); Faults().ReleasePin(); EXPECT_EQ(unlocked.get().result, Unlock::Ok);
    Ril().Notify(0); Complete(); Observe("unlock-and-other-removal-completed");
    auto actual = Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Absent, CardKind::Unknown);
}
TEST_F(SessionLifecycle, DEFECT_002_ReadyReplyFromReplacedCard)
{
    Start(1); Change(0, {State::Ready, CardKind::Usim, "old-card"});
    Ril().Notify(0); Settle(); ASSERT_GT(Ril().Pending(), 0u);
    Ril().Change(0, {}); Settle();
    ASSERT_GT(Ril().RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.state == State::Absent; }), 0u);
    Settle();
    Ril().Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin}); Settle();
    ASSERT_GT(Ril().RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.iccid == "new-card"; }), 0u);
    Settle(); Observe("replacement-card-locked");
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
    ASSERT_GT(Ril().RespondAll(), 0u); Complete(); Observe("old-card-ready-response");
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_003_GenericRilErrorIsNotWrongPassword)
{
    Start(1); Change(0, {State::Locked, CardKind::Usim, "error-A", Lock::Pin});
    Faults().NextPin(RilFaultControls::PinFault::GenericTransportError);
    auto actual = UnlockPin(0, "1234");
    RecordProperty("observed_error", actual.error);
    RecordProperty("observed_unlock_result", static_cast<int>(actual.result));
    EXPECT_NE(actual.error, 0); EXPECT_EQ(actual.result, Unlock::Failure);
    Observe("generic-error-without-password-decision");
}
TEST_F(SessionLifecycle, DEFECT_004_PinReplyFromReplacedCard)
{
    Start(1); Change(0, {State::Locked, CardKind::Usim, "old-card", Lock::Pin});
    Faults().NextPin(RilFaultControls::PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return UnlockPin(0, "1234"); });
    FinishHeldPin release{Faults()}; ASSERT_TRUE(Faults().AwaitHeldPin());
    Change(0, {}); Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin});
    Faults().ReleasePin(); auto oldResult = unlocked.get();
    RecordProperty("old_request_result", static_cast<int>(oldResult.result));
    Observe("old-pin-result-after-card-replacement");
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_005_RefreshReturnContractConflict)
{
    // Client header promises TELEPHONY_SUCCESS; real delegate returns a SimState integer.
    // Keep the independent assertion red until the public contract is adjudicated.
    EXPECT_EQ(RefreshToLocked(), 0) << "core_service_client.h RefreshSimState @return contract";
}
}
