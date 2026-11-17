// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <gtest/gtest.h>
#include <cstdlib>
namespace SimHost {
struct QueryInput { const char *name; const char *configured; const char *vendor; };
class QueryTest : public testing::TestWithParam<QueryInput> {
protected:
    void SetUp() override
    {
        setenv("SIM_G2A_SLOT_PARAMETER", GetParam().configured, 1);
        if (GetParam().vendor) setenv("SIM_G2A_VENDOR_RESULT", GetParam().vendor, 1);
        else unsetenv("SIM_G2A_VENDOR_RESULT");
    }
    void TearDown() override
    {
        unsetenv("SIM_G2A_SLOT_PARAMETER"); unsetenv("SIM_G2A_VENDOR_RESULT");
    }
};
}
