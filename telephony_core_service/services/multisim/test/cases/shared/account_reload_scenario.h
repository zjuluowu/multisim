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
inline constexpr int kSparseCurrentAccount = 41;
inline constexpr int kSparseHistoricalAccount = 49;

// CD-04 exercises the same Provider images on both backends. Legacy observation is
// deliberately separate from the required Target result.
template<class SetStorage, class Reload, class ReadName>
void ExerciseSparseAccountReload(SetStorage setStorage, Reload reload, ReadName readName, bool preservesHistorical)
{
    using V = ReloadView;
    const auto verifyHistorical = [&](const std::u16string &current, const std::u16string &historical) {
        EXPECT_EQ(readName(V::Historical, kSparseCurrentAccount),
            preservesHistorical ? current : std::u16string {});
        EXPECT_EQ(readName(V::Historical, kSparseHistoricalAccount),
            preservesHistorical ? historical : std::u16string {});
        EXPECT_TRUE(readName(V::Historical, 42).empty());
    };
    setStorage(u"Current A", u"Historical A", false, true);
    ASSERT_TRUE(reload(V::Current));
    ASSERT_TRUE(reload(V::Historical));
    EXPECT_EQ(readName(V::Current, kSparseCurrentAccount), u"Current A");
    EXPECT_TRUE(readName(V::Current, kSparseHistoricalAccount).empty());
    EXPECT_TRUE(readName(V::Current, 42).empty());
    verifyHistorical(u"Current A", u"Historical A");

    setStorage(u"Current B", u"Historical B", true, true);
    ASSERT_TRUE(reload(V::Current));
    EXPECT_EQ(readName(V::Current, kSparseCurrentAccount), u"Current B");
    verifyHistorical(u"Current A", u"Historical A");
    ASSERT_TRUE(reload(V::Historical));
    verifyHistorical(u"Current B", u"Historical B");

    // Provider image after deleting row 49. Row 41 must keep its original ID.
    setStorage(u"Current C", u"", false, false);
    ASSERT_TRUE(reload(V::Historical));
    verifyHistorical(u"Current C", u"");
    EXPECT_EQ(readName(V::Current, kSparseCurrentAccount), u"Current B");
    ASSERT_TRUE(reload(V::Current));
    EXPECT_EQ(readName(V::Current, kSparseCurrentAccount), u"Current C");
}
} // namespace SimGuard

#endif // SIM_TEST_ACCOUNT_RELOAD_SCENARIO_H
