// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "internal/common/observations.h"
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace SimGuard {
// Device facts and transport scheduling only; never produces an SUT observation.
class FakeRil {
public:
    using Reply = std::function<void(const DeviceCard &, bool)>;
    struct Request { int slot; DeviceCard atRequest; Reply reply; };
    void Configure(int slots)
    {
        std::lock_guard<std::mutex> guard(lock);
        cards.assign(slots, {}); attempts.assign(slots, 3); pins.assign(slots,"1234"); pinFailure = false;
    }
    void BindNotify(std::function<void(int)> callback)
    {
        std::lock_guard<std::mutex> guard(lock);
        notify = std::move(callback);
    }
    void Change(int slot, DeviceCard fact)
    {
        { std::lock_guard<std::mutex> guard(lock); if(cards.at(slot).iccid!=fact.iccid)pins.at(slot)="1234"; cards.at(slot) = std::move(fact); }
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
    size_t RespondMatching(const std::function<bool(int, const DeviceCard &)> &matches, bool error = false)
    {
        std::vector<Request> batch;
        {
            std::lock_guard<std::mutex> guard(lock);
            for (auto it = pending.begin(); it != pending.end();) {
                if (matches(it->slot, it->atRequest)) { batch.push_back(*it); it = pending.erase(it); }
                else ++it;
            }
            if (!batch.empty()) last = batch.back();
        }
        for (const auto &request : batch) request.reply(request.atRequest, error);
        return batch.size();
    }
    size_t RespondAll(bool error = false)
    {
        return RespondMatching([](int, const DeviceCard &) { return true; }, error);
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
        if (pin != pins.at(slot)) return {0, Unlock::Incorrect, --attempts.at(slot)};
        cards.at(slot).state = State::Ready;
        return {0, Unlock::Ok, attempts.at(slot)};
    }
    // External card credentials/facts only; the SUT still processes the RIL
    // response and an independent status query before any READY assertion.
    void AcceptPuk(int slot,const std::string &newPin,const UnlockResult &reply) {
        if(reply.result!=Unlock::Ok)return;
        std::lock_guard<std::mutex> guard(lock);
        pins.at(slot)=newPin;attempts.at(slot)=3;cards.at(slot).state=State::Ready;
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
    std::vector<std::string> pins;
    std::vector<Request> pending;
    Request last{};
};
}
