#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "account/topology_service.h"
#include "role_assignment/assignment_service.h"
#include "role_assignment/assignment_store_adapter.h"

using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr int32_t kLegacyLocalPointerNull = 83886087;
constexpr auto kRaw1 = "8900000000000000001";
constexpr auto kEncoded0 = "5BC332BC693934EB3D0DEAED37C106D2DEFD6F9767721BFE2B1AC6E209D6B67D";
constexpr auto kEncoded1 = "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";
constexpr auto kPrimary = "persist.telephony.MainSlotId";
constexpr auto kIccid = "persist.telephony.MainCard.Iccid";
constexpr auto kData = "persist.telephony.MainCellularDataSlotId";
constexpr auto kPreference = "persist.telephony.userPreferPrimarySlot";

class AccountStorePort final : public AccountStore {
public:
    AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteLabel(const AccountLabelPatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &) override { return {-1,false,true}; }
    AccountLoadResult LoadAccounts(const AccountLoadRequest &r) override { return {r,-1,false,false,{}, {}}; }
    AccountIdentityMatch MatchAccountByIccid(const std::string &) override { return {-1,true,std::nullopt}; }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &) override
    { return {-1,std::nullopt,false,true,EffectKnowledge::Rejected}; }
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &) override { return {-1,false,true}; }
    int32_t QueryImsPreferenceByIccid(const std::string &, int32_t &) override { return -1; }
};
class Parameters final : public AssignmentParameterPort {
public:
    std::map<std::string,std::string> values {{kPrimary,"0"},{kData,"0"},{kIccid,kEncoded0},{kPreference,"false"}};
    std::vector<std::pair<std::string,std::string>> writes;
    AssignmentParameterResult Read(const std::string &key,const std::string &fallback,std::string &value) override
    { value=values.count(key)?values[key]:fallback; return {0,true}; }
    AssignmentParameterResult Write(const std::string &key,const std::string &value) override
    { writes.emplace_back(key,value); values[key]=value; return {0,true}; }
};
class Radio final : public RadioExecutionPort {
public:
    bool directSupported {true};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    { return {slot, slot.legacySlotId == 0 ? 0 : 1, directSupported, false, 9}; }
    void SwitchPrimaryRadio(const RadioSwitchRequest &request) override { requests.push_back(request); }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++ended; }
    std::vector<RadioSwitchRequest> requests;
    unsigned ended {0};
};
class Output final : public AssignmentOutputPort {
public:
    std::vector<std::pair<std::string,bool>> phases;
    std::vector<std::string> roles;
    std::function<void()> onPrimaryRole;
    void PublishPrimaryPhase(bool done,bool user) override { phases.emplace_back(done?"done":"start",user); }
    void PublishMainCardChanged(SlotId) override
    {
        roles.emplace_back("primary");
        if (onPrimaryRole) { auto action = std::move(onPrimaryRole); action(); }
    }
    void PublishDefaultDataChanged(SlotId,SubscriptionId) override { roles.emplace_back("data"); }
};
class Completion final : public LegacyIntCompletionPort {
public:
    std::vector<int32_t> results;
    void CompleteLegacyInt(const CompletionPlan &,int32_t result) override { results.push_back(result); }
};

AccountRecord Account(SubscriptionId id,const std::string &iccid,SlotId slot)
{
    AccountProjection current; current.active=true; current.legacyPosition=slot;
    return {id,iccid,1,current,current};
}
CardDiscoveryObservation Discovery(int slot,uint64_t generation,const std::string &iccid)
{ return {{RuntimeEpoch {6,2},CardApplicationTarget {SlotId {slot},"USIM"},generation},generation,iccid}; }
AccountDiscoveryStoreResult Stored(SubscriptionId id)
{ return {0,id,true,true,EffectKnowledge::ConfirmedApplied}; }

class ControlledRetryScheduler final : public RetrySchedulerPort {
public:
    ControlledRetryScheduler() { worker_ = std::thread([this] { Worker(); }); }
    ~ControlledRetryScheduler() { StopAndJoin(); }

