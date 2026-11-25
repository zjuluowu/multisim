#include <gmock/gmock.h>
#include "sim_test.h"
#include "sim_manager.h"
#include "core_service_sim.h"
#include "parameter_mock.h"
#include "security_boundary.h"
#include "hisysevent.h"
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

TEST_F(RealSimPrimary, IndependentDefaultDataUsesOriginalManagerContract)
{
    constexpr auto dataKey = "persist.telephony.MainCellularDataSlotId";
    Sim028Host::PrimaryEnvironment rig(false);
    ASSERT_TRUE(rig.StartLockedCard());
    auto &core = CoreManagerInner::GetInstance();
    int32_t primaryBefore = -1;
    ASSERT_EQ(rig.manager->GetPrimarySlotId(primaryBefore), TELEPHONY_ERR_SUCCESS);
    const auto originalParameters = rig.parametersDb;
    rig.ClearObservations();

    EXPECT_EQ(core.SetDefaultCellularDataSlotId(-1), TELEPHONY_ERR_SLOTID_INVALID);
    EXPECT_EQ(core.SetDefaultCellularDataSlotId(100), TELEPHONY_ERR_SLOTID_INVALID);
    EXPECT_TRUE(rig.writes.empty());
    EXPECT_TRUE(rig.broadcasts.empty());

    rig.observation.Begin();
    std::vector<std::string> diagnostics;
    Sim028Host::ObserveCallingThreadDiagnostics([&](const std::string &name) {
        if (name == "SET_DEFAULT_CELLULAR_DATA" || name == "ACCOUNT_NOTIFY") {
            diagnostics.push_back(name);
            rig.observation.Add("diagnostic", {{"name", name}});
        }
    });
    const auto result = core.SetDefaultCellularDataSlotId(1);
    Sim028Host::ObserveCallingThreadDiagnostics({});
    EXPECT_EQ(result, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(diagnostics, (std::vector<std::string> {"SET_DEFAULT_CELLULAR_DATA", "ACCOUNT_NOTIFY"}));
    rig.observation.Add("api_return", {{"result", std::to_string(result)}});
    int32_t selectedSim = -1;
    EXPECT_EQ(core.GetDefaultCellularDataSimId(selectedSim), TELEPHONY_ERR_SUCCESS);
    testing::Test::RecordProperty("default_data_observation", rig.observation.Snapshot(rig.parametersDb,
        {{"primary", std::to_string(primaryBefore)}, {"data", std::to_string(core.GetDefaultCellularDataSlotId())},
            {"subscription", std::to_string(selectedSim)}}));
    EXPECT_EQ(core.SetDefaultCellularDataSlotId(1), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.GetDefaultCellularDataSlotId(), 1);
    int32_t simId = -1;
    EXPECT_EQ(core.GetDefaultCellularDataSimId(simId), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(simId, rig.manager->GetSimId(1));
    ASSERT_GT(simId, 0);

    // The in-process Manager accepts an in-range slot even with no active account.
    // A completed parameter failure still advances its legacy memory projection.
    EXPECT_CALL(rig.identity, WriteParameter(dataKey, "0")).WillOnce(testing::Return(-1));
    EXPECT_EQ(core.SetDefaultCellularDataSlotId(0), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.GetDefaultCellularDataSlotId(), 0);
    simId = -1;
    const auto slotZeroSimId = rig.manager->GetSimId(0);
    EXPECT_EQ(core.GetDefaultCellularDataSimId(simId),
        slotZeroSimId > 0 ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
    EXPECT_EQ(simId, slotZeroSimId > 0 ? slotZeroSimId : -1);

    int32_t primaryAfter = -1;
    EXPECT_EQ(rig.manager->GetPrimarySlotId(primaryAfter), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(primaryAfter, primaryBefore);
    auto expectedParameters = originalParameters;
    expectedParameters[dataKey] = "1";
    EXPECT_EQ(rig.parametersDb, expectedParameters);
    EXPECT_EQ(rig.writes, (std::vector<std::pair<std::string, std::string>> {{dataKey, "1"}, {dataKey, "1"}}));
    ASSERT_EQ(rig.broadcasts.size(), 3u);
    for (size_t i = 0; i < rig.broadcasts.size(); ++i) {
        const auto &want = rig.broadcasts[i];
        EXPECT_EQ(want.GetAction(),
            OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_DATA_SUBSCRIPTION_CHANGED);
        EXPECT_EQ(want.GetIntParam("slotId", -99), -99);
        if (i < 2) { EXPECT_EQ(want.GetIntParam("simId", -99), rig.manager->GetSimId(1)); }
    }
}

void VerifyDefaultRoleContract(bool voice)
{
    using namespace OHOS::Security::AccessToken;
    using namespace testing;
    Sim028Host::PrimaryEnvironment rig(false);
    ASSERT_TRUE(rig.StartLockedCard());
    if (voice) {
        EXPECT_CALL(rig.identity, TokenType(101)).WillRepeatedly(Return(TOKEN_NATIVE));
        EXPECT_CALL(rig.identity, Verify(101, Permission::SET_TELEPHONY_STATE))
            .WillRepeatedly(Return(PERMISSION_GRANTED));
    }
    auto &inner = CoreManagerInner::GetInstance();
    const auto setRole = [&](int slot) {
        return voice ? rig.core.SetDefaultVoiceSlotId(slot) : inner.SetDefaultSmsSlotId(slot);
    };
    const auto getRole = [&] { return voice ? inner.GetDefaultVoiceSlotId() : inner.GetDefaultSmsSlotId(); };
    const auto simId = rig.manager->GetSimId(1);
    ASSERT_GT(simId, 0);
    rig.ClearObservations();
    const auto parameters = rig.parametersDb;
    std::vector<int> writes;
    int providerResult = 0;
    EXPECT_CALL(*rig.provider, Update(Truly([](const auto &uri) {
        return uri.ToString() == "datashare:///com.ohos.simability/sim/sim_info/set_card";
    }), _, _)).WillRepeatedly([&](auto &, const auto &predicates, const auto &bucket) {
        EXPECT_TRUE(predicates.Requests().empty());
        EXPECT_EQ(bucket.valuesMap.size(), 2u);
        EXPECT_EQ(Sim028Host::ProviderRow::Text(bucket.valuesMap.at(SimData::CARD_TYPE)), voice ? "1" : "2");
        writes.push_back(std::stoi(Sim028Host::ProviderRow::Text(bucket.valuesMap.at(SimData::SIM_ID))));
        return providerResult;
    });
    const auto action = voice ?
        OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_VOICE_SUBSCRIPTION_CHANGED :
        OHOS::EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_SMS_SUBSCRIPTION_CHANGED;
    std::vector<int> announcements;
    bool publish = true;
    EXPECT_CALL(rig.services, Publish(Truly([&](const auto &event) {
        return event.GetWant().GetAction() == action;
    }), _)).WillRepeatedly([&](const auto &event, const auto &info) {
        EXPECT_EQ(event.GetCode(), 1);
        EXPECT_EQ(event.GetData(), voice ? "defaultVoiceSimIdChanged" : "defaultSmsSimIdChanged");
        EXPECT_FALSE(info.IsOrdered());
        announcements.push_back(event.GetWant().GetIntParam("simId", -1));
        return publish;
    });
    unsigned accountNotifications = 0;
    Sim028Host::ObserveCallingThreadDiagnostics([&](const auto &name) {
        if (name == "ACCOUNT_NOTIFY") { ++accountNotifications; }
    });
    EXPECT_EQ(setRole(100), TELEPHONY_ERR_SLOTID_INVALID);
    EXPECT_EQ(setRole(1), TELEPHONY_ERR_SUCCESS);
    const int selected = getRole();
    EXPECT_EQ(selected, 1);
    EXPECT_EQ(setRole(1), TELEPHONY_ERR_SUCCESS);
    providerResult = -1;
    EXPECT_EQ(setRole(-1), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_EQ(getRole(), 1);
    providerResult = 0;
    publish = false;
    const auto clearResult = setRole(-1);
    EXPECT_EQ(clearResult, TELEPHONY_ERR_PUBLISH_BROADCAST_FAIL);
    const int cleared = getRole();
    EXPECT_EQ(cleared, voice ? -1 : (rig.manager->IsSimActive(0) ? 0 : 1));
    EXPECT_EQ(setRole(-1), TELEPHONY_ERR_SUCCESS);
    Sim028Host::ObserveCallingThreadDiagnostics({});
    EXPECT_EQ(accountNotifications, 3u);
    EXPECT_EQ(writes, (std::vector<int> {simId, simId, 0, 0, 0}));
    EXPECT_EQ(announcements, (std::vector<int> {simId, 0}));
    EXPECT_TRUE(rig.writes.empty());
    EXPECT_EQ(rig.parametersDb, parameters);

    using SimGuard::Primary::Json;
    testing::Test::RecordProperty("default_call_message_observation", Json({
        {"selected", std::to_string(selected)}, {"cleared", std::to_string(cleared)},
        {"clearResult", std::to_string(clearResult)}, {"writes", std::to_string(writes.size())},
        {"announcements", std::to_string(announcements.size())},
        {"accountNotifications", std::to_string(accountNotifications)},
        {"identity", writes == std::vector<int> {simId, simId, 0, 0, 0} ? "true" : "false"}
    }));
}

TEST_F(RealSimPrimary, DefaultVoiceProviderAndNotificationContract)
{
    VerifyDefaultRoleContract(true);
}

TEST_F(RealSimPrimary, DefaultSmsProviderAndNotificationContract)
{
    VerifyDefaultRoleContract(false);
}
}
