// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <gtest/gtest.h>
#include <string>
namespace SimGuard
{
// Abort the whole GoogleTest body after reporting kSkip, including from a
// non-void capability helper. GoogleTest catches AssertionException and still
// invokes TearDown. No dummy success result escapes into business assertions.
[[noreturn]] inline void SkipCapability(const std::string &reason)
{
    GTEST_MESSAGE_(reason.c_str(), ::testing::TestPartResult::kSkip);
    throw testing::AssertionException(
        testing::TestPartResult(testing::TestPartResult::kSkip, __FILE__, __LINE__, reason.c_str()));
}
} // namespace SimGuard