    std::optional<uint64_t> ScheduleRetry(const ScheduledRetryEvent &event, uint64_t delayMilliseconds,
        const std::weak_ptr<ScheduledRetrySink> &sink) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!accepting_) { ++audit_.rejected; return std::nullopt; }
        const uint64_t id = nextTask_++;
        tasks_.push_back({id, clock_ + delayMilliseconds, event, sink});
        ++audit_.scheduled;
        ++audit_.pending;
        changed_.notify_all();
        return id;
    }
    bool CancelRetry(uint64_t schedulerTask) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        for (auto found = tasks_.begin(); found != tasks_.end(); ++found) {
            if (found->id != schedulerTask) { continue; }
            tasks_.erase(found);
            --audit_.pending;
            ++audit_.cancelled;
            return true;
        }
        return false;
    }
    size_t CancelAllForOwner(RuntimeEpoch epoch, RequestOwner owner) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        size_t cancelled = 0;
        for (auto found = tasks_.begin(); found != tasks_.end();) {
            if (found->event.request.epoch == epoch && found->event.request.owner == owner) {
                found = tasks_.erase(found);
                --audit_.pending;
                ++cancelled;
            } else {
                ++found;
            }
        }
        audit_.cancelled += cancelled;
        changed_.notify_all();
        return cancelled;
    }
    void StopAcceptingAndCancel() override
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!accepting_) { return; }
        accepting_ = false;
        audit_.accepting = false;
        audit_.cancelled += audit_.pending;
        audit_.pending = 0;
        tasks_.clear();
        ownerTasks_.clear();
        changed_.notify_all();
    }
    RetrySchedulerAudit Audit() const override
    {
        std::lock_guard<std::mutex> guard(lock_);
        return audit_;
    }
    bool PostOwner(std::function<void()> action)
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!accepting_) { return false; }
        ownerTasks_.push_back(std::move(action));
        ++audit_.pending;
        changed_.notify_all();
        return true;
    }
    template<class F> auto OwnerCall(F action) -> decltype(action())
    {
        std::promise<decltype(action())> ready;
        auto future = ready.get_future();
        if (!PostOwner([&] {
            if constexpr (std::is_void_v<decltype(action())>) {
                action(); ready.set_value();
            } else {
                ready.set_value(action());
            }
        })) {
            throw std::runtime_error("scheduler stopped");
        }
        return future.get();
    }
    void Advance(uint64_t milliseconds)
    {
        std::lock_guard<std::mutex> guard(lock_);
        clock_ += milliseconds;
        changed_.notify_all();
    }
    bool WaitUntil(size_t delivered, uint64_t executedAtLeast,
        std::chrono::milliseconds timeout = std::chrono::seconds(3))
    {
        std::unique_lock<std::mutex> guard(lock_);
        return changed_.wait_for(guard, timeout, [&] {
            return audit_.delivered >= delivered && audit_.scheduled >= executedAtLeast;
        });
    }
    bool WaitIdle(std::chrono::milliseconds timeout = std::chrono::seconds(3))
    {
        std::unique_lock<std::mutex> guard(lock_);
        return changed_.wait_for(guard, timeout, [&] {
            return audit_.pending == 0 && audit_.running == 0;
        });
    }
    void StopAndJoin()
    {
        StopAcceptingAndCancel();
        std::thread worker;
        {
            std::lock_guard<std::mutex> guard(lock_);
            stopping_ = true;
            worker.swap(worker_);
            changed_.notify_all();
        }
        if (worker.joinable()) { worker.join(); }
    }
    bool RedeliverLastOnOwner(const std::shared_ptr<AssignmentService> &service)
    {
        ScheduledRetryEvent event;
        {
            std::lock_guard<std::mutex> guard(lock_);
            if (!lastDelivered_) { return false; }
            event = *lastDelivered_;
        }
        return OwnerCall([service, event] { return service->AcceptScheduledC1Retry(event); });
    }
private:
    struct Task {
        uint64_t id;
        uint64_t due;
        ScheduledRetryEvent event;
        std::weak_ptr<ScheduledRetrySink> sink;
    };
    void Worker()
    {
        std::unique_lock<std::mutex> guard(lock_);
        for (;;) {
            changed_.wait(guard, [&] {
                if (stopping_) { return true; }
                if (!ownerTasks_.empty()) { return true; }
                return std::any_of(tasks_.begin(), tasks_.end(), [&](const Task &task) {
                    return task.due <= clock_;
                });
            });
            if (stopping_) { return; }
            std::function<void()> owner;
            if (!ownerTasks_.empty()) {
                owner = std::move(ownerTasks_.front()); ownerTasks_.pop_front(); --audit_.pending;
                ++audit_.running; changed_.notify_all(); guard.unlock();
                owner(); guard.lock(); --audit_.running; changed_.notify_all(); continue;
            }
            auto found = std::find_if(tasks_.begin(), tasks_.end(), [&](const Task &task) {
                return task.due <= clock_;
            });
            Task task = *found; tasks_.erase(found); --audit_.pending; ++audit_.running;
            changed_.notify_all(); guard.unlock();
            bool accepted = false;
            if (auto sink = task.sink.lock()) { accepted = sink->AcceptScheduledC1Retry(task.event); }
            guard.lock(); --audit_.running;
            if (accepted) { ++audit_.delivered; lastDelivered_ = task.event; }
            else { ++audit_.rejected; }
            changed_.notify_all();
        }
    }
    mutable std::mutex lock_;
    std::condition_variable changed_;
    std::thread worker_;
    std::deque<Task> tasks_;
    std::deque<std::function<void()>> ownerTasks_;
    uint64_t clock_ {0};
    uint64_t nextTask_ {1};
    bool accepting_ {true};
    bool stopping_ {false};
    RetrySchedulerAudit audit_;
    std::optional<ScheduledRetryEvent> lastDelivered_;
};

