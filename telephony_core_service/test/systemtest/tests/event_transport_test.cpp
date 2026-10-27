#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include "ffrt_runtime.h"
#include "tel_event_handler.h"

namespace {
using OHOS::AppExecFwk::InnerEvent;
using OHOS::Telephony::TelEventHandler;
using namespace std::chrono_literals;

class Completion {
public:
    void Signal()
    {
        std::lock_guard<ffrt::mutex> guard(lock_);
        done_ = true;
        changed_.notify_all();
    }
    bool Await()
    {
        std::unique_lock<ffrt::mutex> guard(lock_);
        return changed_.wait_for(guard, 2s, [&] { return done_; });
    }
private:
    ffrt::mutex lock_;
    ffrt::condition_variable changed_;
    bool done_ = false;
};
class SchedulerRetirement {
public:
    ~SchedulerRetirement()
    {
        if (!SimHost::AwaitSchedulerStopped(2s)) {
            // Stop this process before destroying referenced observers or running another case.
            std::fputs("External scheduler cleanup failed; subsequent cases blocked\n", stderr);
            std::quick_exit(2);
        }
    }
};
class EventObserver {
public:
    MOCK_METHOD(void, Receive, (uint32_t, bool));
};
class RecordingHandler : public TelEventHandler {
public:
    RecordingHandler(EventObserver &observer, Completion &completed)
        : TelEventHandler("synthetic-protocol"), observer_(observer), completed_(completed) {}
    void ProcessEvent(const InnerEvent::Pointer &event) override
    {
        auto payload = event->GetSharedObject<int>();
        observer_.Receive(event->GetInnerEventId(), payload && *payload == 17);
        completed_.Signal();
    }
private:
    EventObserver &observer_;
    Completion &completed_;
};
}

TEST(ProductionEventTransport, DeliversFormalEnvelope)
{
    RecordProperty("case_id", "INFRA-EVENT-004");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "EVT-PRODUCTION");
    SimHost::ResetSchedulerEvidence();
    testing::StrictMock<EventObserver> observer;
    Completion completed;
    SchedulerRetirement retirement;
    EXPECT_CALL(observer, Receive(23, true)).Times(testing::AtLeast(1));
    auto handler = std::make_shared<RecordingHandler>(observer, completed);
    auto event = InnerEvent::Get(23, std::make_shared<int>(17));
    const bool accepted = handler->SendEvent(event);
    EXPECT_TRUE(accepted) << "EVT-PRODUCTION";
    const bool converged = accepted && completed.Await();
    EXPECT_TRUE(converged) << "EVT-PRODUCTION";
    RecordProperty("converged", converged ? "true" : "false");
    handler.reset();
    // The observer outlives worker retirement, including owner destruction on the worker itself.
    const bool cleaned = SimHost::AwaitSchedulerStopped(2s);
    EXPECT_TRUE(cleaned) << "EVT-PRODUCTION";
    EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u) << "EVT-PRODUCTION";
    RecordProperty("cleanup_completed", cleaned ? "true" : "false");
}

TEST(ProductionEventTransport, CancelsPendingAndReapsWorker)
{
    RecordProperty("case_id", "INFRA-EVENT-005");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "EVT-CANCEL");
    SimHost::ResetSchedulerEvidence();
    testing::StrictMock<EventObserver> observer;
    Completion completed;
    SchedulerRetirement retirement;
    EXPECT_CALL(observer, Receive(testing::_, testing::_)).Times(0);
    {
        auto handler = std::make_shared<RecordingHandler>(observer, completed);
        auto event = InnerEvent::Get(23, std::make_shared<int>(17));
        EXPECT_TRUE(handler->SendEvent(event, 60000)) << "EVT-CANCEL";
        handler->RemoveAllEvents();
    }
    const bool cleaned = SimHost::AwaitSchedulerStopped(2s);
    EXPECT_TRUE(cleaned) << "EVT-CANCEL";
    EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u) << "EVT-CANCEL";
    RecordProperty("cleanup_completed", cleaned ? "true" : "false");
}

TEST(SchedulerBoundary, RunningTaskCannotBeCancelled)
{
    RecordProperty("case_id", "INFRA-FFRT-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "FFRT-CANCEL-RUNNING");
    SimHost::ResetSchedulerEvidence();
    Completion started;
    Completion release;
    std::atomic<bool> released{false};
    SchedulerRetirement retirement;
    {
        ffrt::queue queue("synthetic-gated-task");
        auto task = queue.submit_h([&] {
            started.Signal();
            released = release.Await();
        });
        const bool running = started.Await();
        EXPECT_TRUE(running) << "FFRT-CANCEL-RUNNING";
        if (running) EXPECT_EQ(queue.cancel(task), 1) << "FFRT-CANCEL-RUNNING";
        release.Signal(); // Always release before wait/destruction, also on assertion failure.
        queue.wait(task);
        EXPECT_TRUE(released.load()) << "FFRT-CANCEL-RUNNING";
    }
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(2s)) << "FFRT-CANCEL-RUNNING";
    EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u) << "FFRT-CANCEL-RUNNING";
}

TEST(SchedulerBoundary, ProductionGeneralTaskWaitsAndCleansUp)
{
    RecordProperty("case_id", "INFRA-FFRT-002");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "FFRT-GENERAL");
    SimHost::ResetSchedulerEvidence();
    std::atomic<bool> completed{false};
    SchedulerRetirement retirement;
    OHOS::Telephony::TelFFRTUtils::SubmitSync([&completed] { completed = true; });
    EXPECT_TRUE(completed.load()) << "FFRT-GENERAL";
    SimHost::StopCallingThreadTasks();
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(2s)) << "FFRT-GENERAL";
    EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, 0u) << "FFRT-GENERAL";
}
