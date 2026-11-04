// SPDX-License-Identifier: Apache-2.0
#include "contracts/sim_sut.h"
#include "fakes/ril_device.h"
#include "platform_spy.h"
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
class LegacySimAdapter final : public ISimSutAdapter {
public:
    explicit LegacySimAdapter(FakeRil &device) : device_(device), ril_(std::make_shared<testing::StrictMock<MockTelRilManager>>())
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
            device_.RequestStatus(slot, [owner, id](const DeviceCard &card, bool error) {
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
            return TELEPHONY_ERR_SUCCESS;
        });
        EXPECT_CALL(*ril_, UnlockPin(_, _, _)).WillRepeatedly([this](int slot, const std::string &pin, const auto &event) {
            const auto result = device_.Pin(slot, pin);
            auto response = std::make_shared<LockStatusResp>();
            response->result = result.result == Unlock::Ok ? UNLOCK_SUCCESS :
                result.result == Unlock::Incorrect ? UNLOCK_PASSWORD_ERR : -1;
            response->remain = result.remaining;
            Deliver(std::weak_ptr<EventHandler>(event->GetOwner()), event->GetInnerEventId(), response);
            return TELEPHONY_ERR_SUCCESS;
        });
    }
    ~LegacySimAdapter() override
    {
        try { Stop(); } catch (...) { std::terminate(); }
        external_.reset();
        parameterScope_.reset();
    }
    void Start(int physicalSlots) override
    {
        if (!states_.empty() || physicalSlots < 1 || physicalSlots > 2)
            throw std::invalid_argument("legacy state host supports one or two physical slots");
        device_.Configure(physicalSlots);
        device_.BindNotify([this](int slot) {
            auto handler = std::dynamic_pointer_cast<TelEventHandler>(handlers_.at({slot, RadioEvent::RADIO_SIM_STATE_CHANGE}).lock());
            if (!handler || !handler->SendEvent(RadioEvent::RADIO_SIM_STATE_CHANGE))
                throw std::runtime_error("RIL event receiver unavailable");
        });
        for (int slot = 0; slot < physicalSlots; ++slot) {
            auto state = std::make_shared<SimStateManager>(ril_);
            states_.push_back(state);
            state->Init(slot);
        }
        Settle();
    }
    void Stop() override
    {
        Settle();
        device_.ClearTransport();
        states_.clear(); // real destructors unregister RIL handlers
        if (!handlers_.empty()) throw std::runtime_error("RIL subscriptions leaked");
        if (!SimHost::AwaitSchedulerStopped(std::chrono::seconds(2)))
            throw std::runtime_error("event worker leaked");
        if (SimHost::GetSchedulerEvidence().taskFailures)
            throw std::runtime_error("event callback threw");
    }
    void Settle() override
    {
        if (!SimHost::AwaitSchedulerIdle(std::chrono::seconds(2)))
            throw std::runtime_error("event queue failed to settle within budget");
        if (SimHost::GetSchedulerEvidence().taskFailures)
            throw std::runtime_error("event callback threw");
    }
    StateSnapshot Observe() override
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
    std::vector<StateNotification> Notifications() override
    {
        Settle();
        std::lock_guard<std::mutex> guard(spy_.mutex);
        return spy_.notifications;
    }
    UnlockResult UnlockPin(int slot, const std::string &pin) override
    {
        LockStatusResponse response{};
        auto error = states_.at(slot)->UnlockPin(slot, pin, response);
        Settle();
        return {error, response.result == UNLOCK_OK ? Unlock::Ok :
            response.result == UNLOCK_INCORRECT ? Unlock::Incorrect : Unlock::Failure, response.remain};
    }
private:
    FakeRil &device_;
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
}
std::unique_ptr<ISimSutAdapter> CreateSimSut(FakeRil &ril) { return std::make_unique<LegacySimAdapter>(ril); }
}
