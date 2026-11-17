// SPDX-License-Identifier: Apache-2.0
#include "sim_test.h"
#include "accesstoken_kit.h"
#include "core_service_client.h"
#include "internal/common/capability.h"
#include "internal/common/observations.h"
#include "iservice_registry.h"
#include "sim_constant.h"
#include "system_ability_definition.h"
#include "telephony_observer.h"
#include "telephony_observer_client.h"
#include "token_setproc.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace SimGuard
{
namespace OT = OHOS::Telephony;
using namespace std::chrono_literals;
namespace
{
const char *Resource(const char *name)
{
    if (const char *value = std::getenv(name))
        return value;
    static const std::map<std::string, std::string> values = []
    {
        std::map<std::string, std::string> fields;
        const char *path = std::getenv("SIM_DEVICE_RESOURCE_FILE");
        if (!path)
            return fields;
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty() || line[0] == '#')
                continue;
            const auto at = line.find('=');
            if (at != std::string::npos)
                fields.emplace(line.substr(0, at), line.substr(at + 1));
        }
        return fields;
    }();
    const auto found = values.find(name);
    return found == values.end() ? nullptr : found->second.c_str();
}
State Normalize(OT::SimState state)
{
    switch (state)
    {
    case OT::SimState::SIM_STATE_NOT_PRESENT:
        return State::Absent;
    case OT::SimState::SIM_STATE_LOCKED:
        return State::Locked;
    case OT::SimState::SIM_STATE_READY:
        return State::Ready;
    case OT::SimState::SIM_STATE_NOT_READY:
        return State::NotReady;
    default:
        return State::Unknown;
    }
}
CardKind Normalize(OT::CardType kind)
{
    if (kind == OT::CardType::SINGLE_MODE_USIM_CARD)
        return CardKind::Usim;
    if (kind == OT::CardType::SINGLE_MODE_SIM_CARD)
        return CardKind::Sim;
    return CardKind::Unknown;
}
struct Events
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<StateNotification> items;
    bool enabled = true;
};
class Observer final : public OT::TelephonyObserver
{
  public:
    explicit Observer(std::shared_ptr<Events> events) : events_(std::move(events)) {}
    void OnSimStateUpdated(int32_t slot, OT::CardType kind, OT::SimState state, OT::LockReason reason) override
    {
        std::lock_guard<std::mutex> lock(events_->mutex);
        if (!events_->enabled)
            return;
        events_->items.push_back({slot, Normalize(state), Normalize(kind),
                                  reason == OT::LockReason::SIM_PIN   ? Lock::Pin
                                  : reason == OT::LockReason::SIM_PUK ? Lock::Puk
                                                                      : Lock::None});
        events_->changed.notify_all();
    }