struct Fixture {
    SlotCatalog catalog {{
        {SlotId{0},EndpointKind::Physical,0,RadioDomainId{"m0"},VisibilityClass::Ordinary,false},
        {SlotId{1},EndpointKind::Physical,1,RadioDomainId{"m1"},VisibilityClass::Ordinary,false}}, {2,2,2,2,2}};
    AccountStorePort accountStore;
    TopologyService topology {catalog,accountStore,RuntimeEpoch{6,2}};
    Parameters parameters;
    AssignmentStoreAdapter store {parameters};
    Radio radio;
    Output output;
    Completion completion;
    AssignmentService service {catalog,store,radio,output,topology,completion,RuntimeEpoch{6,2}};
    BindingRef target;
    BindingRef Seed()
    {
        EXPECT_TRUE(topology.AcceptSlotAssembly({0},true));
        EXPECT_TRUE(topology.AcceptSlotAssembly({1},true));
        EXPECT_TRUE(topology.AcceptStoredDiscovery(Discovery(0,1,"8900000000000000000"),
            Account({2},"8900000000000000000",{0}),Stored({2})).has_value());
        auto targetValue=topology.AcceptStoredDiscovery(Discovery(1,2,kRaw1),
            Account({1},kRaw1,{1}),Stored({1}));
        EXPECT_TRUE(targetValue.has_value());
        return targetValue.value_or(BindingRef{});
    }
    RequestId Start(bool threePhase = false)
    {
        if (threePhase) { radio.directSupported = false; }
        target=Seed();
        return service.StartPrimarySwitch({SlotTarget{SlotId{1}},true,
            {SynchronousCompletionRoute{SynchronousWaitRef{44}},false},std::nullopt,target});
    }
};

RadioSwitchResult Result(const RadioSwitchRequest &request, EffectKnowledge effect, int32_t legacyResult,
    bool notify = false);

struct AsyncFixture {
    SlotCatalog catalog {{
        {SlotId{0},EndpointKind::Physical,0,RadioDomainId{"m0"},VisibilityClass::Ordinary,false},
        {SlotId{1},EndpointKind::Physical,1,RadioDomainId{"m1"},VisibilityClass::Ordinary,false}}, {2,2,2,2,2}};
    AccountStorePort accountStore;
    TopologyService topology {catalog,accountStore,RuntimeEpoch{6,2}};
    Parameters parameters;
    AssignmentStoreAdapter store {parameters};
    Radio radio;
    Output output;
    Completion completion;
    std::shared_ptr<AssignmentService> service = std::make_shared<AssignmentService>(catalog, store, radio,
        output, topology, completion, RuntimeEpoch{6,2});
    std::shared_ptr<ControlledRetryScheduler> schedulerOwner=std::make_shared<ControlledRetryScheduler>();
    ControlledRetryScheduler &scheduler=*schedulerOwner;
    BindingRef target;
    AsyncFixture() { service->BindRetryScheduler(schedulerOwner); }
    ~AsyncFixture()
    {
        scheduler.StopAndJoin();
        service.reset();
    }
    bool StopAndDrain()
    {
        scheduler.StopAndJoin();
        return service && service->StopAcceptingAndCancelScheduledRetries();
    }
    void Seed()
    {
        EXPECT_TRUE(topology.AcceptSlotAssembly({0},true));
        EXPECT_TRUE(topology.AcceptSlotAssembly({1},true));
        EXPECT_TRUE(topology.AcceptStoredDiscovery(Discovery(0,1,"8900000000000000000"),
            Account({2},"8900000000000000000",{0}),Stored({2})).has_value());
        const auto found=topology.AcceptStoredDiscovery(Discovery(1,2,kRaw1),
            Account({1},kRaw1,{1}),Stored({1}));
        ASSERT_TRUE(found.has_value());
        target=found.value_or(BindingRef{});
    }
    RequestId StartC1()
    {
        radio.directSupported=false;
        Seed();
        return scheduler.OwnerCall([this, service=service] {
            return service->StartPrimarySwitch({SlotTarget{{1}},true,
                {SynchronousCompletionRoute{SynchronousWaitRef{51}},false},std::nullopt,target});
        });
    }
    void DeliverRound(size_t begin, bool failCheck=true)
    {
        for (size_t i=begin;i<begin+2;++i) {
            scheduler.OwnerCall([this, request=radio.requests[i], failCheck, service=service] {
                service->AcceptRadioStep(Result(request,
                    failCheck ? EffectKnowledge::ConfirmedNotApplied : EffectKnowledge::ConfirmedApplied,
                    failCheck ? kLegacyLocalPointerNull : 0));
            });
        }
        for (size_t i=begin+2;i<begin+4;++i) {
            scheduler.OwnerCall([this, request=radio.requests[i], service=service] {
                service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
            });
        }
    }
};

