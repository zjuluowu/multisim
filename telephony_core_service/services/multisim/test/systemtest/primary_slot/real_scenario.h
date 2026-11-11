#pragma once
#include "sim_manager.h"
#include "core_service_sim.h"
#include "core_manager_inner.h"
#include "mock_tel_ril_manager.h"
#include "external_services_mock.h"
#include "parameter_mock.h"
#include "security_boundary.h"
#include "ffrt_runtime.h"
#include "radio_event.h"
#include "tel_ril_sim_parcel.h"
#include "sim_data.h"
#include "sim_data_type.h"
#include "sim_constant.h"
#include "multi_sim_controller.h"
#include "radio_protocol_controller.h"
#include "telephony_permission.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>
#include <regex>
namespace Sim028Host {
using namespace OHOS;
using namespace OHOS::Telephony;
using namespace std::chrono_literals;
using testing::_;
using Value = DataShare::DataShareValueObject::Type;
// External relational row storage only: no SIM state transitions, selection, or business decisions.
class ProviderRow {
public:
    std::map<std::string, Value> cells;
    ProviderRow()
    {
        for (const auto &column : {SimData::SIM_ID, SimData::SLOT_INDEX, SimData::IMS_SWITCH,
            SimData::CARD_TYPE, SimData::IS_MAIN_CARD, SimData::IS_VOICE_CARD, SimData::IS_MESSAGE_CARD,
            SimData::IS_CELLULAR_DATA_CARD, SimData::IS_ACTIVE, SimData::IS_ESIM, SimData::SIM_LABEL_INDEX,
            SimData::PHY_CARD, SimData::LSI, SimData::MNC_LEN}) cells[column] = int64_t(0);
        for (const auto &column : {SimData::ICC_ID, SimData::CARD_ID, SimData::SHOW_NAME,
            SimData::PHONE_NUMBER, SimData::COUNTRY_CODE, SimData::LANGUAGE, SimData::IMSI,
            SimData::OPERATOR_NAME, SimData::GID1, SimData::GID2, SimData::SPN, SimData::EHPLMN})
            cells[column] = std::string();
        cells[SimData::SIM_ID] = int64_t(1);
        cells[SimData::SLOT_INDEX] = int64_t(-1); // known account, not yet bound after restart
        cells[SimData::IS_ACTIVE] = int64_t(1); // persisted user activation
        cells[SimData::ICC_ID] = std::string("8900000000000000001");
    }
    static std::string Text(const Value &value)
    {
        if (const auto *p = std::get_if<std::string>(&value)) return *p;
        if (const auto *p = std::get_if<int64_t>(&value)) return std::to_string(*p);
        throw std::logic_error("Provider fixture supports string/integer cells only");
    }
    bool Matches(const DataShare::DataSharePredicates &predicate) const
    {
        for (const auto &op : predicate.Requests()) {
            auto it = cells.find(op.field);
            if (it == cells.end()) throw std::logic_error("Unsupported Provider column: " + op.field);
            auto left = Text(it->second), right = Text(op.value.value);
            if (op.kind == "EqualTo") { if (left != right) return false; }
            else if (op.kind == "NotEqualTo") { if (left == right) return false; }
            else if (op.kind == "GreaterThan") { if (std::stoll(left) <= std::stoll(right)) return false; }
            else if (op.kind == "Unlike") {
                std::string pattern;
                for (char c : right) {
                    if (c == '%') pattern += ".*";
                    else if (c == '_') pattern += ".";
                    else if (std::isalnum(static_cast<unsigned char>(c))) pattern += c;
                    else throw std::logic_error("Unsupported SQL LIKE fixture character");
                }
                if (std::regex_match(left, std::regex(pattern))) return false;
            } else throw std::logic_error("Unsupported Provider predicate: " + op.kind);
        }
        return true;
    }
    auto Query(const DataShare::DataSharePredicates &predicate)
    {
        std::vector<std::string> columns;
        DataShare::DataShareResultSet::Row row;
        for (const auto &[key, value] : cells) { columns.push_back(key); row.push_back(value); }
        std::vector<DataShare::DataShareResultSet::Row> rows;
        if (Matches(predicate)) rows.push_back(row);
        return std::make_shared<DataShare::DataShareResultSet>(columns, rows);
    }
};
void StopExternalRunner();
class AccountObserver final : public TelEventHandler {
public:
    explicit AccountObserver(std::function<void()> loaded) : TelEventHandler("sim028-account-observer"), loaded_(std::move(loaded)) {}
    void ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event) override
    { if (event && event->GetInnerEventId() == RadioEvent::RADIO_SIM_ACCOUNT_LOADED) loaded_(); }
private:
    std::function<void()> loaded_;
};
class RealScenario {
public:
    testing::StrictMock<SimHost::MockParameterService> parameters;
    SimHost::ScopedParameterService parameterScope{parameters};
    testing::StrictMock<PrimarySlotHost::ExternalIdentity> identity;
    PrimarySlotHost::IdentityScope security{identity};
    testing::StrictMock<SimHost::MockExternalServices> services;
    SimHost::ScopedExternalServices external{services};
    std::shared_ptr<testing::StrictMock<SimHost::MockDataShareHelper>> provider =
        std::make_shared<testing::StrictMock<SimHost::MockDataShareHelper>>();
    std::shared_ptr<testing::StrictMock<MockTelRilManager>> ril =
        std::make_shared<testing::StrictMock<MockTelRilManager>>();
    std::shared_ptr<SimManager> manager;
    CoreServiceSim core;
    std::shared_ptr<AccountObserver> accountObserver;
    bool accountLoaded = false;
    std::mutex mutex;
    std::condition_variable changed;
    ProviderRow row;
    std::map<std::string, std::string> parametersDb{{"const.telephony.slotCount", "2"}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::vector<std::string> events;
    std::vector<AAFwk::Want> broadcasts;
    struct Pending { int slot; std::weak_ptr<AppExecFwk::EventHandler> owner; uint32_t id; };
    std::vector<Pending> statusRequests;
    unsigned updates = 0;
    bool autoStatus = false;
    std::map<std::pair<int, int>, std::weak_ptr<AppExecFwk::EventHandler>> notifications;
    template<class T> static void Reply(const Pending &request, std::shared_ptr<T> response)
    {
        auto owner = std::dynamic_pointer_cast<TelEventHandler>(request.owner.lock());
        if (!owner || !owner->SendEvent(AppExecFwk::InnerEvent::Get(request.id, response)))
            throw std::runtime_error("Real RIL response owner unavailable");
    }
    static auto CardStatus(int slot)
    {
        auto response = std::make_shared<SimCardStatusInfo>();
        response->index = slot; response->simType = ICC_USIM_TYPE;
        response->simState = slot == 1 ? ICC_CONTENT_PIN : ICC_CARD_ABSENT;
        response->iccid = slot == 1 ? "8900000000000000001" : "";
        return response;
    }
    RealScenario(bool directRil = false)
    {
        parametersDb["const.vendor.ril.set_primary_slot_support"] = directRil ? "true" : "false";
        EXPECT_CALL(parameters, Read(_, _, _, _)).WillRepeatedly([this](auto key, auto fallback, auto out, auto size) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = parametersDb.find(key);
            return SimHost::CopyParameterValue(it == parametersDb.end() ? fallback : it->second.c_str(), out, size);
        });
        EXPECT_CALL(identity, WriteParameter(_, _)).WillRepeatedly([this](auto key, auto value) {
            std::lock_guard<std::mutex> lock(mutex);
            writes.emplace_back(key, value); parametersDb[key] = value;
            std::cout << "TRACE parameter " << key << "=" << value << "\n"; changed.notify_all(); return 0;
        });
        EXPECT_CALL(services, Publish(_, _)).WillRepeatedly([this](const auto &event, const auto &) {
            std::lock_guard<std::mutex> lock(mutex);
            events.push_back(event.GetWant().GetAction()); broadcasts.push_back(event.GetWant());
            std::cout << "TRACE notify " << event.GetWant().GetAction()
                << " slot=" << event.GetWant().GetIntParam("slotId", -1)
                << " sim=" << event.GetWant().GetIntParam("simId", -1)
                << " done=" << event.GetWant().GetBoolParam("setDone", false)
                << " user=" << event.GetWant().GetBoolParam("isUserSet", false) << "\n";
            changed.notify_all(); return true;
        });
        EXPECT_CALL(services, CreateDataShare(_, _, _, _, _)).WillRepeatedly([this](const auto &token,
            const auto &uri, const auto &ext, int, bool) -> std::shared_ptr<DataShare::DataShareHelper> {
            EXPECT_NE(token, nullptr);
            if (uri != "datashare:///com.ohos.simability") return nullptr; // other providers explicitly absent
            EXPECT_TRUE(ext.empty()); return provider;
        });
        EXPECT_CALL(*provider, Release()).WillRepeatedly(testing::Return(true));
        EXPECT_CALL(*provider, Query(_, _, _, _)).WillRepeatedly([this](auto &uri, const auto &predicate, auto &, auto *) {
            EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
            std::lock_guard<std::mutex> lock(mutex); return row.Query(predicate);
        });
        EXPECT_CALL(*provider, Update(_, _, _)).WillRepeatedly([this](auto &uri, const auto &predicate, const auto &bucket) {
            EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
            std::lock_guard<std::mutex> lock(mutex);
            if (row.Matches(predicate)) for (const auto &[key, value] : bucket.valuesMap) row.cells[key] = value;
            ++updates; changed.notify_all(); return 0; // verified Provider RdbStore::Update status, not row count
        });
        EXPECT_CALL(*ril, RegisterCoreNotify(_, _, _, _)).WillRepeatedly([this](int slot, const auto &owner, int id, int *) {
            std::lock_guard<std::mutex> lock(mutex); notifications[{slot, id}] = owner; return 0;
        });
        EXPECT_CALL(*ril, UnRegisterCoreNotify(_, _, _)).WillRepeatedly(testing::Return(0));
        EXPECT_CALL(*ril, SetActiveSim(1, _, 1, _)).WillRepeatedly([](int, int, int, const auto &event) {
            auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::NONE;
            auto owner = std::dynamic_pointer_cast<TelEventHandler>(event->GetOwner());
            EXPECT_TRUE(owner && owner->SendEvent(AppExecFwk::InnerEvent::Get(event->GetInnerEventId(), response)));
            return 0;
        });
        // The locked-card EF read is explicitly unavailable. Match the actual
        // TelRilSim::ErrorIccIoResponse transport shape, retaining the original holder.
        EXPECT_CALL(*ril, GetSimIO(1, _, _)).WillRepeatedly([](int slot, const auto &info, const auto &event) {
            EXPECT_THAT(info.fileId, testing::AnyOf(ELEMENTARY_FILE_ICCID, ELEMENTARY_FILE_LI, ELEMENTARY_FILE_PBR));
            EXPECT_EQ(info.command, 0xc0);
            auto request = event->template GetUniqueObject<IccToRilMsg>();
            if (!request) throw std::logic_error("Missing real IccToRilMsg holder");
            auto response = std::make_shared<IccFromRilMsg>(request->controlHolder);
            auto error = std::make_shared<RadioResponseInfo>(); error->error = ErrType::ERR_GENERIC_FAILURE;
            response->fileData.exception = error;
            Reply(Pending{slot, event->GetOwner(), event->GetInnerEventId()}, response);
            return 0; // request accepted; actual device reply is failure, never a fake EF value
        });
        EXPECT_CALL(*ril, SimStkIsReady(1, _)).WillRepeatedly([](int slot, const auto &event) {
            auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::ERR_GENERIC_FAILURE;
            Reply(Pending{slot, event->GetOwner(), event->GetInnerEventId()}, response); return 0;
        });
        EXPECT_CALL(*ril, GetSimStatus(_, _)).WillRepeatedly([this](int slot, const auto &event) {
            Pending request{slot, event->GetOwner(), event->GetInnerEventId()};
            bool respond;
            {
                std::lock_guard<std::mutex> lock(mutex);
                respond = autoStatus;
                if (!respond) statusRequests.push_back(request);
                changed.notify_all();
            }
            if (respond) Reply(request, CardStatus(slot));
            return 0;
        });
    }
    bool StartLockedCard()
    {
        manager = std::make_shared<SimManager>(ril);
        CoreManagerInner::GetInstance().OnInit(nullptr, manager, ril);
        core.SetSimManager(manager);
        // Same logical slot-vector sizing as real CoreService::Init, including the extra slot.
        if (!manager->OnInit(SIM_SLOT_COUNT_MD + 1)) return false;
        accountObserver = std::make_shared<AccountObserver>([this] {
            std::lock_guard<std::mutex> lock(mutex); accountLoaded = true; changed.notify_all();
        });
        manager->RegisterCoreNotify(1, accountObserver, RadioEvent::RADIO_SIM_ACCOUNT_LOADED);
        std::vector<Pending> pending;
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!changed.wait_for(lock, 3s, [&] { return updates >= 2; })) return false;
            autoStatus = true;
            pending.swap(statusRequests);
        }
        for (const auto &request : pending) Reply(request, CardStatus(request.slot));
        { std::lock_guard<std::mutex> lock(mutex); autoStatus = true; }
        ffrt::wait(); // real RefreshSimState callers have received their correlated replies
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, 5s, [&] { return accountLoaded; });
    }
    void ClearObservations()
    { std::lock_guard<std::mutex> lock(mutex); writes.clear(); events.clear(); broadcasts.clear(); }
    // Caller holds mutex. setDone denotes completion, including failure, not success.
    void ExpectNotifications(bool success, int attempts, bool userSet)
    {
        std::vector<std::string> mainAndData;
        std::vector<bool> done;
        for (const auto &want : broadcasts) {
            const auto action = want.GetAction();
            if (action == EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_MAIN_SUBSCRIPTION_CHANGED) {
                EXPECT_EQ(want.GetIntParam("slotId", -1), 1);
                EXPECT_EQ(want.GetIntParam("simId", -1), 1); mainAndData.push_back("primary");
            } else if (action == EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_DATA_SUBSCRIPTION_CHANGED) {
                EXPECT_EQ(want.GetIntParam("simId", -1), 1); mainAndData.push_back("data");
            } else if (action == EventFwk::CommonEventSupport::COMMON_EVENT_SET_PRIMARY_SLOT_STATUS) {
                done.push_back(want.GetBoolParam("setDone", true));
                EXPECT_EQ(want.GetBoolParam("isUserSet", !userSet), userSet);
            }
        }
        if (success) EXPECT_THAT(mainAndData, testing::ElementsAre("primary", "data"));
        else EXPECT_TRUE(mainAndData.empty()) << "NO_SUCCESS_NOTIFICATION_ON_FAILURE";
        ASSERT_EQ(done.size(), static_cast<unsigned>(attempts * 2));
        for (int attempt = 0; attempt < attempts; ++attempt) {
            EXPECT_FALSE(done[attempt * 2]); EXPECT_TRUE(done[attempt * 2 + 1]);
        }
    }
    void Permit()
    {
        std::cout << "TRACE input request=" << testing::UnitTest::GetInstance()->current_test_info()->name()
            << " slot=1 userSet=true\n";
        using namespace OHOS::Security::AccessToken;
        EXPECT_CALL(identity, TokenType(101)).Times(2).WillRepeatedly(testing::Return(TOKEN_NATIVE));
        EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE)).WillOnce(testing::Return(PERMISSION_GRANTED));
    }
    ~RealScenario()
    {
        if (manager && accountObserver)
            manager->UnRegisterCoreNotify(1, accountObserver, RadioEvent::RADIO_SIM_ACCOUNT_LOADED);
        accountObserver.reset();
        core.SetSimManager(nullptr);
        CoreManagerInner::GetInstance().OnInit(nullptr, nullptr, nullptr);
        ffrt::wait();
        std::weak_ptr<SimManager> retiring = manager;
        manager.reset();
        SimHost::StopCallingThreadTasks();
        StopExternalRunner();
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (!retiring.expired() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
        EXPECT_TRUE(retiring.expired()) << "REAL_MANAGER_LIFETIME_NOT_DRAINED";
        EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s)) << "REAL_GRAPH_LIFETIME_NOT_DRAINED";
        testing::Test::RecordProperty("workers_after_cleanup", SimHost::GetSchedulerEvidence().workers);
        testing::Test::RecordProperty("ril_references_after_cleanup", static_cast<int>(ril.use_count()));
        EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u);
        const bool verified = testing::Mock::VerifyAndClearExpectations(ril.get());
        testing::Test::RecordProperty("strict_ril_expectations_verified", verified ? "PASS" : "FAIL");
        EXPECT_TRUE(verified) << "STRICT_RIL_EXPECTATIONS";
    }
};
}
