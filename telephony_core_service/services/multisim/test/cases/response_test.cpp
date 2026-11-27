#include "sim_protocol_test.h"
#include "shared/unlock_pin_scenario.h"
namespace SimGuard {
using SimAdvancedTest=SimTest; // Historical suite label, not a Framework Fixture.

TEST_F(SimAdvancedTest, PIN_001)
{
    StateProtocol protocol(*this);
    protocol.Start(1);
    protocol.Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    ExerciseWrongThenSuccessfulPin(
        [&](const char *pin) { return protocol.UnlockPin(0, pin); },
        [&] { ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Usim); });
    // Card readiness is established by a separate RIL state observation.
    protocol.Notify(0); protocol.Complete(); protocol.Observe("pin-success-status");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(SimAdvancedTest, PIN_002_CommandFailure)
{
    StateProtocol protocol(*this);
    protocol.Start(1);
    protocol.Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    protocol.FailPin(true);
    auto failed = protocol.UnlockPin(0, "1234");
    EXPECT_NE(failed.error, 0);
    EXPECT_EQ(failed.result, Unlock::Failure);
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
    protocol.Observe("pin-command-failure");
}
TEST_F(SimAdvancedTest, DUAL_001)
{
    StateProtocol protocol(*this);
    protocol.Start(2);
    protocol.Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    protocol.Change(1, {State::Locked, CardKind::Sim, "fixture-B", Lock::Puk});
    protocol.Change(0, {});
    const auto snapshot = protocol.Observe();
    ASSERT_EQ(snapshot.cards.size(), 2u);
    ExpectCard(snapshot.cards.at(0), 0, State::Absent, CardKind::Unknown);
    ExpectCard(snapshot.cards.at(1), 1, State::Locked, CardKind::Sim);
    protocol.Change(0, {State::Ready, CardKind::Sim, "fixture-C"});
    ExpectCard(protocol.Observe().cards.at(1), 1, State::Locked, CardKind::Sim);
}
TEST_F(SimAdvancedTest, RIL_001_DuplicateAndError)
{
    StateProtocol protocol(*this);
    protocol.Start(1);
    protocol.Change(0, {});
    protocol.Notify(0); protocol.Settle(); ASSERT_GT(protocol.RespondAll(true), 0u);
    protocol.Complete(); protocol.Observe("ril-error");
    // Error-state preservation is a C observation, not an approved transition policy.
    // Preserve the query trace; only later valid device facts carry the strong state Oracle.
    protocol.Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    protocol.ReplayLast(); protocol.Complete(); protocol.Observe("duplicate-ready");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
// Separate suite: preserves failing evidence, never silently skipped/xfail or counted as compatibility PASS.
TEST_F(SimAdvancedTest, DEFECT_001_StaleReadyAfterRemoval)
{
    StateProtocol protocol(*this);
    protocol.Start(1);
    protocol.Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    protocol.Notify(0); protocol.Settle(); ASSERT_GT(protocol.Pending(), 0u); // hold old READY responses
    protocol.DeviceChange(0, {}); protocol.Settle();
    size_t removals = 0;
    for (unsigned round = 0; round < 32; ++round) {
        auto delivered = protocol.RespondMatching([](int slot, const DeviceCard &card) {
            return slot == 0 && card.state == State::Absent;
        });
        removals += delivered;
        protocol.Settle();
        if (!delivered) break;
    }
    ASSERT_GT(removals, 0u) << "no actual removal response to inject";
    protocol.Observe("removed");
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
    ASSERT_GT(protocol.RespondAll(), 0u) << "no held READY response to inject";
    protocol.Complete(); protocol.Observe("stale-ready");
    // Physical card remains absent; its old response must not resurrect a ready card.
    ExpectCard(protocol.Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
}
}
