/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_UNLOCK_PIN_SCENARIO_H
#define SIM_TEST_UNLOCK_PIN_SCENARIO_H

#include <gtest/gtest.h>

namespace SimGuard {
template<class UnlockPin, class VerifyLocked>
void ExerciseWrongThenSuccessfulPin(UnlockPin unlockPin, VerifyLocked verifyLocked)
{
    const auto wrong = unlockPin("0000");
    using UnlockDecision = decltype(wrong.result);
    EXPECT_EQ(wrong.error, 0);
    EXPECT_EQ(wrong.result, UnlockDecision::Incorrect);
    EXPECT_EQ(wrong.remaining, 2);
    verifyLocked();

    const auto accepted = unlockPin("1234");
    EXPECT_EQ(accepted.error, 0);
    EXPECT_EQ(accepted.result, UnlockDecision::Ok);
}
} // namespace SimGuard
#endif
