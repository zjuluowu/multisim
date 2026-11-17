// SPDX-License-Identifier: Apache-2.0
#include "internal/common/capability.h"
#include "primary_environment.h"
#include "sim_protocol_test.h"
#include <future>
#include <optional>
namespace SimGuard
{
using namespace OHOS::Telephony;
using namespace std::chrono_literals;
using Rig = Sim028Host::PrimaryEnvironment;
struct SimTest::Environment
{
    std::unique_ptr<Rig> rig;
    std::optional<PrimaryPath> path;
    unsigned expected = 6;
    struct Captured
    {
        Rig::Pending transport;
        RadioProtocol protocol{};
    };
    std::vector<Captured> requests;
    std::future<int> caller;
    int result = 0;
    std::optional<Card> declared;
    std::function<PrimaryReply(const PrimaryRequest &)> policy;
    size_t handled = 0;
    void Respond(const PrimaryRequest &request, PrimaryReply kind, int delta = 0)
    {
        if (kind == PrimaryReply::Hold)
            return;
        Captured r;
        {
            std::lock_guard<std::mutex> lock(rig->mutex);
            r = requests.at(request.token);
        }
        if (kind == PrimaryReply::GenericError || path == PrimaryPath::Direct)
        {
            auto reply = std::make_shared<RadioResponseInfo>();
            reply->error = kind == PrimaryReply::GenericError ? ErrType::ERR_GENERIC_FAILURE : ErrType::NONE;
            SimGuard::Primary::Fields fields{{"slot", std::to_string(r.transport.slot)},
                                             {"error", std::to_string(static_cast<int>(reply->error))}};
            if (path == PrimaryPath::ThreePhase)
            {
                fields["session"] = std::to_string(r.protocol.sessionId);
                fields["phase"] = std::to_string(static_cast<int>(r.protocol.phase));
            }
            rig->observation.Add("ril_response", fields);
            Rig::Reply(r.transport, reply);
            return;
        }
        auto reply = std::make_shared<RadioProtocol>(r.protocol);
        reply->sessionId += delta;
        auto pending = r.transport;
        if (kind == PrimaryReply::Notify)
        {
            reply->phase = RadioProtocolPhase::RADIO_PROTOCOL_PHASE_NOTIFY;
            reply->status = RadioProtocolStatus::RADIO_PROTOCOL_STATUS_SUCCESS;
            std::lock_guard<std::mutex> lock(rig->mutex);
            pending = {r.transport.slot, rig->notifications.at({r.transport.slot, RADIO_SIM_RADIO_PROTOCOL_NOTIFY}),
                       RADIO_SIM_RADIO_PROTOCOL_NOTIFY};
        }
        rig->observation.Add("ril_response", {{"slot", std::to_string(r.transport.slot)},
                                              {"session", std::to_string(reply->sessionId)},
                                              {"phase", std::to_string(static_cast<int>(reply->phase))}});
        Rig::Reply(pending, reply);
    }
    void Capture(Captured request)
    {
        PrimaryRequest view;
        {
            std::lock_guard<std::mutex> lock(rig->mutex);
            const auto i = requests.size();
            requests.push_back(request);
            view = {i,
                    request.transport.slot,
                    request.protocol.sessionId,
                    static_cast<int>(request.protocol.phase),
                    request.protocol.modemId,
                    request.protocol.technology};
            rig->changed.notify_all();
        }
        // Inline response policy preserves the actual legacy callback timing;
        // no generic event dispatcher, business state or synthetic result.
        if (policy)
        {
            Respond(view, policy(view));
            std::lock_guard<std::mutex> lock(rig->mutex);
            ++handled;
            rig->changed.notify_all();
        }
    }
    void Mapping(bool flipped)
    {
        auto &s = *rig;
        EXPECT_CALL(*s.ril, GetRadioProtocol(testing::_, testing::_))
            .Times(2)
            .WillRepeatedly(
                [flipped](int slot, const auto &event)
                {
                    auto reply = std::make_shared<RadioProtocol>();
                    reply->slotId = slot;
                    reply->modemId = flipped ? 1 - slot : slot;
                    reply->technology = flipped ? (slot == 1 ? 0x11 : 0x22) : (slot == 0 ? 0x11 : 0x22);
                    Rig::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, reply);
                    return 0;
                });
        s.manager->GetRadioProtocol(0);
        s.manager->GetRadioProtocol(1);
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (
            (s.manager->GetRadioProtocolSlotIdByModemId(flipped ? 0 : 1) != 1 ||
             (flipped && (s.manager->GetRadioProtocolTech(1) != 0x11 || s.manager->GetRadioProtocolTech(0) != 0x22))) &&
            std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        ASSERT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(flipped ? 0 : 1), 1);
        if (flipped)
        {
            EXPECT_EQ(s.manager->GetRadioProtocolTech(1), 0x11);
            EXPECT_EQ(s.manager->GetRadioProtocolTech(0), 0x22);
        }
    }
};
SimTest::SimTest() = default;
SimTest::~SimTest() = default;
void SimTest::SetUp() { environment_ = std::make_unique<Environment>(); }
void SimTest::TearDown()
{
    RecordProperty("business_assertions_before_cleanup", HasFailure() ? "FAIL" : "PASS");
    environment_.reset();
}
Card SimTest::AddCard(std::string name, CardKind kind, State state, Lock lock)
{
    if (kind != CardKind::Usim || state != State::Locked || lock != Lock::Pin)
        SkipCapability("UNSUPPORTED: controlled Primary material is a PIN-locked USIM");
    if (environment_->declared)
        SkipCapability("UNSUPPORTED: Primary fixture has one controlled target card; additional resources are not "
                       "declared by this material");
    Card card{std::move(name), 1, kind, "8900000000000000001"};
    environment_->declared = card;
    return card;
}
void SimTest::StartPrimary(const Card &card)
{
    auto &e = *environment_;
    // Legacy closure has two modem routes and one controlled PIN-locked USIM.
    if (card.physicalSlot != 1 || card.kind != CardKind::Usim || card.iccid != "8900000000000000001")
        SkipCapability("UNSUPPORTED: requested Primary resource is unavailable in this controlled material");
    e.rig = std::make_unique<Rig>(e.path == PrimaryPath::Direct);
    auto &s = *e.rig;
    ASSERT_TRUE(s.StartLockedCard());
    bool present = false;
    EXPECT_EQ(s.manager->HasSimCard(card.physicalSlot, present), TELEPHONY_ERR_SUCCESS);
    ASSERT_TRUE(present);
    ASSERT_TRUE(s.manager->IsSimActive(card.physicalSlot));
    if (e.path != PrimaryPath::Direct)
        e.Mapping(!e.path.has_value());
    s.ClearObservations();
    s.Permit();
    EXPECT_CALL(*s.ril, SetPrimarySlot(testing::_, testing::_))
        .Times(e.path == PrimaryPath::Direct ? 1 : 0)
        .WillRepeatedly(
            [&e](int slot, const auto &event)
            {
                EXPECT_EQ(slot, 1);
                EXPECT_TRUE(std::dynamic_pointer_cast<MultiSimController>(event->GetOwner()));
                auto &s = *e.rig;
                s.observation.Add("ril_primary", {{"slot", std::to_string(slot)}});
                e.Capture({{slot, event->GetOwner(), event->GetInnerEventId()}, {}});
                return 0;
            });
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_))
        .Times(e.path == PrimaryPath::ThreePhase ? e.expected : 0)
        .WillRepeatedly(
            [&e](int slot, RadioProtocol protocol, const auto &event)
            {
                EXPECT_EQ(protocol.slotId, slot);
                EXPECT_TRUE(std::dynamic_pointer_cast<RadioProtocolController>(event->GetOwner()));
                if (e.expected == 24 && static_cast<int>(protocol.phase) == 4)
                    EXPECT_EQ(protocol.status, RadioProtocolStatus::RADIO_PROTOCOL_STATUS_FAIL);
                auto &s = *e.rig;
                s.observation.Add("ril_protocol", {{"slot", std::to_string(slot)},
                                                   {"session", std::to_string(protocol.sessionId)},
                                                   {"phase", std::to_string(static_cast<int>(protocol.phase))},
                                                   {"modem", std::to_string(protocol.modemId)},
                                                   {"technology", std::to_string(protocol.technology)}});
                e.Capture({{slot, event->GetOwner(), event->GetInnerEventId()}, protocol});
                return 0;
            });
}
int SimTest::SelectPrimary(const Card &card)
{
    auto &e = *environment_;
    return e.result = e.rig->SetPrimarySlotId(card.physicalSlot);
}
void SimTest::ExpectPrimaryResult(bool success)
{
    EXPECT_EQ(environment_->result, success ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_LOCAL_PTR_NULL);
}
void SimTest::ExpectPrimaryIdle()
{
    auto &s = *environment_->rig;
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (s.manager->IsSetPrimarySlotIdInProgress() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
}
void SimTest::ExpectPrimary(const Card &card, const std::string &identity)
{
    auto &s = *environment_->rig;
    int actual = -1;
    EXPECT_EQ(s.manager->GetPrimarySlotId(actual), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(actual, card.physicalSlot);
    EXPECT_EQ(s.manager->GetDefaultCellularDataSlotId(), card.physicalSlot);
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], std::to_string(card.physicalSlot))
        << "PRIMARY_COMMIT_TARGET";
    EXPECT_EQ(s.parametersDb["persist.telephony.MainCellularDataSlotId"], std::to_string(card.physicalSlot));
    if (!identity.empty())
        EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"], identity);
}
void PrimaryProtocol::ExpectPreviousPrimary(int previous, const std::string &identity)
{
    auto &s = *test_.environment_->rig;
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], std::to_string(previous));
    EXPECT_NE(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    if (!identity.empty())
        EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"], identity) << "ROLLBACK_ICCID";
}
void PrimaryProtocol::ExpectNotifications(bool committed, unsigned requests, bool direct)
{
    auto &s = *test_.environment_->rig;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.ExpectNotifications(committed, requests, direct);
}
void SimTest::ExpectPrimaryChanged(const Card &card)
{
    auto &e = *environment_;
    auto &s = *e.rig;
    EXPECT_EQ(card.physicalSlot, 1); // Capability of this controlled Legacy route.
    std::lock_guard<std::mutex> lock(s.mutex);
    s.ExpectNotifications(true, e.path.has_value() ? 1 : 0, e.path == PrimaryPath::Direct);
}
void PrimaryProtocol::AutoReply(std::function<PrimaryReply(const PrimaryRequest &)> policy)
{
    auto &e = *test_.environment_;
    if (!e.requests.empty())
        throw std::logic_error("Configure response policy before starting the request");
    e.policy = std::move(policy);
}
void PrimaryProtocol::Configure(PrimaryPath path, unsigned expected)
{
    auto &e = *test_.environment_;
    if (e.rig)
        throw std::logic_error("Configure before initialization");
    e.path = path;
    e.expected = expected;
}
void PrimaryProtocol::SetPreviousPrimary(int slot, const std::string &identity)
{
    auto &s = *test_.environment_->rig;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.parametersDb["persist.telephony.MainSlotId"] = std::to_string(slot);
    if (!identity.empty())
        s.parametersDb["persist.telephony.MainCard.Iccid"] = identity;
}
void PrimaryProtocol::ExpectUncommitted(const Card &card)
{
    auto &s = *test_.environment_->rig;
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_NE(s.parametersDb["persist.telephony.MainSlotId"], std::to_string(card.physicalSlot)) << "NO_EARLY_COMMIT";
}
void PrimaryProtocol::Begin(const Card &card)
{
    auto &e = *test_.environment_;
    e.caller = std::async(std::launch::async, [&e, card] { return e.rig->SetPrimarySlotId(card.physicalSlot); });
}
std::vector<PrimaryRequest> PrimaryProtocol::WaitRequests(size_t count, std::chrono::milliseconds timeout)
{
    auto &e = *test_.environment_;
    auto &s = *e.rig;
    std::unique_lock<std::mutex> lock(s.mutex);
    if (!s.changed.wait_for(lock, timeout,
                            [&] { return e.requests.size() >= count && (!e.policy || e.handled >= count); }))
        throw std::runtime_error("Primary protocol request deadline exceeded");
    std::vector<PrimaryRequest> out;
    for (size_t i = 0; i < e.requests.size(); ++i)
    {
        const auto &r = e.requests[i];
        out.push_back({i, r.transport.slot, r.protocol.sessionId, static_cast<int>(r.protocol.phase),
                       r.protocol.modemId, r.protocol.technology});
    }
    return out;
}
void PrimaryProtocol::ExpectPending(std::chrono::milliseconds duration)
{
    auto &e = *test_.environment_;
    EXPECT_EQ(e.caller.wait_for(duration), std::future_status::timeout);
    EXPECT_TRUE(e.rig->manager->IsSetPrimarySlotIdInProgress());
}
void PrimaryProtocol::Reply(const PrimaryRequest &request, int delta)
{
    test_.environment_->Respond(request, PrimaryReply::Success, delta);
}
void PrimaryProtocol::Notify(const PrimaryRequest &request)
{
    test_.environment_->Respond(request, PrimaryReply::Notify);
}
void PrimaryProtocol::Reject(const PrimaryRequest &request)
{
    test_.environment_->Respond(request, PrimaryReply::GenericError);
}
void PrimaryProtocol::Finish(std::chrono::milliseconds timeout)
{
    auto &e = *test_.environment_;
    if (e.caller.valid())
    {
        ASSERT_EQ(e.caller.wait_for(timeout), std::future_status::ready);
        e.result = e.caller.get();
    }
}
void PrimaryProtocol::ExpectRoute(const Card &card)
{
    EXPECT_EQ(test_.environment_->rig->manager->GetRadioProtocolSlotIdByModemId(0), card.physicalSlot);
}
void SimTest::Start()
{
    if (!environment_->declared)
        throw std::logic_error("Prepare a card before Start");
    StartPrimary(*environment_->declared);
}
int SimTest::CardCapacity()
{
    if (!environment_->rig)
        throw std::logic_error("Start Primary before querying its initialized resources");
    return environment_->rig->manager->GetMaxSimCount();
}
int SimTest::ReadCapacity()
{
    if (!environment_->rig)
        throw std::logic_error("Start Primary before querying its initialized resources");
    return environment_->rig->manager->GetRealSimCount();
}
bool SimTest::ParameterBoundaryObserved()
{
    SkipCapability("UNSUPPORTED: use the initialization-free count material for parameter diagnostics");
}
State SimTest::ReadCardState(const Card &card)
{
    if (!environment_->rig)
        throw std::logic_error("Start before observing the card");
    SimState state{};
    EXPECT_EQ(environment_->rig->manager->GetSimState(card.physicalSlot, state), TELEPHONY_ERR_SUCCESS);
    switch (state)
    {
    case SimState::SIM_STATE_NOT_PRESENT:
        return State::Absent;
    case SimState::SIM_STATE_LOCKED:
        return State::Locked;
    case SimState::SIM_STATE_READY:
        return State::Ready;
    case SimState::SIM_STATE_NOT_READY:
        return State::NotReady;
    default:
        return State::Unknown;
    }
}
Card SimTest::ExistingCard(int slot)
{
    if (!environment_->rig)
        throw std::logic_error("Start before observing the card");
    auto &manager = *environment_->rig->manager;
    if (slot < 0 || slot >= manager.GetMaxSimCount())
        SkipCapability("UNSUPPORTED: card endpoint is outside this material");
    CardType kind{};
    EXPECT_EQ(manager.GetCardType(slot, kind), TELEPHONY_ERR_SUCCESS);
    std::u16string identity;
    EXPECT_EQ(manager.GetSimIccId(slot, identity), TELEPHONY_ERR_SUCCESS);
    return {"existing-" + std::to_string(slot), slot,
            kind == CardType::SINGLE_MODE_USIM_CARD  ? CardKind::Usim
            : kind == CardType::SINGLE_MODE_SIM_CARD ? CardKind::Sim
                                                     : CardKind::Unknown,
            std::string(identity.begin(), identity.end())};
}
void SimTest::ExpectReadableCard(const Card &card)
{
    auto state = ReadCardState(card);
    bool present = false;
    EXPECT_EQ(environment_->rig->manager->HasSimCard(card.physicalSlot, present), TELEPHONY_ERR_SUCCESS);
    if (state == State::Absent)
        EXPECT_FALSE(present);
    if (state == State::Ready || state == State::Locked)
        EXPECT_TRUE(present);
}
void SimTest::ExpectCardState(const Card &card, State state)
{
    EXPECT_EQ(ReadCardState(card), state);
    ExpectReadableCard(card);
    CardType kind{};
    EXPECT_EQ(environment_->rig->manager->GetCardType(card.physicalSlot, kind), TELEPHONY_ERR_SUCCESS);
    const auto observed = kind == CardType::SINGLE_MODE_USIM_CARD  ? CardKind::Usim
                          : kind == CardType::SINGLE_MODE_SIM_CARD ? CardKind::Sim
                                                                   : CardKind::Unknown;
    EXPECT_EQ(observed, state == State::Absent ? CardKind::Unknown : card.kind);
}
void SimTest::ExpectStateNotification(const Card &, State, Lock)
{
    SkipCapability("UNSUPPORTED: this Primary material observes role notifications, not card-state subscriptions");
}
void SimTest::ChangeCard(const Card &, State, Lock)
{
    SkipCapability("UNSUPPORTED: this Primary bootstrap material does not declare mutable card states");
}
UnlockResult SimTest::UnlockPin(const Card &, const std::string &)
{
    SkipCapability("UNSUPPORTED: this Primary bootstrap material does not declare PIN mutation");
}
UnlockResult SimTest::UnlockPuk(const Card &, const std::string &, const std::string &)
{
    SkipCapability("UNSUPPORTED: this Primary bootstrap material does not declare PUK mutation");
}

} // namespace SimGuard
