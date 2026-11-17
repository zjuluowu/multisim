// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sim_test.h"
#include "internal/common/observations.h"
#include "internal/common/state_assertions.h"
#include <chrono>
#include <functional>
namespace SimGuard
{
// Protocol facts, not production objects or transport-owner pointers.
struct PrimaryRequest
{
    size_t token;
    int slot;
    int session;
    int phase;
    int modem;
    int technology;
};
enum class PrimaryReply
{
    Success,
    GenericError,
    Notify,
    Hold
};
enum class PrimaryPath
{
    Direct,
    ThreePhase
};
class PrimaryProtocol
{
  public:
    explicit PrimaryProtocol(SimTest &test) : test_(test) {}
    void AutoReply(std::function<PrimaryReply(const PrimaryRequest &)> policy);
    void Configure(PrimaryPath path, unsigned expectedRequests = 6);
    void ExpectPreviousPrimary(int previous, const std::string &identity = "");
    void ExpectNotifications(bool committed, unsigned requests, bool direct);
    void SetPreviousPrimary(int slot, const std::string &identity = "");
    void ExpectUncommitted(const Card &card);
    void Begin(const Card &card);
    std::vector<PrimaryRequest> WaitRequests(size_t count, std::chrono::milliseconds timeout = std::chrono::seconds(3));
    void ExpectPending(std::chrono::milliseconds duration);
    void Reply(const PrimaryRequest &request, int sessionDelta = 0);
    void Notify(const PrimaryRequest &request);
    void Reject(const PrimaryRequest &request);
    void Finish(std::chrono::milliseconds timeout = std::chrono::seconds(3));
    void ExpectRoute(const Card &card);

  private:
    SimTest &test_;
};

// Exact card-wire control. No Fake, Host type or scheduler escapes this API.
class StateProtocol {
public:
    explicit StateProtocol(SimTest &test):test_(test) {}
    void Start(int slots);
    void StartWithRil(int slots,bool available);
    void Stop();
    void Recreate(bool available=true);
    StateSnapshot Observe();
    void Observe(const char *step);
    void Settle();
    void Complete();
    void Change(int slot,DeviceCard fact);
    void DeviceChange(int slot,DeviceCard fact);
    void ChangeWhileStopped(int slot,DeviceCard fact);
    UnlockResult UnlockPin(int slot,const std::string &pin);
    UnlockResult UnlockPuk(int slot,const std::string &newPin,const std::string &puk);
    int Refresh(int slot);
    void Notify(int slot);
    size_t Pending();
    size_t RespondAll(bool error=false);
    size_t RespondMatching(const std::function<bool(int,const DeviceCard &)> &match,bool error=false);
    void ReplayLast();
    void FailPin(bool fail);
    void NextStatus(StatusFault fault);
    void NextPin(PinFault fault);
    unsigned StatusRevision();
    bool AwaitStatusAfter(unsigned revision);
    bool AwaitHeldPin();
    void ReleasePin();
    void SetPukRemaining(int remaining);
private:
    SimTest &test_;
    std::string trace_;
};
} // namespace SimGuard
