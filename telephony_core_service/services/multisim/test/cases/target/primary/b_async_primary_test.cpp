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
#include "device/resource/resource_radio_runtime.h"
#include "infrastructure/assembly/primary_host_lifecycle.h"
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
    AccountLoadResult LoadAccounts(const AccountLoadRequest &r) override { return {r,-1,false,false,{}, {}}; }
    AccountIdentityMatch MatchAccountByIccid(const std::string &) override { return {-1,true,std::nullopt}; }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &) override
    { return {-1,std::nullopt,false,true,EffectKnowledge::Rejected}; }
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &) override { return {-1,false,true}; }
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
    void ReportDefaultDataSelection(SlotId) override {}
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

class ControlledRetryScheduler final : public RetrySchedulerPort, public RadioOwnerDispatchPort {
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
        std::unique_lock<std::mutex> guard(lock_);
        if (blockStop_) {
            stopEntered_ = true;
            changed_.notify_all();
            changed_.wait(guard, [&] { return !blockStop_; });
        }
        if (!accepting_) { return; }
        accepting_ = false;
        audit_.accepting = false;
        audit_.cancelled += tasks_.size();
        audit_.pending -= tasks_.size();
        tasks_.clear();
        changed_.notify_all();
    }
    RetrySchedulerAudit Audit() const override
    {
        std::lock_guard<std::mutex> guard(lock_);
        return audit_;
    }
    bool rejectOwnerDispatch {false};
    void BlockStopGate()
    {
        std::lock_guard<std::mutex> guard(lock_);
        blockStop_ = true;
    }
    bool WaitStopEntered(std::chrono::milliseconds timeout=std::chrono::seconds(3))
    {
        std::unique_lock<std::mutex> guard(lock_);
        return changed_.wait_for(guard,timeout,[&] { return stopEntered_; });
    }
    void ReleaseStopGate()
    {
        std::lock_guard<std::mutex> guard(lock_);
        blockStop_ = false;
        changed_.notify_all();
    }
    void BlockOwnerQueue()
    {
        std::lock_guard<std::mutex> guard(lock_);
        blockOwner_=true;
    }
    void ReleaseOwnerQueue()
    {
        std::lock_guard<std::mutex> guard(lock_);
        blockOwner_=false;
        changed_.notify_all();
    }
    bool PostRadioOwnerTask(std::function<void()> task) override
    {
        if (rejectOwnerDispatch) { return false; }
        std::lock_guard<std::mutex> guard(lock_);
        if (stopping_) { return false; }
        ownerTasks_.push_back(std::move(task));
        ++audit_.pending;
        changed_.notify_all();
        return true;
    }
    bool RadioOwnerCallbacksDrained() const override
    {
        std::lock_guard<std::mutex> guard(lock_);
        return ownerTasks_.empty() && runningOwner_ == 0;
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
    bool WaitOwnerIdle(std::chrono::milliseconds timeout = std::chrono::seconds(3))
    {
        std::unique_lock<std::mutex> guard(lock_);
        return changed_.wait_for(guard, timeout, [&] {
            return ownerTasks_.empty() && runningOwner_ == 0;
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
                if (!blockOwner_ && !ownerTasks_.empty()) { return true; }
                return std::any_of(tasks_.begin(), tasks_.end(), [&](const Task &task) {
                    return task.due <= clock_;
                });
            });
            if (stopping_) { return; }
            std::function<void()> owner;
            if (!ownerTasks_.empty()) {
                owner = std::move(ownerTasks_.front()); ownerTasks_.pop_front(); --audit_.pending;
                ++audit_.running; ++runningOwner_; changed_.notify_all(); guard.unlock();
                owner(); guard.lock(); --audit_.running; --runningOwner_; changed_.notify_all(); continue;
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
    bool blockOwner_ {false};
    bool blockStop_ {false};
    bool stopEntered_ {false};
    size_t runningOwner_ {0};
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

class ResourcePlatform final : public RadioPlatformPort {
public:
    bool submitAllowed {true};
    bool omitSerial {false};
    bool directSupported {true};
    bool alreadyPrimary {false};
    std::vector<std::pair<uint64_t, RadioSwitchRequest>> requests;
    unsigned unrelatedRilRequests {0};
    bool SubmitUnrelatedRilRequest() { ++unrelatedRilRequests; return true; }
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    { return {slot, alreadyPrimary ? 0 : (slot.legacySlotId == 0 ? 0 : 1), directSupported, false, 9}; }
    RadioSubmitDisposition disposition {RadioSubmitDisposition::HandoffAccepted};
    std::function<void(uint64_t,const RadioSwitchRequest &)> onSynchronousResponse;
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t token) override
    {
        if (onSynchronousResponse) { onSynchronousResponse(token, request); }
        if (disposition == RadioSubmitDisposition::ConfirmedNotSent) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        if (!submitAllowed) { return {RadioSubmitDisposition::HandoffUnknown, std::nullopt}; }
        requests.emplace_back(token, request);
        return {disposition, omitSerial ? std::nullopt :
            std::make_optional(static_cast<int32_t>(token))};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++ended; }
    unsigned ended {0};
};

struct ResourceFixture {
    SlotCatalog catalog {{
        {SlotId{0},EndpointKind::Physical,0,RadioDomainId{"m0"},VisibilityClass::Ordinary,false},
        {SlotId{1},EndpointKind::Physical,1,RadioDomainId{"m1"},VisibilityClass::Ordinary,false}}, {2,2,2,2,2}};
    AccountStorePort accountStore;
    TopologyService topology {catalog,accountStore,RuntimeEpoch{6,2}};
    Parameters parameters;
    AssignmentStoreAdapter store {parameters};
    ResourcePlatform platform;
    Output output;
    Completion completion;
    const ResourceKey key {"host-primary-radio:conservative-shared"};
    ResourceBarrier barrier {{6,2}};
    ResourceRadioRuntime radio {barrier, platform, {key}, ResourceRadioRuntime::Mode::Execute};
    std::shared_ptr<AssignmentService> service=std::make_shared<AssignmentService>(catalog,store,radio,
        output,topology,completion,RuntimeEpoch{6,2});
    std::shared_ptr<ControlledRetryScheduler> schedulerOwner=std::make_shared<ControlledRetryScheduler>();
    ControlledRetryScheduler &scheduler=*schedulerOwner;
    BindingRef target;
    ResourceFixture()
    {
        EXPECT_TRUE(barrier.RegisterForInitialCheck(key));
        EXPECT_TRUE(radio.Initialize(key, {{6,2}, true, true}));
        radio.BindResultSink(service);
        radio.BindOwnerDispatch(schedulerOwner);
        service->BindRetryScheduler(schedulerOwner);
    }
    ~ResourceFixture() { scheduler.StopAndJoin(); }
    BindingRef Seed()
    {
        EXPECT_TRUE(topology.AcceptSlotAssembly({0},true));
        EXPECT_TRUE(topology.AcceptSlotAssembly({1},true));
        EXPECT_TRUE(topology.AcceptStoredDiscovery(Discovery(0,1,"8900000000000000000"),
            Account({2},"8900000000000000000",{0}),Stored({2})).has_value());
        auto found=topology.AcceptStoredDiscovery(Discovery(1,2,kRaw1),Account({1},kRaw1,{1}),Stored({1}));
        EXPECT_TRUE(found.has_value());
        return found.value_or(BindingRef{});
    }
    RequestId Start(bool threePhase)
    {
        if (!platform.alreadyPrimary) { platform.directSupported=!threePhase; }
        target=Seed();
        return scheduler.OwnerCall([this, service=service] { return service->StartPrimarySwitch(
            {SlotTarget{{1}},true,{SynchronousCompletionRoute{SynchronousWaitRef{61}},false},
             std::nullopt,target}); });
    }
    RequestId StartC0()
    {
        target=Seed();
        platform.directSupported=false;
        const auto slot0=topology.ReadPrimaryTarget({0});
        EXPECT_TRUE(slot0.has_value());
        return scheduler.OwnerCall([this, service=service, slot0] { return service->StartPrimarySwitch(
            {SlotTarget{{0}},true,{SynchronousCompletionRoute{SynchronousWaitRef{63}},false},
             std::nullopt,slot0 ? std::optional<BindingRef>{slot0->binding} : std::nullopt}); });
    }
    void DeliverRound(size_t begin, bool failCheck=true)
    {
        for (size_t i=begin;i<begin+2;++i) {
            EXPECT_TRUE(ResultAt(i, failCheck ? EffectKnowledge::ConfirmedNotApplied :
                EffectKnowledge::ConfirmedApplied, failCheck ? kLegacyLocalPointerNull : 0));
        }
        for (size_t i=begin+2;i<begin+4;++i) {
            EXPECT_TRUE(ResultAt(i,EffectKnowledge::ConfirmedApplied,0));
        }
    }
    bool ResultAt(size_t index, EffectKnowledge effect, int32_t legacyResult, bool notify=false,
        bool ended=true, bool reusable=true, bool verified=false)
    {
        const auto &sent=platform.requests[index];
        const bool accepted=radio.AcceptDeviceResult({sent.first,
            Result(sent.second,effect,legacyResult,notify),ended,reusable,verified,
            std::make_optional(static_cast<int32_t>(sent.first))});
        EXPECT_TRUE(scheduler.WaitOwnerIdle());
        return accepted;
    }
};

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

TEST(TargetPrimaryB,ResourceBarrierCheckingBlocksSendUntilInitialRecovery)
{
    SlotCatalog catalog {{
        {SlotId{0},EndpointKind::Physical,0,RadioDomainId{"m0"},VisibilityClass::Ordinary,false},
        {SlotId{1},EndpointKind::Physical,1,RadioDomainId{"m1"},VisibilityClass::Ordinary,false}}, {2,2,2,2,2}};
    ResourcePlatform platform;
    const ResourceKey key {"host-primary-radio:checking"};
    ResourceBarrier barrier({6,2});
    ASSERT_TRUE(barrier.RegisterForInitialCheck(key));
    ResourceRadioRuntime radio(barrier,platform,{key},ResourceRadioRuntime::Mode::Execute);
    class Sink final : public RadioResultSink {
    public:
        void AcceptRadioStep(const RadioSwitchResult &) override {}
        void AcceptLateRadioEvidence(const RadioSwitchResult &) override {}
    };
    auto sink=std::make_shared<Sink>();
    radio.BindResultSink(sink);
    const RadioSwitchRequest request {{RequestId{1,{6,2},RequestOwner::Assignment},2,0},
        {1},{"m1"},PrimarySwitchMode::WithoutModemRestart,std::nullopt,1,0,1};
    EXPECT_EQ(radio.ReadResourceState(key),ResourceState::Checking);
    radio.SwitchPrimaryRadio(request);
    EXPECT_TRUE(platform.requests.empty());
    EXPECT_FALSE(radio.Initialize(key,{{6,2},false,true}));
    EXPECT_TRUE(radio.Initialize(key,{{6,2},true,true}));
    EXPECT_EQ(radio.ReadResourceState(key),ResourceState::Available);
}

TEST(TargetPrimaryB,HostLifecycleOldHandleRejectedAndAcceptedRequestCompletes)
{
    ResourceFixture f;
    const auto request=f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    EXPECT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
    f.service->StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{65}},false},std::nullopt,f.target});
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1}));
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0}));
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
    EXPECT_TRUE(lifecycle.IsStopped());
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.platform.requests.front().second.step.request,request);
}

