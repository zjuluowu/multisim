#include "sim_test.h"
#include "shared/cellular_selection_scenario.h"
using namespace SimGuard;
using RealSimPrimary = SimTest; // Preserve the historical suite identity.
TEST_F(RealSimPrimary, C0CoreToRealManagerAndController)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    ExerciseC0Selection(
        [&](int) { return SelectPrimary(target); },
        [&](int, const char *identity) { ExpectPrimary(target, identity); },
        [&] { ExpectPrimaryIdle(); ExpectCardState(target, State::Locked); },
        [&](int) { ExpectPrimaryChanged(target); });
}
