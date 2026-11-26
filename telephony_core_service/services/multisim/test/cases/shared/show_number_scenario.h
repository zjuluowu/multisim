/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_SHOW_NUMBER_SCENARIO_H
#define SIM_TEST_SHOW_NUMBER_SCENARIO_H

#include <cstddef>
#include <string>
#include <gtest/gtest.h>
#include "telephony_errors.h"

namespace SimGuard {
inline constexpr char kStoredPhoneNumber[] = "555123";
inline constexpr char16_t kStoredDisplayNumber[] = u"555123";

struct FailedShowNumberResult {
    int writeResult;
    int readResult;
    std::size_t commands;
    std::u16string number;
};

template<class Set, class Get, class CountCommands>
void ExerciseFailedShowNumber(Set set, Get get, CountCommands count, FailedShowNumberResult &result)
{
    ASSERT_NO_FATAL_FAILURE(set(1, u"555999", result.writeResult));
    EXPECT_EQ(result.writeResult, OHOS::Telephony::TELEPHONY_ERR_FAIL);
    result.commands = count();
    EXPECT_EQ(result.commands, 1u);
    ASSERT_NO_FATAL_FAILURE(get(1, result.number, result.readResult));
    EXPECT_EQ(result.readResult, OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(result.number, kStoredDisplayNumber);
}
} // namespace SimGuard

#endif // SIM_TEST_SHOW_NUMBER_SCENARIO_H
