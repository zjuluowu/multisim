#include "sim_protocol_test.h"
#include <map>
using namespace SimGuard;
using namespace std::chrono_literals;
using RealSimPrimary = SimTest;
TEST_F(RealSimPrimary, BDirectRilSuccessCommitsAfterReply)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    PrimaryProtocol protocol(*this);
    protocol.Configure(PrimaryPath::Direct);
    StartPrimary(target);
    protocol.Begin(target);
    auto request = protocol.WaitRequests(1).at(0);
    protocol.ExpectUncommitted(target);
    protocol.ExpectPending(50ms);
    protocol.Reply(request);
    protocol.Finish();
    ExpectPrimaryResult(true);
    ExpectPrimaryIdle();
    ExpectPrimary(target);
    protocol.ExpectNotifications(true, 1, true);
}
TEST_F(RealSimPrimary, BDirectRilFailureKeepsPreviousPrimary)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    PrimaryProtocol protocol(*this);
    protocol.Configure(PrimaryPath::Direct);
    StartPrimary(target);
    protocol.SetPreviousPrimary(0);
    protocol.Begin(target);
    protocol.Reject(protocol.WaitRequests(1).at(0));
    protocol.Finish();
    ExpectPrimaryResult(false);
    ExpectPrimaryIdle();
    protocol.ExpectPreviousPrimary(0);
    protocol.ExpectNotifications(false, 1, true);
}
TEST_F(RealSimPrimary, C1ThreePhasesWithStaleCheckResponseRejected)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    PrimaryProtocol protocol(*this);
    protocol.Configure(PrimaryPath::ThreePhase);
    StartPrimary(target);
    protocol.Begin(target);
    int session = -1;
    for (unsigned stage = 0; stage < 3; ++stage)
    {
        auto requests = protocol.WaitRequests(2 * (stage + 1));
        protocol.ExpectPending(10ms);
        for (unsigned i = 2 * stage; i < 2 * stage + 2; ++i)
        {
            const auto &request = requests.at(i);
            if (session == -1)
                session = request.session;
            EXPECT_EQ(request.session, session);
            EXPECT_EQ(request.phase, stage == 0 ? 1 : stage == 1 ? 2 : 4);
            EXPECT_EQ(request.modem, stage == 0 ? request.slot : 1 - request.slot);
            EXPECT_EQ(request.technology,
                      stage == 0 ? (request.slot == 0 ? 0x11 : 0x22) : (request.slot == 0 ? 0x22 : 0x11));
            if (stage == 0 && request.slot == 0)
            {
                protocol.Reply(request, 99);
                protocol.ExpectPending(20ms);
                EXPECT_EQ(protocol.WaitRequests(2).size(), 2u);
            }
            if (stage == 1)
                protocol.Notify(request);
            else
                protocol.Reply(request);
        }
    }
    protocol.Finish();
    ExpectPrimaryResult(true);
    protocol.ExpectRoute(target);
    ExpectPrimaryIdle();
    ExpectPrimary(target);
    protocol.ExpectNotifications(true, 1, false);
}
TEST_F(RealSimPrimary, BRealTimeoutKeepsPreviousPrimary)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    PrimaryProtocol protocol(*this);
    protocol.Configure(PrimaryPath::Direct);
    StartPrimary(target);
    protocol.SetPreviousPrimary(0);
    const auto start = std::chrono::steady_clock::now();
    protocol.Begin(target);
    protocol.WaitRequests(1); // Accepted, intentionally no reply.
    protocol.Finish(50s);
    ExpectPrimaryResult(false);
    EXPECT_GE(std::chrono::steady_clock::now() - start, 44s);
    ExpectPrimaryIdle();
    protocol.ExpectPreviousPrimary(0);
    protocol.ExpectNotifications(false, 1, true);
}
TEST_F(RealSimPrimary, C1CheckFailureRestoresParametersAndExhaustsFiveRetries)
{
    auto target = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    PrimaryProtocol protocol(*this);
    protocol.Configure(PrimaryPath::ThreePhase, 24);
    protocol.AutoReply(
        [](const PrimaryRequest &request)
        {
            EXPECT_EQ(request.modem, request.slot);
            EXPECT_TRUE(request.phase == 1 || request.phase == 4);
            return request.phase == 1 ? PrimaryReply::GenericError : PrimaryReply::Success;
        });
    StartPrimary(target);
    protocol.SetPreviousPrimary(0, "previous-parameter-value");
    SelectPrimary(target); // Same synchronous public return and inline device replies as the original.
    ExpectPrimaryResult(false);
    protocol.ExpectPreviousPrimary(0, "previous-parameter-value");
    auto requests = protocol.WaitRequests(24, 12s);
    EXPECT_EQ(requests.size(), 24u);
    std::map<int, std::vector<std::pair<int, int>>> phases;
    for (const auto &request : requests)
        phases[request.session].emplace_back(request.slot, request.phase);
    EXPECT_EQ(phases.size(), 6u);
    for (const auto &[session, sequence] : phases)
    {
        const std::vector<std::pair<int, int>> expected{{0, 1}, {1, 1}, {0, 4}, {1, 4}};
        EXPECT_EQ(sequence, expected);
    }
    ExpectPrimaryIdle();
    protocol.ExpectPreviousPrimary(0, "previous-parameter-value");
    protocol.ExpectNotifications(false, 6, false);
}
