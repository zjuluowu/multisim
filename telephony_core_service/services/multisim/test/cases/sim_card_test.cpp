// SPDX-License-Identifier: Apache-2.0
#include "sim_test.h"
namespace SimGuard {
TEST_F(SimTest, LIFE_001)
{
    auto card = AddCard("A", 0, CardKind::Usim, State::Absent);
    Start();
    ExpectCardState(card, State::Absent);
    ExpectStateNotification(card, State::Absent);
    ChangeCard(card, State::Locked, Lock::Pin);
    ExpectCardState(card, State::Locked);
    ExpectStateNotification(card, State::Locked, Lock::Pin);
    ChangeCard(card, State::Ready);
    ExpectCardState(card, State::Ready);
    ExpectStateNotification(card, State::Ready);
    ChangeCard(card, State::Absent);
    ExpectCardState(card, State::Absent);
    ExpectStateNotification(card, State::Absent);
}
TEST_F(SimTest, LIFE_002)
{
    auto card = AddCard("B", 0, CardKind::Sim, State::Ready);
    Start();
    ExpectCardState(card, State::Ready);
    ExpectStateNotification(card, State::Ready);
    ChangeCard(card, State::Absent);
    ExpectCardState(card, State::Absent);
    ExpectStateNotification(card, State::Absent);
    // Replace the physical card with a different application and identity.
    auto replacement = card;
    replacement.kind = CardKind::Usim;
    replacement.iccid = "synthetic-replacement";
    ChangeCard(replacement, State::Ready);
    ExpectCardState(replacement, State::Ready);
    ExpectStateNotification(replacement, State::Ready);
}
TEST_F(SimTest, PUK_001)
{
    auto card = AddCard("D", 0, CardKind::Usim, State::Locked, Lock::Puk);
    Start();
    ExpectCardState(card, State::Locked);
    ExpectStateNotification(card, State::Locked, Lock::Puk);
    ChangeCard(card, State::Absent);
    ExpectCardState(card, State::Absent);
    ExpectStateNotification(card, State::Absent);
}
TEST_F(SimTest, DualSimPinUnlock)
{
    auto a = AddCard("A", 0, CardKind::Usim, State::Ready);
    auto b = AddCard("B", 1, CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Locked);
    ExpectStateNotification(b, State::Locked, Lock::Pin);
    auto result = UnlockPin(b, "1234");
    EXPECT_EQ(result.error, 0);
    EXPECT_EQ(result.result, Unlock::Ok);
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(a, State::Ready);
    ExpectStateNotification(b, State::Ready);
}
}
