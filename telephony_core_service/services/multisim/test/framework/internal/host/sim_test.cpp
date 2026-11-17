// SPDX-License-Identifier: Apache-2.0
#include "sim_protocol_test.h"
#include "internal/common/capability.h"
#include "internal/common/state_assertions.h"
#include <algorithm>
#include "internal/common/ril_device.h"
#include "internal/common/ril_fault_controls.h"
#include "state_platform_spy.h"
#include "sim_state_manager.h"
#include "mock_tel_ril_manager.h"
#include "external_services_mock.h"
#include "parameter_mock.h"
#include "ffrt_runtime.h"
#include "radio_event.h"
#include "tel_ril_sim_parcel.h"
#include <chrono>
#include <map>

namespace SimGuard {
using namespace OHOS::Telephony;
using OHOS::AppExecFwk::InnerEvent;
using OHOS::AppExecFwk::EventHandler;
using testing::_;

namespace {
int WireState(const DeviceCard &card)
{
    switch (card.state) {
        case State::Absent: return ICC_CARD_ABSENT;
        case State::Ready: return ICC_CONTENT_READY;
        case State::Unknown: return ICC_CONTENT_UNKNOWN;
        case State::Locked:
            if (card.reason == Lock::Pin) return ICC_CONTENT_PIN;
            if (card.reason == Lock::Puk) return ICC_CONTENT_PUK;
            throw std::invalid_argument("locked card requires device lock reason");
        default: throw std::invalid_argument("unsupported device input");
    }
}
template<class T> void Deliver(const std::weak_ptr<EventHandler> &owner, uint32_t id, std::shared_ptr<T> payload)
{
    auto handler = std::dynamic_pointer_cast<TelEventHandler>(owner.lock());
    if (!handler || !handler->SendEvent(InnerEvent::Get(id, payload)))
        throw std::runtime_error("RIL response receiver unavailable");
}
}
struct SimTest::Environment {
public:
    std::vector<std::pair<Card, std::pair<State, Lock>>> prepared;
    bool started = false;
    std::string trace;
    std::map<int, size_t> notificationStarts;
    Environment()
        : device_(ownedDevice_), faults_(&ownedFaults_), ril_(std::make_shared<testing::StrictMock<MockTelRilManager>>())
    {
        EXPECT_CALL(parameters_, Read(testing::StrEq("const.telephony.vsimModemCount"), _, _, _))
            .WillRepeatedly([](const char *, const char *, char *buffer, unsigned size) {
                return SimHost::CopyParameterValue("0", buffer, size);
            });
        parameterScope_ = std::make_unique<SimHost::ScopedParameterService>(parameters_);
        EXPECT_CALL(services_, Publish(_, _)).WillRepeatedly([this](const auto &, const auto &) {
            std::lock_guard<std::mutex> guard(spy_.mutex);
            ++spy_.commonEvents;
            return true;
        });
        external_ = std::make_unique<SimHost::ScopedExternalServices>(services_);
        EXPECT_CALL(*ril_, RegisterCoreNotify(_, _, _, _)).WillRepeatedly([this](int slot, const auto &handler, int event, int *) {
            handlers_[{slot, event}] = handler;
            return TELEPHONY_ERR_SUCCESS;
        });
        EXPECT_CALL(*ril_, UnRegisterCoreNotify(_, _, _)).WillRepeatedly([this](int slot, const auto &, int event) {
            handlers_.erase({slot, event});
            return TELEPHONY_ERR_SUCCESS;
        });
        EXPECT_CALL(*ril_, GetSimStatus(_, _)).WillRepeatedly([this](int slot, const auto &event) {
            std::weak_ptr<EventHandler> owner = event->GetOwner();
            auto id = event->GetInnerEventId();
            const auto fault = faults_ ? faults_->TakeStatusFault() : StatusFault::None;
            device_.RequestStatus(slot, [owner, id, fault](const DeviceCard &card, bool error) {
                if (fault == StatusFault::Drop) return;
                if (fault == StatusFault::Malformed) {
                    Deliver(owner, id, std::make_shared<std::string>("invalid-status-payload"));
                    return;
                }
                if (error) {
                    auto response = std::make_shared<RadioResponseInfo>();
                    response->error = ErrType::ERR_GENERIC_FAILURE;
                    Deliver(owner, id, response);
                } else {
                    auto response = std::make_shared<SimCardStatusInfo>();
                    response->simState = WireState(card);
                    response->simType = card.kind == CardKind::Usim ? ICC_USIM_TYPE :
                        card.kind == CardKind::Sim ? ICC_SIM_TYPE : ICC_UNKNOWN_TYPE;
                    response->iccid = card.iccid;
                    Deliver(owner, id, response);
                }
            });
            if (faults_) faults_->StatusRequested();
            return TELEPHONY_ERR_SUCCESS;
        });
        EXPECT_CALL(*ril_, UnlockPin(_, _, _)).WillRepeatedly([this](int slot, const std::string &pin, const auto &event) {
            const auto fault = faults_ ? faults_->TakePinFault() : PinFault::None;
            const std::weak_ptr<EventHandler> owner = event->GetOwner();
            const auto id = event->GetInnerEventId();
            if (fault == PinFault::GenericTransportError) {
                auto error = std::make_shared<RadioResponseInfo>();
                error->error = ErrType::ERR_GENERIC_FAILURE;
                Deliver(owner, id, error);
                return TELEPHONY_ERR_SUCCESS;
            }
            const auto result = device_.Pin(slot, pin);
            auto response = std::make_shared<LockStatusResp>();
            response->result = result.result == Unlock::Ok ? UNLOCK_SUCCESS :
                result.result == Unlock::Incorrect ? UNLOCK_PASSWORD_ERR : UNLOCK_OTHER_ERR;
            response->remain = result.remaining;
            auto reply = [owner, id, response] { Deliver(owner, id, response); };
            if (fault == PinFault::Hold) faults_->HoldPin(reply);
            else reply();
            return TELEPHONY_ERR_SUCCESS;
        });
        EXPECT_CALL(*ril_, UnlockPuk(_, _, _, _)).WillRepeatedly([this](int slot, const auto &puk, const auto &newPin, const auto &event) {
            if (!faults_) throw std::logic_error("PUK device response is not configured");
            auto result = faults_->PukReply(puk, newPin);
            device_.AcceptPuk(slot,newPin,result);
            auto response = std::make_shared<LockStatusResp>();
            response->result = result.result == Unlock::Ok ? UNLOCK_SUCCESS :
                result.result == Unlock::Incorrect ? UNLOCK_PASSWORD_ERR : UNLOCK_OTHER_ERR;
            response->remain = result.remaining;
            Deliver(std::weak_ptr<EventHandler>(event->GetOwner()), event->GetInnerEventId(), response);
            return TELEPHONY_ERR_SUCCESS;
        });
    }
    ~Environment()
    {
        try { Stop(); } catch (...) { std::terminate(); }
        external_.reset();
        parameterScope_.reset();
    }
    void Start(int physicalSlots)
    {
        StartWithRil(physicalSlots, true);
    }
    void StartWithRil(int physicalSlots, bool available)
    {
        StartComponents(physicalSlots, available, true);
    }
    void Recreate(bool available)
    {
        if (!configuredSlots_) throw std::logic_error("No prior session to recreate");
        Stop();
        StartComponents(configuredSlots_, available, false);
    }
    void StartComponents(int physicalSlots, bool available, bool resetDevice)
    {
        if (!states_.empty() || physicalSlots < 1 || physicalSlots > 2)
            throw std::invalid_argument("legacy state host supports one or two physical slots");
        configuredSlots_ = physicalSlots;
        if (resetDevice) device_.Configure(physicalSlots);
        device_.BindNotify([this](int slot) {
            auto handler = std::dynamic_pointer_cast<TelEventHandler>(handlers_.at({slot, RadioEvent::RADIO_SIM_STATE_CHANGE}).lock());
            if (!handler || !handler->SendEvent(RadioEvent::RADIO_SIM_STATE_CHANGE))
                throw std::runtime_error("RIL event receiver unavailable");
        });
        for (int slot = 0; slot < physicalSlots; ++slot) {
            auto state = std::make_shared<SimStateManager>(available ? ril_ : nullptr);
            states_.push_back(state);
            state->Init(slot);
            ++createdInstances_;
        }
        testing::Test::RecordProperty("real_instances_created", static_cast<int>(createdInstances_));
        Settle();
    }
    void Complete()
    {
        for (unsigned round = 0; round < 32; ++round) {
            Settle();
            if (!device_.Pending()) return;
            device_.RespondAll();
        }
        throw std::runtime_error("status transport failed to converge");
    }
    void Change(int slot, DeviceCard card)
    {
        notificationStarts[slot] = Notifications().size();
        device_.Change(slot, std::move(card));
        Settle();
        if (!device_.Pending()) throw std::runtime_error("SUT did not issue a real RIL status request");
        Complete();
    }
    void NotifyAndComplete(int slot)
    {
        notificationStarts[slot] = Notifications().size();
        device_.Notify(slot); Complete();
    }
    void Stop()
    {
        Settle();
        device_.ClearTransport();
        std::vector<std::weak_ptr<SimStateManager>> retiring(states_.begin(), states_.end());
        states_.clear(); // real destructors unregister RIL handlers
        for (const auto &instance : retiring) {
            if (!instance.expired()) throw std::runtime_error("Real state instance not destroyed");
            ++retiredInstances_;
        }
        if (!retiring.empty()) testing::Test::RecordProperty("real_instances_destroyed", static_cast<int>(retiredInstances_));
        if (!handlers_.empty()) throw std::runtime_error("RIL subscriptions leaked");
        if (!SimHost::AwaitSchedulerStopped(std::chrono::seconds(2)))
            throw std::runtime_error("event worker leaked");
        if (SimHost::GetSchedulerEvidence().taskFailures)
            throw std::runtime_error("event callback threw");
    }
    void Settle()
    {
        if (!SimHost::AwaitSchedulerIdle(std::chrono::seconds(2)))
            throw std::runtime_error("event queue failed to settle within budget");
        if (SimHost::GetSchedulerEvidence().taskFailures)
            throw std::runtime_error("event callback threw");
    }
    StateSnapshot Observe()
    {
        Settle();
        StateSnapshot snapshot;
        for (size_t slot = 0; slot < states_.size(); ++slot) {
            auto &state = states_[slot];
            snapshot.cards.push_back({static_cast<int>(slot), state->HasSimCard(),
                NormalizeState(static_cast<int>(state->GetSimState())),
                NormalizeKind(static_cast<int>(state->GetCardType())), state->GetIccid()});
        }
        return snapshot;
    }
    std::vector<StateNotification> Notifications()
    {
        Settle();
        std::lock_guard<std::mutex> guard(spy_.mutex);
        return spy_.notifications;
    }
    UnlockResult UnlockPin(int slot, const std::string &pin)
    {
        LockStatusResponse response{};
        auto error = states_.at(slot)->UnlockPin(slot, pin, response);
        Settle();
        return {error, response.result == UNLOCK_OK ? Unlock::Ok :
            response.result == UNLOCK_INCORRECT ? Unlock::Incorrect : Unlock::Failure, response.remain};
    }
    int Refresh(int slot) { return states_.at(slot)->RefreshSimState(slot); }
    UnlockResult UnlockPuk(int slot, const std::string &newPin, const std::string &puk)
    {
        LockStatusResponse response{};
        auto error = states_.at(slot)->UnlockPuk(slot, newPin, puk, response);
        Settle();
        return {error, response.result == UNLOCK_OK ? Unlock::Ok :
            response.result == UNLOCK_INCORRECT ? Unlock::Incorrect : Unlock::Failure, response.remain};
    }
public:
    FakeRil ownedDevice_;
    RilFaultControls ownedFaults_;
    FakeRil &device_;
    RilFaultControls *faults_ = nullptr;
    int configuredSlots_ = 0;
    unsigned createdInstances_ = 0;
    unsigned retiredInstances_ = 0;
    PlatformSpy spy_;
    ScopedPlatformSpy spyBinding_{spy_};
    testing::StrictMock<SimHost::MockExternalServices> services_;
    testing::StrictMock<SimHost::MockParameterService> parameters_;
    std::unique_ptr<SimHost::ScopedParameterService> parameterScope_;
    std::unique_ptr<SimHost::ScopedExternalServices> external_;
    std::shared_ptr<testing::StrictMock<MockTelRilManager>> ril_;
    std::map<std::pair<int, int>, std::weak_ptr<EventHandler>> handlers_;
    std::vector<std::shared_ptr<SimStateManager>> states_;
};

SimTest::SimTest() = default;
SimTest::~SimTest() = default;
void SimTest::SetUp() { environment_ = std::make_unique<Environment>(); }
void SimTest::TearDown()
{
    if (environment_) {
        RecordProperty("pending_responses", std::to_string(environment_->ownedDevice_.Pending()));
        std::string calls;
        for (const auto &call : environment_->ownedDevice_.Calls()) calls += call + ";";
        RecordProperty("external_calls", calls);
        if (!environment_->trace.empty()) RecordProperty("observations", environment_->trace);
        EXPECT_NO_THROW(environment_->Stop());
        environment_.reset();
    }
}
Card SimTest::AddCard(std::string name, int slot, CardKind kind, State state, Lock lock)
{
    if (environment_->started) throw std::logic_error("Prepare cards before Start");
    if(slot<0)throw std::invalid_argument("Invalid physical endpoint");
    if(slot>=CardCapacity())SkipCapability("UNSUPPORTED: controlled Legacy component supports two physical card resources");
    for (const auto &entry : environment_->prepared) {
        if (entry.first.name == name || entry.first.physicalSlot == slot)
            throw std::invalid_argument("Duplicate test card name or physical resource");
    }
    Card card{std::move(name), slot, kind, "synthetic-card-" + std::to_string(slot)};
    environment_->prepared.push_back({card, {state, lock}});
    return card;
}
Card SimTest::AddCard(std::string name, CardKind kind, State state, Lock lock)
{ return AddCard(std::move(name), static_cast<int>(environment_->prepared.size()), kind, state, lock); }
int SimTest::CardCapacity() { return 2; } // Explicit capacity of this controlled Legacy slice.
Card SimTest::ExistingCard(int slot) {
    if(slot<0)throw std::invalid_argument("Negative card endpoint");
    if(slot>=CardCapacity())SkipCapability("UNSUPPORTED: query endpoint exceeds controlled Legacy resources");
    if(!environment_->started) environment_->Start(CardCapacity());
    environment_->started=true;
    environment_->Complete();
    const auto value=environment_->Observe().cards.at(slot);
    return {"existing-"+std::to_string(slot),slot,value.kind,value.iccid};
}
State SimTest::ReadCardState(const Card &card) { return environment_->Observe().cards.at(card.physicalSlot).state; }
void SimTest::Start()
{
    int slots = 1;
    for (const auto &entry : environment_->prepared) slots = std::max(slots, entry.first.physicalSlot + 1);
    environment_->Start(slots);
    environment_->started = true;
    for (const auto &entry : environment_->prepared)
        ChangeCard(entry.first, entry.second.first, entry.second.second);
}
void SimTest::ChangeCard(const Card &card, State state, Lock lock)
{
    environment_->Change(card.physicalSlot, {state,
        state == State::Absent ? CardKind::Unknown : card.kind,
        state == State::Absent ? "" : card.iccid, lock});
    environment_->trace += "card-event:" + Describe(environment_->Observe()) + "\n";
    RecordProperty("observations", environment_->trace);
}
UnlockResult SimTest::UnlockPin(const Card &card, const std::string &pin)
{
    auto result = environment_->UnlockPin(card.physicalSlot, pin);
    // Success does not imply READY: request a separate device-status observation.
    environment_->NotifyAndComplete(card.physicalSlot);
    return result;
}
UnlockResult SimTest::UnlockPuk(const Card &card,const std::string &newPin,const std::string &puk)
{ auto result=environment_->UnlockPuk(card.physicalSlot,newPin,puk);environment_->NotifyAndComplete(card.physicalSlot);return result; }
void SimTest::ExpectReadableCard(const Card &card)
{
    const auto actual=environment_->Observe().cards.at(card.physicalSlot);
    // UNKNOWN/NOT_READY do not determine ICC presence; do not manufacture a
    // status-change event just to make a read-only query look initialized.
    if(actual.state==State::Absent)EXPECT_FALSE(actual.present);
    if(actual.state==State::Ready || actual.state==State::Locked)EXPECT_TRUE(actual.present);
}
void SimTest::ExpectCardState(const Card &card, State state)
{
    const auto actual = environment_->Observe().cards.at(card.physicalSlot);
    EXPECT_EQ(actual.slot, card.physicalSlot);
    if(state!=State::Unknown)EXPECT_EQ(actual.present, state != State::Absent);
    EXPECT_EQ(actual.state, state);
    EXPECT_EQ(actual.kind, state == State::Absent ? CardKind::Unknown : card.kind);
}
void SimTest::ExpectStateNotification(const Card &card, State state, Lock lock)
{
    const auto events = environment_->Notifications();
    bool found = false;
    for (size_t i = environment_->notificationStarts.at(card.physicalSlot); i < events.size(); ++i) {
        const auto &event = events[i];
        if (event.slot == card.physicalSlot && event.state == state && event.reason == lock) found = true;
    }
    EXPECT_TRUE(found) << "Missing actual production state notification for " << card.name;
}
void StateProtocol::Start(int slots) { if(slots>test_.CardCapacity())SkipCapability("UNSUPPORTED: requested controlled endpoints exceed the Legacy material");test_.environment_->Start(slots); }
void StateProtocol::StartWithRil(int slots, bool available) { if(slots>test_.CardCapacity())SkipCapability("UNSUPPORTED: requested controlled endpoints exceed the Legacy material");test_.environment_->StartWithRil(slots, available); }
void StateProtocol::Stop() { test_.environment_->Stop(); }
void StateProtocol::Recreate(bool available) { test_.environment_->Recreate(available); }
StateSnapshot StateProtocol::Observe() { return test_.environment_->Observe(); }
void StateProtocol::Settle() { test_.environment_->Settle(); }
void StateProtocol::Complete() { test_.environment_->Complete(); }
void StateProtocol::Change(int slot, DeviceCard card) { test_.environment_->Change(slot, std::move(card)); Observe("card-event"); }
UnlockResult StateProtocol::UnlockPin(int slot, const std::string &pin) { return test_.environment_->UnlockPin(slot, pin); }
UnlockResult StateProtocol::UnlockPuk(int slot, const std::string &pin, const std::string &puk)
{ return test_.environment_->UnlockPuk(slot, pin, puk); }
int StateProtocol::Refresh(int slot) { return test_.environment_->Refresh(slot); }
void StateProtocol::Observe(const char *step)
{
    trace_ += std::string(step) + ":" + Describe(Observe()) + "\n";
    testing::Test::RecordProperty("observations", trace_);
}

void StateProtocol::DeviceChange(int slot,DeviceCard card){test_.environment_->ownedDevice_.Change(slot,std::move(card));}
void StateProtocol::ChangeWhileStopped(int slot,DeviceCard card){
    if(!test_.environment_->states_.empty())throw std::logic_error("ChangeWhileStopped requires a stopped SUT");
    test_.environment_->ownedDevice_.BindNotify([](int){});DeviceChange(slot,std::move(card));
}
void StateProtocol::Notify(int slot){test_.environment_->ownedDevice_.Notify(slot);}
size_t StateProtocol::Pending(){return test_.environment_->ownedDevice_.Pending();}
size_t StateProtocol::RespondAll(bool error){return test_.environment_->ownedDevice_.RespondAll(error);}
size_t StateProtocol::RespondMatching(const std::function<bool(int,const DeviceCard &)> &match,bool error){return test_.environment_->ownedDevice_.RespondMatching(match,error);}
void StateProtocol::ReplayLast(){test_.environment_->ownedDevice_.ReplayLast();}
void StateProtocol::FailPin(bool fail){test_.environment_->ownedDevice_.FailPin(fail);}
void StateProtocol::NextStatus(StatusFault fault){test_.environment_->ownedFaults_.NextStatus(fault);}
void StateProtocol::NextPin(PinFault fault){test_.environment_->ownedFaults_.NextPin(fault);}
unsigned StateProtocol::StatusRevision(){return test_.environment_->ownedFaults_.StatusRevision();}
bool StateProtocol::AwaitStatusAfter(unsigned revision){return test_.environment_->ownedFaults_.AwaitStatusAfter(revision);}
bool StateProtocol::AwaitHeldPin(){return test_.environment_->ownedFaults_.AwaitHeldPin();}
void StateProtocol::ReleasePin(){test_.environment_->ownedFaults_.ReleasePin();}
void StateProtocol::SetPukRemaining(int remaining){test_.environment_->ownedFaults_.SetPukRemaining(remaining);}

void SimTest::StartPrimary(const Card &){SkipCapability("UNSUPPORTED: Primary requires the real Core/Manager/Controller material");}
int SimTest::SelectPrimary(const Card &){SkipCapability("UNSUPPORTED: Primary requires the real Core/Manager/Controller material");}
void SimTest::ExpectPrimary(const Card &,const std::string &){SkipCapability("UNSUPPORTED: no Primary observation in this material");}
void SimTest::ExpectPrimaryChanged(const Card &){SkipCapability("UNSUPPORTED: no Primary notification boundary in this material");}
void SimTest::ExpectPrimaryResult(bool){SkipCapability("UNSUPPORTED: no Primary request in this material");}
void SimTest::ExpectPrimaryIdle(){SkipCapability("UNSUPPORTED: no Primary request in this material");}

int SimTest::ReadCapacity(){SkipCapability("UNSUPPORTED: initialization-free capacity measurement uses the count material");}
bool SimTest::ParameterBoundaryObserved(){SkipCapability("UNSUPPORTED: count-material parameter observation is not selected");}

}