RadioSwitchResult Result(const RadioSwitchRequest &request, EffectKnowledge effect, int32_t legacyResult,
    bool notify)
{ return {request.step,request.target,request.expectedBinding,request.protocolSession,request.phase,notify,effect,legacyResult}; }
std::string Observation(const Fixture &f,int api)
{
    std::set<uint32_t> sessions;
    for (const auto &request : f.radio.requests) { sessions.insert(request.protocolSession); }
    const int retries = std::max(0, static_cast<int>(sessions.size()) - 1);
    std::string roles="[";
    for(size_t i=0;i<f.output.roles.size();++i){roles+=(i?",":"");roles+="\""+f.output.roles[i]+"\"";}
    roles+="]";
    return "{\"api_result\":"+std::to_string(api)+
        ",\"primary_role\":"+f.parameters.values.at(kPrimary)+
        ",\"default_data_role\":"+f.parameters.values.at(kData)+
        ",\"parameters\":{\"user_preference\":"+(f.parameters.values.at(kPreference)=="true"?"true":"false")+
        ",\"primary_slot\":"+f.parameters.values.at(kPrimary)+",\"primary_iccid\":\""+f.parameters.values.at(kIccid)+
        "\",\"default_data_slot\":"+f.parameters.values.at(kData)+"},\"notifications\":"+roles+
        ",\"device_switch_commands\":"+std::to_string(f.radio.requests.size())+
        ",\"retry_count\":"+std::to_string(retries)+
        ",\"switching\":"+(f.service.ReadLegacyAssignment().primarySwitchInProgress?"true":"false")+"}";
}
void VerifyBRequest(const Fixture &f)
{
    ASSERT_EQ(f.radio.requests.size(),1u);
    EXPECT_EQ(f.radio.requests[0].mode,PrimarySwitchMode::WithoutModemRestart);
    EXPECT_EQ(f.radio.requests[0].target.legacySlotId,1);
    EXPECT_TRUE(f.output.roles.empty());
    EXPECT_EQ(f.output.phases.front(),std::make_pair(std::string("start"),true));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_TRUE(f.completion.results.empty());
}
} // namespace

TEST(TargetPrimaryB,BSuccessCommitsOnlyAfterMatchingReply)
{
    Fixture f; const auto request=f.Start(); VerifyBRequest(f);
    f.service.AcceptRadioStep(Result(f.radio.requests[0],EffectKnowledge::ConfirmedApplied,0));
    EXPECT_FALSE(f.service.AcceptPrimarySwitchTimeout(request,90000));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");
    EXPECT_EQ(f.parameters.values.at(kData),"1");
    EXPECT_EQ(f.parameters.values.at(kIccid),kEncoded1);
    EXPECT_EQ(f.output.roles,(std::vector<std::string>{"primary","data"}));
    EXPECT_EQ(f.output.phases.back(),std::make_pair(std::string("done"),true));
    EXPECT_EQ(f.radio.ended,1u);
    EXPECT_FALSE(f.service.ReadLegacyAssignment().primarySwitchInProgress);
    RecordProperty("scenario_id","SIM028-LEGACY-B-SUCCESS-DUAL-PRIMARY0");
    RecordProperty("primary_observation",Observation(f,0));
    RecordProperty("validation_level","async_business");
}

TEST(TargetPrimaryB,BErrorKeepsPreviousRolesAndCompletesOriginalFailure)
{
    Fixture f; const auto request=f.Start(); VerifyBRequest(f);
    f.service.AcceptRadioStep(Result(f.radio.requests[0],EffectKnowledge::ConfirmedNotApplied,
        kLegacyLocalPointerNull));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    EXPECT_TRUE(f.output.roles.empty());
    EXPECT_EQ(f.output.phases.back(),std::make_pair(std::string("done"),true));
    EXPECT_FALSE(f.service.ReadLegacyAssignment().primarySwitchInProgress);
    RecordProperty("scenario_id","SIM028-LEGACY-B-ERROR-DUAL-PRIMARY0");
    RecordProperty("primary_observation",Observation(f,kLegacyLocalPointerNull));
}

