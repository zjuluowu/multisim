#include "query_inputs.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include "core_service_sim.h"
#include "sim_manager.h"
#include "mock_tel_ril_manager.h"
#include "parameter_mock.h"
#include "telephony_ext_wrapper.h"

class UpstreamRealSimCount : public SimHost::QueryTest {};
TEST_P(UpstreamRealSimCount, OriginalCoreAndManager)
{
    using namespace OHOS::Telephony;
    const char *configured = std::getenv("SIM_G2A_SLOT_PARAMETER");
    ASSERT_NE(configured, nullptr);
    const char *vendor = std::getenv("SIM_G2A_VENDOR_RESULT");
    testing::StrictMock<SimHost::MockParameterService> parameters;
    EXPECT_CALL(parameters, Read(testing::StrEq("const.telephony.slotCount"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([configured](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue(configured, value, length);
        });
    SimHost::ScopedParameterService installed(parameters);
    if (vendor) { TELEPHONY_EXT_WRAPPER.InitTelephonyExtWrapper(); }
    CoreServiceSim core;
    EXPECT_EQ(core.GetRealSimCount(), std::atoi(configured));
    auto ril = std::make_shared<testing::StrictMock<MockTelRilManager>>();
    auto manager = std::make_shared<SimManager>(ril);
    core.SetSimManager(manager);
    EXPECT_EQ(core.GetRealSimCount(), vendor ? std::atoi(vendor) : std::atoi(configured));
    EXPECT_EQ(core.GetSimManager().get(), manager.get());
    core.SetSimManager(nullptr);
    EXPECT_EQ(core.GetRealSimCount(), std::atoi(configured));
}

INSTANTIATE_TEST_SUITE_P(Configurations, UpstreamRealSimCount, testing::ValuesIn(QUERY_INPUTS),
    [](const auto &info) { return info.param.name; });
