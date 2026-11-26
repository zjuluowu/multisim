/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_CELLULAR_SELECTION_SCENARIO_H
#define SIM_TEST_CELLULAR_SELECTION_SCENARIO_H

#include <gtest/gtest.h>
#include "telephony_errors.h"

namespace SimGuard {
inline constexpr int kRequestedCellularSlot = 1;
inline constexpr char kPrimarySlotOnePersistedIdentity[] =
    "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";

template<class Select, class CheckPrimary, class CheckIdle, class CheckNotification>
void ExerciseC0Selection(Select select, CheckPrimary primary, CheckIdle idle, CheckNotification notified)
{
    EXPECT_EQ(select(kRequestedCellularSlot), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    idle();
    primary(kRequestedCellularSlot, kPrimarySlotOnePersistedIdentity);
    notified(kRequestedCellularSlot);
}

template<class Select, class ReadSlot, class ReadSubscription>
void ExerciseIndependentDataSelection(Select select, ReadSlot readSlot, ReadSubscription readSubscription,
    int targetSubscription, int &result, int &slot, int &subscription)
{
    result = select(kRequestedCellularSlot);
    EXPECT_EQ(result, OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    slot = readSlot();
    EXPECT_EQ(slot, kRequestedCellularSlot);
    EXPECT_EQ(readSubscription(subscription), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(subscription, targetSubscription);
}
} // namespace SimGuard

#endif // SIM_TEST_CELLULAR_SELECTION_SCENARIO_H
