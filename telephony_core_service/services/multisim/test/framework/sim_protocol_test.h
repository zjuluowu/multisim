// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sim_test.h"
#include <chrono>
namespace SimGuard {
// Protocol facts, not production objects or transport-owner pointers.
struct PrimaryRequest { size_t token; int slot; int session; int phase; int modem; int technology; };
enum class PrimaryPath { Direct, ThreePhase };
class PrimaryProtocol {
public:
    explicit PrimaryProtocol(SimTest &test) : test_(test) {}
    void Configure(PrimaryPath path, unsigned expectedRequests = 6);
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
}
