#include "sim_protocol_test.h"
#include <future>
namespace SimGuard {
class SessionLifecycle : public SimTest {
protected:
    StateProtocol protocol{*this};
    void ObservePending(int slots)
    {
        auto actual = protocol.Observe();
        ASSERT_EQ(actual.cards.size(), static_cast<size_t>(slots));
        RecordProperty("pending_state_policy", "C observation only; no initial/restart state policy frozen");
    }
    void RespondFor(int slot)
    {
        ASSERT_GT(protocol.RespondMatching([slot](int target, const DeviceCard &) { return target == slot; }), 0u);
        protocol.Settle();
    }
    int RefreshToLocked()
    {
        protocol.Start(1); protocol.Change(0, {State::Ready, CardKind::Usim, "refresh-A"});
        protocol.DeviceChange(0, {State::Locked, CardKind::Usim, "refresh-A", Lock::Puk}); protocol.Settle();
        auto revision = protocol.StatusRevision();
        auto refreshed = std::async(std::launch::async, [&] { return protocol.Refresh(0); });
        EXPECT_TRUE(protocol.AwaitStatusAfter(revision));
        protocol.Complete();
        const auto result = refreshed.get();
        RecordProperty("raw_refresh_return", result);
        protocol.Observe("explicit-refresh-completed");
        return result;
    }
};
struct FinishHeldPin {
    StateProtocol &protocol;
    ~FinishHeldPin() { protocol.ReleasePin(); }
};
TEST_F(SessionLifecycle, INIT_001_SingleEnumeration)
{
    protocol.Start(1); ObservePending(1); protocol.Observe("initialized-without-card-response");
    protocol.Change(0, {State::Ready, CardKind::Usim, "init-A"});
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, INIT_002_MissingRilThenRecreate)
{
    protocol.StartWithRil(1, false); ObservePending(1); protocol.Observe("ril-unavailable");
    protocol.Recreate(true); ObservePending(1);
    protocol.Change(0, {State::Locked, CardKind::Sim, "init-B", Lock::Pin});
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, INIT_DUAL_002_ReversedEnumeration)
{
    protocol.Start(2); ObservePending(2);
    protocol.DeviceChange(0, {State::Ready, CardKind::Usim, "dual-A"});
    protocol.DeviceChange(1, {State::Locked, CardKind::Sim, "dual-B", Lock::Puk});
    protocol.Settle(); RespondFor(1); protocol.Observe("second-slot-response-first");
    auto partial = protocol.Observe();
    ExpectCard(partial.cards.at(1), 1, State::Locked, CardKind::Sim);
    protocol.Complete(); protocol.Observe("both-enumerated");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_002_MalformedThenRecovery)
{
    protocol.Start(1); protocol.NextStatus(StatusFault::Malformed);
    protocol.DeviceChange(0, {State::Ready, CardKind::Usim, "callback-A"});
    protocol.Complete(); ObservePending(1); protocol.Observe("malformed-status");
    RecordProperty("injected_fault", "wrong typed RIL response payload");
    protocol.Notify(0); protocol.Complete(); protocol.Observe("valid-status-recovery");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_003_DroppedThenRecovery)
{
    protocol.Start(1); protocol.NextStatus(StatusFault::Drop);
    protocol.DeviceChange(0, {State::Locked, CardKind::Usim, "callback-B", Lock::Puk});
    protocol.Complete(); ObservePending(1); protocol.Observe("status-response-not-delivered");
    RecordProperty("injected_fault", "RIL response dropped; no timeout-policy assertion");
    protocol.Notify(0); protocol.Complete(); protocol.Observe("new-status-response");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, PUK_002_WrongThenSuccessfulUnlock)
{
    protocol.Start(1); protocol.Change(0, {State::Locked, CardKind::Usim, "puk-A", Lock::Puk});
    protocol.SetPukRemaining(7);
    auto wrong = protocol.UnlockPuk(0, "1234", "00000000");
    EXPECT_EQ(wrong.error, 0); EXPECT_EQ(wrong.result, Unlock::Incorrect); EXPECT_EQ(wrong.remaining, 7);
    auto ok = protocol.UnlockPuk(0, "1234", "12345678");
    EXPECT_EQ(ok.error, 0); EXPECT_EQ(ok.result, Unlock::Ok); EXPECT_EQ(ok.remaining, 7);
    protocol.Change(0, {State::Ready, CardKind::Usim, "puk-A"});
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, REFRESH_001_ExplicitStatusQuery)
{
    RefreshToLocked(); // The return-code contract is checked separately in DEFECT_005.
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, RECREATE_001_DualCardReenumeration)
{
    protocol.Start(2);
    protocol.Change(0, {State::Ready, CardKind::Usim, "restart-A"});
    protocol.Change(1, {State::Locked, CardKind::Sim, "restart-B", Lock::Pin});
    protocol.Recreate(); ObservePending(2); protocol.Observe("new-real-SUT-before-enumeration");
    protocol.Notify(1); protocol.Notify(0); protocol.Complete(); protocol.Observe("reenumerated-device-facts");
    auto actual = protocol.Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Locked, CardKind::Sim);
    RecordProperty("persistence_scope", "external card facts; no Provider/DB recovery claim");
}
TEST_F(SessionLifecycle, RECREATE_002_ReplacementWhileStopped)
{
    protocol.Start(1); protocol.Change(0, {State::Ready, CardKind::Usim, "old-card"}); protocol.Stop();
    // Hardware changes while the host is down; no delivery to a dead receiver.
    protocol.ChangeWhileStopped(0, {State::Locked, CardKind::Sim, "new-card", Lock::Puk});
    protocol.Recreate(); ObservePending(1);
    protocol.Notify(0); protocol.Complete(); protocol.Observe("new-card-after-real-recreation");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DUAL_002_PinWhileOtherCardRemoved)
{
    protocol.Start(2);
    protocol.Change(0, {State::Locked, CardKind::Usim, "async-A", Lock::Pin});
    protocol.Change(1, {State::Ready, CardKind::Sim, "async-B"});
    protocol.NextPin(PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return protocol.UnlockPin(0, "1234"); });
    FinishHeldPin release{protocol}; ASSERT_TRUE(protocol.AwaitHeldPin());
    protocol.Change(1, {}); protocol.ReleasePin(); EXPECT_EQ(unlocked.get().result, Unlock::Ok);
    protocol.Notify(0); protocol.Complete(); protocol.Observe("unlock-and-other-removal-completed");
    auto actual = protocol.Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Absent, CardKind::Unknown);
}
TEST_F(SessionLifecycle, DEFECT_002_ReadyReplyFromReplacedCard)
{
    protocol.Start(1); protocol.Change(0, {State::Ready, CardKind::Usim, "old-card"});
    protocol.Notify(0); protocol.Settle(); ASSERT_GT(protocol.Pending(), 0u);
    protocol.DeviceChange(0, {}); protocol.Settle();
    ASSERT_GT(protocol.RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.state == State::Absent; }), 0u);
    protocol.Settle();
    protocol.DeviceChange(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin}); protocol.Settle();
    ASSERT_GT(protocol.RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.iccid == "new-card"; }), 0u);
    protocol.Settle(); protocol.Observe("replacement-card-locked");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
    ASSERT_GT(protocol.RespondAll(), 0u); protocol.Complete(); protocol.Observe("old-card-ready-response");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_003_GenericRilErrorIsNotWrongPassword)
{
    protocol.Start(1); protocol.Change(0, {State::Locked, CardKind::Usim, "error-A", Lock::Pin});
    protocol.NextPin(PinFault::GenericTransportError);
    auto actual = protocol.UnlockPin(0, "1234");
    RecordProperty("observed_error", actual.error);
    RecordProperty("observed_unlock_result", static_cast<int>(actual.result));
    EXPECT_NE(actual.error, 0); EXPECT_EQ(actual.result, Unlock::Failure);
    protocol.Observe("generic-error-without-password-decision");
}
TEST_F(SessionLifecycle, DEFECT_004_PinReplyFromReplacedCard)
{
    protocol.Start(1); protocol.Change(0, {State::Locked, CardKind::Usim, "old-card", Lock::Pin});
    protocol.NextPin(PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return protocol.UnlockPin(0, "1234"); });
    FinishHeldPin release{protocol}; ASSERT_TRUE(protocol.AwaitHeldPin());
    protocol.Change(0, {}); protocol.Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin});
    protocol.ReleasePin(); auto oldResult = unlocked.get();
    RecordProperty("old_request_result", static_cast<int>(oldResult.result));
    protocol.Observe("old-pin-result-after-card-replacement");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_005_RefreshReturnContractConflict)
{
    // Client header promises TELEPHONY_SUCCESS; real delegate returns a SimState integer.
    // Regression: refresh returns the documented success code, not a state enum.
    EXPECT_EQ(RefreshToLocked(), 0) << "core_service_client.h RefreshSimState @return contract";
}
}
