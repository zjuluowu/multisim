#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, ExistingCardQueriesAgreeWithoutControlledMutation)
{
    const auto capacity = CardCapacity();
    ASSERT_GE(capacity, 0);
    auto card = ExistingCard();
    ExpectReadableCard(card); // Independent state/type/presence queries; no card manipulation.
}
