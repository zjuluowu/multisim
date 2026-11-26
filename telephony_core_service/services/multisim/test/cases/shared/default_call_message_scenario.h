/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_DEFAULT_CALL_MESSAGE_SCENARIO_H
#define SIM_TEST_DEFAULT_CALL_MESSAGE_SCENARIO_H

#include <vector>
#include <gtest/gtest.h>
#include "telephony_errors.h"

namespace SimGuard {
struct DefaultCallMessageResult {
    int selected;
    int cleared;
    int clearResult;
};

template<class SetRole, class GetSlot, class SetProviderFailure, class RejectPublication>
DefaultCallMessageResult ExerciseDefaultCallMessage(SetRole setRole, GetSlot getSlot,
    SetProviderFailure providerFailure, RejectPublication rejectPublication, int clearedSlot)
{
    using namespace OHOS::Telephony;
    EXPECT_EQ(setRole(100), TELEPHONY_ERR_SLOTID_INVALID);
    EXPECT_EQ(setRole(1), TELEPHONY_ERR_SUCCESS);
    const int selected = getSlot();
    EXPECT_EQ(selected, 1);
    EXPECT_EQ(setRole(1), TELEPHONY_ERR_SUCCESS);
    providerFailure(true);
    EXPECT_EQ(setRole(-1), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_EQ(getSlot(), 1);
    providerFailure(false);
    rejectPublication();
    const int clearResult = setRole(-1);
    EXPECT_EQ(clearResult, TELEPHONY_ERR_PUBLISH_BROADCAST_FAIL);
    const int cleared = getSlot();
    EXPECT_EQ(cleared, clearedSlot);
    EXPECT_EQ(setRole(-1), TELEPHONY_ERR_SUCCESS);
    return {selected, cleared, clearResult};
}

inline std::vector<int> DefaultCallMessageWrites(int selectedSim)
{
    return {selectedSim, selectedSim, 0, 0, 0};
}
} // namespace SimGuard

#endif // SIM_TEST_DEFAULT_CALL_MESSAGE_SCENARIO_H
