#include "sim_test.h"
using namespace SimGuard;
using RealSimPrimary = SimTest; // Preserve the historical suite identity.
TEST_F(RealSimPrimary, C0CoreToRealManagerAndController)
{
    auto target=AddCard("B",CardKind::Usim,State::Locked,Lock::Pin);
    StartPrimary(target);
    EXPECT_EQ(SelectPrimary(target), 0);
    ExpectPrimaryIdle();
    ExpectPrimary(target, "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2");
    ExpectPrimaryNotifications(true, 0, false);
}
