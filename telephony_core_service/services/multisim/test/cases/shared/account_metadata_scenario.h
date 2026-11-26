/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_ACCOUNT_METADATA_SCENARIO_H
#define SIM_TEST_ACCOUNT_METADATA_SCENARIO_H

#include <string>
#include <gtest/gtest.h>
#include "telephony_errors.h"

namespace SimGuard {
template<typename SetName, typename GetName, typename SetLabel, typename RejectWrites>
void ExerciseAccountMetadata(SetName setName, GetName getName, SetLabel setLabel, RejectWrites rejectWrites)
{
    using namespace OHOS::Telephony;
    EXPECT_EQ(setName(u"Host account"), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(setName(u"Host account"), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(setName(u""), TELEPHONY_ERR_SUCCESS);
    std::u16string name = u"not the cache";
    EXPECT_EQ(getName(name), TELEPHONY_ERR_SUCCESS);
    EXPECT_TRUE(name.empty());
    EXPECT_EQ(setLabel(11), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(setLabel(3), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(setLabel(3), TELEPHONY_ERR_SUCCESS);
    rejectWrites();
    EXPECT_EQ(setName(u"failed"), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_EQ(setLabel(4), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    name = u"not the cache";
    EXPECT_EQ(getName(name), TELEPHONY_ERR_SUCCESS);
    EXPECT_TRUE(name.empty());
}
} // namespace SimGuard

#endif
