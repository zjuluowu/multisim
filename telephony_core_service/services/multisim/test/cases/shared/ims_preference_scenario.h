/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_IMS_PREFERENCE_SCENARIO_H
#define SIM_TEST_IMS_PREFERENCE_SCENARIO_H

#include <cstdint>
#include <gtest/gtest.h>
#include "telephony_errors.h"

namespace SimGuard {
enum class ImsReadMode { Row, NoRow, Failure };

struct ImsPreferenceResult {
    int32_t first;
    int32_t repeat;
    int32_t positive;
    int32_t failure;
    int32_t external;
    int32_t empty;
    int32_t failedRead;
    int32_t value;
};

template<class Save, class Query, class SetWriteResult, class SetStoredValue, class SetReadMode>
ImsPreferenceResult ExerciseBoundImsPreference(Save save, Query query, SetWriteResult setWriteResult,
    SetStoredValue setStoredValue, SetReadMode setReadMode)
{
    using namespace OHOS::Telephony;
    int32_t value = 99;
    EXPECT_EQ(save(-1, 100), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(query(-1, value), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(value, 99);
    const auto first = save(1, 17);
    const auto repeat = save(1, 17);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(repeat, 0);
    EXPECT_EQ(query(1, value), 0);
    EXPECT_EQ(value, 17);
    setWriteResult(6);
    const auto positive = save(1, -9);
    EXPECT_EQ(positive, 6);
    EXPECT_EQ(query(1, value), 0);
    EXPECT_EQ(value, -9);
    setWriteResult(-23);
    const auto failure = save(1, 123);
    EXPECT_EQ(failure, -23);
    setStoredValue(33);
    EXPECT_EQ(query(1, value), 0);
    EXPECT_EQ(value, 33);
    const auto external = value;
    setStoredValue(-1);
    EXPECT_EQ(query(1, value), 0);
    EXPECT_EQ(value, -1);
    setReadMode(ImsReadMode::NoRow);
    const auto empty = query(1, value);
    EXPECT_EQ(empty, 0);
    EXPECT_EQ(value, -1);
    setReadMode(ImsReadMode::Failure);
    const auto failedRead = query(1, value);
    EXPECT_EQ(failedRead, 0);
    EXPECT_EQ(value, -1);
    return {first, repeat, positive, failure, external, empty, failedRead, value};
}
} // namespace SimGuard

#endif // SIM_TEST_IMS_PREFERENCE_SCENARIO_H
