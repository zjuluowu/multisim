// SPDX-License-Identifier: Apache-2.0
#include "runner/contract_driver.h"
#include "contracts/sim_session.h"
#include "fakes/ril_fault_controls.h"
#include <future>
namespace SimGuard {
class SessionLifecycle : public ContractDriver {
protected:
    RilFaultControls faults;
    void SetUp() override { sut = CreateSimSessionSut(ril, faults); }
    void TearDown() override { ContractDriver::TearDown(); sut.reset(); }
    ISimSessionAdapter &Session() { return static_cast<ISimSessionAdapter &>(*sut); }
    void ObservePending(int slots)
    {
        auto actual = sut->Observe();
        ASSERT_EQ(actual.cards.size(), static_cast<size_t>(slots));
        RecordProperty("pending_state_policy", "C observation only; no initial/restart state policy frozen");
    }
    void RespondFor(int slot)
    {
        ASSERT_GT(ril.RespondMatching([slot](int target, const DeviceCard &) { return target == slot; }), 0u);
        sut->Settle();
    }
    int RefreshToLocked()
    {
        sut->Start(1); Change(0, {State::Ready, CardKind::Usim, "refresh-A"});
        ril.Change(0, {State::Locked, CardKind::Usim, "refresh-A", Lock::Puk}); sut->Settle();
        auto revision = faults.StatusRevision();
        auto refreshed = std::async(std::launch::async, [&] { return Session().Refresh(0); });
        EXPECT_TRUE(faults.AwaitStatusAfter(revision));
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
    sut->Start(1); ObservePending(1); Observe("initialized-without-card-response");
    Change(0, {State::Ready, CardKind::Usim, "init-A"});
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, INIT_002_MissingRilThenRecreate)
{
    Session().StartWithRil(1, false); ObservePending(1); Observe("ril-unavailable");
    Session().Recreate(true); ObservePending(1);
    Change(0, {State::Locked, CardKind::Sim, "init-B", Lock::Pin});
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, INIT_DUAL_002_ReversedEnumeration)
{
    sut->Start(2); ObservePending(2);
    ril.Change(0, {State::Ready, CardKind::Usim, "dual-A"});
    ril.Change(1, {State::Locked, CardKind::Sim, "dual-B", Lock::Puk});
    sut->Settle(); RespondFor(1); Observe("second-slot-response-first");
    auto partial = sut->Observe();
    ExpectCard(partial.cards.at(1), 1, State::Locked, CardKind::Sim);
    Complete(); Observe("both-enumerated");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_002_MalformedThenRecovery)
{
    sut->Start(1); faults.NextStatus(RilFaultControls::StatusFault::Malformed);
    ril.Change(0, {State::Ready, CardKind::Usim, "callback-A"});
    Complete(); ObservePending(1); Observe("malformed-status");
    RecordProperty("injected_fault", "wrong typed RIL response payload");
    ril.Notify(0); Complete(); Observe("valid-status-recovery");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, CALLBACK_003_DroppedThenRecovery)
{
    sut->Start(1); faults.NextStatus(RilFaultControls::StatusFault::Drop);
    ril.Change(0, {State::Locked, CardKind::Usim, "callback-B", Lock::Puk});
    Complete(); ObservePending(1); Observe("status-response-not-delivered");
    RecordProperty("injected_fault", "RIL response dropped; no timeout-policy assertion");
    ril.Notify(0); Complete(); Observe("new-status-response");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, PUK_002_WrongThenSuccessfulUnlock)
{
    sut->Start(1); Change(0, {State::Locked, CardKind::Usim, "puk-A", Lock::Puk});
    faults.SetPukRemaining(7);
    auto wrong = Session().UnlockPuk(0, "1234", "00000000");
    EXPECT_EQ(wrong.error, 0); EXPECT_EQ(wrong.result, Unlock::Incorrect); EXPECT_EQ(wrong.remaining, 7);
    auto ok = Session().UnlockPuk(0, "1234", "12345678");
    EXPECT_EQ(ok.error, 0); EXPECT_EQ(ok.result, Unlock::Ok); EXPECT_EQ(ok.remaining, 7);
    Change(0, {State::Ready, CardKind::Usim, "puk-A"});
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SessionLifecycle, REFRESH_001_ExplicitStatusQuery)
{
    RefreshToLocked(); // Raw return contract conflict is retained separately in DEFECT_005.
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
}
TEST_F(SessionLifecycle, RECREATE_001_DualCardReenumeration)
{
    sut->Start(2);
    Change(0, {State::Ready, CardKind::Usim, "restart-A"});
    Change(1, {State::Locked, CardKind::Sim, "restart-B", Lock::Pin});
    Session().Recreate(); ObservePending(2); Observe("new-real-SUT-before-enumeration");
    ril.Notify(1); ril.Notify(0); Complete(); Observe("reenumerated-device-facts");
    auto actual = sut->Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Locked, CardKind::Sim);
    RecordProperty("persistence_scope", "external card facts; no Provider/DB recovery claim");
}
TEST_F(SessionLifecycle, RECREATE_002_ReplacementWhileStopped)
{
    sut->Start(1); Change(0, {State::Ready, CardKind::Usim, "old-card"}); sut->Stop();
    // Hardware changes while the host is down; no delivery to a dead receiver.
    ril.BindNotify([](int) {});
    ril.Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Puk});
    Session().Recreate(); ObservePending(1);
    ril.Notify(0); Complete(); Observe("new-card-after-real-recreation");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DUAL_002_PinWhileOtherCardRemoved)
{
    sut->Start(2);
    Change(0, {State::Locked, CardKind::Usim, "async-A", Lock::Pin});
    Change(1, {State::Ready, CardKind::Sim, "async-B"});
    faults.NextPin(RilFaultControls::PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return sut->UnlockPin(0, "1234"); });
    FinishHeldPin release{faults}; ASSERT_TRUE(faults.AwaitHeldPin());
    Change(1, {}); faults.ReleasePin(); EXPECT_EQ(unlocked.get().result, Unlock::Ok);
    ril.Notify(0); Complete(); Observe("unlock-and-other-removal-completed");
    auto actual = sut->Observe();
    ExpectCard(actual.cards.at(0), 0, State::Ready, CardKind::Usim);
    ExpectCard(actual.cards.at(1), 1, State::Absent, CardKind::Unknown);
}
TEST_F(SessionLifecycle, DEFECT_002_ReadyReplyFromReplacedCard)
{
    sut->Start(1); Change(0, {State::Ready, CardKind::Usim, "old-card"});
    ril.Notify(0); sut->Settle(); ASSERT_GT(ril.Pending(), 0u);
    ril.Change(0, {}); sut->Settle();
    ASSERT_GT(ril.RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.state == State::Absent; }), 0u);
    sut->Settle();
    ril.Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin}); sut->Settle();
    ASSERT_GT(ril.RespondMatching([](int slot, const DeviceCard &c) { return slot == 0 && c.iccid == "new-card"; }), 0u);
    sut->Settle(); Observe("replacement-card-locked");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
    ASSERT_GT(ril.RespondAll(), 0u); Complete(); Observe("old-card-ready-response");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_003_GenericRilErrorIsNotWrongPassword)
{
    sut->Start(1); Change(0, {State::Locked, CardKind::Usim, "error-A", Lock::Pin});
    faults.NextPin(RilFaultControls::PinFault::GenericTransportError);
    auto actual = sut->UnlockPin(0, "1234");
    RecordProperty("observed_error", actual.error);
    RecordProperty("observed_unlock_result", static_cast<int>(actual.result));
    EXPECT_NE(actual.error, 0); EXPECT_EQ(actual.result, Unlock::Failure);
    Observe("generic-error-without-password-decision");
}
TEST_F(SessionLifecycle, DEFECT_004_PinReplyFromReplacedCard)
{
    sut->Start(1); Change(0, {State::Locked, CardKind::Usim, "old-card", Lock::Pin});
    faults.NextPin(RilFaultControls::PinFault::Hold);
    auto unlocked = std::async(std::launch::async, [&] { return sut->UnlockPin(0, "1234"); });
    FinishHeldPin release{faults}; ASSERT_TRUE(faults.AwaitHeldPin());
    Change(0, {}); Change(0, {State::Locked, CardKind::Sim, "new-card", Lock::Pin});
    faults.ReleasePin(); auto oldResult = unlocked.get();
    RecordProperty("old_request_result", static_cast<int>(oldResult.result));
    Observe("old-pin-result-after-card-replacement");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Sim);
}
TEST_F(SessionLifecycle, DEFECT_005_RefreshReturnContractConflict)
{
    // Client header promises TELEPHONY_SUCCESS; real delegate returns a SimState integer.
    // Keep the independent assertion red until the public contract is adjudicated.
    EXPECT_EQ(RefreshToLocked(), 0) << "core_service_client.h RefreshSimState @return contract";
}
}
