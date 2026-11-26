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

TEST_F(RealSimPrimary, AccountMetadataUsesLegacyPredicatesAndFieldCaches)
{
    using namespace testing;
    Sim028Host::PrimaryEnvironment rig(false);
    ASSERT_TRUE(rig.StartLockedCard());
    auto &manager = *rig.manager;
    auto &inner = CoreManagerInner::GetInstance();
    IccAccountInfo original;
    ASSERT_EQ(manager.GetSimAccountInfo(1, false, original), TELEPHONY_ERR_SUCCESS);
    ASSERT_GT(original.simId, 0);
    const auto iccid = OHOS::Str16ToStr8(original.iccId);
    unsigned calls = 0;
    int providerResult = 0;
    std::vector<std::string> fieldTrace;
    EXPECT_CALL(*rig.provider, Update(_, _, _)).WillRepeatedly([&](auto &uri, const auto &predicates,
        const auto &bucket) {
        EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
        EXPECT_EQ(bucket.valuesMap.size(), 1u);
        EXPECT_EQ(predicates.Requests().size(), 1u);
        const auto &where = predicates.Requests().at(0);
        const bool name = bucket.valuesMap.count(SimData::SHOW_NAME);
        EXPECT_EQ(where.field, name ? SimData::SIM_ID : SimData::ICC_ID);
        EXPECT_EQ(where.kind, "EqualTo");
        EXPECT_EQ(Sim028Host::ProviderRow::Text(where.value.value),
            name ? std::to_string(original.simId) : iccid);
        const auto &field = bucket.valuesMap.begin()->first;
        fieldTrace.emplace_back(field);
        ++calls;
        return providerResult;
    });
    unsigned notifications = 0;
    Sim028Host::ObserveCallingThreadDiagnostics([&](const auto &name) {
        if (name == "ACCOUNT_NOTIFY") { ++notifications; }
    });
    EXPECT_EQ(inner.SetShowName(1, u"Host account"), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(inner.SetShowName(1, u"Host account"), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(inner.SetShowName(1, u""), TELEPHONY_ERR_SUCCESS);
    std::u16string name = u"not the cache";
    EXPECT_EQ(inner.GetShowName(1, name), TELEPHONY_ERR_SUCCESS);
    EXPECT_TRUE(name.empty());
    EXPECT_EQ(manager.SetSimLabelIndex(original.simId, 3), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(inner.SetSimLabelIndex(iccid, 11), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(inner.SetSimLabelIndex(iccid, 3), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(inner.SetSimLabelIndex(iccid, 3), TELEPHONY_ERR_SUCCESS);
    providerResult = -1;
    EXPECT_EQ(inner.SetShowName(1, u"failed"), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_EQ(inner.SetSimLabelIndex(iccid, 4), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    Sim028Host::ObserveCallingThreadDiagnostics({});
    IccAccountInfo current;
    ASSERT_EQ(manager.GetSimAccountInfo(1, false, current), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(current.simLabelIndex, 3);
    EXPECT_TRUE(current.showName.empty());
    EXPECT_EQ(current.simId, original.simId);
    EXPECT_EQ(current.showNumber, original.showNumber);
    EXPECT_EQ(notifications, 5u);
    EXPECT_EQ(calls, 7u);
    EXPECT_EQ(fieldTrace, (std::vector<std::string> {
        "show_name", "show_name", "show_name", "sim_label_index", "sim_label_index", "show_name", "sim_label_index"}));
    using SimGuard::Primary::Json;
    testing::Test::RecordProperty("account_metadata_observation", Json({
        {"label", std::to_string(current.simLabelIndex)}, {"emptyName", current.showName.empty() ? "true" : "false"},
        {"providerCalls", std::to_string(calls)}, {"accountNotifications", std::to_string(notifications)},
        {"identity", current.simId == original.simId ? "true" : "false"}
    }));
}

TEST_F(RealSimPrimary, UnknownIccidLabelCanSucceedWithoutChangingAccount)
{
    using namespace testing;
    Sim028Host::PrimaryEnvironment rig(false);
    ASSERT_TRUE(rig.StartLockedCard());
    IccAccountInfo before;
    ASSERT_EQ(rig.manager->GetSimAccountInfo(1, false, before), TELEPHONY_ERR_SUCCESS);
    const std::string missingIccid = "8900000000000099999";
    constexpr int32_t label = 7;
    EXPECT_CALL(*rig.provider, Update(_, _, _)).WillOnce([&](auto &uri, const auto &predicates, const auto &bucket) {
        EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
        EXPECT_EQ(bucket.valuesMap.size(), 1u);
        EXPECT_EQ(Sim028Host::ProviderRow::Text(bucket.valuesMap.at(SimData::SIM_LABEL_INDEX)), "7");
        EXPECT_EQ(predicates.Requests().size(), 1u);
        const auto &predicate = predicates.Requests().at(0);
        EXPECT_EQ(predicate.field, SimData::ICC_ID);
        EXPECT_EQ(predicate.kind, "EqualTo");
        EXPECT_EQ(Sim028Host::ProviderRow::Text(predicate.value.value), missingIccid);
        EXPECT_FALSE(rig.row.Matches(predicates));
        if (rig.companionRow) { EXPECT_FALSE(rig.companionRow->Matches(predicates)); }
        return 0;
    });

    EXPECT_EQ(CoreManagerInner::GetInstance().SetSimLabelIndex(missingIccid, label), TELEPHONY_ERR_SUCCESS);
    IccAccountInfo after;
    ASSERT_EQ(rig.manager->GetSimAccountInfo(1, false, after), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(after.simId, before.simId);
    EXPECT_EQ(after.simLabelIndex, before.simLabelIndex);
}

TEST_F(RealSimPrimary, ImsPreferencePreservesProviderReturnAndReadSource)
{
    using namespace testing;
    Sim028Host::PrimaryEnvironment rig(false);
    ASSERT_TRUE(rig.StartLockedCard());
    auto &inner = CoreManagerInner::GetInstance();
    IccAccountInfo account;
    ASSERT_EQ(rig.manager->GetSimAccountInfo(1, false, account), TELEPHONY_ERR_SUCCESS);
    const auto iccid = OHOS::Str16ToStr8(account.iccId);
    ASSERT_FALSE(iccid.empty());
    std::string predicate = iccid;
    const auto checkPredicate = [&](auto &uri, const auto &where) {
        EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
        EXPECT_EQ(where.Requests().size(), 1u);
        EXPECT_EQ(where.Requests().at(0).kind, "EqualTo");
        EXPECT_EQ(where.Requests().at(0).field, SimData::ICC_ID);
        EXPECT_EQ(Sim028Host::ProviderRow::Text(where.Requests().at(0).value.value), predicate);
    };
    unsigned writes = 0;
    unsigned reads = 0;
    unsigned notifications = 0;
    int32_t rawWrite = 0;
    enum class ReadMode { Row, NoRow, Failure };
    ReadMode readMode = ReadMode::Row;
    std::vector<int32_t> written;
    EXPECT_CALL(*rig.provider, Update(_, _, _)).WillRepeatedly([&](auto &uri, const auto &where, const auto &values) {
        checkPredicate(uri, where);
        EXPECT_EQ(values.valuesMap.size(), 1u);
        const auto &value = values.valuesMap.at(SimData::IMS_SWITCH);
        written.push_back(static_cast<int32_t>(std::get<int64_t>(value)));
        ++writes;
        if (rawWrite >= 0 && predicate == iccid) { rig.row.cells[SimData::IMS_SWITCH] = value; }
        return rawWrite;
    });
    EXPECT_CALL(*rig.provider, Query(_, _, _, _)).WillRepeatedly(
        [&](auto &uri, const auto &where, const auto &columns, auto *) -> std::shared_ptr<OHOS::DataShare::DataShareResultSet> {
            checkPredicate(uri, where);
            EXPECT_TRUE(columns.empty());
            ++reads;
            if (readMode == ReadMode::Failure) { return nullptr; }
            if (readMode == ReadMode::NoRow) { return std::make_shared<OHOS::DataShare::DataShareResultSet>(
                std::vector<std::string> {}, std::vector<OHOS::DataShare::DataShareResultSet::Row> {}); }
            return rig.row.Query(where);
        });
    Sim028Host::ObserveCallingThreadDiagnostics([&](const auto &name) {
        if (name == "ACCOUNT_NOTIFY") { ++notifications; }
    });
    int32_t value = 99;
    EXPECT_EQ(inner.SaveImsSwitch(-1, 100), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(inner.QueryImsSwitch(-1, value), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(value, 99);
    const auto first = inner.SaveImsSwitch(1, 17);
    const auto repeat = inner.SaveImsSwitch(1, 17);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(repeat, 0);
    EXPECT_EQ(inner.QueryImsSwitch(1, value), 0);
    EXPECT_EQ(value, 17);
    rawWrite = 6;
    const auto positive = inner.SaveImsSwitch(1, -9);
    EXPECT_EQ(positive, 6);
    EXPECT_EQ(inner.QueryImsSwitch(1, value), 0);
    EXPECT_EQ(value, -9);
    rawWrite = -23;
    const auto failure = inner.SaveImsSwitch(1, 123);
    EXPECT_EQ(failure, -23);
    rig.row.cells[SimData::IMS_SWITCH] = int64_t(33); // Independent Provider edit.
    EXPECT_EQ(inner.QueryImsSwitch(1, value), 0);
    EXPECT_EQ(value, 33);
    const auto external = value;
    rig.row.cells[SimData::IMS_SWITCH] = int64_t(-1); // A row with explicit UNKNOWN.
    EXPECT_EQ(inner.QueryImsSwitch(1, value), 0);
    EXPECT_EQ(value, -1);
    readMode = ReadMode::NoRow;
    const auto empty = inner.QueryImsSwitch(1, value);
    EXPECT_EQ(empty, 0);
    EXPECT_EQ(value, -1);
    readMode = ReadMode::Failure;
    const auto failedRead = inner.QueryImsSwitch(1, value);
    EXPECT_EQ(failedRead, 0);
    EXPECT_EQ(value, -1);
    const auto commonReads = reads;
    const auto commonWrites = writes;
    EXPECT_EQ(written, (std::vector<int32_t> {17, 17, -9, 123}));
    EXPECT_EQ(commonReads, 6u);
    EXPECT_EQ(commonWrites, 4u);

    if (!rig.companionRow) {
        // In the one-active model slot 0 has a cache position with an empty ICCID.
        predicate.clear();
        readMode = ReadMode::NoRow;
        rawWrite = 0;
        EXPECT_EQ(inner.SaveImsSwitch(0, 7), 0);
        EXPECT_EQ(inner.QueryImsSwitch(0, value), 0);
        EXPECT_EQ(value, -1);
        EXPECT_EQ(writes, commonWrites + 1);
        EXPECT_EQ(reads, commonReads + 1);
        RecordProperty("empty_iccid_provider_access", "true");
    }
    Sim028Host::ObserveCallingThreadDiagnostics({});
    EXPECT_EQ(notifications, 0u);
    RecordProperty("ims_preference_observation", SimGuard::Primary::Json({
        {"first", std::to_string(first)}, {"repeat", std::to_string(repeat)},
        {"positive", std::to_string(positive)}, {"failure", std::to_string(failure)},
        {"externalValue", std::to_string(external)}, {"noRow", std::to_string(empty)},
        {"failedRead", std::to_string(failedRead)}, {"unknown", std::to_string(value)},
        {"writes", std::to_string(commonWrites)}, {"reads", std::to_string(commonReads)},
        {"notifications", std::to_string(notifications)}
    }));
}

TEST_F(RealSimPrimary, ShowNumberReadsAccountAfterMsisdnFailure)
{
    using namespace testing;
    Sim028Host::PrimaryEnvironment rig(false);
    rig.row.cells[SimData::PHONE_NUMBER] = std::string("555123");
    ASSERT_TRUE(rig.StartLockedCard());
    unsigned cardCommands = 0;
    EXPECT_CALL(*rig.ril, GetSimIO(1, Field(&SimIoRequestInfo::fileId, ELEMENTARY_FILE_MSISDN), _))
        .WillOnce([&](int slot, const auto &info, const auto &event) {
            ++cardCommands;
            EXPECT_EQ(info.command, 0xc0);
            auto request = event->template GetUniqueObject<IccToRilMsg>();
            EXPECT_TRUE(request);
            auto response = std::make_shared<IccFromRilMsg>(request->controlHolder);
            auto failed = std::make_shared<RadioResponseInfo>();
            failed->error = ErrType::ERR_GENERIC_FAILURE;
            response->fileData.exception = failed;
            auto owner = std::dynamic_pointer_cast<TelEventHandler>(event->GetOwner());
            EXPECT_TRUE(owner && owner->SendEvent(OHOS::AppExecFwk::InnerEvent::Get(event->GetInnerEventId(), response)));
            EXPECT_EQ(slot, 1);
            return 0;
        });
    EXPECT_CALL(*rig.provider, Update(_, _, _)).Times(0);
    const auto failure = rig.manager->SetShowNumber(1, u"555999");
    EXPECT_EQ(failure, TELEPHONY_ERR_FAIL);
    EXPECT_EQ(cardCommands, 1u);
    std::u16string number;
    const auto result = rig.manager->GetShowNumber(1, number);
    EXPECT_EQ(result, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(number, u"555123");
    RecordProperty("show_number_observation", SimGuard::Primary::Json({
        {"cachedNumber", OHOS::Str16ToStr8(number)}, {"readResult", std::to_string(result)},
        {"failed", failure == TELEPHONY_ERR_FAIL ? "true" : "false"}, {"commands", std::to_string(cardCommands)}
    }));
}
}
