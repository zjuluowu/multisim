#include <gtest/gtest.h>
#include "inner_event.h"
using OHOS::AppExecFwk::InnerEvent;

TEST(EventAdapter, SharedTypeSafety)
{
    RecordProperty("case_id", "INFRA-EVENT-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "EVT-TYPE");
    const auto input = std::make_shared<int>(7);
    auto event = InnerEvent::Get(12, input, 3);
    EXPECT_EQ(event->GetSharedObject<int>(), input) << "EVT-TYPE";
    EXPECT_EQ(event->GetSharedObject<std::string>(), nullptr) << "EVT-TYPE";
    EXPECT_EQ(event->GetUniqueObject<int>(), nullptr) << "EVT-TYPE";
}

TEST(EventAdapter, UniqueOwnership)
{
    RecordProperty("case_id", "INFRA-EVENT-002");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "EVT-OWNERSHIP");
    auto input = std::make_unique<int>(9);
    auto event = InnerEvent::Get(12, input);
    EXPECT_EQ(input, nullptr) << "EVT-OWNERSHIP";
    EXPECT_EQ(event->GetUniqueObject<std::string>(), nullptr) << "EVT-OWNERSHIP";
    auto received = event->GetUniqueObject<int>();
    ASSERT_NE(received, nullptr) << "EVT-OWNERSHIP";
    EXPECT_EQ(*received, 9) << "EVT-OWNERSHIP";
    EXPECT_EQ(event->GetUniqueObject<int>(), nullptr) << "EVT-OWNERSHIP";
}

TEST(EventAdapter, WeakLifetimeAndMetadata)
{
    RecordProperty("case_id", "INFRA-EVENT-003");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "EVT-LIFETIME");
    auto input = std::make_shared<int>(11);
    auto event = InnerEvent::Get(23, std::weak_ptr<int>(input), 5);
    const auto time = InnerEvent::Clock::now();
    event->SetHandleTime(time);
    EXPECT_EQ(event->GetSharedObject<int>(), input) << "EVT-LIFETIME";
    input.reset();
    EXPECT_EQ(event->GetSharedObject<int>(), nullptr) << "EVT-LIFETIME";
    EXPECT_EQ(event->GetInnerEventId(), 23u) << "EVT-LIFETIME";
    EXPECT_EQ(event->GetParam(), 5) << "EVT-LIFETIME";
    EXPECT_EQ(event->GetHandleTime(), time) << "EVT-LIFETIME";
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