TEST(TargetPrimaryB,BTimeoutAt45000KeepsRolesAndRejectsLateReply)
{
    Fixture f; const auto request=f.Start(); VerifyBRequest(f);
    EXPECT_FALSE(f.service.AcceptPrimarySwitchTimeout(request,44999));
    EXPECT_TRUE(f.service.AcceptPrimarySwitchTimeout(request,45000));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    EXPECT_TRUE(f.output.roles.empty());
    const auto late=Result(f.radio.requests[0],EffectKnowledge::ConfirmedApplied,0);
    f.service.AcceptRadioStep(late);
    f.service.AcceptLateRadioEvidence(late);
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_FALSE(f.service.ReadLegacyAssignment().primarySwitchInProgress);
    RecordProperty("scenario_id","SIM028-LEGACY-B-TIMEOUT-DUAL-PRIMARY0");
    RecordProperty("primary_observation", Observation(f,kLegacyLocalPointerNull));
    RecordProperty("timeout_clock","controlled_ms; production deadline=45000");
    f.service.StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{46}},false},std::nullopt,f.target});
    EXPECT_EQ(f.completion.results.size(),2u);
    EXPECT_NE(f.completion.results.back(),0);
    EXPECT_EQ(f.radio.requests.size(),1u);
}

TEST(TargetPrimaryB,BindingChangeAndUnknownEffectsPreserveIsolation)
{
    Fixture changed; const auto request=changed.Start(); ASSERT_EQ(changed.radio.requests.size(),1u);
    changed.topology.AcceptCardRemoval(changed.target);
    EXPECT_FALSE(changed.topology.ReadPrimaryTarget({1}).has_value());
    const auto replacement=changed.topology.AcceptStoredDiscovery(Discovery(1,3,"replacement-card"),
        Account({9},"replacement-card",{1}),Stored({9}));
    ASSERT_TRUE(replacement.has_value());
    changed.service.AcceptRadioStep(Result(changed.radio.requests[0],EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(changed.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(changed.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(changed.parameters.values.at(kData),"0");
    EXPECT_TRUE(changed.output.roles.empty());
    changed.service.StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{47}},false},std::nullopt,*replacement});
    EXPECT_EQ(changed.radio.requests.size(),1u);

    Fixture unknown; unknown.Start(); ASSERT_EQ(unknown.radio.requests.size(),1u);
    unknown.service.AcceptRadioStep(Result(unknown.radio.requests[0],EffectKnowledge::Unknown,-1));
    EXPECT_TRUE(unknown.output.roles.empty());
    EXPECT_EQ(unknown.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(unknown.completion.results.size(),1u);
    unknown.service.StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{48}},false},std::nullopt,unknown.target});
    EXPECT_EQ(unknown.radio.requests.size(),1u);

    Fixture conflict; conflict.Start();
    conflict.service.AcceptRadioStep(Result(conflict.radio.requests[0],EffectKnowledge::ConfirmedApplied,
        kLegacyLocalPointerNull));
    EXPECT_EQ(conflict.parameters.values.at(kPrimary),"0");
    EXPECT_TRUE(conflict.output.roles.empty());
    conflict.service.StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{49}},false},std::nullopt,conflict.target});
    EXPECT_EQ(conflict.radio.requests.size(),1u);
    ASSERT_EQ(conflict.completion.results.size(),2u);
    conflict.service.AcceptLateRadioEvidence(Result(conflict.radio.requests[0],
        EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
    EXPECT_EQ(conflict.completion.results.size(),2u);
}

TEST(TargetPrimaryB,C1ThreePhaseSuccessRejectsStaleSession)
{
    Fixture f; f.Start(true);
    ASSERT_EQ(f.radio.requests.size(),2u);
    EXPECT_EQ(f.radio.requests[0].mode,PrimarySwitchMode::ThreePhaseProtocol);
    EXPECT_EQ(f.radio.requests[0].phase,1u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");
    EXPECT_TRUE(f.output.roles.empty());

    auto stale=Result(f.radio.requests[0],EffectKnowledge::ConfirmedApplied,0); ++stale.protocolSession;
    f.service.AcceptRadioStep(stale);
    EXPECT_TRUE(f.completion.results.empty());
    const auto check = f.radio.requests;
    for (const auto &request : check) {
        f.service.AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
    }
    ASSERT_EQ(f.radio.requests.size(),4u);
    EXPECT_EQ(f.radio.requests[2].phase,2u);
    ASSERT_EQ(f.radio.requests.size(),4u);
    // Legacy UPDATE completion is driven by each RIL NOTIFY, not by RadioResponseInfo ACKs.
    for (size_t i=2;i<4;++i) {
        auto notify=Result(f.radio.requests[i],EffectKnowledge::ConfirmedApplied,0,true);
        f.service.AcceptRadioStep(notify);
    }
    ASSERT_EQ(f.radio.requests.size(),6u);
    EXPECT_EQ(f.radio.requests[4].phase,4u);
    for (size_t i=4;i<6;++i) {
        f.service.AcceptRadioStep(Result(f.radio.requests[i],EffectKnowledge::ConfirmedApplied,0));
    }
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");
    EXPECT_EQ(f.parameters.values.at(kData),"1");
    EXPECT_EQ(f.output.roles,(std::vector<std::string>{"primary","data"}));
    EXPECT_EQ(f.output.phases,(std::vector<std::pair<std::string,bool>>{{"start",false},{"done",false}}));
    EXPECT_EQ(f.radio.ended,1u);
    RecordProperty("scenario_id","SIM028-LEGACY-C1-SUCCESS-STALE-DUAL-PRIMARY0");
    RecordProperty("validation_level","three_phase_business");
    RecordProperty("primary_observation",Observation(f,0));
}

TEST(TargetPrimaryB,C1SharedWriteQualificationBlocksC0InDifferentDomainUntilRelease)
{
    Fixture f; f.Start(true);
    ASSERT_EQ(f.radio.requests.size(),2u);
    const auto slot0=f.topology.ReadPrimaryTarget({0}); ASSERT_TRUE(slot0.has_value());
    const auto command=SwitchPrimaryCommand{SlotTarget{{0}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{45}},false},std::nullopt,slot0->binding};
    f.service.StartPrimarySwitch(command);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1}));
    EXPECT_EQ(f.radio.requests.size(),2u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");

    for (size_t i=0;i<2;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    for (size_t i=2;i<4;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0,true)); }
    for (size_t i=4;i<6;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0}));

    f.service.StartPrimarySwitch(command);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0,0}));
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kData),"0");
}

