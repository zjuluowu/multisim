/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef SIM_TEST_ACCOUNT_RELOAD_SCENARIO_H
#define SIM_TEST_ACCOUNT_RELOAD_SCENARIO_H

#include <string>
#include <gtest/gtest.h>

namespace SimGuard {
enum class ReloadView { Current, Historical };

// Legacy's historical cache indexes the provider rows by simId - 1.
inline constexpr int kCurrentAccount = 2;
inline constexpr int kHistoricalAccount = 1;
inline constexpr char kCurrentReloadIccid[] = "8900000000000000001";
inline constexpr char kHistoricalReloadIccid[] = "8900000000000000009";

template<class SetStorage, class Reload, class ReadName>
void ExerciseFreshAccountReload(SetStorage setStorage, Reload reload, ReadName readName)
{
    using V = ReloadView;
    setStorage(u"Current A", u"Historical A");
    ASSERT_TRUE(reload(V::Current));
    ASSERT_TRUE(reload(V::Historical));
    EXPECT_EQ(readName(V::Current, kCurrentAccount), u"Current A");
    EXPECT_TRUE(readName(V::Current, kHistoricalAccount).empty());
    EXPECT_EQ(readName(V::Historical, kCurrentAccount), u"Current A");
    EXPECT_EQ(readName(V::Historical, kHistoricalAccount), u"Historical A");

    setStorage(u"Current B", u"Historical B");
    ASSERT_TRUE(reload(V::Current));
    EXPECT_EQ(readName(V::Current, kCurrentAccount), u"Current B");
    EXPECT_EQ(readName(V::Historical, kCurrentAccount), u"Current A");
    EXPECT_EQ(readName(V::Historical, kHistoricalAccount), u"Historical A");

    ASSERT_TRUE(reload(V::Historical));
    EXPECT_EQ(readName(V::Historical, kCurrentAccount), u"Current B");
    EXPECT_EQ(readName(V::Historical, kHistoricalAccount), u"Historical B");
}
} // namespace SimGuard

#endif // SIM_TEST_ACCOUNT_RELOAD_SCENARIO_H