  private:
    std::shared_ptr<Events> events_;
};
struct Snapshot
{
    OT::SimState state;
    OT::CardType kind;
    std::u16string iccid;
};
Snapshot Read(int slot, bool identity = false)
{
    Snapshot value{};
    auto &client = OT::CoreServiceClient::GetInstance();
    if (client.GetSimState(slot, value.state, 1000) != 0 || client.GetCardType(slot, value.kind) != 0)
        throw std::runtime_error("Live SIM query failed");
    if (identity)
    {
        bool present = false;
        if (client.HasSimCard(slot, present, 1000) != 0)
            throw std::runtime_error("Original card presence query failed");
        if (present && client.GetSimIccId(slot, value.iccid) != 0)
            throw std::runtime_error("Original card identity is not observable for safe restoration");
    }
    return value;
}
} // namespace
struct SimTest::Environment
{
    int socket = -1;
    bool reserved = false;
    std::vector<std::pair<Card, std::pair<State, Lock>>> prepared;
    std::map<int, Snapshot> before;
    int capacity = 0;
    std::string capabilities;
    std::shared_ptr<Events> events = std::make_shared<Events>();
    OHOS::sptr<Observer> observer;
    std::vector<int> subscriptions;
    std::map<int, size_t> cursor;
    ~Environment()
    {
        if (socket >= 0)
            close(socket);
    }
    std::string Command(const std::string &message)
    {
        const auto data = message + "\n";
        size_t sent = 0;
        while (sent < data.size())
        {
            auto n = send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
            if (n <= 0)
                throw std::runtime_error("Device resource controller write failed");
            sent += static_cast<size_t>(n);
        }
        std::string response;
        char c;
        while (response.size() < 512)
        {
            if (recv(socket, &c, 1, 0) != 1)
                throw std::runtime_error("Device resource controller timed out");
            if (c == '\n')
                return response;
            response += c;
        }
        throw std::runtime_error("Invalid resource controller response");
    }
    void RequireMutable(int slot, const std::string &capability)
    {
        using namespace OHOS::Security::AccessToken;
        if (AccessTokenKit::VerifyAccessToken(GetSelfTokenID(), "ohos.permission.SET_TELEPHONY_STATE") !=
            PERMISSION_GRANTED)
            SkipCapability("BLOCKED: requested operation lacks SET_TELEPHONY_STATE");
        if (slot < 0 || slot >= capacity)
            SkipCapability("UNSUPPORTED: requested endpoint exceeds product resource capacity");
        if (!reserved)
        {
            const char *path = Resource("SIM_DEVICE_CONTROL_SOCKET");
            const char *token = Resource("SIM_DEVICE_FIXTURE_TOKEN");
            if (!path || !token)
                SkipCapability("UNSUPPORTED: mutation requires a controlled fixture with restoration");
            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            if (std::strlen(path) >= sizeof(address.sun_path))
                SkipCapability("BLOCKED: controller socket path too long");
            std::strcpy(address.sun_path, path);
            socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (socket < 0)
                SkipCapability("BLOCKED: controller socket unavailable");
            timeval timeout{10, 0};
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            if (connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
                SkipCapability("BLOCKED: controlled fixture unavailable");
            capabilities = " " + Command(std::string("SIM_TEST_V1 RESERVE ") + token) + " ";
            if (capabilities.find(" OK ") == std::string::npos)
                SkipCapability("BLOCKED: fixture reservation rejected");
            if (capabilities.find(" RESTORE ") == std::string::npos)
                SkipCapability("UNSUPPORTED: resource restoration not supported");
            reserved = true;
        }
        if (capabilities.find(" " + capability + " ") == std::string::npos)
            SkipCapability("UNSUPPORTED: requested fixture capability " + capability);
        if (!before.count(slot))
        {
            try
            {
                before.emplace(slot, Read(slot, true));
            }
            catch (const std::exception &e)
            {
                SkipCapability(std::string("BLOCKED: ") + e.what());
            }
        }
        if (std::find(subscriptions.begin(), subscriptions.end(), slot) == subscriptions.end())
        {
            if (!observer)
                observer = new Observer(events);
            if (OT::TelephonyObserverClient::GetInstance().AddStateObserver(
                    observer, slot, OT::TelephonyObserverBroker::OBSERVER_MASK_SIM_STATE, false) != 0)
                SkipCapability("BLOCKED: requested state notification subscription rejected");
            subscriptions.push_back(slot);
        }
    }
    void Change(const Card &card, State state, Lock lock)
    {
        RequireMutable(card.physicalSlot, "CHANGE");
        if (!card.iccid.empty())
        {
            RequireMutable(card.physicalSlot, "IDENTITY");
            const char *declared = Resource("SIM_DEVICE_ALLOWED_ICCIDS");
            if (!declared || card.iccid.find_first_not_of("0123456789") != std::string::npos ||
                ("," + std::string(declared) + ",").find("," + card.iccid + ",") == std::string::npos)
                SkipCapability("UNSUPPORTED: requested replacement ICCID is not a declared real device resource");
        }
        {
            std::lock_guard<std::mutex> guard(events->mutex);
            cursor[card.physicalSlot] = events->items.size();
        }
        const auto request = "CHANGE " + card.name + " " + std::to_string(card.physicalSlot) + " " +
                             std::to_string(static_cast<int>(card.kind)) + " " +
                             std::to_string(static_cast<int>(state)) + " " + std::to_string(static_cast<int>(lock));
        const auto reply = Command(request + (card.iccid.empty() ? "" : " ID " + card.iccid));
        if (reply.rfind("UNSUPPORTED", 0) == 0)
            SkipCapability("UNSUPPORTED: " + reply);
        if (reply.rfind("BLOCKED", 0) == 0)
            SkipCapability("BLOCKED: " + reply);
        if (reply != "OK")
            throw std::runtime_error("Device resource preparation failed");
        const std::u16string identity(card.iccid.begin(), card.iccid.end());
        Wait(card.physicalSlot, state, state == State::Absent ? CardKind::Unknown : card.kind,
             card.iccid.empty() ? nullptr : &identity);
    }
    void Wait(int slot, State expected, CardKind kind, const std::u16string *identity = nullptr)
    {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        auto matches = [&]
        {
            const auto current = Read(slot, identity != nullptr);
            return Normalize(current.state) == expected && Normalize(current.kind) == kind &&
                   (!identity || current.iccid == *identity);
        };
        for (;;)
        {
            size_t revision;
            {
                std::lock_guard<std::mutex> lock(events->mutex);
                revision = events->items.size();
            }
            if (matches())
                return;
            std::unique_lock<std::mutex> lock(events->mutex);
            if (events->items.size() != revision)
                continue;
            if (events->changed.wait_until(lock, deadline) == std::cv_status::timeout)
            {
                lock.unlock();
                if (!matches())
                    throw std::runtime_error("Timed out waiting for live SIM observation");
                return;
            }
        }
    }
};
SimTest::SimTest() = default;
SimTest::~SimTest() = default;
void SimTest::SetUp()
{
    RecordProperty("verification_level", "live_SA_IPC_state_registry_integration");
    using namespace OHOS::Security::AccessToken;
    if (AccessTokenKit::VerifyAccessToken(GetSelfTokenID(), "ohos.permission.GET_TELEPHONY_STATE") !=
        PERMISSION_GRANTED)
        GTEST_SKIP() << "BLOCKED: native test identity lacks GET_TELEPHONY_STATE";
    auto sa = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (sa == nullptr || sa->CheckSystemAbility(OHOS::TELEPHONY_CORE_SERVICE_SYS_ABILITY_ID) == nullptr)
        GTEST_SKIP() << "BLOCKED: telephony Core SA is not ready";
    environment_ = std::make_unique<Environment>();
    environment_->capacity = OT::CoreServiceClient::GetInstance().GetMaxSimCount();
    if (environment_->capacity < 0)
        GTEST_SKIP() << "BLOCKED: SIM resource capacity query failed";
}
void SimTest::TearDown()
{
    if (!environment_)
        return;
    if (environment_->reserved)
    {
        try
        {
            EXPECT_EQ(environment_->Command("RESTORE"), "OK");
            for (const auto &[slot, before] : environment_->before)
            {
                environment_->Wait(static_cast<int>(slot), Normalize(before.state), Normalize(before.kind),
                                   &before.iccid);
                const auto now = Read(static_cast<int>(slot), true);
                EXPECT_TRUE(now.state == before.state && now.kind == before.kind && now.iccid == before.iccid)
                    << "Device fixture failed to restore original SIM resource";
            }
        }
        catch (const std::exception &e)
        {
            ADD_FAILURE() << e.what();
        }
    }
    for (int slot : environment_->subscriptions)
        EXPECT_EQ(OT::TelephonyObserverClient::GetInstance().RemoveStateObserver(
                      slot, OT::TelephonyObserverBroker::OBSERVER_MASK_SIM_STATE),
                  0);
    {
        std::lock_guard<std::mutex> lock(environment_->events->mutex);
        environment_->events->enabled = false;
    }
    OHOS::wptr<Observer> retiring(environment_->observer);
    environment_->observer = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (retiring.promote() != nullptr && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_TRUE(retiring.promote() == nullptr) << "Native observer ownership did not drain";
    environment_.reset();
}
Card SimTest::AddCard(std::string name, int slot, CardKind kind, State state, Lock lock)
{
    if (name.empty() ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") !=
            std::string::npos ||
        slot < 0)
        throw std::invalid_argument("Invalid controlled card resource");
    if (slot >= environment_->capacity)
        SkipCapability("UNSUPPORTED: requested endpoint exceeds product resource capacity");
    for (const auto &prepared : environment_->prepared)
        if (prepared.first.name == name || prepared.first.physicalSlot == slot)
            throw std::invalid_argument("Duplicate test card name or physical resource");
    Card card{std::move(name), slot, kind, ""};
    environment_->prepared.push_back({card, {state, lock}});
    return card;
}
Card SimTest::AddCard(std::string name, CardKind kind, State state, Lock lock)
{
    return AddCard(std::move(name), static_cast<int>(environment_->prepared.size()), kind, state, lock);
}
int SimTest::CardCapacity() { return environment_->capacity; }
Card SimTest::ExistingCard(int slot)
{
    if (slot < 0 || slot >= environment_->capacity)
        SkipCapability("UNSUPPORTED: requested card endpoint is unavailable");
    const auto value = Read(slot);
    return {"existing-" + std::to_string(slot), slot, Normalize(value.kind), ""};
}
State SimTest::ReadCardState(const Card &card) { return Normalize(Read(card.physicalSlot).state); }
void SimTest::Start()
{
    for (const auto &p : environment_->prepared)
        environment_->Change(p.first, p.second.first, p.second.second);
}
void SimTest::ChangeCard(const Card &card, State state, Lock lock) { environment_->Change(card, state, lock); }
UnlockResult SimTest::UnlockPin(const Card &card, const std::string &pin)
{
    environment_->RequireMutable(card.physicalSlot, "PIN");
    const char *authorized = Resource("SIM_DEVICE_ALLOWED_PINS");
    const std::string allowed = authorized ? "," + std::string(authorized) + "," : "";
    if (!authorized || allowed.find("," + pin + ",") == std::string::npos)
        SkipCapability("UNSUPPORTED: PIN input is not declared by the controlled device resource");
    {
        std::lock_guard<std::mutex> lock(environment_->events->mutex);
        environment_->cursor[card.physicalSlot] = environment_->events->items.size();
    }
    OT::LockStatusResponse response{};
    const auto code = OT::CoreServiceClient::GetInstance().UnlockPin(card.physicalSlot,
                                                                     std::u16string(pin.begin(), pin.end()), response);
    if (code == 0 && response.result == OT::UNLOCK_OK)
        environment_->Wait(card.physicalSlot, State::Ready, card.kind);
    return {code,
            code != 0                                 ? Unlock::Failure
            : response.result == OT::UNLOCK_OK        ? Unlock::Ok
            : response.result == OT::UNLOCK_INCORRECT ? Unlock::Incorrect
                                                      : Unlock::Failure,
            response.remain};
}
UnlockResult SimTest::UnlockPuk(const Card &card, const std::string &newPin, const std::string &puk)
{
    environment_->RequireMutable(card.physicalSlot, "PUK");
    const char *authorized = Resource("SIM_DEVICE_ALLOWED_PUKS");
    const char *pins = Resource("SIM_DEVICE_ALLOWED_NEW_PINS");
    if (!authorized || !pins || ("," + std::string(authorized) + ",").find("," + puk + ",") == std::string::npos ||
        ("," + std::string(pins) + ",").find("," + newPin + ",") == std::string::npos)
        SkipCapability("UNSUPPORTED: PUK/new PIN is not declared by the controlled resource");
    {std::lock_guard<std::mutex> lock(environment_->events->mutex);environment_->cursor[card.physicalSlot]=environment_->events->items.size();}
    OT::LockStatusResponse response{};
    const auto code =
        OT::CoreServiceClient::GetInstance().UnlockPuk(card.physicalSlot, std::u16string(newPin.begin(), newPin.end()),
                                                       std::u16string(puk.begin(), puk.end()), response);
    if(code==0 && response.result==OT::UNLOCK_OK)environment_->Wait(card.physicalSlot,State::Ready,card.kind);
    return {code,
            code != 0                                 ? Unlock::Failure
            : response.result == OT::UNLOCK_OK        ? Unlock::Ok
            : response.result == OT::UNLOCK_INCORRECT ? Unlock::Incorrect
                                                      : Unlock::Failure,
            response.remain};
}
void SimTest::ExpectReadableCard(const Card &card)
{
    const auto actual = Read(card.physicalSlot);
    bool present = false;
    EXPECT_EQ(OT::CoreServiceClient::GetInstance().HasSimCard(card.physicalSlot, present, 1000), 0);
    const auto state = Normalize(actual.state);
    if (state == State::Absent)
        EXPECT_FALSE(present);
    if (state == State::Ready || state == State::Locked)
        EXPECT_TRUE(present);
}
void SimTest::ExpectCardState(const Card &card, State expected)
{
    const auto actual = Read(card.physicalSlot);
    EXPECT_EQ(Normalize(actual.state), expected);
    EXPECT_EQ(Normalize(actual.kind), expected == State::Absent ? CardKind::Unknown : card.kind);
    bool present = false;
    EXPECT_EQ(OT::CoreServiceClient::GetInstance().HasSimCard(card.physicalSlot, present, 1000), 0);
    if (expected != State::Unknown)
        EXPECT_EQ(present, expected != State::Absent);
}
void SimTest::ExpectStateNotification(const Card &card, State expected, Lock reason)
{
    auto events = environment_->events;
    std::unique_lock<std::mutex> lock(events->mutex);
    EXPECT_TRUE(events->changed.wait_for(
        lock, 10s,
        [&]
        {
            return std::any_of(events->items.begin() + environment_->cursor.at(card.physicalSlot), events->items.end(),
                               [&](const auto &e)
                               { return e.slot == card.physicalSlot && e.state == expected && e.reason == reason; });
        }))
        << "Missing real state registry notification";
}
void SimTest::StartPrimary(const Card &)
{
    SkipCapability("UNSUPPORTED: SA/IPC material does not declare Primary RIL/persistence observation; use the native "
                   "Primary component target");
}
int SimTest::SelectPrimary(const Card &)
{
    SkipCapability("UNSUPPORTED: SA/IPC material does not declare Primary RIL/persistence observation; use the native "
                   "Primary component target");
}
void SimTest::ExpectPrimary(const Card &, const std::string &)
{
    SkipCapability("UNSUPPORTED: Primary observation belongs to the native controlled component material");
}
void SimTest::ExpectPrimaryChanged(const Card &)
{
    SkipCapability("UNSUPPORTED: this integration material does not declare Primary notification observation");
}
void SimTest::ExpectPrimaryResult(bool)
{
    SkipCapability("UNSUPPORTED: this integration material does not declare Primary operation control");
}
void SimTest::ExpectPrimaryIdle()
{
    SkipCapability("UNSUPPORTED: this integration material does not declare Primary operation control");
}

int SimTest::ReadCapacity() { return OT::CoreServiceClient::GetInstance().GetMaxSimCount(); }
bool SimTest::ParameterBoundaryObserved()
{
    SkipCapability("UNSUPPORTED: SA/IPC tests do not observe private parameter reads");
}

} // namespace SimGuard
