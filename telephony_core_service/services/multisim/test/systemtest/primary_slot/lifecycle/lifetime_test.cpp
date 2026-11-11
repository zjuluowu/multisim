#include "real_scenario.h"
#include "external_runner.h"
#include "owned_card_graph.h"
#include <future>
#include <atomic>

namespace {
using namespace Sim028Host;
using namespace std::chrono_literals;

class RunnerLifetime : public testing::Test {
protected:
    std::shared_ptr<AppExecFwk::EventHandler> handler = std::make_shared<AppExecFwk::EventHandler>();
    void SetUp() override { ASSERT_TRUE(StartExternalRunner()); }
    void TearDown() override
    {
        EXPECT_TRUE(StopExternalRunner());
        handler.reset();
        EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
        EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u);
    }
};
TEST_F(RunnerLifetime, NormalCompletionAndRepeatedStop)
{
    std::promise<void> completed;
    ASSERT_TRUE(handler->PostTask([&] { completed.set_value(); }, "complete", 0));
    ASSERT_EQ(completed.get_future().wait_for(3s), std::future_status::ready);
    EXPECT_TRUE(StopExternalRunner());
    EXPECT_TRUE(StopExternalRunner());
    EXPECT_EQ(ExternalRunnerState(), RunnerState::Stopped);
    EXPECT_FALSE(handler->PostTask([] { ADD_FAILURE() << "LATE_CALLBACK"; }, "late", 0));
}
TEST_F(RunnerLifetime, PendingTaskCancelledAndCapturedOwnerReleased)
{
    auto owner = std::make_shared<int>(7);
    std::weak_ptr<int> retiring = owner;
    std::atomic<unsigned> calls{0};
    ASSERT_TRUE(handler->PostTask([owner, &calls] { ++calls; }, "pending", 180000));
    owner.reset();
    EXPECT_FALSE(retiring.expired());
    EXPECT_TRUE(StopExternalRunner());
    EXPECT_TRUE(retiring.expired());
    EXPECT_EQ(calls, 0u);
    EXPECT_FALSE(handler->PostTask([&] { ++calls; }, "late", 0));
}
TEST_F(RunnerLifetime, InFlightStopJoinsAndRejectsCallbackRepost)
{
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    std::atomic<bool> repostAccepted{true};
    ASSERT_TRUE(handler->PostTask([&, gate] {
        entered.set_value(); gate.wait();
        repostAccepted = handler->PostTask([] { ADD_FAILURE() << "POST_DURING_STOP"; }, "repost", 0);
    }, "in-flight", 0));
    // Release the callback even when a check fails: no assertion leaves a worker blocked.
    auto enteredStatus = entered.get_future().wait_for(3s);
    EXPECT_EQ(enteredStatus, std::future_status::ready);
    auto stopping = std::async(std::launch::async, [] { return StopExternalRunner(); });
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (ExternalRunnerState() != RunnerState::Stopping && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_EQ(ExternalRunnerState(), RunnerState::Stopping);
    EXPECT_EQ(stopping.wait_for(0s), std::future_status::timeout);
    EXPECT_FALSE(StartExternalRunner());
    EXPECT_FALSE(handler->PostTask([] { ADD_FAILURE(); }, "rejected", 0));
    release.set_value();
    EXPECT_TRUE(stopping.get());
    EXPECT_FALSE(repostAccepted);
}
TEST_F(RunnerLifetime, ConcurrentStopIsIdempotent)
{
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    ASSERT_TRUE(handler->PostTask([&, gate] { entered.set_value(); gate.wait(); }, "in-flight", 0));
    EXPECT_EQ(entered.get_future().wait_for(3s), std::future_status::ready);
    auto first = std::async(std::launch::async, [] { return StopExternalRunner(); });
    auto second = std::async(std::launch::async, [] { return StopExternalRunner(); });
    release.set_value();
    EXPECT_TRUE(first.get()); EXPECT_TRUE(second.get());
    EXPECT_EQ(ExternalRunnerState(), RunnerState::Stopped);
}
TEST_F(RunnerLifetime, CallbackCannotSynchronouslyJoinItself)
{
    std::promise<bool> result;
    ASSERT_TRUE(handler->PostTask([&] { result.set_value(StopExternalRunner()); }, "self-stop", 0));
    auto future = result.get_future();
    ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
    EXPECT_FALSE(future.get());
    EXPECT_EQ(ExternalRunnerState(), RunnerState::Running);
}

TEST(RealCardLifetime, MissingRilInitializationReleasesState)
{
    auto state = std::make_shared<SimStateManager>(nullptr);
    std::weak_ptr<SimStateManager> retiring = state;
    state->Init(0); // real initialization exits before allocating its handler
    state.reset();
    EXPECT_TRUE(retiring.expired());
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
}

TEST(RealCardLifetime, InitializedStateUnregistersRilAndReleases)
{
    RealScenario environment;
    auto state = std::make_shared<SimStateManager>(environment.ril);
    std::weak_ptr<SimStateManager> retiring = state;
    state->Init(0);
    state.reset();
    EXPECT_TRUE(retiring.expired());
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
    EXPECT_EQ(environment.ril.use_count(), 1);
    std::lock_guard<std::mutex> lock(environment.mutex);
    EXPECT_TRUE(environment.notifications.empty());
}

TEST(RealCardLifetime, PublicDetachReleasesRealStateFileAndRil)
{
    RealScenario environment;
    OwnedCardGraph graph(environment.ril);
    ASSERT_NE(graph.records, nullptr);
    std::weak_ptr<SimStateManager> state = graph.state;
    std::weak_ptr<SimFileManager> files = graph.files;
    std::weak_ptr<IccFile> records = graph.records;
    std::weak_ptr<IccFileController> controller = graph.files->GetIccFileController();
    graph.Stop(); graph.Stop();
    EXPECT_TRUE(state.expired()); EXPECT_TRUE(files.expired());
    EXPECT_TRUE(records.expired()); EXPECT_TRUE(controller.expired());
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
    EXPECT_EQ(environment.ril.use_count(), 1);
    {
        std::lock_guard<std::mutex> lock(environment.mutex);
        EXPECT_TRUE(environment.notifications.empty());
    }
    testing::Test::RecordProperty("real_card_lifetime", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
} // namespace
