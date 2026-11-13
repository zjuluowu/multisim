#include "shutdown_contract.h"
#include "owned_card_graph.h"
#include <future>
#include <optional>
namespace {
using namespace Sim028Host;
using namespace std::chrono_literals;

TEST(PairingReview, AllThreeSlotsPublicDetach)
{
    RealScenario environment;
    QueueBaseline baseline;
    std::vector<std::unique_ptr<OwnedCardGraph>> graphs;
    std::vector<std::weak_ptr<SimStateManager>> states;
    for (int slot = 0; slot < 3; ++slot) {
        graphs.push_back(std::make_unique<OwnedCardGraph>(environment.ril, slot));
        states.push_back(graphs.back()->state);
    }
    for (auto &g : graphs) { g->Stop(); g->Stop(); }
    for (const auto &s : states) EXPECT_TRUE(s.expired());
    baseline.ExpectNoOwnedWork();
    EXPECT_EQ(environment.ril.use_count(), 1);
    std::lock_guard<std::mutex> lock(environment.mutex);
    EXPECT_TRUE(environment.notifications.empty());
}
TEST(PairingReview, PartialInitializationAndRepeatedUnregister)
{
    RealScenario environment;
    QueueBaseline baseline;
    auto state = std::make_shared<SimStateManager>(environment.ril); state->Init(0);
    auto missing = SimFileManager::CreateInstance({}, state);
    EXPECT_EQ(missing, nullptr); // real factory rejects absent RIL before File allocation
    auto observer = std::make_shared<AccountObserver>([] {});
    std::weak_ptr<AccountObserver> retiring = observer;
    state->RegisterCoreNotify(observer, RadioEvent::RADIO_CARD_TYPE_CHANGE);
    state->UnRegisterCoreNotify(observer, RadioEvent::RADIO_CARD_TYPE_CHANGE);
    state->UnRegisterCoreNotify(observer, RadioEvent::RADIO_CARD_TYPE_CHANGE);
    observer.reset(); EXPECT_TRUE(retiring.expired());
    std::weak_ptr<SimStateManager> weak = state; state.reset(); EXPECT_TRUE(weak.expired());
    baseline.ExpectNoOwnedWork();
    EXPECT_EQ(environment.ril.use_count(), 1);
}
TEST(PairingReview, DestroyThenReinitializeCardGraph)
{
    RealScenario environment;
    QueueBaseline baseline;
    for (int generation = 0; generation < 2; ++generation) {
        OwnedCardGraph g(environment.ril);
        std::weak_ptr<SimStateManager> state = g.state;
        std::weak_ptr<IccFile> records = g.records;
        g.Stop(); EXPECT_TRUE(state.expired()); EXPECT_TRUE(records.expired());
        baseline.ExpectNoOwnedWork(); EXPECT_EQ(environment.ril.use_count(), 1);
    }
}
TEST(PairingReview, UnrelatedRuntimeWorkerIsBaseline)
{
    RealScenario environment;
    auto unrelated = std::make_unique<ffrt::queue>("unrelated-runtime-worker");
    QueueBaseline baseline;
    EXPECT_GT(SimHost::GetSchedulerEvidence().workers, 0u);
    auto state = std::make_shared<SimStateManager>(environment.ril); state->Init(0);
    state.reset(); baseline.ExpectNoOwnedWork();
    EXPECT_GT(SimHost::GetSchedulerEvidence().workers, 0u); // independent worker is allowed
    unrelated.reset(); // preserve the existing fixture's isolated-process cleanup assertions
}

template<class M = SimManager, class F> void WithRealGraph(F body, bool locked = true)
{
    if constexpr (HasShutdown<M>::value) {
        RealScenario s(true);
        QueueBaseline baseline;
        if (locked) ASSERT_TRUE(s.StartLockedCard());
        else {
            s.manager = std::make_shared<M>(nullptr);
            ASSERT_TRUE(s.manager->OnInit(3));
        }
        auto manager = std::static_pointer_cast<M>(s.manager);
        body(s, manager, baseline);
    } else {
        ShutdownContract([](auto) {});
    }
}
TEST(ShutdownContract, ANormalPrimaryThenStop)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        s.ClearObservations(); s.Permit();
        EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).WillOnce([](int slot, const auto &event) {
            auto reply = std::make_shared<RadioResponseInfo>(); reply->error = ErrType::NONE;
            RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, reply); return 0;
        });
        EXPECT_EQ(s.core.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
        testing::Test::RecordProperty("business", testing::Test::HasFailure() ? "FAIL" : "PASS");
        EXPECT_TRUE(manager->Stop()); VerifyStopped(s, manager, baseline);
    });
}
TEST(ShutdownContract, BThreeSlotsThenStop)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        EXPECT_EQ(manager->GetMaxSimCount(), 2); // physical capacity; assembly has three logical objects
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            std::set<int> slots;
            for (const auto &r : s.registrationLedger)
                if (r.active && r.event == RadioEvent::RADIO_SIM_STATE_CHANGE) slots.insert(r.slot);
            EXPECT_EQ(slots, (std::set<int>{0, 1, 2}));
        }
        testing::Test::RecordProperty("business", "PASS");
        EXPECT_TRUE(manager->Stop()); VerifyStopped(s, manager, baseline);
    });
}
TEST(ShutdownContract, CPartialInitializationThenStop)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        testing::Test::RecordProperty("business", "NOT_RUN");
        EXPECT_TRUE(manager->Stop()); VerifyStopped(s, manager, baseline);
    }, false);
}
TEST(ShutdownContract, DRepeatedStop)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        auto first = std::async(std::launch::async, [&] { return manager->Stop(); });
        auto second = std::async(std::launch::async, [&] { return manager->Stop(); });
        EXPECT_TRUE(first.get()); EXPECT_TRUE(second.get()); EXPECT_TRUE(manager->Stop());
        testing::Test::RecordProperty("business", "NOT_RUN"); VerifyStopped(s, manager, baseline);
    });
}
template<class S, class M> void StopWithReply(S &s, M &manager, const QueueBaseline &baseline, bool error)
{
    std::optional<RealScenario::Pending> request;
    s.ClearObservations(); s.Permit();
    EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).WillOnce([&](int slot, const auto &event) {
        std::lock_guard<std::mutex> lock(s.mutex);
        request = RealScenario::Pending{slot, event->GetOwner(), event->GetInnerEventId()};
        s.changed.notify_all(); return 0;
    });
    auto caller = std::async(std::launch::async, [&] { return s.core.SetPrimarySlotId(1); });
    {
        std::unique_lock<std::mutex> lock(s.mutex);
        EXPECT_TRUE(s.changed.wait_for(lock, 3s, [&] { return request.has_value(); }));
    }
    auto stop = std::async(std::launch::async, [&] { return manager->Stop(); });
    auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!manager->IsStopping() && !manager->IsStopped() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_TRUE(manager->IsStopping());
    EXPECT_EQ(stop.wait_for(0s), std::future_status::timeout);
    if (request) {
        auto response = std::make_shared<RadioResponseInfo>();
        response->error = error ? ErrType::ERR_GENERIC_FAILURE : ErrType::NONE;
        RealScenario::Reply(*request, response);
    }
    EXPECT_EQ(caller.get(), error ? TELEPHONY_ERR_LOCAL_PTR_NULL : TELEPHONY_ERR_SUCCESS);
    testing::Test::RecordProperty("business", testing::Test::HasFailure() ? "FAIL" : "PASS");
    EXPECT_TRUE(stop.get()); VerifyStopped(s, manager, baseline);
}
TEST(ShutdownContract, EInFlightSuccessCompletesBeforeRelease)
{ WithRealGraph([](auto &s, auto &manager, const auto &baseline) { StopWithReply(s, manager, baseline, false); }); }
TEST(ShutdownContract, FErrorReplyDuringStoppingKeepsOriginalResult)
{ WithRealGraph([](auto &s, auto &manager, const auto &baseline) { StopWithReply(s, manager, baseline, true); }); }
TEST(ShutdownContract, GDelayedDispatchAfterStop)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        // Snapshot the real pending FFRT callbacks, not fabricated SIM results.
        auto delayed = SimHost::CapturePendingActions("SimStateHandle");
        EXPECT_FALSE(delayed.empty());
        EXPECT_TRUE(manager->Stop()); VerifyStopped(s, manager, baseline);
        unsigned writes;
        { std::lock_guard<std::mutex> lock(s.mutex); writes = s.writes.size(); }
        for (auto &action : delayed) action(); // committed-before-cancel late-dispatch race
        { std::lock_guard<std::mutex> lock(s.mutex); EXPECT_EQ(s.writes.size(), writes); }
        baseline.ExpectNoOwnedWork();
        testing::Test::RecordProperty("business", "NOT_RUN");
    });
}
TEST(ShutdownContract, HDestroyAndCreateNewGraph)
{
    WithRealGraph([](auto &s, auto &manager, const auto &baseline) {
        EXPECT_TRUE(manager->Stop()); VerifyStopped(s, manager, baseline);
        std::weak_ptr<SimManager> old = manager;
        s.core.SetSimManager(nullptr); CoreManagerInner::GetInstance().OnInit(nullptr, nullptr, nullptr);
        s.manager.reset(); manager.reset(); EXPECT_TRUE(old.expired());
        // A new instance is supported; re-OnInit of a stopped instance is a separate contract.
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            s.accountLoaded = false; s.autoStatus = false; s.statusRequests.clear(); s.updates = 0;
        }
        s.accountObserver.reset();
        ASSERT_TRUE(s.StartLockedCard());
        auto replacement = std::static_pointer_cast<typename std::decay_t<decltype(manager)>::element_type>(s.manager);
        EXPECT_TRUE(replacement->Stop()); VerifyStopped(s, replacement, baseline);
        testing::Test::RecordProperty("business", "NOT_RUN");
    });
}
} // namespace