TEST(TargetPrimaryB,C0ReentrantPrimaryNotificationCannotAdmitConflictingRequest)
{
    Fixture f; f.target=f.Seed(); f.radio.directSupported=false;
    const auto slot0=f.topology.ReadPrimaryTarget({0}); ASSERT_TRUE(slot0.has_value());
    f.output.onPrimaryRole=[&] {
        f.service.StartPrimarySwitch({SlotTarget{{1}},true,
            {SynchronousCompletionRoute{SynchronousWaitRef{46}},false},std::nullopt,f.target});
    };
    f.service.StartPrimarySwitch({SlotTarget{{0}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{47}},false},std::nullopt,slot0->binding});
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0}));
    EXPECT_EQ(f.radio.requests.size(),0u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    EXPECT_EQ(f.output.roles,(std::vector<std::string>{"primary","data"}));
}

TEST(TargetPrimaryB,C1BindingChangeRestoresPrewrittenPrimaryParameters)
{
    Fixture f; f.Start(true);
    ASSERT_EQ(f.radio.requests.size(),2u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");
    f.topology.AcceptCardRemoval(f.target);
    for (size_t i=0;i<2;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    for (size_t i=2;i<4;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0,true)); }
    for (size_t i=4;i<6;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kIccid),kEncoded0);
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    EXPECT_EQ(f.service.ReadLegacyAssignment().lastPrimary, std::make_optional(SlotId {0}));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(f.service.ReadLegacyAssignment().primarySwitchInProgress);
    EXPECT_TRUE(f.output.roles.empty());
}

TEST(TargetPrimaryB,C1BackgroundRetryUsesControlledAsyncScheduler)
{
    AsyncFixture f;
    const auto request=f.StartC1();
    ASSERT_EQ(f.radio.requests.size(),2u);
    for (uint32_t round=0;round<6;++round) {
        const size_t begin=round*4;
        ASSERT_EQ(f.radio.requests.size(),begin+2u);
        f.DeliverRound(begin);
        ASSERT_EQ(f.radio.requests.size(),begin+4u);
        EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
        if (round<5) {
            const auto scheduled=f.scheduler.Audit().scheduled;
            EXPECT_EQ(scheduled, static_cast<uint64_t>(round + 1));
            f.scheduler.Advance(1);
            ASSERT_TRUE(f.scheduler.WaitUntil(round+1, round+1));
            ASSERT_TRUE(f.scheduler.WaitIdle());
            ASSERT_EQ(f.radio.requests.size(),begin+6u);
            EXPECT_EQ(f.radio.requests.back().step.request, request);
            EXPECT_EQ(f.radio.requests.back().step.attempt, round+1);
        }
    }
    EXPECT_EQ(f.radio.requests.size(),24u);
    std::set<uint32_t> sessions;
    for (const auto &radio : f.radio.requests) { sessions.insert(radio.protocolSession); }
    EXPECT_EQ(sessions.size(),6u);
    EXPECT_EQ(f.output.phases.size(),12u);
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kIccid),kEncoded0);
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    const auto audit=f.scheduler.Audit();
    EXPECT_EQ(audit.scheduled,5u);
    EXPECT_EQ(audit.delivered,5u);
    EXPECT_TRUE(f.scheduler.RedeliverLastOnOwner(f.service)==false);
    EXPECT_EQ(audit.pending,0u);
}

