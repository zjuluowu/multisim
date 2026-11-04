// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace SimGuard {
// Device facts and transport scheduling only; never produces an SUT observation.
struct DeviceCard {
    State state = State::Absent;
    CardKind kind = CardKind::Unknown;
    std::string iccid;
    Lock reason = Lock::None;
};
class FakeRil {
public:
    using Reply = std::function<void(const DeviceCard &, bool)>;
    struct Request { int slot; DeviceCard atRequest; Reply reply; };
    void Configure(int slots)
    {
        std::lock_guard<std::mutex> guard(lock);
        cards.assign(slots, {}); attempts.assign(slots, 3); pinFailure = false;
    }
    void BindNotify(std::function<void(int)> callback)
    {
        std::lock_guard<std::mutex> guard(lock);
        notify = std::move(callback);
    }
    void Change(int slot, DeviceCard fact)
    {
        { std::lock_guard<std::mutex> guard(lock); cards.at(slot) = std::move(fact); }
        Notify(slot);
    }
    void Notify(int slot)
    {
        std::function<void(int)> callback;
        { std::lock_guard<std::mutex> guard(lock); callback = notify; }
        if (!callback) throw std::logic_error("RIL notification boundary not bound");
        callback(slot);
    }
    void RequestStatus(int slot, Reply reply)
    {
        std::lock_guard<std::mutex> guard(lock);
        pending.push_back({slot, cards.at(slot), std::move(reply)});
        calls.push_back("status:" + std::to_string(slot));
    }
    size_t Pending() { std::lock_guard<std::mutex> guard(lock); return pending.size(); }
    void Respond(size_t index = 0, bool error = false)
    {
        Request request;
        {
            std::lock_guard<std::mutex> guard(lock);
            request = pending.at(index);
            pending.erase(pending.begin() + index);
            last = request;
        }
        request.reply(request.atRequest, error);
    }
    void ReplayLast()
    {
        Request request;
        { std::lock_guard<std::mutex> guard(lock); request = last; }
        request.reply(request.atRequest, false);
    }
    UnlockResult Pin(int slot, const std::string &pin)
    {
        std::lock_guard<std::mutex> guard(lock);
        calls.push_back("pin:" + std::to_string(slot)); // never record credentials
        if (pinFailure) return {-1, Unlock::Failure, attempts.at(slot)};
        if (pin != "1234") return {0, Unlock::Incorrect, --attempts.at(slot)};
        cards.at(slot).state = State::Ready;
        return {0, Unlock::Ok, attempts.at(slot)};
    }
    void FailPin(bool fail) { std::lock_guard<std::mutex> guard(lock); pinFailure = fail; }
    std::vector<std::string> Calls() { std::lock_guard<std::mutex> guard(lock); return calls; }
    void ClearTransport()
    {
        std::lock_guard<std::mutex> guard(lock);
        pending.clear(); last = {}; notify = {}; calls.clear();
    }
private:
    std::function<void(int)> notify;
    bool pinFailure = false;
    std::vector<std::string> calls;
    std::mutex lock;
    std::vector<DeviceCard> cards;
    std::vector<int> attempts;
    std::vector<Request> pending;
    Request last{};
};
}
