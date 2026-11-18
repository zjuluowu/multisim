#include <gmock/gmock.h>
#include "sim_test.h"
#include "sim_manager.h"
#include "core_service_sim.h"
#include "parameter_mock.h"
#include "security_boundary.h"
#include "external_services_mock.h"
#include "ffrt_runtime.h"
#include "internal/host/primary/primary_environment.h"
#include <future>
#include <optional>
namespace {
using namespace OHOS::Telephony;
using RealSimPrimary=SimGuard::SimTest;
using namespace std::chrono_literals;
TEST_F(RealSimPrimary, InitializesRealManagerAndControllerWithoutRil)
{
    testing::StrictMock<SimHost::MockParameterService> parameters;
    EXPECT_CALL(parameters, Read(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly([](const char *key, const char *fallback, char *out, unsigned size) {
            return SimHost::CopyParameterValue(std::string(key) == "const.telephony.slotCount" ? "2" : fallback, out, size);
        });
    SimHost::ScopedParameterService installed(parameters);
    testing::StrictMock<PrimarySlotHost::ExternalIdentity> identity;
    PrimarySlotHost::IdentityScope security(identity);
    testing::StrictMock<SimHost::MockExternalServices> services;
    EXPECT_CALL(services, Publish(testing::_, testing::_)).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(services, CreateDataShare(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(nullptr)); // explicit unavailable Provider; never fabricated success.
    SimHost::ScopedExternalServices external(services);
    auto manager = std::make_shared<SimManager>(nullptr);
    ASSERT_TRUE(manager->OnInit(SIM_SLOT_COUNT_MD + 1));
    EXPECT_EQ(manager->GetMaxSimCount(), 2);
    testing::Test::RecordProperty("primary_max_sim_count", manager->GetMaxSimCount());
    auto observedNoRil = [&](int result) { testing::Test::RecordProperty("primary_direct_no_ril_result", result); testing::Test::RecordProperty("primary_direct_no_ril_symbol", Sim028Host::PrimaryEnvironment::ErrorSymbol(result)); return result; };
    EXPECT_EQ(observedNoRil(manager->SetPrimarySlotId(1, true)), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_FALSE(manager->IsSetPrimarySlotIdInProgress());
    EXPECT_TRUE(manager->Stop());
    SimHost::StopCallingThreadTasks();
    manager.reset();
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
}
}
