#include "internal/common/lifecycle_fixture.h"
#include <future>
#include <optional>
namespace {
using namespace Sim028Host;
using namespace std::chrono_literals;

TEST(PairingReview, AllThreeSlotsPublicDetach)
{
    PrimaryEnvironment environment;
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
TEST(PairingReview, IccidObserverUnregisterReleasesSubscriber)
{
    PrimaryEnvironment environment;
    QueueBaseline baseline;
    auto state = std::make_shared<SimStateManager>(environment.ril);
    state->Init(0);
    auto observer = std::make_shared<AccountObserver>([] {});
    std::weak_ptr<AccountObserver> retiring = observer;
    state->RegisterCoreNotify(observer, RadioEvent::RADIO_SIM_ICCID_LOADED);
    state->UnRegisterCoreNotify(observer, RadioEvent::RADIO_SIM_ICCID_LOADED);
    state->UnRegisterCoreNotify(observer, RadioEvent::RADIO_SIM_ICCID_LOADED);
    observer.reset();
    EXPECT_TRUE(retiring.expired());
    state.reset();
    baseline.ExpectNoOwnedWork();
    EXPECT_EQ(environment.ril.use_count(), 1);
}

TEST(PairingReview, PartialInitializationAndRepeatedUnregister)
{
    PrimaryEnvironment environment;
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
    PrimaryEnvironment environment;
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
    PrimaryEnvironment environment;
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
        WatchBoundaryScope watchers;
        PrimaryEnvironment s(true);
        // FFRT's caller executor is runtime-owned; create it before measuring
        // component queues. Stop is still required to drain all SIM tasks.
        ffrt::submit([] {}); ffrt::wait();
        QueueBaseline baseline;
        if (locked) ASSERT_TRUE(s.StartLockedCard());
        else {
            s.manager = std::make_shared<M>(nullptr);
            ASSERT_TRUE(s.manager->OnInit(3));
            s.lifetimeSamples=TelLifetimeEvidence::Live();
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
            PrimaryEnvironment::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, reply); return 0;
        });
        EXPECT_EQ(s.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
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
        EXPECT_EQ(manager->SetPrimarySlotId(1,true),TELEPHONY_ERR_NO_SIM_CARD);
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
    std::optional<PrimaryEnvironment::Pending> request;
    s.ClearObservations(); s.Permit();
    EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).WillOnce([&](int slot, const auto &event) {
        std::lock_guard<std::mutex> lock(s.mutex);
        request = PrimaryEnvironment::Pending{slot, event->GetOwner(), event->GetInnerEventId()};
        s.changed.notify_all(); return 0;
    });
    auto caller = std::async(std::launch::async, [&] { return s.SetPrimarySlotId(1); });
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
        PrimaryEnvironment::Reply(*request, response);
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
        EXPECT_EQ(delayed.size(),3u);
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

TEST(ShutdownExtra, WatcherLateCallbackAfterOwnerDestruction)
{
    std::vector<WatchedCallback> callbacks;
    {
        WatchBoundaryScope watchers;
        PrimaryEnvironment s(true); ffrt::submit([]{});ffrt::wait();QueueBaseline baseline;
        ASSERT_TRUE(s.StartLockedCard());callbacks=WatchCallbacks();ASSERT_EQ(callbacks.size(),5u);
        for(auto &callback:callbacks)callback.invoke(); // real registered callback while Running
        EXPECT_TRUE(s.manager->Stop());VerifyStopped(s,s.manager,baseline);
        EXPECT_EQ(ActiveWatchers(),0u);
        for(auto &callback:callbacks)callback.invoke(); // closed context while root still alive
    }
    for(auto &callback:callbacks)callback.invoke(); // stale service copy after all actors destroyed
}
TEST(ShutdownExtra, PartialWatcherRegistration)
{
    WatchBoundaryScope watchers;FailWatcherRegistration(1);
    PrimaryEnvironment s(true);ffrt::submit([]{});ffrt::wait();QueueBaseline baseline;
    ASSERT_TRUE(s.StartLockedCard());ASSERT_EQ(ActiveWatchers(),4u);
    EXPECT_TRUE(s.manager->Stop());VerifyStopped(s,s.manager,baseline);
}
TEST(ShutdownExtra, FailedWatcherRemovalKeepsStoppingAndRetries)
{
    WatchBoundaryScope watchers;
    PrimaryEnvironment s(true);ffrt::submit([]{});ffrt::wait();QueueBaseline baseline;
    ASSERT_TRUE(s.StartLockedCard());auto callbacks=WatchCallbacks();ASSERT_EQ(callbacks.size(),5u);
    FailWatcherRemoval(true);EXPECT_FALSE(s.manager->Stop());
    EXPECT_TRUE(s.manager->IsStopping());EXPECT_FALSE(s.manager->IsStopped());EXPECT_EQ(ActiveWatchers(),5u);
    unsigned writes;{std::lock_guard<std::mutex> lock(s.mutex);writes=s.writes.size();}
    for(auto &callback:callbacks)callback.invoke(); // service still registered, actor access is fenced
    {std::lock_guard<std::mutex> lock(s.mutex);EXPECT_EQ(s.writes.size(),writes);}
    FailWatcherRemoval(false);EXPECT_TRUE(s.manager->Stop());VerifyStopped(s,s.manager,baseline);
}
TEST(ShutdownExtra, ObserverReleaseOutsideLockAllowsReentrantDestruction)
{
    ObserverHandler observers;
    struct Reentrant final:TelEventHandler {
        ObserverHandler &owner;bool &destroyed;
        Reentrant(ObserverHandler &o,bool &d):TelEventHandler("reentrant-subscriber"),owner(o),destroyed(d){}
        ~Reentrant(){owner.RemoveAll();destroyed=true;}
    };
    bool destroyed=false;auto subscriber=std::make_shared<Reentrant>(observers,destroyed);
    observers.RegObserver(17,subscriber);std::weak_ptr<Reentrant> weak=subscriber;
    subscriber.reset();observers.RemoveAll();EXPECT_TRUE(destroyed);EXPECT_TRUE(weak.expired());
}
void StopC1AtPhase(unsigned stopStage)
{
    WatchBoundaryScope watchers;PrimaryEnvironment s;
    ffrt::submit([]{});ffrt::wait();QueueBaseline baseline;
    ASSERT_TRUE(s.StartLockedCard());s.ClearObservations();s.Permit();
    EXPECT_CALL(*s.ril,SetPrimarySlot(testing::_,testing::_)).Times(0);
    EXPECT_CALL(*s.ril,GetRadioProtocol(testing::_,testing::_)).Times(2).WillRepeatedly([](int slot,const auto &event){
        auto value=std::make_shared<RadioProtocol>();value->slotId=slot;value->modemId=slot;value->technology=slot?0x22:0x11;
        PrimaryEnvironment::Reply({slot,event->GetOwner(),event->GetInnerEventId()},value);return 0;
    });
    s.manager->GetRadioProtocol(0);s.manager->GetRadioProtocol(1);
    auto deadline=std::chrono::steady_clock::now()+3s;
    while(s.manager->GetRadioProtocolSlotIdByModemId(1)!=1 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    ASSERT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(1),1);
    struct Request {PrimaryEnvironment::Pending transport;RadioProtocol value;};std::vector<Request> requests;
    EXPECT_CALL(*s.ril,SetRadioProtocol(testing::_,testing::_,testing::_)).Times(6).WillRepeatedly([&](int slot,RadioProtocol value,const auto &event){
        EXPECT_EQ(value.slotId,slot);EXPECT_TRUE(std::dynamic_pointer_cast<RadioProtocolController>(event->GetOwner()));
        std::lock_guard<std::mutex> lock(s.mutex);requests.push_back({{slot,event->GetOwner(),event->GetInnerEventId()},value});s.changed.notify_all();return 0;
    });
    auto caller=std::async(std::launch::async,[&]{return s.core.SetPrimarySlotId(1);});std::future<bool> stopper;int session=-1;
    for(unsigned stage=0;stage<3;++stage){
        std::vector<Request> pair;
        {std::unique_lock<std::mutex> lock(s.mutex);
         ASSERT_TRUE(s.changed.wait_for(lock,3s,[&]{return requests.size()>=(stage+1)*2;}));
         pair.assign(requests.begin()+stage*2,requests.begin()+stage*2+2);
         EXPECT_EQ(caller.wait_for(0s),std::future_status::timeout);
         // C1 saves tentative parameters before CHECK and restores them on failure.
         // A successful MAIN/DATA broadcast must still wait for COMPLETE.
         for(const auto &want:s.broadcasts)
             EXPECT_NE(want.GetAction(),EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_MAIN_SUBSCRIPTION_CHANGED);}
        if(stage==stopStage){
            stopper=std::async(std::launch::async,[&]{return s.manager->Stop();});auto limit=std::chrono::steady_clock::now()+3s;
            while(!s.manager->IsStopping()&&!s.manager->IsStopped()&&std::chrono::steady_clock::now()<limit)std::this_thread::yield();
            ASSERT_TRUE(s.manager->IsStopping());EXPECT_EQ(stopper.wait_for(0s),std::future_status::timeout);
            EXPECT_EQ(s.manager->SetPrimarySlotId(0,true),TELEPHONY_ERR_LOCAL_PTR_NULL)<<"NEW_REQUEST_REJECTED_WHILE_STOPPING";
        }
        for(auto &request:pair){
            auto reply=std::make_shared<RadioProtocol>(request.value);
            if(session<0)session=reply->sessionId;EXPECT_EQ(reply->sessionId,session);
            EXPECT_EQ(static_cast<int>(reply->phase),stage==0?1:stage==1?2:4);
            EXPECT_EQ(reply->modemId,stage==0?request.transport.slot:1-request.transport.slot);
            std::cout<<"STOP_C1 stage="<<stage<<" session="<<session<<" slot="<<request.transport.slot<<"\n";
            PrimaryEnvironment::Reply(request.transport,reply);
            if(stage==1){
                reply=std::make_shared<RadioProtocol>(*reply);reply->phase=RadioProtocolPhase::RADIO_PROTOCOL_PHASE_NOTIFY;
                reply->status=RadioProtocolStatus::RADIO_PROTOCOL_STATUS_SUCCESS;
                std::weak_ptr<AppExecFwk::EventHandler> owner;
                {std::lock_guard<std::mutex> lock(s.mutex);owner=s.notifications.at({request.transport.slot,RADIO_SIM_RADIO_PROTOCOL_NOTIFY});}
                PrimaryEnvironment::Reply({request.transport.slot,owner,RADIO_SIM_RADIO_PROTOCOL_NOTIFY},reply);
            }
        }
    }
    ASSERT_EQ(caller.wait_for(3s),std::future_status::ready);EXPECT_EQ(caller.get(),TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(stopper.wait_for(3s),std::future_status::ready);EXPECT_TRUE(stopper.get());
    {std::lock_guard<std::mutex> lock(s.mutex);EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"],"1");s.ExpectNotifications(true,1,false);}
    testing::Test::RecordProperty("business","PASS");VerifyStopped(s,s.manager,baseline);
}
TEST(ShutdownExtra,C1StopAtCheck){StopC1AtPhase(0);}
TEST(ShutdownExtra,C1StopAtUpdate){StopC1AtPhase(1);}
TEST(ShutdownExtra,C1StopAtComplete){StopC1AtPhase(2);}

TEST(ShutdownExtra, WatcherInFlightIsDrainedAndSelfStopIsRejected)
{
    WatchBoundaryScope watchers;PrimaryEnvironment s(true);ffrt::submit([]{});ffrt::wait();QueueBaseline baseline;
    std::promise<void> entered,release;auto released=release.get_future().share();std::atomic<bool> intercept{false};
    // Configure all mock behavior before any production threads start. Only an
    // atomic latch changes during execution; no concurrent EXPECT_CALL mutation.
    s.beforeParameterRead=[&](const char *key){
        if(std::string(key)!="persist.ril.reboot_detect_sim0" || !intercept.exchange(false))return;
        EXPECT_FALSE(s.manager->Stop())<<"CALLBACK_MUST_NOT_WAIT_FOR_ITSELF";
        EXPECT_FALSE(s.manager->IsStopping());entered.set_value();released.wait();
    };
    ASSERT_TRUE(s.StartLockedCard());auto callbacks=WatchCallbacks();
    auto it=std::find_if(callbacks.begin(),callbacks.end(),[](const auto &c){return c.key=="persist.ril.reboot_detect_sim0";});ASSERT_NE(it,callbacks.end());
    intercept=true;
    auto callback=std::async(std::launch::async,[&]{it->invoke();});
    ASSERT_EQ(entered.get_future().wait_for(3s),std::future_status::ready);
    auto stopper=std::async(std::launch::async,[&]{return s.manager->Stop();});auto deadline=std::chrono::steady_clock::now()+3s;
    while(!s.manager->IsStopping() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    EXPECT_TRUE(s.manager->IsStopping());EXPECT_EQ(stopper.wait_for(0s),std::future_status::timeout);
    release.set_value();callback.get();EXPECT_TRUE(stopper.get());VerifyStopped(s,s.manager,baseline);
    s.beforeParameterRead={};
}

TEST(ShutdownExtra, OwnedTaskClosureDestructionIsPartOfStopBarrier)
{
    for(bool threadMode:{false,true}) {
        auto group=std::make_shared<TelLifecycle>();
        std::promise<void> entered,release,start;auto released=release.get_future().share();auto started=start.get_future().share();
        struct ExitLatch {
            std::promise<void> &entered;std::shared_future<void> released;
            ExitLatch(std::promise<void> &e,std::shared_future<void> r):entered(e),released(std::move(r)){}
            ~ExitLatch(){entered.set_value();released.wait();}
        };
        auto latch=std::make_shared<ExitLatch>(entered,released);std::weak_ptr<ExitLatch> weak=latch;
        auto resource=std::make_shared<int>(17);std::weak_ptr<int> resourceWeak=resource;
        ASSERT_TRUE(group->Submit(resource,[latch,started]{started.wait();},threadMode));resource.reset();latch.reset();start.set_value();
        ASSERT_EQ(entered.get_future().wait_for(3s),std::future_status::ready);
        auto stop=std::async(std::launch::async,[&]{
            auto deadline=std::chrono::steady_clock::now()+3s;
            if(!group->BeginStop(deadline))return false;
            if(!group->SealWhenIdle([]{return true;},deadline)){group->FinishStop(false);return false;}
            group->JoinOwnedWork();group->FinishStop(true);return true;
        });
        auto deadline=std::chrono::steady_clock::now()+3s;
        while(group->GetState()==TelLifecycle::State::Running && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        EXPECT_EQ(group->GetState(),TelLifecycle::State::Stopping);
        EXPECT_EQ(stop.wait_for(0s),std::future_status::timeout);
        release.set_value();EXPECT_TRUE(stop.get());EXPECT_TRUE(weak.expired());EXPECT_TRUE(resourceWeak.expired());
        EXPECT_EQ(group->GetState(),TelLifecycle::State::Stopped);
        SimHost::StopCallingThreadTasks();EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
    }
}
} // namespace
