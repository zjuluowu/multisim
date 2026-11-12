#pragma once
#include "sim_manager.h"
#include "lifecycle/lifecycle_diagnostics.h"
#include "lifecycle/external_runner.h"
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
#include "initialization_trace.h"
#include "adapters/legacy/legacy_primary_adapter.h"
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>
#include <regex>
#include <optional>
#include <set>
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
    auto Query(const DataShare::DataSharePredicates &predicate, const ProviderRow *companion = nullptr)
    {
        std::vector<std::string> columns;
        DataShare::DataShareResultSet::Row row;
        for (const auto &[key, value] : cells) { columns.push_back(key); row.push_back(value); }
        std::vector<DataShare::DataShareResultSet::Row> rows;
        if (Matches(predicate)) rows.push_back(row);
        if (companion && companion->Matches(predicate)) {
            DataShare::DataShareResultSet::Row other;
            for (const auto &column : columns) other.push_back(companion->cells.at(column));
            rows.push_back(std::move(other));
        }
        return std::make_shared<DataShare::DataShareResultSet>(columns, rows);
    }
};
class AccountObserver final : public TelEventHandler {
public:
    explicit AccountObserver(std::function<void()> loaded) : AccountObserver(
        std::function<void(int)>([loaded = std::move(loaded)](int) { loaded(); })) {}
    explicit AccountObserver(std::function<void(int)> loaded) : TelEventHandler("sim028-account-observer"), loaded_(std::move(loaded)) {}
    void ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event) override
    {
        if (event && event->GetInnerEventId() == RadioEvent::RADIO_SIM_ACCOUNT_LOADED) {
            InitTrace("account_loaded_event", event->GetParam(), this, event->GetInnerEventId());
            loaded_(event->GetParam());
        }
    }
