// SPDX-License-Identifier: Apache-2.0
#include "sim_test.h"
#include "core_service_client.h"
#include "telephony_observer.h"
#include "telephony_observer_client.h"
#include "system_ability_definition.h"
#include "iservice_registry.h"
#include "accesstoken_kit.h"
#include "token_setproc.h"
#include "sim_constant.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <map>
#include <fstream>
#include <thread>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace SimGuard {
namespace OT = OHOS::Telephony;
using namespace std::chrono_literals;
namespace {
const char *Resource(const char *name)
{
    if (const char *value = std::getenv(name)) return value;
    static const std::map<std::string, std::string> values = [] {
        std::map<std::string, std::string> fields;
        const char *path = std::getenv("SIM_DEVICE_RESOURCE_FILE");
        if (!path) return fields;
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto at = line.find('=');
            if (at != std::string::npos) fields.emplace(line.substr(0, at), line.substr(at + 1));
        }
        return fields;
    }();
    const auto found = values.find(name);
    return found == values.end() ? nullptr : found->second.c_str();
}
State Normalize(OT::SimState state)
{
    switch (state) {
        case OT::SimState::SIM_STATE_NOT_PRESENT: return State::Absent;
        case OT::SimState::SIM_STATE_LOCKED: return State::Locked;
        case OT::SimState::SIM_STATE_READY: return State::Ready;
        case OT::SimState::SIM_STATE_NOT_READY: return State::NotReady;
        default: return State::Unknown;
    }
}
CardKind Normalize(OT::CardType kind)
{
    if (kind == OT::CardType::SINGLE_MODE_USIM_CARD) return CardKind::Usim;
    if (kind == OT::CardType::SINGLE_MODE_SIM_CARD) return CardKind::Sim;
    return CardKind::Unknown;
}
struct Events {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<StateNotification> items;
    bool enabled = true;
};
class Observer final : public OT::TelephonyObserver {
public:
    explicit Observer(std::shared_ptr<Events> events) : events_(std::move(events)) {}
    void OnSimStateUpdated(int32_t slot, OT::CardType kind, OT::SimState state, OT::LockReason reason) override
    {
        std::lock_guard<std::mutex> lock(events_->mutex);
        if (!events_->enabled) return;
        events_->items.push_back({slot, Normalize(state), Normalize(kind), reason == OT::LockReason::SIM_PIN ?
            Lock::Pin : reason == OT::LockReason::SIM_PUK ? Lock::Puk : Lock::None});
        events_->changed.notify_all();
    }
private:
    std::shared_ptr<Events> events_;
};
struct Snapshot { OT::SimState state; OT::CardType kind; std::u16string iccid; };
Snapshot Read(int slot)
{
    Snapshot value{};
    auto &client = OT::CoreServiceClient::GetInstance();
    if (client.GetSimState(slot, value.state, 1000) != 0 || client.GetCardType(slot, value.kind) != 0)
        throw std::runtime_error("Live SIM query failed");
    if (Normalize(value.state) != State::Absent && client.GetSimIccId(slot, value.iccid) != 0)
        throw std::runtime_error("Live card identity query failed");
    return value;
}
}
struct SimTest::Environment {
    int socket = -1;
    bool reserved = false;
    std::vector<std::pair<Card, std::pair<State, Lock>>> prepared;
    std::vector<Snapshot> before;
    std::shared_ptr<Events> events = std::make_shared<Events>();
    OHOS::sptr<Observer> observer;
    std::vector<int> subscriptions;
    std::map<int, size_t> cursor;
    ~Environment() { if (socket >= 0) close(socket); }
    std::string Command(const std::string &message)
    {
        const auto data = message + "\n";
        size_t sent = 0;
        while (sent < data.size()) {
            auto n = send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) throw std::runtime_error("Device resource controller write failed");
            sent += static_cast<size_t>(n);
        }
        std::string response;
        char c;
        while (response.size() < 512) {
            if (recv(socket, &c, 1, 0) != 1) throw std::runtime_error("Device resource controller timed out");
            if (c == '\n') return response;
            response += c;
        }
        throw std::runtime_error("Invalid resource controller response");
    }
    void Change(const Card &card, State state, Lock lock)
    {
        { std::lock_guard<std::mutex> guard(events->mutex); cursor[card.physicalSlot] = events->items.size(); }
        const auto request = "CHANGE " + card.name + " " + std::to_string(card.physicalSlot) + " " +
            std::to_string(static_cast<int>(card.kind)) + " " + std::to_string(static_cast<int>(state)) + " " +
            std::to_string(static_cast<int>(lock));
        if (Command(request) != "OK") throw std::runtime_error("Device resource preparation failed");
        Wait(card.physicalSlot, state, state == State::Absent ? CardKind::Unknown : card.kind);
    }
    void Wait(int slot, State expected, CardKind kind, const std::u16string *identity = nullptr)
    {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        auto matches = [&] {
            const auto current = Read(slot);
            return Normalize(current.state) == expected && Normalize(current.kind) == kind &&
                (!identity || current.iccid == *identity);
        };
        for (;;) {
            size_t revision;
            { std::lock_guard<std::mutex> lock(events->mutex); revision = events->items.size(); }
            if (matches()) return;
            std::unique_lock<std::mutex> lock(events->mutex);
            if (events->items.size() != revision) continue;
            if (events->changed.wait_until(lock, deadline) == std::cv_status::timeout) {
                lock.unlock();
                if (!matches()) throw std::runtime_error("Timed out waiting for live SIM observation");
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
    if (AccessTokenKit::VerifyAccessToken(GetSelfTokenID(), "ohos.permission.GET_TELEPHONY_STATE") != PERMISSION_GRANTED ||
        AccessTokenKit::VerifyAccessToken(GetSelfTokenID(), "ohos.permission.SET_TELEPHONY_STATE") != PERMISSION_GRANTED)
        GTEST_SKIP() << "BLOCKED: native test identity lacks GET/SET_TELEPHONY_STATE";
    auto sa = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (sa == nullptr || sa->CheckSystemAbility(OHOS::TELEPHONY_CORE_SERVICE_SYS_ABILITY_ID) == nullptr)
        GTEST_SKIP() << "BLOCKED: telephony Core SA is not ready";
    const char *path = Resource("SIM_DEVICE_CONTROL_SOCKET");
    const char *token = Resource("SIM_DEVICE_FIXTURE_TOKEN");
    if (!path || !token || !Resource("SIM_DEVICE_ALLOWED_PINS")) GTEST_SKIP() << "UNSUPPORTED: controlled SIM fixture and restoration controller required";
    environment_ = std::make_unique<Environment>();
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (std::strlen(path) >= sizeof(address.sun_path)) GTEST_SKIP() << "BLOCKED: controller socket path too long";
    std::strcpy(address.sun_path, path);
    environment_->socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (environment_->socket < 0) GTEST_SKIP() << "BLOCKED: resource socket unavailable";
    timeval timeout{10, 0};
    setsockopt(environment_->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(environment_->socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(environment_->socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
        GTEST_SKIP() << "BLOCKED: controlled SIM fixture unavailable";
    if (environment_->Command(std::string("SIM_TEST_V1 RESERVE ") + token) != "OK CHANGE PIN RESTORE NOTIFY DUAL")
        GTEST_SKIP() << "UNSUPPORTED: fixture must guarantee physical CHANGE, RESTORE, notifications and dual SIM";
    environment_->reserved = true;
    if (OT::CoreServiceClient::GetInstance().GetMaxSimCount() < 2)
        GTEST_SKIP() << "UNSUPPORTED: dual physical SIM resource required";
    environment_->observer = new Observer(environment_->events);
    for (int slot = 0; slot < 2; ++slot) {
        try { environment_->before.push_back(Read(slot)); }
        catch (const std::exception &) { GTEST_SKIP() << "BLOCKED: SIM service queries are not ready"; }
        if (OT::TelephonyObserverClient::GetInstance().AddStateObserver(environment_->observer, slot,
            OT::TelephonyObserverBroker::OBSERVER_MASK_SIM_STATE, false) != 0)
            GTEST_SKIP() << "BLOCKED: state registry subscription rejected";
        environment_->subscriptions.push_back(slot);
    }
}
void SimTest::TearDown()
{
    if (!environment_) return;
    if (environment_->reserved) {
        try {
        EXPECT_EQ(environment_->Command("RESTORE"), "OK");
        for (size_t slot = 0; slot < environment_->before.size(); ++slot) {
            const auto &before = environment_->before[slot];
            environment_->Wait(static_cast<int>(slot), Normalize(before.state), Normalize(before.kind), &before.iccid);
            const auto now = Read(static_cast<int>(slot));
            EXPECT_TRUE(now.state == before.state && now.kind == before.kind && now.iccid == before.iccid)
                << "Device fixture failed to restore original SIM resource";
        }
        } catch (const std::exception &e) { ADD_FAILURE() << e.what(); }
    }
    for (int slot : environment_->subscriptions)
        EXPECT_EQ(OT::TelephonyObserverClient::GetInstance().RemoveStateObserver(slot,
            OT::TelephonyObserverBroker::OBSERVER_MASK_SIM_STATE), 0);
    { std::lock_guard<std::mutex> lock(environment_->events->mutex); environment_->events->enabled = false; }
    OHOS::wptr<Observer> retiring(environment_->observer);
    environment_->observer = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (retiring.promote() != nullptr && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    EXPECT_TRUE(retiring.promote() == nullptr) << "Native observer ownership did not drain";
    environment_.reset();
}
Card SimTest::AddCard(std::string name, int slot, CardKind kind, State state, Lock lock)
{
    if (name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos || slot < 0 || slot > 1)
        throw std::invalid_argument("Invalid controlled card resource");
    Card card{std::move(name), slot, kind, ""};
    environment_->prepared.push_back({card, {state, lock}});
    return card;
}
void SimTest::Start() { for (const auto &p : environment_->prepared) environment_->Change(p.first, p.second.first, p.second.second); }
void SimTest::ChangeCard(const Card &card, State state, Lock lock) { environment_->Change(card, state, lock); }
UnlockResult SimTest::UnlockPin(const Card &card, const std::string &pin)
{
    const char *authorized = Resource("SIM_DEVICE_ALLOWED_PINS");
    const std::string allowed = authorized ? "," + std::string(authorized) + "," : "";
    if (!authorized || allowed.find("," + pin + ",") == std::string::npos) throw std::runtime_error("PIN input is not declared by the controlled device resource");
    { std::lock_guard<std::mutex> lock(environment_->events->mutex); environment_->cursor[card.physicalSlot] = environment_->events->items.size(); }
    OT::LockStatusResponse response{};
    const auto code = OT::CoreServiceClient::GetInstance().UnlockPin(card.physicalSlot, std::u16string(pin.begin(), pin.end()), response);
    if (code == 0 && response.result == OT::UNLOCK_OK) environment_->Wait(card.physicalSlot, State::Ready, card.kind);
    return {code, code != 0 ? Unlock::Failure : response.result == OT::UNLOCK_OK ? Unlock::Ok : response.result == OT::UNLOCK_INCORRECT ? Unlock::Incorrect : Unlock::Failure, response.remain};
}
void SimTest::ExpectCardState(const Card &card, State expected)
{
    const auto actual = Read(card.physicalSlot);
    EXPECT_EQ(Normalize(actual.state), expected);
    EXPECT_EQ(Normalize(actual.kind), expected == State::Absent ? CardKind::Unknown : card.kind);
    bool present = false;
    EXPECT_EQ(OT::CoreServiceClient::GetInstance().HasSimCard(card.physicalSlot, present, 1000), 0);
    EXPECT_EQ(present, expected != State::Absent);
}
void SimTest::ExpectStateNotification(const Card &card, State expected, Lock reason)
{
    auto events = environment_->events;
    std::unique_lock<std::mutex> lock(events->mutex);
    EXPECT_TRUE(events->changed.wait_for(lock, 10s, [&] {
        return std::any_of(events->items.begin() + environment_->cursor.at(card.physicalSlot), events->items.end(),
            [&](const auto &e) { return e.slot == card.physicalSlot && e.state == expected && e.reason == reason; });
    })) << "Missing real state registry notification";
}
}
