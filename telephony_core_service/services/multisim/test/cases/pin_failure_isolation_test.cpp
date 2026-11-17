#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, WrongUsimPinDoesNotBlockSimPeer)
{
    auto a = AddCard("A", CardKind::Usim, State::Locked, Lock::Pin);
    auto b = AddCard("B", CardKind::Sim, State::Locked, Lock::Pin);
    Start();
    const auto rejected = UnlockPin(a, "0000");
    EXPECT_EQ(rejected.error, 0);
    EXPECT_EQ(rejected.result, Unlock::Incorrect);
    ExpectCardState(a, State::Locked);
    ExpectCardState(b, State::Locked); // Peer before unlock.
    EXPECT_EQ(UnlockPin(b, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Locked);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(b, State::Ready);
}
