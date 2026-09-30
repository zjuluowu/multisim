#include <gmock/gmock.h>
#include "mock_tel_ril_manager.h"
#include "sim_state_type.h"
using namespace OHOS::Telephony;
using OHOS::AppExecFwk::InnerEvent;
TEST(RilAdapter, FormalStatusResponse)
{
    RecordProperty("case_id", "INFRA-RIL-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "RIL-TRANSPORT");
    testing::StrictMock<MockTelRilManager> boundary;
    InnerEvent::Pointer reply(nullptr, nullptr);
    EXPECT_CALL(boundary, GetSimStatus(0, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([&reply](int32_t, const InnerEvent::Pointer &request) {
            auto payload = std::make_shared<SimCardStatusInfo>();
            payload->simState = static_cast<int32_t>(IccSimStatus::ICC_CARD_ABSENT);
            reply = InnerEvent::Get(request->GetInnerEventId(), payload, request->GetParam());
            reply->SetOwner(request->GetOwner());
            return TELEPHONY_ERR_SUCCESS;
        });
    ITelRilManager &ril = boundary;
    auto request = InnerEvent::Get(3, 0);
    ASSERT_EQ(ril.GetSimStatus(0, request), TELEPHONY_ERR_SUCCESS) << "RIL-TRANSPORT";
    ASSERT_NE(reply, nullptr) << "RIL-TRANSPORT";
    EXPECT_EQ(reply->GetInnerEventId(), request->GetInnerEventId()) << "RIL-TRANSPORT";
    EXPECT_EQ(reply->GetParam(), request->GetParam()) << "RIL-TRANSPORT";
    auto payload = reply->GetSharedObject<SimCardStatusInfo>();
    ASSERT_NE(payload, nullptr) << "RIL-TRANSPORT";
    EXPECT_EQ(payload->simState, static_cast<int32_t>(IccSimStatus::ICC_CARD_ABSENT)) << "RIL-TRANSPORT";
}
