#pragma once
#include "real_scenario.h"
#include <type_traits>
#include <set>
namespace Sim028Host {
// Future production lifecycle API, never a fake implementation. Missing methods
// are an explicit failing/BLOCKED contract, not a skipped or passing test.
template<class T, class = void> struct HasShutdown : std::false_type {};
template<class T> struct HasShutdown<T, std::void_t<
    decltype(std::declval<T &>().Stop()), decltype(std::declval<const T &>().IsStopping()),
    decltype(std::declval<const T &>().IsStopped())>> : std::bool_constant<
        std::is_same_v<decltype(std::declval<T &>().Stop()), bool>> {};
struct QueueBaseline {
    std::set<uintptr_t> addresses;
    unsigned failures;
    QueueBaseline() : failures(SimHost::GetSchedulerEvidence().taskFailures)
    { for (const auto &q : SimHost::GetQueueEvidence()) addresses.insert(q.address); }
    void ExpectNoOwnedWork() const
    {
        for (const auto &q : SimHost::GetQueueEvidence())
            EXPECT_TRUE(addresses.count(q.address)) << "OWNED_QUEUE_REMAINS " << q.name
                << " pending=" << q.pending << " running=" << q.running;
        EXPECT_EQ(SimHost::GetSchedulerEvidence().taskFailures, failures);
    }
};
template<class Body> void ShutdownContract(Body body)
{
    if constexpr (HasShutdown<OHOS::Telephony::SimManager>::value) {
        body(std::true_type{});
    } else {
        testing::Test::RecordProperty("business", "NOT_RUN");
        testing::Test::RecordProperty("resource_lifetime", "BLOCKED");
        testing::Test::RecordProperty("async_safety", "BLOCKED");
        ADD_FAILURE() << "BLOCKED_PRODUCTION_STOP_API: requires bool SimManager::Stop(), "
            "IsStopping() const and IsStopped() const; no mock shutdown substituted";
    }
}
template<class Manager> void VerifyStopped(RealScenario &s, const Manager &manager, const QueueBaseline &baseline)
{
    EXPECT_TRUE(manager->IsStopped());
    EXPECT_EQ(s.ril.use_count(), 1) << "PRODUCTION_RIL_REFERENCES_BEFORE_ROOT_RELEASE";
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        EXPECT_TRUE(s.notifications.empty()) << "RIL_SUBSCRIPTIONS_BEFORE_ROOT_RELEASE";
        for (const auto &r : s.registrationLedger) {
            EXPECT_FALSE(r.active) << "UNPAIRED_RIL_EVENT slot=" << r.slot << " event=" << r.event;
            EXPECT_TRUE(r.owner.expired()) << "RIL_OBSERVER_ALIVE slot=" << r.slot << " event=" << r.event;
        }
    }
    if (s.accountObserver) EXPECT_EQ(s.accountObserver.use_count(), 1);
    baseline.ExpectNoOwnedWork();
    EXPECT_FALSE(manager->IsSetPrimarySlotIdInProgress());
    auto observer = std::make_shared<AccountObserver>([] { ADD_FAILURE() << "CALLBACK_AFTER_STOP"; });
    std::weak_ptr<AccountObserver> weak = observer;
    manager->RegisterCoreNotify(1, observer, RadioEvent::RADIO_SIM_ACCOUNT_LOADED);
    observer.reset();
    EXPECT_TRUE(weak.expired()) << "REGISTRATION_AFTER_STOP";
    testing::Test::RecordProperty("resource_lifetime", testing::Test::HasFailure() ? "FAIL" : "PASS");
    testing::Test::RecordProperty("async_safety", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
}