private:
    std::function<void(int)> loaded_;
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
    SimGuard::Primary::Observation observation;
    SimGuard::Primary::LegacyAdapter core{observation};
    std::shared_ptr<AccountObserver> accountObserver;
    bool accountLoaded = false;
    std::mutex mutex;
    std::condition_variable changed;
    ProviderRow row;
    std::optional<ProviderRow> companionRow;
    std::set<int> loadedSlots;
    const bool dual = std::getenv("SIM028_INIT_MODEL") &&
        std::string(std::getenv("SIM028_INIT_MODEL")) == "dual-active-primary0";
    struct BootstrapCompletion {
        std::mutex mutex;
        std::condition_variable changed;
        std::set<std::thread::id> pending;
        unsigned started = 0, completed = 0;
    };
    std::shared_ptr<BootstrapCompletion> bootstrap = std::make_shared<BootstrapCompletion>();
    void TrackBootstrapRequest()
    {
        std::lock_guard<std::mutex> lock(bootstrap->mutex);
        if (bootstrap->pending.insert(std::this_thread::get_id()).second) ++bootstrap->started;
    }
    bool AwaitBootstrap()
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto remaining = [&] { return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::max(deadline - std::chrono::steady_clock::now(), std::chrono::steady_clock::duration::zero())); };
        for (;;) {
            const auto revision = SimHost::GetDetachedThreadEvidence().revision;
            if (!SimHost::AwaitSchedulerReadyIdle(remaining()) ||
                !SimHost::AwaitDetachedThreadsIdle(remaining()) ||
                !SimHost::AwaitSchedulerReadyIdle(remaining())) return false;
            std::unique_lock<std::mutex> lock(bootstrap->mutex);
            const bool done = bootstrap->changed.wait_for(lock, remaining(), [&] { return bootstrap->pending.empty(); });
            testing::Test::RecordProperty("initialization_bootstrap_requests", bootstrap->started);
            testing::Test::RecordProperty("initialization_bootstrap_completions", bootstrap->completed);
            lock.unlock();
            const auto detached = SimHost::GetDetachedThreadEvidence();
            if (done && detached.active == 0 && detached.revision == revision) {
                testing::Test::RecordProperty("initialization_detached_callables", 0);
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) return false;
        }
    }
    std::map<std::string, std::string> parametersDb{{"const.telephony.slotCount", "2"}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::vector<std::string> events;
    std::vector<AAFwk::Want> broadcasts;
    struct Pending {
        int slot; std::weak_ptr<AppExecFwk::EventHandler> owner; uint32_t id;
        uint64_t request = InitRequestId(); // Host correlation only, never an RIL serial
    };
    std::vector<Pending> statusRequests;
    unsigned updates = 0;
    bool autoStatus = false;
    std::map<std::pair<int, int>, std::weak_ptr<AppExecFwk::EventHandler>> notifications;
    struct Registration {
        int slot, event;
        std::weak_ptr<AppExecFwk::EventHandler> owner;
        bool active;
    };
    std::vector<Registration> registrationLedger; // weak only; full multi-owner pairing ledger
    template<class T> static void Reply(const Pending &request, std::shared_ptr<T> response)
    {
        auto owner = std::dynamic_pointer_cast<TelEventHandler>(request.owner.lock());
        InitTrace("ril_response_enqueue", request.slot, owner.get(), request.id, request.request);
        if (!owner || !owner->SendEvent(AppExecFwk::InnerEvent::Get(request.id, response)))
            throw std::runtime_error("Real RIL response owner unavailable");
    }
    static auto CardStatus(int slot)
    {
        auto response = std::make_shared<SimCardStatusInfo>();
        response->index = slot; response->simType = ICC_USIM_TYPE;
        const bool dualCard = std::getenv("SIM028_INIT_MODEL") &&
            std::string(std::getenv("SIM028_INIT_MODEL")) == "dual-active-primary0";
        response->simState = slot == 1 || (dualCard && slot == 0) ? ICC_CONTENT_PIN : ICC_CARD_ABSENT;
        response->iccid = slot == 1 ? "8900000000000000001" : dualCard && slot == 0 ? "8900000000000000000" : "";
        return response;
    }
    RealScenario(bool directRil = false)
    {
        EXPECT_TRUE(StartExternalRunner());
        const testing::Matcher<int> cardSlot = dual ? testing::Matcher<int>(testing::AnyOf(0, 1)) :
            testing::Matcher<int>(testing::Eq(1));
        if (dual) {
            companionRow.emplace();
            companionRow->cells[SimData::SIM_ID] = int64_t(2);
            companionRow->cells[SimData::ICC_ID] = std::string("8900000000000000000");
            std::weak_ptr<BootstrapCompletion> weak = bootstrap;
            SetInitializationAccountProbe([weak] {
                if (auto state = weak.lock()) {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (state->pending.erase(std::this_thread::get_id())) {
                        ++state->completed;
                        state->changed.notify_all();
                    }
                }
            });
        }
        parametersDb["const.vendor.ril.set_primary_slot_support"] = directRil ? "true" : "false";
        EXPECT_CALL(parameters, Read(_, _, _, _)).WillRepeatedly([this](auto key, auto fallback, auto out, auto size) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = parametersDb.find(key);
            return SimHost::CopyParameterValue(it == parametersDb.end() ? fallback : it->second.c_str(), out, size);
        });
        EXPECT_CALL(identity, WriteParameter(_, _)).WillRepeatedly([this](auto key, auto value) {
            std::lock_guard<std::mutex> lock(mutex);
            writes.emplace_back(key, value); parametersDb[key] = value;
            observation.Add("parameter_write", {{"key", key}, {"value", value}});
            std::cout << "TRACE parameter " << key << "=" << value << "\n"; changed.notify_all(); return 0;
        });
        EXPECT_CALL(services, Publish(_, _)).WillRepeatedly([this](const auto &event, const auto &) {
            std::lock_guard<std::mutex> lock(mutex);
            events.push_back(event.GetWant().GetAction()); broadcasts.push_back(event.GetWant());
            const auto &want = event.GetWant();
            observation.Add("publication", {{"action", want.GetAction()},
                {"slot", std::to_string(want.GetIntParam("slotId", -1))},
                {"sim", std::to_string(want.GetIntParam("simId", -1))},
                {"done", want.GetBoolParam("setDone", false) ? "true" : "false"},
                {"user", want.GetBoolParam("isUserSet", false) ? "true" : "false"}});
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
            std::lock_guard<std::mutex> lock(mutex);
            InitTrace("provider_query", -1, provider.get(), 0, 0,
                "row_slot=" + ProviderRow::Text(row.cells[SimData::SLOT_INDEX]) +
                " row_active=" + ProviderRow::Text(row.cells[SimData::IS_ACTIVE]));
            return row.Query(predicate, companionRow ? &*companionRow : nullptr);
        });
        EXPECT_CALL(*provider, Update(_, _, _)).WillRepeatedly([this](auto &uri, const auto &predicate, const auto &bucket) {
            EXPECT_EQ(uri.ToString(), "datashare:///com.ohos.simability/sim/sim_info");
            std::lock_guard<std::mutex> lock(mutex);
            if (row.Matches(predicate)) for (const auto &[key, value] : bucket.valuesMap) row.cells[key] = value;
            if (companionRow && companionRow->Matches(predicate))
                for (const auto &[key, value] : bucket.valuesMap) companionRow->cells[key] = value;
            InitTrace("provider_update", -1, provider.get(), 0, 0,
                "row_slot=" + ProviderRow::Text(row.cells[SimData::SLOT_INDEX]) +
                " row_active=" + ProviderRow::Text(row.cells[SimData::IS_ACTIVE]));
            ++updates; changed.notify_all(); return 0; // verified Provider RdbStore::Update status, not row count
        });
        EXPECT_CALL(*ril, RegisterCoreNotify(_, _, _, _)).WillRepeatedly([this](int slot, const auto &owner, int id, int *) {
            std::lock_guard<std::mutex> lock(mutex);
            bool existing = false;
            std::weak_ptr<AppExecFwk::EventHandler> weak = owner;
            for (const auto &r : registrationLedger)
                existing |= r.active && r.slot == slot && r.event == id &&
                    !r.owner.owner_before(weak) && !weak.owner_before(r.owner);
            if (!existing) registrationLedger.push_back({slot, id, weak, true});
            notifications[{slot, id}] = owner; return 0;
        });
        EXPECT_CALL(*ril, UnRegisterCoreNotify(_, _, _)).WillRepeatedly([this](int slot, const auto &owner, int id) {
            std::lock_guard<std::mutex> lock(mutex);
            std::weak_ptr<AppExecFwk::EventHandler> weak = owner;
            for (auto &r : registrationLedger)
                if (r.active && r.slot == slot && r.event == id &&
                    !r.owner.owner_before(weak) && !weak.owner_before(r.owner)) r.active = false;
            auto it = notifications.find({slot, id});
            if (it != notifications.end() && it->second.lock() == owner) {
                notifications.erase(it);
                for (const auto &r : registrationLedger)
                    if (r.active && r.slot == slot && r.event == id) notifications[{slot, id}] = r.owner;
            }
            return 0;
        });
        EXPECT_CALL(*ril, SetActiveSim(cardSlot, _, 1, _)).WillRepeatedly([this](int slot, int, int, const auto &event) {
            EXPECT_TRUE(slot == 1 || (dual && slot == 0));
            auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::NONE;
            auto owner = std::dynamic_pointer_cast<TelEventHandler>(event->GetOwner());
            EXPECT_TRUE(owner && owner->SendEvent(AppExecFwk::InnerEvent::Get(event->GetInnerEventId(), response)));
            return 0;
        });
        // The locked-card EF read is explicitly unavailable. Match the actual
        // TelRilSim::ErrorIccIoResponse transport shape, retaining the original holder.
        EXPECT_CALL(*ril, GetSimIO(cardSlot, _, _)).WillRepeatedly([this](int slot, const auto &info, const auto &event) {
            EXPECT_TRUE(slot == 1 || (dual && slot == 0));
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
        EXPECT_CALL(*ril, SimStkIsReady(cardSlot, _)).WillRepeatedly([this](int slot, const auto &event) {
            EXPECT_TRUE(slot == 1 || (dual && slot == 0));
            auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::ERR_GENERIC_FAILURE;
            Reply(Pending{slot, event->GetOwner(), event->GetInnerEventId()}, response); return 0;
        });
        EXPECT_CALL(*ril, GetSimStatus(_, _)).WillRepeatedly([this](int slot, const auto &event) {
            Pending request{slot, event->GetOwner(), event->GetInnerEventId()};
            InitTrace("ril_status_request", slot, event->GetOwner().get(), request.id, request.request);
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
        if (dual) {
            // Successful device I/O during bootstrap only. These expectations
            // are shadowed by Times(0) at the preparation boundary, before the
            // unchanged business-specific strict expectations are installed.
            EXPECT_CALL(*ril, SetPrimarySlot(testing::AnyOf(0, 1), _)).WillRepeatedly([this](int slot, const auto &event) {
                TrackBootstrapRequest();
                InitTrace("bootstrap_primary_request", slot, event->GetOwner().get(), event->GetInnerEventId());
                EXPECT_THAT(slot, testing::AnyOf(0, 1));
                EXPECT_TRUE(std::dynamic_pointer_cast<MultiSimController>(event->GetOwner()));
                auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::NONE;
                Reply({slot, event->GetOwner(), event->GetInnerEventId()}, response);
                return 0;
            });
            EXPECT_CALL(*ril, GetRadioProtocol(testing::AnyOf(0, 1), _)).WillRepeatedly([](int slot, const auto &event) {
                EXPECT_THAT(slot, testing::AnyOf(0, 1));
                auto response = std::make_shared<RadioProtocol>();
                response->slotId = slot; response->modemId = slot;
                response->technology = slot == 0 ? 0x11 : 0x22;
                Reply({slot, event->GetOwner(), event->GetInnerEventId()}, response);
                return 0;
            });
            EXPECT_CALL(*ril, SetRadioProtocol(testing::AnyOf(0, 1), _, _)).WillRepeatedly([this](int slot, RadioProtocol protocol, const auto &event) {
                if (protocol.phase == RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK) TrackBootstrapRequest();
                InitTrace("bootstrap_protocol_request", slot, event->GetOwner().get(), event->GetInnerEventId(), 0,
                    "session=" + std::to_string(protocol.sessionId) + " phase=" + std::to_string(static_cast<int>(protocol.phase)));
                EXPECT_EQ(protocol.slotId, slot);
                EXPECT_THAT(protocol.phase, testing::AnyOf(RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK,
                    RadioProtocolPhase::RADIO_PROTOCOL_PHASE_UPDATE, RadioProtocolPhase::RADIO_PROTOCOL_PHASE_COMPLETE));
                if (protocol.phase == RadioProtocolPhase::RADIO_PROTOCOL_PHASE_UPDATE) {
                    protocol.phase = RadioProtocolPhase::RADIO_PROTOCOL_PHASE_NOTIFY;
                    protocol.status = RadioProtocolStatus::RADIO_PROTOCOL_STATUS_SUCCESS;
                    std::weak_ptr<AppExecFwk::EventHandler> owner;
                    { std::lock_guard<std::mutex> lock(mutex); owner = notifications.at({slot, RADIO_SIM_RADIO_PROTOCOL_NOTIFY}); }
                    Reply({slot, owner, RADIO_SIM_RADIO_PROTOCOL_NOTIFY}, std::make_shared<RadioProtocol>(protocol));
                } else Reply({slot, event->GetOwner(), event->GetInnerEventId()}, std::make_shared<RadioProtocol>(protocol));
                return 0;
            });
        }
    }
    bool StartLockedCard()
    {
        manager = std::make_shared<SimManager>(ril);
        InitTrace("manager_constructed", -1, manager.get());
        CoreManagerInner::GetInstance().OnInit(nullptr, manager, ril);
        core.SetSimManager(manager);
        // Same logical slot-vector sizing as real CoreService::Init, including the extra slot.
        if (!manager->OnInit(SIM_SLOT_COUNT_MD + 1)) return false;
        InitTrace("manager_oninit_returned", -1, manager.get());
        accountObserver = std::make_shared<AccountObserver>(std::function<void(int)>([this](int slot) {
            std::lock_guard<std::mutex> lock(mutex);
            loadedSlots.insert(slot); if (slot == 1) accountLoaded = true;
            changed.notify_all();
        }));
        manager->RegisterCoreNotify(1, accountObserver, RadioEvent::RADIO_SIM_ACCOUNT_LOADED);
        std::vector<Pending> pending;
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!changed.wait_for(lock, 3s, [&] { return updates >= 2; })) return false;
            autoStatus = true;
            pending.swap(statusRequests);
        }
        const std::string order = std::getenv("SIM028_INIT_ORDER") ? std::getenv("SIM028_INIT_ORDER") : "native-arrival";
        testing::Test::RecordProperty("initialization_order", order);
        if (order == "present-first") {
            for (const auto &request : pending) if (request.slot == 1) Reply(request, CardStatus(request.slot));
            std::unique_lock<std::mutex> lock(mutex);
            if (!changed.wait_for(lock, 5s, [&] { return accountLoaded; })) return false;
            lock.unlock();
            InitTrace("account_event_before_absent_replies", 1, manager.get());
            for (const auto &request : pending) if (request.slot != 1) Reply(request, CardStatus(request.slot));
        } else {
            if (order == "reverse") std::reverse(pending.begin(), pending.end());
            for (const auto &request : pending) Reply(request, CardStatus(request.slot));
        }
        { std::lock_guard<std::mutex> lock(mutex); autoStatus = true; }
        ffrt::wait(); // real RefreshSimState callers have received their correlated replies
        std::unique_lock<std::mutex> lock(mutex);
        const bool loaded = changed.wait_for(lock, 5s, [&] { return accountLoaded && (!dual || loadedSlots.count(0)); });
        lock.unlock();
        InitTrace("account_wait_returned", 1, manager.get());
        // The account event precedes later cross-slot refresh tasks. Admit the
        // business request only after all currently runnable Host work finishes.
        const bool legacyWait = std::getenv("SIM028_INIT_OLD_WAIT") != nullptr;
        const bool drained = legacyWait || SimHost::AwaitSchedulerReadyIdle(5s);
        bool present = false;
        const auto presenceResult = manager->HasSimCard(1, present);
        SimState state = SimState::SIM_STATE_UNKNOWN;
        const auto stateResult = manager->GetSimState(1, state);
        const bool active = manager->IsSimActive(1);
        InitTrace("precondition_snapshot", 1, manager.get(), 0, 0,
            "loaded=" + std::to_string(loaded) + " drained=" + (legacyWait ? "NOT_RUN" : std::to_string(drained)) +
            " active=" + std::to_string(active) + " present=" + std::to_string(present) +
            " presence_result=" + std::to_string(presenceResult) +
            " sim_state=" + std::to_string(static_cast<int>(state)) +
            " state_result=" + std::to_string(stateResult));
        testing::Test::RecordProperty("initialization_account_event", loaded ? "PASS" : "FAIL");
        testing::Test::RecordProperty("initialization_ready_idle", legacyWait ? "NOT_RUN" : drained ? "PASS" : "FAIL");
        testing::Test::RecordProperty("initialization_active", active ? "PASS" : "FAIL");
        testing::Test::RecordProperty("initialization_present",
            present && presenceResult == TELEPHONY_ERR_SUCCESS ? "PASS" : "FAIL");
        testing::Test::RecordProperty("initialization_model", dual ? "dual-active-primary0" : "single-active");
        bool baseline = true;
        if (dual && loaded && drained && active) {
            baseline = AwaitBootstrap();
            manager->GetRadioProtocol(0); manager->GetRadioProtocol(1);
            baseline = baseline && AwaitBootstrap() && manager->IsSimActive(0) &&
                !manager->IsSetPrimarySlotIdInProgress();
            if (baseline) baseline = manager->SetPrimarySlotId(0, false) == TELEPHONY_ERR_SUCCESS;
            baseline = baseline && AwaitBootstrap();
            int primary = -1;
            const auto result = manager->GetPrimarySlotId(primary);
            const auto data = manager->GetDefaultCellularDataSlotId();
            baseline = baseline && result == TELEPHONY_ERR_SUCCESS && primary == 0 && data == 0 &&
                manager->IsSimActive(0) && manager->IsSimActive(1) && !manager->IsSetPrimarySlotIdInProgress();
            testing::Test::RecordProperty("initialization_primary", primary);
            testing::Test::RecordProperty("initialization_data", data);
            testing::Test::RecordProperty("initialization_public_baseline", baseline ? "PASS" : "FAIL");
            InitTrace("public_primary0_prepared", 0, manager.get(), 0, 0,
                "primary=" + std::to_string(primary) + " data=" + std::to_string(data));
            EXPECT_CALL(*ril, SetPrimarySlot(_, _)).Times(0);
            EXPECT_CALL(*ril, SetRadioProtocol(_, _, _)).Times(0);
            EXPECT_CALL(*ril, GetRadioProtocol(_, _)).Times(0);
        }
        DumpQueues("initialization_admission");
        return legacyWait ? loaded : loaded && drained && active && present && presenceResult == TELEPHONY_ERR_SUCCESS && baseline && !testing::Test::HasFailure();
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
        observation.Begin();
        std::cout << "TRACE input request=" << testing::UnitTest::GetInstance()->current_test_info()->name()
            << " slot=1 userSet=true\n";
        using namespace OHOS::Security::AccessToken;
        EXPECT_CALL(identity, TokenType(101)).Times(2).WillRepeatedly(testing::Return(TOKEN_NATIVE));
        EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE)).WillOnce(testing::Return(PERMISSION_GRANTED));
    }
    void ExportPrimaryObservation()
    {
        SimGuard::Primary::Fields parameters;
        { std::lock_guard<std::mutex> lock(mutex); parameters = parametersDb; }
        SimGuard::Primary::Fields state;
        // Observe only after the original business assertions completed successfully.
        // Queries run without the fixture lock, because real parameter I/O reenters it.
        if (!testing::Test::HasFailure() && manager) {
            int primary = -1, data = -1;
            const int primaryResult = manager->GetPrimarySlotId(primary);
            data = manager->GetDefaultCellularDataSlotId();
            state = {{"primary", std::to_string(primary)}, {"data", std::to_string(data)},
                {"primary_result", std::to_string(primaryResult)},
                {"switching", manager->IsSetPrimarySlotIdInProgress() ? "true" : "false"}};
        }
        testing::Test::RecordProperty("primary_observation", observation.Snapshot(parameters, state));
    }
    ~RealScenario()
    {
        ExportPrimaryObservation();
        if (dual) SetInitializationAccountProbe({});
        LifecycleCheckpoint("before_cleanup", manager.get(), ril.get());
        DumpQueues("before_cleanup");
        if (manager && accountObserver)
            manager->UnRegisterCoreNotify(1, accountObserver, RadioEvent::RADIO_SIM_ACCOUNT_LOADED);
        std::weak_ptr<AccountObserver> retiringObserver = accountObserver;
        accountObserver.reset();
        core.SetSimManager(nullptr);
        CoreManagerInner::GetInstance().OnInit(nullptr, nullptr, nullptr);
        ffrt::wait();
        std::weak_ptr<SimManager> retiring = manager;
        manager.reset();
        SimHost::StopCallingThreadTasks();
        EXPECT_TRUE(StopExternalRunner());
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (!retiring.expired() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
        EXPECT_TRUE(retiring.expired()) << "REAL_MANAGER_LIFETIME_NOT_DRAINED";
        EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s)) << "REAL_GRAPH_LIFETIME_NOT_DRAINED";
        testing::Test::RecordProperty("workers_after_cleanup", SimHost::GetSchedulerEvidence().workers);
        testing::Test::RecordProperty("ril_references_after_cleanup", static_cast<int>(ril.use_count()));
        testing::Test::RecordProperty("account_observer_expired", retiringObserver.expired() ? "true" : "false");
        testing::Test::RecordProperty("object_lifetime", SimHost::GetSchedulerEvidence().workers == 0 &&
            ril.use_count() == 1 && retiring.expired() && retiringObserver.expired() ? "PASS" : "FAIL");
        DumpQueues("after_cleanup");
        LifecycleCheckpoint("after_cleanup", nullptr, ril.get());
        EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u);
        const bool verified = testing::Mock::VerifyAndClearExpectations(ril.get());
        testing::Test::RecordProperty("strict_ril_expectations_verified", verified ? "PASS" : "FAIL");
        EXPECT_TRUE(verified) << "STRICT_RIL_EXPECTATIONS";
    }
};
}