TEST(TargetPrimaryB,HostLifecycleCloseIntentRejectsStartBeforeOwnerSeal)
{
    ResourceFixture f;
    const auto request=f.Start(false);
    f.scheduler.BlockOwnerQueue();
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    auto close=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    EXPECT_EQ(close.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
    const auto rejected=f.service->StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{71}},false},std::nullopt,f.target});
    EXPECT_NE(rejected.value,request.value);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1}));
    const auto &sent=f.platform.requests[0];
    EXPECT_TRUE(f.radio.AcceptDeviceResult({sent.first,
        Result(sent.second,EffectKnowledge::ConfirmedApplied,0),true,true,false,
        std::make_optional(static_cast<int32_t>(sent.first))}));
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0}));
}

TEST(TargetPrimaryB,HostLifecyclePostedSealOutlivesCoordinator)
{
    ResourceFixture f;
    f.scheduler.BlockOwnerQueue();
    auto lifecycle=std::make_unique<PrimaryHostLifecycle>(f.service,f.radio,
        f.scheduler,f.schedulerOwner);
    EXPECT_EQ(lifecycle->StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    lifecycle.reset();
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
}

TEST(TargetPrimaryB,HostLifecycleConcurrentStopCannotDrainBeforeSeals)
{
    ResourceFixture f;
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockStopGate();
    auto first=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    ASSERT_TRUE(f.scheduler.WaitStopEntered());
    auto second=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    EXPECT_EQ(second.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseStopGate();
    EXPECT_EQ(first.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
}

TEST(TargetPrimaryB,HostLifecycleConcurrentStopHasOneCleaner)
{
    ResourceFixture f;
    f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    auto first=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    auto second=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    EXPECT_EQ(first.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    EXPECT_EQ(second.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
}

TEST(TargetPrimaryB,HostLifecycleOwnerDispatchFailureIsNotDeviceDrain)
{
    ResourceFixture f;
    f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.rejectOwnerDispatch=true;
    EXPECT_FALSE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_TRUE(f.radio.DeviceResourcesDrained());
    f.scheduler.rejectOwnerDispatch=false;
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    f.scheduler.rejectOwnerDispatch=false;
    EXPECT_TRUE(f.radio.FlushOwnerDeliveries());
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.radio.OwnerCallbacksDrained());
    EXPECT_EQ(f.scheduler.Audit().pending,0u);
    EXPECT_EQ(f.scheduler.Audit().running,0u);
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
}

TEST(TargetPrimaryB,HostLifecycleStopOnEmptyHostIsIdempotent)
{
    ResourceFixture f;
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
    EXPECT_TRUE(lifecycle.IsStopped());
    EXPECT_TRUE(f.radio.DeviceResourcesDrained());
}

TEST(TargetPrimaryB,HostLifecycleCancelsQueuedRetryAndOldEpochCannotRun)
{
    ResourceFixture f;
    const auto request=f.Start(true);
    f.DeliverRound(0);
    EXPECT_EQ(f.scheduler.Audit().scheduled,1u);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
    EXPECT_FALSE(f.scheduler.PostOwner([&] { ADD_FAILURE(); }));
    EXPECT_FALSE(f.service->AcceptScheduledC1Retry({request,1,1}));
    EXPECT_EQ(f.platform.requests.size(),4u);
}

TEST(TargetPrimaryB,HostLifecycleStopRequestedFromOwnerCallbackDoesNotJoinSelf)
{
    ResourceFixture f;
    f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    std::promise<PrimaryHostLifecycle::DrainResult> result;
    ASSERT_TRUE(f.scheduler.PostRadioOwnerTask([&] {
        result.set_value(lifecycle.StopAcceptingAndDrain());
    }));
    auto future=result.get_future();
    ASSERT_EQ(future.wait_for(std::chrono::seconds(3)),std::future_status::ready);
    EXPECT_EQ(future.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.service->IsStoppingPrimaryRequests());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);
}

TEST(TargetPrimaryB,HostLifecycleDoesNotStopUnrelatedSharedRilConsumer)
{
    ResourceFixture f;
    const auto request=f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved);
    f.platform.onSynchronousResponse={};
    EXPECT_TRUE(f.platform.SubmitUnrelatedRilRequest());
    EXPECT_EQ(f.platform.unrelatedRilRequests,1u);
    EXPECT_EQ(request.epoch,RuntimeEpoch({6,2}));
    EXPECT_EQ(f.platform.requests.size(),1u);
}

TEST(TargetPrimaryB,HostLifecycleOldAndNewInstancesRemainIsolated)
{
    ResourceFixture oldHost;
    oldHost.Start(false);
    PrimaryHostLifecycle oldLifecycle(oldHost.service,oldHost.radio,
        oldHost.scheduler,oldHost.schedulerOwner);
    oldHost.scheduler.BlockOwnerQueue();
    EXPECT_EQ(oldLifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    oldHost.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(oldHost.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(oldHost.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(oldLifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::Drained);

    ResourceFixture newHost;
    newHost.Start(false);
    ASSERT_EQ(newHost.platform.requests.size(),1u);
    EXPECT_TRUE(newHost.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(newHost.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(newHost.service->IsStoppingPrimaryRequests());
}

TEST(TargetPrimaryB,HostLifecycleStopDuringC1CheckPreservesMemberResponsibility)
{
    ResourceFixture f;
    f.Start(true);
    ASSERT_EQ(f.platform.requests.size(),2u);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved);
    EXPECT_TRUE(f.ResultAt(1,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,HostLifecycleStopAndExternalResultRaceCompletesOnce)
{
    ResourceFixture f;
    f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockStopGate();
    auto stop=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    ASSERT_TRUE(f.scheduler.WaitStopEntered());
    ASSERT_TRUE(f.service->IsStoppingPrimaryRequests());
    const auto result=Result(f.platform.requests[0].second,
        EffectKnowledge::ConfirmedApplied,0);
    auto ownerResult=std::async(std::launch::async,[&, service=f.service] {
        service->AcceptRadioStep(result);
        service->AcceptRadioStep(result);
    });
    auto timeout=std::async(std::launch::async,[&, service=f.service] {
        EXPECT_FALSE(service->AcceptPrimarySwitchTimeout(result.step.request,0));
    });
    ownerResult.get(); timeout.get();
    f.scheduler.BlockOwnerQueue();
    f.scheduler.ReleaseStopGate();
    EXPECT_EQ(stop.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,HostLifecycleStopAndLateEvidencePreserveOriginalCompletion)
{
    ResourceFixture f;
    f.Start(false);
    const auto unknown=Result(f.platform.requests[0].second,EffectKnowledge::Unknown,83886087);
    f.service->AcceptRadioStep(unknown);
    ASSERT_EQ(f.completion.results,(std::vector<int32_t>{83886087}));
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockStopGate();
    auto stop=std::async(std::launch::async,[&] { return lifecycle.StopAcceptingAndDrain(); });
    ASSERT_TRUE(f.scheduler.WaitStopEntered());
    auto late=std::async(std::launch::async,[&, service=f.service] {
        service->AcceptLateRadioEvidence(Result(f.platform.requests[0].second,
            EffectKnowledge::ConfirmedApplied,0));
    });
    late.get();
    f.scheduler.BlockOwnerQueue();
    f.scheduler.ReleaseStopGate();
    EXPECT_EQ(stop.get(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{83886087}));
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,HostLifecycleHandoffUnknownKeepsDeviceResponsibility)
{
    ResourceFixture f;
    f.platform.submitAllowed=false;
    f.Start(false);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved);
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
    EXPECT_TRUE(f.radio.ReadResourceState(f.key));
}

TEST(TargetPrimaryB,HostLifecycleQuarantinedResultIsNotDrained)
{
    ResourceFixture f;
    f.Start(false);
    PrimaryHostLifecycle lifecycle(f.service,f.radio,f.scheduler,f.schedulerOwner);
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_FALSE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0,false,false,false));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),
        PrimaryHostLifecycle::DrainResult::DeviceEffectsUnresolved);
}

TEST(TargetPrimaryB,ResourceHandoffAcceptedRequiresExpectedSerial)
{
    ResourceFixture f;
    f.platform.omitSerial=true;
    f.Start(false);
    EXPECT_EQ(f.platform.requests.size(),1u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_FALSE(f.radio.WasLastSubmissionAccepted(f.platform.requests[0].second));
}

TEST(TargetPrimaryB,ResourceSubmitUnknownQuarantinesInsteadOfConfirmedCancel)
{
    ResourceFixture f;
    f.platform.submitAllowed=false;
    f.Start(false);
    EXPECT_EQ(f.platform.requests.size(),0u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_TRUE(f.completion.results.empty());
}

TEST(TargetPrimaryB,ResourceSubmitResponseBeforeReturnIsBufferedUntilHandoffClassified)
{
    ResourceFixture f;
    f.platform.onSynchronousResponse=[&](uint64_t token,const RadioSwitchRequest &request) {
        EXPECT_TRUE(f.radio.AcceptDeviceResult({token,
            Result(request,EffectKnowledge::ConfirmedApplied,0),true,true,false,
            std::make_optional(static_cast<int32_t>(token))}));
    };
    f.Start(false);
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(f.platform.requests.size(),1u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
}

TEST(TargetPrimaryB,ResourceSubmitDuplicatesDeduplicateAndConflictsQuarantine)
{
    ResourceFixture duplicate;
    duplicate.platform.onSynchronousResponse=[&](uint64_t token,const RadioSwitchRequest &request) {
        const RadioDeviceResult evidence {token,Result(request,EffectKnowledge::ConfirmedApplied,0),true,true,
            false,std::make_optional(static_cast<int32_t>(token))};
        EXPECT_TRUE(duplicate.radio.AcceptDeviceResult(evidence));
        EXPECT_TRUE(duplicate.radio.AcceptDeviceResult(evidence));
    };
    duplicate.Start(false);
    ASSERT_TRUE(duplicate.scheduler.WaitOwnerIdle());
    EXPECT_EQ(duplicate.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(duplicate.radio.ReadResourceState(duplicate.key),ResourceState::Available);

    ResourceFixture conflict;
    conflict.platform.onSynchronousResponse=[&](uint64_t token,const RadioSwitchRequest &request) {
        EXPECT_TRUE(conflict.radio.AcceptDeviceResult({token,
            Result(request,EffectKnowledge::ConfirmedApplied,0),true,true,false,
            std::make_optional(static_cast<int32_t>(token))}));
        EXPECT_TRUE(conflict.radio.AcceptDeviceResult({token,
            Result(request,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull),true,true,false,
            std::make_optional(static_cast<int32_t>(token))}));
    };
    conflict.Start(false);
    EXPECT_EQ(conflict.radio.ReadResourceState(conflict.key),ResourceState::Quarantined);
    EXPECT_TRUE(conflict.completion.results.empty());
}

TEST(TargetPrimaryB,ResourceSubmitConfirmedNotSentRejectsContradictoryCallback)
{
    ResourceFixture f;
    f.platform.disposition=RadioSubmitDisposition::ConfirmedNotSent;
    f.platform.onSynchronousResponse=[&](uint64_t token,const RadioSwitchRequest &request) {
        EXPECT_TRUE(f.radio.AcceptDeviceResult({token,
            Result(request,EffectKnowledge::ConfirmedApplied,0),true,true,false,
            std::make_optional(static_cast<int32_t>(token))}));
    };
    f.Start(false);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_TRUE(f.completion.results.empty());
}

TEST(TargetPrimaryB,ResourceBarrierIsNotAcquiredForC0)
{
    ResourceFixture f;
    f.platform.alreadyPrimary=true;
    f.StartC0();
    EXPECT_TRUE(f.platform.requests.empty());
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
}

TEST(TargetPrimaryB,ResourceBarrierConfirmedNotSentReturnsAvailable)
{
    ResourceFixture f;
    f.platform.disposition=RadioSubmitDisposition::ConfirmedNotSent;
    f.Start(false);
    EXPECT_TRUE(f.platform.requests.empty());
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_TRUE(f.completion.results.empty());
}

TEST(TargetPrimaryB,ResourceBarrierBEndsBeforeResultIsDelivered)
{
    ResourceFixture f;
    f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    EXPECT_EQ(f.radio.ReadTransportSerial(f.platform.requests[0].first),
        std::make_optional(static_cast<int32_t>(f.platform.requests[0].first)));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
    EXPECT_TRUE(f.completion.results.empty());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_FALSE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.radio.RejectedStaleResults(),1u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"1");
    EXPECT_EQ(f.parameters.values.at(kData),"1");
}

TEST(TargetPrimaryB,ResourceBarrierRejectsWrongBindingWithoutReleasingLease)
{
    ResourceFixture f;
    f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    auto wrong=Result(f.platform.requests[0].second,EffectKnowledge::ConfirmedApplied,0);
    wrong.expectedBinding=BindingRef{{9},{1},9};
    EXPECT_FALSE(f.radio.AcceptDeviceResult({f.platform.requests[0].first,wrong,true,true,false,
        std::make_optional(static_cast<int32_t>(f.platform.requests[0].first))}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
    EXPECT_TRUE(f.completion.results.empty());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
}

TEST(TargetPrimaryB,ResourceRejectsOldSerialOnCurrentTransportToken)
{
    ResourceFixture f;
    f.Start(true);
    ASSERT_EQ(f.platform.requests.size(),2u);
    const auto &sent=f.platform.requests[1];
    auto response=Result(sent.second,EffectKnowledge::ConfirmedApplied,0);
    EXPECT_FALSE(f.radio.AcceptDeviceResult({sent.first,response,true,true}));
    EXPECT_FALSE(f.radio.AcceptDeviceResult({sent.first,response,true,true,false,
        std::make_optional(static_cast<int32_t>(f.platform.requests[0].first))}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
    EXPECT_EQ(f.radio.RejectedStaleResults(),2u);
}

TEST(TargetPrimaryB,ResourceBarrierRejectsDuplicateMemberSubmission)
{
    ResourceFixture f;
    f.Start(true);
    ASSERT_EQ(f.platform.requests.size(),2u);
    const auto duplicate=f.platform.requests[0].second;
    f.radio.SwitchPrimaryRadio(duplicate);
    EXPECT_FALSE(f.radio.WasLastSubmissionAccepted(duplicate));
    EXPECT_EQ(f.platform.requests.size(),2u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
}

TEST(TargetPrimaryB,ResourceBarrierBTimeoutQuarantinesUntilRealRecovery)
{
    ResourceFixture f;
    const auto request=f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    EXPECT_TRUE(f.service->AcceptPrimarySwitchTimeout(request,45000));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_FALSE(f.service->AcceptC1RetryDue(request));
    EXPECT_FALSE(f.radio.Recover(f.platform.requests[0].second.step,
        {f.platform.requests[0].second.step,EffectKnowledge::Unknown,false,true,true}));
    EXPECT_FALSE(f.radio.Recover(f.platform.requests[0].second.step,
        {f.platform.requests[0].second.step,EffectKnowledge::ConfirmedNotApplied,true,true,true}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,ResourceBarrierLateBResultRecoversOnlyWithCurrentStateProof)
{
    ResourceFixture f;
    const auto request=f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    ASSERT_TRUE(f.service->AcceptPrimarySwitchTimeout(request,45000));
    EXPECT_FALSE(f.ResultAt(0,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull,
        false,true,true,false));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull,
        false,true,true,true));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_FALSE(f.service->ReadLegacyAssignment().lastPrimary.has_value());
}

TEST(TargetPrimaryB,ResourceRetainsOwnerDeliveryWhenDispatchIsStopped)
{
    ResourceFixture f;
    f.Start(false);
    f.scheduler.rejectOwnerDispatch=true;
    EXPECT_FALSE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results.size(),0u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_TRUE(f.service->ReadLegacyAssignment().primarySwitchInProgress);
    EXPECT_FALSE(f.radio.OwnerCallbacksDrained());
    f.scheduler.rejectOwnerDispatch=false;
    EXPECT_TRUE(f.radio.FlushOwnerDeliveries());
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_TRUE(f.radio.OwnerCallbacksDrained());
}

TEST(TargetPrimaryB,ResourceRequiresDispatcherBeforeProductSubmit)
{
    ResourceFixture f;
    f.radio.BindOwnerDispatch(nullptr);
    EXPECT_EQ(f.platform.requests.size(),0u);
    f.target=f.Seed();
    f.service->StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{64}},false},std::nullopt,f.target});
    EXPECT_EQ(f.platform.requests.size(),0u);
}

TEST(TargetPrimaryB,ResourceC1ConcurrentMemberCallbacksStayOwnerSerialized)
{
    ResourceFixture f;
    f.Start(true);
    ASSERT_EQ(f.platform.requests.size(),2u);
    auto first=std::async(std::launch::async,[&] {
        const auto &sent=f.platform.requests[0];
        return f.radio.AcceptDeviceResult({sent.first,
            Result(sent.second,EffectKnowledge::ConfirmedApplied,0),true,true,false,
            std::make_optional(static_cast<int32_t>(sent.first))});
    });
    auto second=std::async(std::launch::async,[&] {
        const auto &sent=f.platform.requests[1];
        return f.radio.AcceptDeviceResult({sent.first,
            Result(sent.second,EffectKnowledge::ConfirmedApplied,0),true,true,false,
            std::make_optional(static_cast<int32_t>(sent.first))});
    });
    EXPECT_TRUE(first.get()); EXPECT_TRUE(second.get());
    ASSERT_TRUE(f.scheduler.WaitOwnerIdle());
    EXPECT_EQ(f.platform.requests.size(),4u);
    EXPECT_TRUE(f.radio.ReadResourceState(f.key)==ResourceState::InFlight);
}

TEST(TargetPrimaryB,ResourceBarrierC1AutomaticallyRetriesOnlyAfterEachPhaseEnded)
{
    ResourceFixture f;
    const auto request=f.Start(true);
    ASSERT_EQ(f.platform.requests.size(),2u);
    for (uint32_t round=0;round<6;++round) {
        const size_t begin=round*4;
        ASSERT_EQ(f.platform.requests.size(),begin+2u);
        EXPECT_TRUE(f.ResultAt(begin,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
        EXPECT_EQ(f.platform.requests.size(),begin+2u);
        EXPECT_TRUE(f.ResultAt(begin+1,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
        ASSERT_EQ(f.platform.requests.size(),begin+4u);
        EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
        EXPECT_TRUE(f.ResultAt(begin+2,EffectKnowledge::ConfirmedApplied,0));
        EXPECT_TRUE(f.ResultAt(begin+3,EffectKnowledge::ConfirmedApplied,0));
        EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
        EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
        if (round<5) {
            f.scheduler.Advance(1);
            ASSERT_TRUE(f.scheduler.WaitUntil(round+1,round+1));
            ASSERT_TRUE(f.scheduler.WaitIdle());
            ASSERT_EQ(f.platform.requests.size(),begin+6u);
            EXPECT_EQ(f.platform.requests.back().second.step.request,request);
        }
    }
    EXPECT_EQ(f.platform.requests.size(),24u);
    EXPECT_EQ(f.scheduler.Audit().delivered,5u);
    EXPECT_EQ(f.output.phases.size(),12u);
    EXPECT_EQ(f.parameters.values.at(kPrimary),"0");
    EXPECT_EQ(f.parameters.values.at(kIccid),kEncoded0);
    EXPECT_EQ(f.parameters.values.at(kData),"0");
    EXPECT_TRUE(f.radio.DeviceResourcesDrained());
}

TEST(TargetPrimaryB,ResourceBarrierC1UpdateNotifyCannotCompleteWithoutReuseEvidence)
{
    ResourceFixture f;
    f.Start(true);
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_TRUE(f.ResultAt(1,EffectKnowledge::ConfirmedApplied,0));
    ASSERT_EQ(f.platform.requests.size(),4u);
    EXPECT_FALSE(f.ResultAt(2,EffectKnowledge::ConfirmedApplied,0,false,true,true));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::InFlight);
    EXPECT_FALSE(f.ResultAt(2,EffectKnowledge::ConfirmedApplied,0,true,true,false));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_TRUE(f.ResultAt(3,EffectKnowledge::ConfirmedApplied,0,true,true,true,true));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_TRUE(f.ResultAt(2,EffectKnowledge::ConfirmedApplied,0,true,true,true,true));
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
    EXPECT_EQ(f.platform.requests.size(),4u);
    EXPECT_EQ(f.completion.results.size(),1u);
}

TEST(TargetPrimaryB,ResourceBarrierDoesNotScheduleRetryBeforeCompleteMembersEnd)
{
    ResourceFixture f;
    const auto request=f.Start(true);
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
    EXPECT_TRUE(f.ResultAt(1,EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull));
    ASSERT_EQ(f.platform.requests.size(),4u);
    EXPECT_TRUE(f.ResultAt(2,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.scheduler.Audit().scheduled,0u);
    EXPECT_FALSE(f.scheduler.OwnerCall([&] { return f.service->AcceptC1RetryDue(request); }));
    EXPECT_TRUE(f.ResultAt(3,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.scheduler.Audit().scheduled,1u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
}

TEST(TargetPrimaryB,ResourceCallbackAfterOwnerDestructionCannotAccessDanglingSink)
{
    ResourceFixture f;
    f.Start(false);
    ASSERT_EQ(f.platform.requests.size(),1u);
    const auto weak=std::weak_ptr<AssignmentService>(f.service);
    f.service.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_EQ(f.completion.results.size(),0u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Available);
}

TEST(TargetPrimaryB,ResourceBarrierStopSeparatesOwnerAndDeviceDrain)
{
    ResourceFixture f;
    f.Start(false);
    EXPECT_FALSE(f.radio.StopAccepting());
    EXPECT_FALSE(f.radio.DeviceResourcesDrained());
    EXPECT_TRUE(f.ResultAt(0,EffectKnowledge::ConfirmedApplied,0));
    EXPECT_TRUE(f.radio.DeviceResourcesDrained());
    EXPECT_TRUE(f.radio.OwnerCallbacksDrained());
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_FALSE(f.service->StartPrimarySwitch({SlotTarget{{1}},true,
        {SynchronousCompletionRoute{SynchronousWaitRef{62}},false},std::nullopt,f.target}) == RequestId{});
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
