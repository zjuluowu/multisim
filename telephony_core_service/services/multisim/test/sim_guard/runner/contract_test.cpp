// SPDX-License-Identifier: Apache-2.0
#include "contracts/sim_sut.h"
#include "fakes/ril_device.h"
#include "scenarios/compatibility.h"
#include "oracle/card_oracle.h"
#include "runner/contract_driver.h"
#include <gtest/gtest.h>

namespace SimGuard {
class Lifecycle : public ContractDriver, public testing::WithParamInterface<Scenario> {};
TEST_P(Lifecycle, PublicCardState)
{
    const auto &scenario = GetParam();
    sut->Start(scenario.slots);
    for (const auto &card : scenario.sequence) {
        SCOPED_TRACE(scenario.id);
        const auto notificationStart = sut->Notifications().size();
        Change(0, card);
        auto snapshot = sut->Observe();
        ASSERT_EQ(snapshot.cards.size(), 1u);
        ExpectCard(snapshot.cards.at(0), 0, card.state, card.kind);
        // No internal or total event ordering requirement.
        const auto events = sut->Notifications();
        ExpectNotification({events.begin() + notificationStart, events.end()}, 0, card.state, card.reason);
    }
}
INSTANTIATE_TEST_SUITE_P(Compatibility, Lifecycle, testing::ValuesIn(LIFECYCLES),
    [](const auto &info) { return info.param.id; });

TEST_F(ContractDriver, PIN_001)
{
    sut->Start(1);
    Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    auto wrong = sut->UnlockPin(0, "0000");
    EXPECT_EQ(wrong.error, 0);
    EXPECT_EQ(wrong.result, Unlock::Incorrect);
    EXPECT_EQ(wrong.remaining, 2);
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
    auto ok = sut->UnlockPin(0, "1234");
    EXPECT_EQ(ok.error, 0);
    EXPECT_EQ(ok.result, Unlock::Ok);
    // Card readiness is established by a separate RIL state observation.
    ril.Notify(0); Complete(); Observe("pin-success-status");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
TEST_F(ContractDriver, PIN_002_CommandFailure)
{
    sut->Start(1);
    Change(0, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin});
    ril.FailPin(true);
    auto failed = sut->UnlockPin(0, "1234");
    EXPECT_NE(failed.error, 0);
    EXPECT_EQ(failed.result, Unlock::Failure);
    ExpectCard(sut->Observe().cards.at(0), 0, State::Locked, CardKind::Usim);
    Observe("pin-command-failure");
}
TEST_F(ContractDriver, DUAL_001)
{
    sut->Start(2);
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    Change(1, {State::Locked, CardKind::Sim, "fixture-B", Lock::Puk});
    Change(0, {});
    const auto snapshot = sut->Observe();
    ASSERT_EQ(snapshot.cards.size(), 2u);
    ExpectCard(snapshot.cards.at(0), 0, State::Absent, CardKind::Unknown);
    ExpectCard(snapshot.cards.at(1), 1, State::Locked, CardKind::Sim);
    Change(0, {State::Ready, CardKind::Sim, "fixture-C"});
    ExpectCard(sut->Observe().cards.at(1), 1, State::Locked, CardKind::Sim);
}
TEST_F(ContractDriver, RIL_001_DuplicateAndError)
{
    sut->Start(1);
    Change(0, {});
    ril.Notify(0); sut->Settle(); ASSERT_GT(ril.RespondAll(true), 0u);
    Complete(); Observe("ril-error");
    // Error-state preservation is a C observation, not an approved transition policy.
    // Preserve the query trace; only later valid device facts carry the strong state Oracle.
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    ril.ReplayLast(); Complete(); Observe("duplicate-ready");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Ready, CardKind::Usim);
}
// Separate suite: preserves failing evidence, never silently skipped/xfail or counted as compatibility PASS.
TEST_F(ContractDriver, DEFECT_001_StaleReadyAfterRemoval)
{
    sut->Start(1);
    Change(0, {State::Ready, CardKind::Usim, "fixture-A"});
    ril.Notify(0); sut->Settle(); ASSERT_GT(ril.Pending(), 0u); // hold old READY responses
    ril.Change(0, {}); sut->Settle();
    size_t removals = 0;
    for (unsigned round = 0; round < 32; ++round) {
        auto delivered = ril.RespondMatching([](int slot, const DeviceCard &card) {
            return slot == 0 && card.state == State::Absent;
        });
        removals += delivered;
        sut->Settle();
        if (!delivered) break;
    }
    ASSERT_GT(removals, 0u) << "no actual removal response to inject";
    Observe("removed");
    ExpectCard(sut->Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
    ASSERT_GT(ril.RespondAll(), 0u) << "no held READY response to inject";
    Complete(); Observe("stale-ready");
    // Physical card remains absent; its old response must not resurrect a ready card.
    ExpectCard(sut->Observe().cards.at(0), 0, State::Absent, CardKind::Unknown);
}
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