TEST(TargetPrimaryB,C1RetrySchedulerRejectsDuplicateAndBindingChange)
{
    AsyncFixture f;
    f.StartC1();
    f.DeliverRound(0);
    EXPECT_EQ(f.scheduler.Audit().scheduled,1u);
    f.topology.AcceptCardRemoval(f.target);
    f.scheduler.Advance(1);
    ASSERT_TRUE(f.scheduler.WaitUntil(0,1));
    EXPECT_TRUE(f.scheduler.WaitIdle());
    EXPECT_EQ(f.radio.requests.size(),4u);
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
    EXPECT_FALSE(f.scheduler.RedeliverLastOnOwner(f.service));
}

TEST(TargetPrimaryB,C1RetrySchedulerStopCancelsAndOldEpochCannotRun)
{
    AsyncFixture f;
    f.StartC1();
    f.DeliverRound(0);
    EXPECT_EQ(f.scheduler.Audit().scheduled,1u);
    EXPECT_TRUE(f.StopAndDrain());
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
    EXPECT_FALSE(f.scheduler.PostOwner([&] { ADD_FAILURE(); }));
    f.scheduler.Advance(100);
    EXPECT_EQ(f.radio.requests.size(),4u);
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_FALSE(f.scheduler.Audit().accepting);
    EXPECT_EQ(f.scheduler.Audit().cancelled,1u);
    f.service->StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{52}},false},std::nullopt,f.target});
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull,-1}));
    EXPECT_EQ(f.radio.requests.size(),4u);
    const auto oldRequest=f.radio.requests.front().step.request;
    EXPECT_FALSE(f.service->AcceptScheduledC1Retry({oldRequest,1,1}));
    const RequestId staleEpoch {oldRequest.value, {6,1}, RequestOwner::Assignment};
    EXPECT_FALSE(f.service->AcceptScheduledC1Retry({staleEpoch,1,1}));
}

TEST(TargetPrimaryB,C1RetryDueAndStopRaceDrainsCallbackBeforeResultDecision)
{
    AsyncFixture f;
    f.StartC1();
    f.DeliverRound(0);
    f.scheduler.Advance(1);
    const bool drained=f.StopAndDrain();
    const bool activeRound=f.radio.requests.size()==6u;
    EXPECT_TRUE(f.radio.requests.size()==4u || activeRound);
    EXPECT_EQ(drained,!activeRound);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_EQ(f.service->HasPendingDeviceEffects(),activeRound);
    EXPECT_TRUE(f.service->StopAcceptingAndCancelScheduledRetries() == drained);
}

TEST(TargetPrimaryB,C1DestroyedOwnerRejectsQueuedRetryWithoutUaf)
{
    AsyncFixture f;
    f.StartC1();
    f.DeliverRound(0);
    ASSERT_EQ(f.scheduler.Audit().scheduled,1u);
    const std::weak_ptr<AssignmentService> expired=f.service;
    f.service.reset();
    EXPECT_TRUE(expired.expired());
    f.scheduler.Advance(1);
    EXPECT_TRUE(f.scheduler.WaitIdle());
    EXPECT_EQ(f.radio.requests.size(),4u);
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_EQ(f.scheduler.Audit().rejected,1u);
}

TEST(TargetPrimaryB,C1StopReportsDrainFailureUntilActiveDeviceResultEnds)
{
    AsyncFixture f;
    f.StartC1();
    EXPECT_FALSE(f.StopAndDrain());
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
    for (size_t i=0;i<2;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    for (size_t i=2;i<4;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0,true)); }
    for (size_t i=4;i<6;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
    EXPECT_TRUE(f.service->StopAcceptingAndCancelScheduledRetries());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(f.scheduler.PostOwner([&] { ADD_FAILURE(); }));
}

TEST(TargetPrimaryB,C1StopDuringActiveRoundCancelsRetryAfterConfirmedComplete)
{
    AsyncFixture f;
    f.StartC1();
    EXPECT_FALSE(f.service->StopAcceptingAndCancelScheduledRetries());
    f.DeliverRound(0);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_EQ(f.scheduler.Audit().scheduled,0u);
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
    EXPECT_TRUE(f.service->StopAcceptingAndCancelScheduledRetries());
    EXPECT_EQ(f.radio.requests.size(),4u);
}

