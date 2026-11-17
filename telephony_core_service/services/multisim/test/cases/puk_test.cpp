#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, PukUnlockPublishesReadyAndRetainsNewPin)
{
    auto card = AddCard("A", CardKind::Usim, State::Locked, Lock::Puk);
    Start();
    EXPECT_EQ(UnlockPuk(card, "2468", "00000000").result, Unlock::Incorrect);
    ExpectCardState(card, State::Locked);
    EXPECT_EQ(UnlockPuk(card, "2468", "12345678").result, Unlock::Ok);
    ExpectCardState(card, State::Ready);
    ExpectStateNotification(card, State::Ready);
    ChangeCard(card, State::Locked, Lock::Pin);
    EXPECT_EQ(UnlockPin(card, "2468").result, Unlock::Ok);
    ExpectCardState(card, State::Ready);
    ExpectStateNotification(card, State::Ready);
}
