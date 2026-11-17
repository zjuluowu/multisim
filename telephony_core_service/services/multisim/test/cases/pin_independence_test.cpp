#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, UnlockingSecondLockedCardLeavesFirstLocked)
{
    auto a = AddCard("A", CardKind::Usim, State::Locked, Lock::Pin);
    auto b = AddCard("B", CardKind::Sim, State::Locked, Lock::Pin);
    Start();
    EXPECT_EQ(UnlockPin(b, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Locked); // The other locked card is unchanged.
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(b, State::Ready);
    EXPECT_EQ(UnlockPin(a, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(a, State::Ready);
}
