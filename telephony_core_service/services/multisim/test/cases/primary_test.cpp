#include "sim_test.h"
using namespace SimGuard;
using RealSimPrimary = SimTest; // Preserve the historical suite identity.
TEST_F(RealSimPrimary, C0CoreToRealManagerAndController)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    EXPECT_EQ(SelectPrimary(target), 0);
    ExpectPrimaryIdle();
    ExpectCardState(target, State::Locked);
    ExpectPrimary(target, "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2");
    ExpectPrimaryChanged(target);
}
