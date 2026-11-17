// SPDX-License-Identifier: Apache-2.0
#include "sim_test.h"
namespace SimGuard {
TEST_F(SimTest, DualSimPinUnlockAfterWrongPin)
{
    auto a = AddCard("A", 0, CardKind::Sim, State::Ready);
    auto b = AddCard("B", 1, CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    EXPECT_EQ(UnlockPin(b, "0000").result, Unlock::Incorrect);
    ExpectCardState(b, State::Locked);
    EXPECT_EQ(UnlockPin(b, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(b, State::Ready);
}
}
