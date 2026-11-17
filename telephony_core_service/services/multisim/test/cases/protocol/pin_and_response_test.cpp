#include "sim_advanced_test.h"
#include "common/state_assertions.h"
namespace SimGuard {
TEST_F(SimAdvancedTest, PIN_001)
{
    Start(1);
    Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    auto wrong = UnlockPin(0, "0000");
    EXPECT_EQ(wrong.error, 0);
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    EXPECT_EQ(wrong.remaining, 2);
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
    auto ok = UnlockPin(0, "1234");
    EXPECT_EQ(ok.error, 0);
    EXPECT_EQ(ok.result, Unlock::Ok);
    // Card readiness is established by a separate RIL state observation.
    Ril().Notify(0); Complete(); Observe("pin-success-status");
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SimAdvancedTest, PIN_002_CommandFailure)
{
    Start(1);
    Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    Ril().FailPin(true);
    auto failed = UnlockPin(0, "1234");
    EXPECT_NE(failed.error, 0);
    EXPECT_EQ(failed.result, Unlock::Failure);
    ExpectCard(Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
    Observe("pin-command-failure");
}
TEST_F(SimAdvancedTest, DUAL_001)
{
    Start(2);
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    Change(1, {State::Locked, CardKind::Sim, "fixture-B", Lock::Puk});
    Change(0, {});
    const auto snapshot = Observe();
    ASSERT_EQ(snapshot.cards.size(), 2u);
    ExpectCard(snapshot.cards.at(0), 0, State::Absent, CardKind::Unknown);
    ExpectCard(snapshot.cards.at(1), 1, State::Locked, CardKind::Sim);
    Change(0, {State::Ready, CardKind::Sim, "fixture-C"});
    ExpectCard(Observe().cards.at(1), 1, State::Locked, CardKind::Sim);
}
TEST_F(SimAdvancedTest, RIL_001_DuplicateAndError)
{
    Start(1);
    Change(0, {});
    Ril().Notify(0); Settle(); ASSERT_GT(Ril().RespondAll(true), 0u);
    Complete(); Observe("ril-error");
    // Error-state preservation is a C observation, not an approved transition policy.
    // Preserve the query trace; only later valid device facts carry the strong state Oracle.
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    Ril().ReplayLast(); Complete(); Observe("duplicate-ready");
    ExpectCard(Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
// Separate suite: preserves failing evidence, never silently skipped/xfail or counted as compatibility PASS.
TEST_F(SimAdvancedTest, DEFECT_001_StaleReadyAfterRemoval)
{
    Start(1);
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    Ril().Notify(0); Settle(); ASSERT_GT(Ril().Pending(), 0u); // hold old READY responses
    Ril().Change(0, {}); Settle();
    size_t removals = 0;
    for (unsigned round = 0; round < 32; ++round) {
        auto delivered = Ril().RespondMatching([](int slot, const DeviceCard &card) {
            return slot == 0 && card.state == State::Absent;
        });
        removals += delivered;
        Settle();
        if (!delivered) break;
    }
    ASSERT_GT(removals, 0u) << "no actual removal response to inject";
    Observe("removed");
    ExpectCard(Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
    ASSERT_GT(Ril().RespondAll(), 0u) << "no held READY response to inject";
    Complete(); Observe("stale-ready");
    // Physical card remains absent; its old response must not resurrect a ready card.
    ExpectCard(Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
}
}
