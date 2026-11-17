#include <gmock/gmock.h>
#include "external_services_mock.h"
using namespace OHOS;
using namespace OHOS::DataShare;
using namespace OHOS::EventFwk;

TEST(ExternalData, EmptyCursorIsNotQueryFailure)
{
    RecordProperty("case_id", "INFRA-DATA-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "DS-CURSOR");
    DataShareResultSet cursor({"slot_index"}, {});
    int count = -1;
    ASSERT_EQ(cursor.GetRowCount(count), E_OK) << "DS-CURSOR";
    EXPECT_EQ(count, 0) << "DS-CURSOR";
    EXPECT_EQ(cursor.GoToFirstRow(), E_ERROR) << "DS-CURSOR";
    int value = 17;
    EXPECT_EQ(cursor.GetInt(0, value), E_ERROR) << "DS-CURSOR";
    EXPECT_EQ(value, 17) << "DS-CURSOR";
    EXPECT_EQ(cursor.Close(), E_OK) << "DS-CURSOR";
    EXPECT_EQ(cursor.GetRowCount(count), E_ERROR) << "DS-CURSOR";
}

TEST(ExternalData, ExplicitQueryResponses)
{
    RecordProperty("case_id", "INFRA-DATA-002");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "DS-QUERY");
    testing::StrictMock<SimHost::MockExternalServices> services;
    auto helper = std::make_shared<testing::StrictMock<SimHost::MockDataShareHelper>>();
    EXPECT_CALL(services, CreateDataShare(testing::_, "datashare:///com.ohos.simability", "", 2, false))
        .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(helper));
    SimHost::ScopedExternalServices scenario(services);
    bool requestMatched = false;
    EXPECT_CALL(*helper, Query(testing::_, testing::_, testing::_, nullptr))
        .WillOnce(testing::Return(nullptr))
        .WillOnce([&requestMatched](Uri &uri, const DataSharePredicates &filter,
            std::vector<std::string> &columns, DatashareBusinessError *) {
            requestMatched = uri.ToString() == "datashare:///com.ohos.simability" &&
                columns == std::vector<std::string>{"slot_index"} &&
                filter.Requests().size() == 1 && filter.Requests()[0].field == "slot_index";
            return std::make_shared<DataShareResultSet>(columns, std::vector<DataShareResultSet::Row>{});
        });
    auto token = std::make_shared<IRemoteObject>(); // Synthetic token; no SA/permission claim.
    auto client = DataShareHelper::Creator(token, "datashare:///com.ohos.simability");
    ASSERT_NE(client, nullptr) << "DS-QUERY";
    Uri uri("datashare:///com.ohos.simability");
    DataSharePredicates filter;
    filter.EqualTo("slot_index", DataShareValueObject(0));
    std::vector<std::string> columns{"slot_index"};
    EXPECT_EQ(client->Query(uri, filter, columns), nullptr) << "DS-QUERY";
    auto empty = client->Query(uri, filter, columns);
    ASSERT_NE(empty, nullptr) << "DS-QUERY";
    EXPECT_TRUE(requestMatched) << "DS-QUERY";
    int count = -1;
    ASSERT_EQ(empty->GetRowCount(count), E_OK) << "DS-QUERY";
    EXPECT_EQ(count, 0) << "DS-QUERY";
    EXPECT_CALL(*helper, Release()).WillOnce(testing::Return(true));
    EXPECT_TRUE(client->Release()) << "DS-QUERY";
}

TEST(ExternalData, OriginalBucketKeepsFirstPut)
{
    RecordProperty("case_id", "INFRA-DATA-003");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "DS-VALUES");
    DataShareValuesBucket values;
    values.Put("slot_index", DataShareValueObject(1));
    values.Put("slot_index", DataShareValueObject(2));
    bool valid = false;
    auto received = values.Get("slot_index", valid);
    ASSERT_TRUE(valid) << "DS-VALUES";
    EXPECT_EQ(std::get<int64_t>(received.value), 1) << "DS-VALUES";
    values.Clear();
    EXPECT_TRUE(values.IsEmpty()) << "DS-VALUES";
}

TEST(ExternalEvents, PublishPreservesEnvelopeAndFailure)
{
    RecordProperty("case_id", "INFRA-CES-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "CES-PUBLISH");
    testing::StrictMock<SimHost::MockExternalServices> services;
    SimHost::ScopedExternalServices scenario(services);
    bool envelopeMatched = false;
    EXPECT_CALL(services, Publish(testing::_, testing::_)).Times(testing::AnyNumber())
        .WillRepeatedly([&envelopeMatched](const CommonEventData &data, const CommonEventPublishInfo &info) {
            envelopeMatched = data.GetWant().GetAction() == CommonEventSupport::COMMON_EVENT_SIM_STATE_CHANGED &&
                data.GetCode() == 7 && !info.IsOrdered() && info.IsSticky();
            return false;
        });
    AAFwk::Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_SIM_STATE_CHANGED);
    CommonEventData data(want, 7, "");
    CommonEventPublishInfo info;
    info.SetOrdered(false);
    info.SetSticky(true);
    EXPECT_FALSE(CommonEventManager::PublishCommonEvent(data, info, nullptr)) << "CES-PUBLISH";
    EXPECT_TRUE(envelopeMatched) << "CES-PUBLISH";
}

TEST(ExternalEvents, MissingProviderAndScopeCleanup)
{
    RecordProperty("case_id", "INFRA-CES-002");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "CES-LIFETIME");
    CommonEventData data;
    EXPECT_FALSE(CommonEventManager::PublishCommonEvent(data)) << "CES-LIFETIME";
    EXPECT_EQ(DataShareHelper::Creator(nullptr, "datashare:///com.ohos.simability"), nullptr) << "CES-LIFETIME";
    {
        testing::StrictMock<SimHost::MockExternalServices> services;
        EXPECT_CALL(services, Publish(testing::_, testing::_)).WillOnce(testing::Return(true));
        SimHost::ScopedExternalServices scenario(services);
        EXPECT_TRUE(CommonEventManager::PublishCommonEvent(data)) << "CES-LIFETIME";
    }
    EXPECT_FALSE(CommonEventManager::PublishCommonEvent(data)) << "CES-LIFETIME";
}