TEST(TargetPrimaryB,C1StopAndQueuedRadioCallbacksRemainOwnerSerialized)
{
    AsyncFixture f;
    f.StartC1();
    ASSERT_TRUE(f.scheduler.PostOwner([service=f.service] {
        EXPECT_FALSE(service->StopAcceptingAndCancelScheduledRetries());
    }));
    for (size_t i=0;i<2;++i) {
        ASSERT_TRUE(f.scheduler.PostOwner([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
        }));
    }
    ASSERT_TRUE(f.scheduler.WaitIdle());
    for (size_t i=2;i<4;++i) {
        f.scheduler.OwnerCall([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0,true));
        });
    }
    for (size_t i=4;i<6;++i) {
        f.scheduler.OwnerCall([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
        });
    }
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,C1UnknownDoesNotScheduleRetry)
{
    AsyncFixture f;
    f.StartC1();
    ASSERT_EQ(f.radio.requests.size(),2u);
    f.scheduler.OwnerCall([&] {
        f.service->AcceptRadioStep(Result(f.radio.requests[0],EffectKnowledge::Unknown,-1));
    });
    EXPECT_EQ(f.scheduler.Audit().scheduled,0u);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    f.scheduler.OwnerCall([&] {
        f.service->AcceptLateRadioEvidence(Result(f.radio.requests[0],
            EffectKnowledge::ConfirmedApplied,0));
    });
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,C1CheckFailureRestoresAndExhaustsFiveRetries)
{
    Fixture f; const auto request=f.Start(true);
    ASSERT_EQ(f.radio.requests.size(),2u);
    const auto originalIccid=kEncoded0;
    for (uint32_t round=0; round<6; ++round) {
        const size_t begin=round*4;
        ASSERT_EQ(f.radio.requests.size(),begin+2);
        EXPECT_EQ(f.radio.requests[begin].phase,1u);
        for (size_t i=begin;i<begin+2;++i) {
            f.service.AcceptRadioStep(Result(f.radio.requests[i],
                EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull));
        }
        ASSERT_EQ(f.radio.requests.size(),begin+4);
        EXPECT_EQ(f.radio.requests[begin+2].phase,4u);
        for (size_t i=begin+2;i<begin+4;++i) {
            f.service.AcceptRadioStep(Result(f.radio.requests[i],EffectKnowledge::ConfirmedApplied,0));
        }
        EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
        EXPECT_EQ(f.parameters.values.at(kData),"0");
        EXPECT_EQ(f.parameters.values.at(kIccid),originalIccid);
        if (round==0) {
            EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
        }
        if (round<5) { ASSERT_TRUE(f.service.AcceptC1RetryDue(request)) << "round=" << round; }
    }
    EXPECT_FALSE(f.service.AcceptC1RetryDue(request));
    EXPECT_EQ(f.radio.requests.size(),24u);
    std::set<uint32_t> sessions;
    for (const auto &request : f.radio.requests) { sessions.insert(request.protocolSession); }
    EXPECT_EQ(sessions.size(), 6u);
    EXPECT_EQ(f.service.ReadLegacyAssignment().lastPrimary, std::make_optional(SlotId {0}));
    EXPECT_EQ(f.output.phases.size(),12u);
    for (const auto &phase : f.output.phases) { EXPECT_FALSE(phase.second); }
    EXPECT_TRUE(f.output.roles.empty());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    RecordProperty("scenario_id","SIM028-LEGACY-C1-CHECK-ERROR-RETRY-DUAL-PRIMARY0");
    RecordProperty("primary_observation",Observation(f,kLegacyLocalPointerNull));
}

TEST(TargetPrimaryB,RejectsWrongAssociationAndDoesNotAdvanceNewRequest)
{
    Fixture f; const auto first=f.Start(); ASSERT_EQ(f.radio.requests.size(),1u);
    auto wrong=f.radio.requests[0]; wrong.step.step++;
    f.service.AcceptRadioStep(Result(wrong,EffectKnowledge::ConfirmedApplied,0));
    auto old=f.radio.requests[0];
    f.service.AcceptRadioStep(Result(old,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    const auto second=f.service.StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{45}},false},std::nullopt,f.target});
    ASSERT_EQ(f.radio.requests.size(),2u);
    f.service.AcceptRadioStep(Result(old,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results.size(),1u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    auto wrongBinding=f.radio.requests[1]; wrongBinding.expectedBinding=BindingRef{{9},{1},1};
    f.service.AcceptRadioStep(Result(wrongBinding,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results.size(),1u);
    auto wrongSession=Result(f.radio.requests[1],EffectKnowledge::ConfirmedApplied,0);
    ++wrongSession.protocolSession;
    f.service.AcceptRadioStep(wrongSession);
    EXPECT_EQ(f.completion.results.size(),1u);
    f.service.AcceptRadioStep(Result(f.radio.requests[1],EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results.back(),0);
    EXPECT_NE(first.value,second.value);
}
