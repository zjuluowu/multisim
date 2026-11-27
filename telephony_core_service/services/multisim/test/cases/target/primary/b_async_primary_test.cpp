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
    std::optional<C0ParameterKey> uncertainKey;
    std::optional<C0ParameterOperation> uncertainWrite;
    std::optional<C0ParameterEvidence> parameterEvidence;
    std::function<void(const std::string &)> beforeRead;
    std::function<void(const C0ParameterOperation &)> beforeTrackedWriteReturns;
    std::optional<std::string> unreadableKey;
    C0ParameterOutcome uncertainOutcome {{-1, false}, EffectKnowledge::Unknown};
    AssignmentParameterResult Read(const std::string &key,const std::string &fallback,std::string &value) override
    {
        if (beforeRead) { beforeRead(key); }
        if (key == unreadableKey) { return {-1, false}; }
        value=values.count(key)?values[key]:fallback;
        return {0,true};
    }
    AssignmentParameterResult Write(const std::string &key,const std::string &value) override
    { writes.emplace_back(key,value); values[key]=value; return {0,true}; }
    C0ParameterOutcome WriteC0Parameter(const C0ParameterOperation &operation, const std::string &key,
        const std::string &value) override
    {
        if (operation.key == uncertainKey) {
            uncertainWrite = operation;
            writes.emplace_back(key, value);
            if (beforeTrackedWriteReturns) { beforeTrackedWriteReturns(operation); }
            return uncertainOutcome;
        }
        const auto outcome = AssignmentParameterPort::WriteC0Parameter(operation, key, value);
        if (beforeTrackedWriteReturns) { beforeTrackedWriteReturns(operation); }
        return outcome;
    }
    std::optional<C0ParameterEvidence> ReadC0ParameterEvidence(const C0ParameterOperation &) override
    {
        return parameterEvidence;
    }
};
const std::vector<RadioProtocolMember> kControlledProtocolMembers {
    {100, {0}, 8}, {200, {1}, 9}
};
const RadioSendQualification kOnlyRadioQualification = [](const RadioPermissionRegistration &permit) {
    return permit([] { return true; });
};

class Radio final : public RadioExecutionPort {
public:
    bool directSupported {true};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    {
        return {slot, slot.legacySlotId == 0 ? 0 : 1, directSupported, false, 9, true,
            PrimaryProtocolRouting {slot.legacySlotId == 0 ? 100u : 200u, kControlledProtocolMembers}};
    }
    RadioSubmitDisposition SwitchPrimaryRadio(const RadioSwitchRequest &request,
        const RadioSendQualification &qualify) override
    {
        if (!qualify([](const RadioSendPermit &stillAccepting) { return stillAccepting(); })) {
            return RadioSubmitDisposition::ConfirmedNotSent;
        }
        requests.push_back(request);
        return RadioSubmitDisposition::HandoffAccepted;
    }
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
    std::function<void()> beforeSchedule;

    std::optional<uint64_t> ScheduleRetry(const ScheduledRetryEvent &event, uint64_t delayMilliseconds,
        const std::weak_ptr<ScheduledRetrySink> &sink) override
    {
        if (beforeSchedule) { beforeSchedule(); }
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
    std::vector<RadioProtocolMember> protocolMembers = kControlledProtocolMembers;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    {
        return {slot, alreadyPrimary || slot.legacySlotId == 0 ? 0 : 1, directSupported, false, 9, true,
            PrimaryProtocolRouting {slot.legacySlotId == 0 ? 100u : 200u, protocolMembers}};
    }
    RadioSubmitDisposition disposition {RadioSubmitDisposition::HandoffAccepted};
    std::function<void(const RadioSwitchRequest &)> beforeSendPermission;
    std::function<void(uint64_t,const RadioSwitchRequest &)> onSynchronousResponse;
    std::function<void()> beforeEnd;
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t token,
        const RadioSendPermit &permit) override
    {
        if (beforeSendPermission) { beforeSendPermission(request); }
        if ((request.member && std::find(protocolMembers.begin(), protocolMembers.end(), *request.member) ==
            protocolMembers.end()) || !permit()) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        if (onSynchronousResponse) { onSynchronousResponse(token, request); }
        if (disposition == RadioSubmitDisposition::ConfirmedNotSent) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        if (!submitAllowed) { return {RadioSubmitDisposition::HandoffUnknown, std::nullopt}; }
        requests.emplace_back(token, request);
        return {disposition, omitSerial ? std::nullopt :
            std::make_optional(static_cast<int32_t>(token))};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override
    {
        ++ended;
        if (beforeEnd) { beforeEnd(); }
    }
    unsigned ended {0};
};

struct ResourceFixture {
    SlotCatalog catalog;
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
    explicit ResourceFixture(std::vector<SlotDescriptor> endpoints = {
        {{0}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
        {{1}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false}})
        : catalog(std::move(endpoints), {2, 2, 2, 2, 2})
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
    radio.SwitchPrimaryRadio(request, kOnlyRadioQualification);
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
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
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
    f.scheduler.BlockOwnerQueue();
    EXPECT_EQ(lifecycle.StopAcceptingAndDrain(),PrimaryHostLifecycle::DrainResult::CallbacksStillInFlight);
    f.scheduler.ReleaseOwnerQueue();
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
    const RadioSwitchRequest request {{RequestId{1,{6,2},RequestOwner::Assignment},2,0},
        {0},{"m1"},PrimarySwitchMode::WithoutModemRestart,std::nullopt,1,0,1};
    EXPECT_EQ(f.radio.SwitchPrimaryRadio(request, kOnlyRadioQualification), RadioSubmitDisposition::HandoffUnknown);
    EXPECT_EQ(f.platform.requests.size(),1u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
}

TEST(TargetPrimaryB,ResourceSubmitUnknownQuarantinesInsteadOfConfirmedCancel)
{
    ResourceFixture f;
    f.platform.submitAllowed=false;
    const auto request = f.Start(false);
    EXPECT_EQ(f.platform.requests.size(),0u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key),ResourceState::Quarantined);
    EXPECT_TRUE(f.completion.results.empty());
    EXPECT_TRUE(f.service->AcceptPrimarySwitchTimeout(request, 45000));
    EXPECT_FALSE(f.service->AcceptPrimarySwitchTimeout(request, 45000));
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
    EXPECT_EQ(f.radio.ReadResourceState(f.key), ResourceState::Quarantined);
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

TEST(TargetPrimaryB,C1InlineRepliesFinishWithoutAccessingRetiredRound)
{
    struct InlineOwner : RadioOwnerDispatchPort {
        bool PostRadioOwnerTask(std::function<void()> task) override { task(); return true; }
        bool RadioOwnerCallbacksDrained() const override { return true; }
    };
    ResourceFixture f;
    auto owner = std::make_shared<InlineOwner>();
    f.radio.BindOwnerDispatch(owner);
    f.platform.onSynchronousResponse = [&](uint64_t token, const RadioSwitchRequest &request) {
        EXPECT_TRUE(f.radio.AcceptDeviceResult({token,
            Result(request, EffectKnowledge::ConfirmedApplied, 0, request.phase == 2),
            true, true, false, static_cast<int32_t>(token)}));
    };
    f.Start(true);

    std::vector<uint32_t> phases;
    for (const auto &entry : f.platform.requests) { phases.push_back(entry.second.phase); }
    EXPECT_EQ(phases, (std::vector<uint32_t>{1, 1, 2, 2, 4, 4}));
    EXPECT_EQ(f.completion.results, (std::vector<int32_t>{0}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key), ResourceState::Available);
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,RadioProtocolsUseNativeMembersInsteadOfEndpointCount)
{
    ResourceFixture fixture({
        {{0}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
        {{1}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false},
        {{2}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
        {{3}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false}});
    fixture.platform.protocolMembers.push_back(kControlledProtocolMembers.back());

    fixture.Start(true);
    for (const uint32_t phase : {1u, 2u, 4u}) {
        const size_t begin = phase == 1 ? 0u : phase == 2 ? 2u : 4u;
        ASSERT_EQ(fixture.platform.requests.size(), begin + 2);
        for (size_t member = 0; member < kControlledProtocolMembers.size(); ++member) {
            const auto request = fixture.platform.requests[begin + member].second;
            EXPECT_EQ(request.phase, phase);
            EXPECT_EQ(request.member, kControlledProtocolMembers[member]);
            EXPECT_EQ(request.memberCount, 2u);
            ASSERT_TRUE(fixture.ResultAt(begin + member, EffectKnowledge::ConfirmedApplied, 0, phase == 2));
        }
        ASSERT_TRUE(fixture.scheduler.WaitOwnerIdle());
    }
    EXPECT_EQ(fixture.completion.results, std::vector<int32_t> {0});
    EXPECT_EQ(fixture.radio.SubmittedCommands(), 6u);
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
}

TEST(TargetPrimaryB,ConflictingNativeMemberRoutesRejectBeforeStoreAndSubmit)
{
    ResourceFixture fixture;
    fixture.platform.protocolMembers.push_back({200, {2}, 11});
    fixture.Start(true);

    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.parameters.writes.empty());
    EXPECT_EQ(fixture.completion.results, std::vector<int32_t> {-1});
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
}

TEST(TargetPrimaryB,RouteRevocationStopsUnsentMemberAndKeepsOriginalRound)
{
    ResourceFixture fixture;
    fixture.platform.beforeSendPermission = [&](const RadioSwitchRequest &request) {
        if (request.target == SlotId {1}) { ++fixture.platform.protocolMembers.back().routeRevision; }
    };

    fixture.Start(true);

    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_EQ(fixture.platform.requests.front().second.phase, 1u);
    EXPECT_EQ(fixture.platform.requests.front().second.member, kControlledProtocolMembers.front());
    EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Quarantined);
    EXPECT_FALSE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0, false, true, true, false));
    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0, false, true, true, true));
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_TRUE(fixture.completion.results.empty());
}

TEST(TargetPrimaryB,RemovedBindingDeniesSendEvenWhenNativeRoutesStayCurrent)
{
    for (bool threePhase : {false, true}) {
        SCOPED_TRACE(threePhase);
        ResourceFixture fixture;
        unsigned attempted = 0;
        fixture.platform.beforeSendPermission = [&](const RadioSwitchRequest &) {
            ++attempted;
            EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::InFlight);
            fixture.topology.AcceptCardRemoval(fixture.target);
        };

        fixture.Start(threePhase);

        EXPECT_EQ(attempted, 1u);
        EXPECT_EQ(fixture.platform.protocolMembers, kControlledProtocolMembers);
        EXPECT_TRUE(fixture.platform.requests.empty());
        EXPECT_EQ(fixture.radio.SubmittedCommands(), 0u);
        EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
        EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Available);
        EXPECT_TRUE(fixture.output.roles.empty());
    }
}

TEST(TargetPrimaryB,RevocationAfterPermissionKeepsFirstMemberAndDeniesRemainingMembers)
{
    ResourceFixture fixture;
    fixture.platform.onSynchronousResponse = [&](uint64_t, const RadioSwitchRequest &request) {
        EXPECT_EQ(request.target, SlotId {0});
        fixture.topology.AcceptCardRemoval(fixture.target);
    };

    fixture.Start(true);

    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Quarantined);
    EXPECT_FALSE(fixture.radio.DeviceResourcesDrained());
    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0, false, true, true, true));
    EXPECT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_TRUE(fixture.output.roles.empty());
}

TEST(TargetPrimaryB,C1StopWithCheckInFlightDoesNotAuthorizeAnotherPhase)
{
    ResourceFixture fixture;
    fixture.Start(true);
    ASSERT_EQ(fixture.platform.requests.size(), 2u);

    EXPECT_FALSE(fixture.service->StopAcceptingAndCancelScheduledRetries());
    for (size_t member = 0; member < 2; ++member) {
        EXPECT_TRUE(fixture.ResultAt(member, EffectKnowledge::ConfirmedApplied, 0));
    }

    EXPECT_EQ(fixture.platform.requests.size(), 2u);
    EXPECT_TRUE(fixture.service->HasPendingDeviceEffects());
    EXPECT_FALSE(fixture.service->StopAcceptingAndCancelScheduledRetries());
    EXPECT_TRUE(fixture.output.roles.empty());
}

TEST(TargetPrimaryB,StopDuringTopologyQualificationDeniesFinalPermission)
{
    class PausingTopology final : public PrimaryTopologyPort {
    public:
        explicit PausingTopology(TopologyService &source) : source_(source), resume_(release.get_future()) {}
        std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
        {
            return source_.ReadPrimaryTarget(slot);
        }
        bool WithCurrentPrimaryTarget(const PrimaryTargetFacts &facts,
            const std::function<void()> &admit) const override
        {
            // The second check owns the B send grant or the C1 first Store write.
            if (++qualifications_ == 1) { return source_.WithCurrentPrimaryTarget(facts, admit); }
            return source_.WithCurrentPrimaryTarget(facts, [&] {
                entered.set_value();
                resume_.wait();
                admit();
            });
        }
        RequestId BeginRouteMapping(const StepRef &parent, SlotTarget target) override
        {
            return source_.BeginRouteMapping(parent, target);
        }
        mutable std::promise<void> entered;
        std::promise<void> release;
    private:
        TopologyService &source_;
        std::future<void> resume_;
        mutable unsigned qualifications_ {0};
    };
    for (const bool c1 : {false, true}) {
        SCOPED_TRACE(c1);
        ResourceFixture fixture;
        PausingTopology topology(fixture.topology);
        auto entered = topology.entered.get_future();
        fixture.service = std::make_shared<AssignmentService>(fixture.catalog, fixture.store, fixture.radio,
            fixture.output, topology, fixture.completion, RuntimeEpoch {6, 2});
        fixture.radio.BindResultSink(fixture.service);
        fixture.service->BindRetryScheduler(fixture.schedulerOwner);
        auto start = std::async(std::launch::async, [&] { return fixture.Start(c1); });
        if (entered.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            topology.release.set_value();
            FAIL() << "The original write or send did not reach the T qualification boundary";
        }
        fixture.service->RequestStopAccepting();
        topology.release.set_value();
        start.get();
        fixture.service.reset();
        EXPECT_EQ(fixture.parameters.writes.size(), c1 ? 0u : 1u);
        EXPECT_TRUE(fixture.platform.requests.empty());
        EXPECT_EQ(fixture.radio.SubmittedCommands(), 0u);
        EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    }
}

TEST(TargetPrimaryB,TimeoutSealsPermissionBeforeBlockingInPlatformEnd)
{
    ResourceFixture fixture;
    std::promise<RequestId> platformEntered;
    auto entered = platformEntered.get_future();
    std::promise<void> releasePermission;
    auto permission = releasePermission.get_future();
    std::promise<void> endEntered;
    auto ending = endEntered.get_future();
    std::promise<void> releaseEnd;
    auto mayEnd = releaseEnd.get_future();
    bool firstSubmit = true;
    fixture.platform.beforeSendPermission = [&](const RadioSwitchRequest &request) {
        if (!firstSubmit) { return; }
        firstSubmit = false;
        platformEntered.set_value(request.step.request);
        permission.wait();
    };
    fixture.platform.beforeEnd = [&] {
        endEntered.set_value();
        mayEnd.wait();
    };
    auto start = std::async(std::launch::async, [&] { return fixture.Start(false); });
    if (entered.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        releasePermission.set_value();
        releaseEnd.set_value();
        FAIL() << "Submit did not reach the send permission boundary";
    }
    const auto original = entered.get();
    auto timeout = std::async(std::launch::async,
        [&] { return fixture.service->AcceptPrimarySwitchTimeout(original, 45000); });
    const auto reachedEnd = ending.wait_for(std::chrono::seconds(3));
    releasePermission.set_value();
    const auto classified = start.wait_for(std::chrono::seconds(3));
    const auto next = [&] {
        return fixture.service->StartPrimarySwitch({SlotTarget{{1}}, true,
            {SynchronousCompletionRoute{SynchronousWaitRef{62}}, false}, std::nullopt, fixture.target});
    };
    if (reachedEnd == std::future_status::ready && classified == std::future_status::ready) {
        EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
        EXPECT_TRUE(fixture.service->HasPendingDeviceEffects()) << "Original timeout delivery is still active";
        const auto writes = fixture.parameters.writes.size();
        next();
        EXPECT_EQ(fixture.parameters.writes.size(), writes) << "A released shared writes before original timeout delivery";
        EXPECT_TRUE(fixture.platform.requests.empty());
    }
    releaseEnd.set_value();

    EXPECT_EQ(reachedEnd, std::future_status::ready);
    EXPECT_EQ(classified, std::future_status::ready)
        << "Platform end held the Assignment commit lock needed by send permission";
    EXPECT_EQ(start.get(), original);
    EXPECT_TRUE(timeout.get());
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {-1, kLegacyLocalPointerNull}));

    fixture.platform.beforeEnd = {};
    next();
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0));
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {-1, kLegacyLocalPointerNull, 0}));
}

TEST(TargetPrimaryB,PlatformHandoffDoesNotHoldAssignmentCommitLock)
{
    for (bool threePhase : {false, true}) {
        SCOPED_TRACE(threePhase ? "C1 CHECK" : "B");
        ResourceFixture fixture;
        std::promise<void> entered;
        auto platformEntered = entered.get_future();
        std::promise<void> release;
        auto releasePlatform = release.get_future();
        fixture.platform.onSynchronousResponse = [&](uint64_t, const RadioSwitchRequest &request) {
            if (!(request.target == SlotId {1})) { return; }
            entered.set_value();
            releasePlatform.wait();
        };
        auto start = std::async(std::launch::async, [&] { return fixture.Start(threePhase); });
        platformEntered.wait();

        auto inspect = std::async(std::launch::async, [&] {
            return fixture.service->HasPendingDeviceEffects();
        });
        const auto readyWhileBlocked = inspect.wait_for(std::chrono::seconds(2));
        release.set_value();
        (void)start.get();
        EXPECT_EQ(readyWhileBlocked, std::future_status::ready);
        EXPECT_TRUE(inspect.get());
    }
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

TEST(TargetPrimaryB,ResourceSubmitMemberCannotDuplicateOrReleaseAnotherInSubmit)
{
    ResourceFixture f;
    const RadioSwitchRequest first {{RequestId{1,{6,2},RequestOwner::Assignment},2,0},
        {0},{"m1"},PrimarySwitchMode::ThreePhaseProtocol,std::nullopt,1,0,2};
    auto second = first;
    second.target = {1};
    auto rival = first;
    rival.step.request.value = 2;
    unsigned firstCalls = 0;
    unsigned secondCalls = 0;
    unsigned rivalCalls = 0;
    f.platform.onSynchronousResponse = [&](uint64_t, const RadioSwitchRequest &request) {
        if (request.step == rival.step) { ++rivalCalls; return; }
        if (request.target == second.target) { ++secondCalls; return; }
        if (++firstCalls != 1) { return; }

        f.radio.SwitchPrimaryRadio(first, kOnlyRadioQualification);
        f.platform.disposition = RadioSubmitDisposition::ConfirmedNotSent;
        f.radio.SwitchPrimaryRadio(second, kOnlyRadioQualification);
        EXPECT_EQ(f.radio.ReadResourceState(f.key), ResourceState::InFlight);
        EXPECT_FALSE(f.radio.Recover(first.step, {first.step, EffectKnowledge::ConfirmedNotApplied,
            true, true, true}));
        f.radio.SwitchPrimaryRadio(rival, kOnlyRadioQualification);
        f.platform.disposition = RadioSubmitDisposition::HandoffAccepted;
    };
    f.radio.SwitchPrimaryRadio(first, kOnlyRadioQualification);
    EXPECT_EQ(firstCalls, 1u);
    EXPECT_EQ(secondCalls, 1u);
    EXPECT_EQ(rivalCalls, 0u);
    EXPECT_EQ(f.radio.ReadResourceState(f.key), ResourceState::Quarantined);
    ASSERT_EQ(f.platform.requests.size(), 1u);
    const auto token = f.platform.requests.front().first;
    EXPECT_EQ(f.radio.ReadTransportSerial(token), static_cast<int32_t>(token));
    EXPECT_TRUE(f.radio.AcceptDeviceResult({token, Result(first, EffectKnowledge::ConfirmedApplied, 0),
        true, true, true, static_cast<int32_t>(token)}));
    EXPECT_EQ(f.radio.ReadResourceState(f.key), ResourceState::Available);
}

TEST(TargetPrimaryB,ResourceStopSealsUnsubmittedMembersAndRetainsOriginalTransmission)
{
    ResourceFixture fixture;
    const RadioSwitchRequest original {{RequestId {1, {6, 2}, RequestOwner::Assignment}, 2, 0},
        {0}, {"m1"}, PrimarySwitchMode::ThreePhaseProtocol, std::nullopt, 1, 1, 2};
    auto unsubmitted = original;
    unsubmitted.target = {1};

    fixture.platform.onSynchronousResponse = [&](uint64_t, const RadioSwitchRequest &) {
        EXPECT_FALSE(fixture.radio.StopAccepting());
        EXPECT_EQ(fixture.radio.SwitchPrimaryRadio(unsubmitted, kOnlyRadioQualification),
            RadioSubmitDisposition::HandoffUnknown);
        EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::InFlight);
    };
    ASSERT_EQ(fixture.radio.SwitchPrimaryRadio(original, kOnlyRadioQualification),
        RadioSubmitDisposition::HandoffAccepted);
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Quarantined);
    const auto token = fixture.platform.requests.front().first;
    const auto result = Result(original, EffectKnowledge::ConfirmedApplied, 0);
    EXPECT_FALSE(fixture.radio.AcceptDeviceResult({token, result, true, true, false, static_cast<int32_t>(token)}));
    EXPECT_TRUE(fixture.radio.AcceptDeviceResult({token, result, true, true, true, static_cast<int32_t>(token)}));
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Available);
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
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));

    f.platform.disposition = RadioSubmitDisposition::HandoffAccepted;
    f.service->StartPrimarySwitch({SlotTarget{{1}}, true,
        {SynchronousCompletionRoute{SynchronousWaitRef{62}}, false}, std::nullopt, f.target});
    ASSERT_EQ(f.platform.requests.size(), 1u);
    EXPECT_TRUE(f.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0));
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull, 0}));
}

TEST(TargetPrimaryB,UncertainPreferenceRetainsOriginalStoreReceiptWithoutRadioCommand)
{
    ResourceFixture fixture;
    fixture.parameters.uncertainKey = C0ParameterKey::Preference;
    const auto original = fixture.Start(false);
    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto write = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(write.original.request, original);
    EXPECT_EQ(write.key, C0ParameterKey::Preference);
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    EXPECT_TRUE(fixture.service->ReadC0Receipt());

    const auto next = [&] {
        fixture.service->StartPrimarySwitch({SlotTarget{{1}}, true,
            {SynchronousCompletionRoute{SynchronousWaitRef{62}}, false}, std::nullopt, fixture.target});
    };
    next();
    EXPECT_EQ(fixture.parameters.writes.size(), 1u);
    EXPECT_TRUE(fixture.platform.requests.empty());

    auto wrong = write;
    ++wrong.original.step;
    fixture.parameters.parameterEvidence = C0ParameterEvidence {wrong, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_FALSE(fixture.service->ReconcileC0Parameter(write));
    EXPECT_TRUE(fixture.service->ReadC0Receipt());

    fixture.parameters.values[kPreference] = "true";
    fixture.parameters.parameterEvidence = C0ParameterEvidence {write, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(fixture.service->ReconcileC0Parameter(write));
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull, -1}));

    fixture.parameters.uncertainKey.reset();
    next();
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0));
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull, -1, 0}));
}

TEST(TargetPrimaryB,DeviceSuccessDoesNotRetireUncertainPrimaryParameter)
{
    ResourceFixture fixture;
    fixture.parameters.uncertainKey = C0ParameterKey::PrimaryIccid;
    const auto request = fixture.Start(false);
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0));

    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto write = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(write.original.request, request);
    EXPECT_GT(write.original.step, fixture.platform.requests.front().second.step.step);
    ASSERT_TRUE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.service->ReadC0Receipt()->outstanding, AssignmentService::C0Stage::PrimaryParameters);
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    EXPECT_EQ(fixture.parameters.values[kData], "0");

    fixture.service->RequestStopAccepting();
    fixture.topology.AcceptCardRemoval(fixture.target);
    EXPECT_FALSE(fixture.service->ReconcileC0Parameter(write));
    fixture.parameters.values[kIccid] = kEncoded1;
    fixture.parameters.parameterEvidence = C0ParameterEvidence {write, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(fixture.service->ReconcileC0Parameter(write));
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_FALSE(fixture.service->ReadLegacyAssignment().lastPrimary);
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,SuccessfulDeviceEndAfterBindingRemovalCannotReportBusinessSuccess)
{
    ResourceFixture fixture;
    fixture.Start(false);
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    const auto original = fixture.platform.requests.front().second.step;
    fixture.platform.beforeEnd = [&] {
        fixture.topology.AcceptCardRemoval(fixture.target);
        const auto replacement = fixture.topology.AcceptStoredDiscovery(Discovery(1, 3, "replacement-card"),
            Account({9}, "replacement-card", {1}), Stored({9}));
        EXPECT_TRUE(replacement);
    };

    EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedApplied, 0));

    ASSERT_EQ(fixture.parameters.writes.size(), 1u);
    EXPECT_EQ(fixture.parameters.writes.front().first, kPreference);
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_FALSE(fixture.service->ReadLegacyAssignment().lastPrimary);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    EXPECT_EQ(fixture.service->ReadPrimaryDeviceEvidence(original), EffectKnowledge::ConfirmedApplied);
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
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
    EXPECT_EQ(f.radio.SwitchPrimaryRadio(duplicate, kOnlyRadioQualification), RadioSubmitDisposition::HandoffUnknown);
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

TEST(TargetPrimaryB,ResourceTimeoutDuringSubmitReleasesOnlyAfterConfirmedNotSent)
{
    for (bool stopAfterTimeout : {false, true}) {
        SCOPED_TRACE(stopAfterTimeout);
        ResourceFixture fixture;
        fixture.platform.disposition = RadioSubmitDisposition::ConfirmedNotSent;
        std::optional<std::pair<uint64_t, RadioSwitchRequest>> original;
        fixture.platform.onSynchronousResponse = [&](uint64_t token, const auto &request) {
            original.emplace(token, request);
            EXPECT_TRUE(fixture.service->AcceptPrimarySwitchTimeout(request.step.request, 45000));
            EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::InFlight);
            EXPECT_FALSE(fixture.radio.DeviceResourcesDrained());
            if (stopAfterTimeout) { EXPECT_FALSE(fixture.radio.StopAccepting()); }
        };

        const auto request = fixture.Start(false);

        ASSERT_TRUE(original);
        EXPECT_EQ(fixture.platform.ended, 1u);
        EXPECT_TRUE(fixture.platform.requests.empty());
        EXPECT_EQ(fixture.radio.SubmittedCommands(), 0u);
        EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Available);
        EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
        EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
        EXPECT_FALSE(fixture.service->AcceptPrimarySwitchTimeout(request, 45000));
        EXPECT_FALSE(fixture.radio.AcceptDeviceResult({original->first,
            Result(original->second, EffectKnowledge::ConfirmedApplied, 0), true, true, true,
            static_cast<int32_t>(original->first)}));
        EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    }
}

TEST(TargetPrimaryB,ResourceTimeoutDuringSubmitRetainsAcceptedOrUnknownHandoff)
{
    for (auto disposition : {RadioSubmitDisposition::HandoffAccepted, RadioSubmitDisposition::HandoffUnknown}) {
        SCOPED_TRACE(static_cast<int>(disposition));
        ResourceFixture fixture;
        fixture.platform.disposition = disposition;
        fixture.platform.onSynchronousResponse = [&](uint64_t, const auto &request) {
            EXPECT_FALSE(fixture.radio.StopAccepting());
            EXPECT_TRUE(fixture.service->AcceptPrimarySwitchTimeout(request.step.request, 45000));
            EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::InFlight);
        };

        fixture.Start(false);

        ASSERT_EQ(fixture.platform.requests.size(), 1u);
        EXPECT_EQ(fixture.radio.SubmittedCommands(), 1u);
        EXPECT_EQ(fixture.radio.ReadResourceState(fixture.key), ResourceState::Quarantined);
        EXPECT_FALSE(fixture.ResultAt(0, EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull));
        EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull,
            false, true, true, true));
        EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
        EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    }
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

TEST(TargetPrimaryB,BQueuedNormalEvidenceRemainsAttributableWhenTimeoutWins)
{
    ResourceFixture fixture;
    const auto request = fixture.Start(false);
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    const auto &[token, original] = fixture.platform.requests.front();
    const auto effect = Result(original, EffectKnowledge::ConfirmedApplied, 0);
    fixture.scheduler.BlockOwnerQueue();
    EXPECT_TRUE(fixture.radio.AcceptDeviceResult({token, effect, true, true, false, static_cast<int32_t>(token)}));
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_FALSE(fixture.radio.OwnerCallbacksDrained());
    EXPECT_TRUE(fixture.service->AcceptPrimarySwitchTimeout(request, 45000));
    EXPECT_FALSE(fixture.service->ReadPrimaryDeviceEvidence(original.step));
    fixture.scheduler.ReleaseOwnerQueue();
    ASSERT_TRUE(fixture.scheduler.WaitOwnerIdle());

    EXPECT_EQ(fixture.service->ReadPrimaryDeviceEvidence(original.step), EffectKnowledge::ConfirmedApplied);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t> {kLegacyLocalPointerNull}));
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_FALSE(fixture.service->ReadLegacyAssignment().lastPrimary);
    EXPECT_TRUE(fixture.radio.OwnerCallbacksDrained());
}

TEST(TargetPrimaryB,BSynchronousRecoveryBeforeUnknownHandoffReturnsPreservesEvidence)
{
    struct InlineOwner final : RadioOwnerDispatchPort {
        bool PostRadioOwnerTask(std::function<void()> task) override { task(); return true; }
        bool RadioOwnerCallbacksDrained() const override { return true; }
    };
    ResourceFixture fixture;
    auto owner = std::make_shared<InlineOwner>();
    fixture.radio.BindOwnerDispatch(owner);
    fixture.platform.disposition = RadioSubmitDisposition::HandoffUnknown;
    fixture.platform.onSynchronousResponse = [&](uint64_t token, const RadioSwitchRequest &original) {
        const auto evidence = Result(original, EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull);
        EXPECT_TRUE(fixture.radio.AcceptDeviceResult({token, evidence, true, true, true, static_cast<int32_t>(token)}));
    };
    fixture.Start(false);
    ASSERT_EQ(fixture.platform.requests.size(), 1u);
    const auto step = fixture.platform.requests.front().second.step;

    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_EQ(fixture.service->ReadPrimaryDeviceEvidence(step), EffectKnowledge::ConfirmedNotApplied);
    EXPECT_FALSE(fixture.service->ReadPrimaryDeviceEvidence({step.request, step.step + 1, step.attempt}));
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_TRUE(fixture.service->HasPendingDeviceEffects());
    EXPECT_TRUE(fixture.completion.results.empty());
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
    EXPECT_EQ(changed.completion.results,(std::vector<int32_t>{kLegacyLocalPointerNull}));
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
    for (size_t i=0;i<2;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    ASSERT_EQ(f.radio.requests.size(),4u);
    for (size_t i=2;i<4;++i) { f.service.AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0,true)); }
    ASSERT_EQ(f.radio.requests.size(),6u);
    f.topology.AcceptCardRemoval(f.target);
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

TEST(TargetPrimaryB,C1UncertainRollbackRetainsOriginalKeyAfterComplete)
{
    ResourceFixture fixture;
    const auto request = fixture.Start(true);
    ASSERT_EQ(fixture.platform.requests.size(), 2u);
    ASSERT_EQ(fixture.parameters.values[kIccid], kEncoded1);
    fixture.parameters.uncertainKey = C0ParameterKey::PrimaryIccid;

    fixture.DeliverRound(0);

    ASSERT_EQ(fixture.platform.requests.size(), 4u);
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto restore = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(restore.original.request, request);
    EXPECT_GT(restore.original.step, fixture.platform.requests.back().second.step.step);
    EXPECT_EQ(restore.key, C0ParameterKey::PrimaryIccid);
    EXPECT_EQ(fixture.parameters.writes.back(), (std::pair<std::string, std::string>{kIccid, kEncoded0}));
    EXPECT_EQ(fixture.parameters.values[kIccid], kEncoded1);
    EXPECT_EQ(fixture.scheduler.Audit().scheduled, 0u);
    ASSERT_TRUE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.service->ReadC0Receipt()->outstanding, AssignmentService::C0Stage::PrimaryParameters);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
    EXPECT_EQ(fixture.output.phases,
        (std::vector<std::pair<std::string, bool>> {{"start", false}, {"done", false}}));
    EXPECT_FALSE(fixture.service->ReconcileC0Parameter(restore));

    fixture.parameters.values[kIccid] = kEncoded0;
    fixture.parameters.parameterEvidence =
        C0ParameterEvidence {restore, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(fixture.service->ReconcileC0Parameter(restore));
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1RecoveryOfFirstRestoreKeyContinuesOriginalIccidBeforeReleasing)
{
    ResourceFixture fixture;
    const auto request = fixture.Start(true);
    ASSERT_EQ(fixture.platform.requests.size(), 2u);
    fixture.parameters.uncertainKey = C0ParameterKey::PrimarySlot;
    fixture.DeliverRound(0);
    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto restore = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(restore.original.request, request);
    EXPECT_EQ(restore.key, C0ParameterKey::PrimarySlot);
    EXPECT_EQ(fixture.parameters.values[kIccid], kEncoded1);
    ASSERT_TRUE(fixture.service->ReadC0Receipt());
    EXPECT_FALSE(fixture.service->ReadC0Receipt()->writes[static_cast<size_t>(C0ParameterKey::PrimaryIccid)]);
    EXPECT_EQ(fixture.parameters.writes.back(), (std::pair<std::string, std::string>{kPrimary, "0"}));

    fixture.parameters.values[kPrimary] = "0";
    fixture.parameters.parameterEvidence =
        C0ParameterEvidence {restore, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    fixture.parameters.uncertainKey = C0ParameterKey::PrimaryIccid;
    std::promise<C0ParameterOperation> entered;
    auto writingIccid = entered.get_future();
    std::promise<void> release;
    auto mayReturn = release.get_future();
    fixture.parameters.beforeTrackedWriteReturns = [&](const C0ParameterOperation &operation) {
        entered.set_value(operation);
        mayReturn.wait();
    };
    auto first = std::async(std::launch::async, [&] { return fixture.service->ReconcileC0Parameter(restore); });
    if (writingIccid.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        FAIL() << "Recovery of the first key did not enter the original ICCID write";
    }
    const auto inFlight = writingIccid.get();
    fixture.parameters.parameterEvidence =
        C0ParameterEvidence {inFlight, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_FALSE(fixture.service->ReconcileC0Parameter(inFlight));
    const auto duringWrite = fixture.service->ReadC0Receipt();
    EXPECT_TRUE(duringWrite && duringWrite->storeCallInFlight);
    release.set_value();
    EXPECT_TRUE(first.get());
    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto restoreIccid = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(restoreIccid.original.request, request);
    EXPECT_GT(restoreIccid.original.step, restore.original.step);
    EXPECT_EQ(restoreIccid.key, C0ParameterKey::PrimaryIccid);
    EXPECT_EQ(fixture.parameters.writes.back(), (std::pair<std::string, std::string>{kIccid, kEncoded0}));
    EXPECT_TRUE(fixture.service->ReadC0Receipt());

    fixture.parameters.values[kIccid] = kEncoded0;
    fixture.parameters.parameterEvidence =
        C0ParameterEvidence {restoreIccid, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(fixture.service->ReconcileC0Parameter(restoreIccid));
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.service->ReadLegacyAssignment().lastPrimary, SlotId {0});
    EXPECT_EQ(fixture.scheduler.Audit().scheduled, 0u);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1KnownUnappliedRestoreDoesNotProjectOldSlotOrRetry)
{
    ResourceFixture fixture;
    fixture.Start(true);
    ASSERT_EQ(fixture.platform.requests.size(), 2u);
    fixture.parameters.uncertainKey = C0ParameterKey::PrimarySlot;
    fixture.parameters.uncertainOutcome = {{-1, true}, EffectKnowledge::ConfirmedNotApplied};
    fixture.DeliverRound(0);

    ASSERT_TRUE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.service->ReadLegacyAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(fixture.parameters.values[kPrimary], "1");
    EXPECT_EQ(fixture.parameters.values[kIccid], kEncoded1);
    EXPECT_EQ(fixture.scheduler.Audit().scheduled, 0u);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1FailedSnapshotReadNeverActsLikeConfirmedEmptyOldValue)
{
    ResourceFixture fixture;
    fixture.parameters.unreadableKey = kPrimary;
    fixture.Start(true);

    EXPECT_TRUE(fixture.parameters.writes.empty());
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{-1}));
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
}

TEST(TargetPrimaryB,C1UncertainPrewriteRetainsOriginalPrefixAndRestoresItAfterProof)
{
    ResourceFixture fixture;
    fixture.parameters.uncertainKey = C0ParameterKey::PrimaryIccid;
    const auto request = fixture.Start(true);
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_TRUE(fixture.output.phases.empty());
    ASSERT_TRUE(fixture.parameters.uncertainWrite);
    const auto first = *fixture.parameters.uncertainWrite;
    EXPECT_EQ(first.original.request, request);
    const auto receipt = fixture.service->ReadC0Receipt();
    ASSERT_TRUE(receipt && receipt->restore && receipt->restoreAfterPrewrite);
    EXPECT_EQ(receipt->restore->mainIccidText, kEncoded0);
    EXPECT_EQ(fixture.parameters.values[kPrimary], "1");
    EXPECT_FALSE(fixture.service->ReadLegacyAssignment().lastPrimary);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));

    fixture.parameters.uncertainKey.reset();
    fixture.parameters.values[kIccid] = kEncoded1;
    fixture.parameters.parameterEvidence =
        C0ParameterEvidence {first, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(fixture.service->ReconcileC0Parameter(first));
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.parameters.values[kPrimary], "0");
    EXPECT_EQ(fixture.parameters.values[kIccid], kEncoded0);
    EXPECT_EQ(fixture.service->ReadLegacyAssignment().lastPrimary, SlotId {0});
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_EQ(fixture.scheduler.Audit().scheduled, 0u);
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1BindingRevocationAfterSlotPrewriteRestoresOriginalKeysWithoutCheck)
{
    ResourceFixture fixture;
    bool revoked = false;
    fixture.parameters.beforeTrackedWriteReturns = [&](const C0ParameterOperation &operation) {
        if (operation.key == C0ParameterKey::PrimarySlot && !revoked) {
            revoked = true;
            fixture.topology.AcceptCardRemoval(fixture.target);
        }
    };

    fixture.Start(true);

    EXPECT_TRUE(revoked);
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.output.phases.empty());
    EXPECT_TRUE(fixture.output.roles.empty());
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.parameters.writes, (std::vector<std::pair<std::string, std::string>> {
        {kPreference, "true"}, {kPrimary, "1"}, {kPrimary, "0"}, {kIccid, kEncoded0}}));
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1ConfirmedUnsentFirstCheckRestoresOriginalPrewrite)
{
    ResourceFixture fixture;
    fixture.platform.disposition = RadioSubmitDisposition::ConfirmedNotSent;
    fixture.Start(true);

    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
    EXPECT_FALSE(fixture.service->ReadC0Receipt());
    EXPECT_EQ(fixture.parameters.values.at(kPrimary), "0");
    EXPECT_EQ(fixture.parameters.values.at(kIccid), kEncoded0);
    EXPECT_EQ(fixture.service->ReadLegacyAssignment().lastPrimary, SlotId {0});
    EXPECT_EQ(fixture.output.phases,
        (std::vector<std::pair<std::string, bool>> {{"start", false}, {"done", false}}));
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1SnapshotDoesNotHoldAssignmentLockAndRechecksBinding)
{
    ResourceFixture fixture;
    std::promise<void> entered;
    auto reading = entered.get_future();
    std::promise<void> release;
    auto mayReturn = release.get_future();
    fixture.parameters.beforeRead = [&](const std::string &key) {
        if (key == kPrimary) {
            entered.set_value();
            mayReturn.wait();
        }
    };
    auto start = std::async(std::launch::async, [&] { return fixture.Start(true); });
    if (reading.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        FAIL() << "C1 did not reach the original parameter snapshot";
    }
    auto inspect = std::async(std::launch::async, [&] { return fixture.service->ReadLegacyAssignment(); });
    const auto readable = inspect.wait_for(std::chrono::seconds(3));
    fixture.topology.AcceptCardRemoval(fixture.target);
    release.set_value();
    start.get();
    inspect.get();

    EXPECT_EQ(readable, std::future_status::ready) << "The original Store read held Assignment's commit lock";
    EXPECT_TRUE(fixture.parameters.writes.empty());
    EXPECT_TRUE(fixture.platform.requests.empty());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{-1}));
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
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
    EXPECT_FALSE(f.service->HasPendingDeviceEffects());
    EXPECT_FALSE(f.scheduler.RedeliverLastOnOwner(f.service));
}

TEST(TargetPrimaryB,C1RetryFreezesFreshNativeRoutesOnlyForNewAttempt)
{
    ResourceFixture fixture;
    const auto request = fixture.Start(true);
    ASSERT_EQ(fixture.platform.requests.size(), 2u);
    fixture.DeliverRound(0);
    ASSERT_EQ(fixture.platform.requests.size(), 4u);
    ASSERT_EQ(fixture.scheduler.Audit().scheduled, 1u);
    const auto firstSession = fixture.platform.requests.front().second.protocolSession;

    auto renewed = kControlledProtocolMembers;
    ++renewed[0].routeRevision;
    ++renewed[1].routeRevision;
    fixture.platform.protocolMembers = renewed;
    fixture.scheduler.Advance(1);
    ASSERT_TRUE(fixture.scheduler.WaitIdle());

    ASSERT_EQ(fixture.platform.requests.size(), 6u);
    for (size_t member = 0; member < renewed.size(); ++member) {
        const auto &frozen = fixture.platform.requests[member].second;
        const auto &next = fixture.platform.requests[4 + member].second;
        EXPECT_EQ(frozen.member, kControlledProtocolMembers[member]);
        EXPECT_EQ(next.member, renewed[member]);
        EXPECT_EQ(next.step.request, request);
        EXPECT_EQ(next.step.attempt, 1u);
        EXPECT_NE(next.protocolSession, firstSession);
        EXPECT_TRUE(fixture.ResultAt(4 + member, EffectKnowledge::ConfirmedApplied, 0));
    }
    ASSERT_EQ(fixture.platform.requests.size(), 8u);
    EXPECT_TRUE(fixture.ResultAt(6, EffectKnowledge::ConfirmedApplied, 0, true));
    EXPECT_TRUE(fixture.ResultAt(7, EffectKnowledge::ConfirmedApplied, 0, true));
    ASSERT_EQ(fixture.platform.requests.size(), 10u);
    EXPECT_TRUE(fixture.ResultAt(8, EffectKnowledge::ConfirmedApplied, 0));
    EXPECT_TRUE(fixture.ResultAt(9, EffectKnowledge::ConfirmedApplied, 0));
    EXPECT_FALSE(fixture.service->HasPendingDeviceEffects());
    EXPECT_TRUE(fixture.radio.DeviceResourcesDrained());
    EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
}

TEST(TargetPrimaryB,C1SchedulingTheOriginalRetryDoesNotHoldAssignmentCommitLock)
{
    for (const bool stop : {false, true}) {
        SCOPED_TRACE(stop);
        ResourceFixture fixture;
        fixture.Start(true);
        ASSERT_EQ(fixture.platform.requests.size(), 2u);
        EXPECT_TRUE(fixture.ResultAt(0, EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull));
        EXPECT_TRUE(fixture.ResultAt(1, EffectKnowledge::ConfirmedNotApplied, kLegacyLocalPointerNull));
        ASSERT_EQ(fixture.platform.requests.size(), 4u);
        EXPECT_TRUE(fixture.ResultAt(2, EffectKnowledge::ConfirmedApplied, 0));
        std::promise<void> entered;
        auto scheduling = entered.get_future();
        std::promise<void> release;
        auto mayReturn = release.get_future();
        fixture.scheduler.beforeSchedule = [&] {
            entered.set_value();
            mayReturn.wait();
        };
        auto completion = std::async(std::launch::async,
            [&] { return fixture.ResultAt(3, EffectKnowledge::ConfirmedApplied, 0); });
        if (scheduling.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            release.set_value();
            FAIL() << "The platform scheduler did not receive the original retry";
        }
        auto inspect = std::async(std::launch::async, [&] { return fixture.service->ReadLegacyAssignment(); });
        const auto available = inspect.wait_for(std::chrono::seconds(3));
        if (stop && available == std::future_status::ready) {
            EXPECT_TRUE(fixture.service->StopAcceptingAndCancelScheduledRetries());
        }
        release.set_value();
        EXPECT_TRUE(completion.get());
        (void)inspect.get();
        EXPECT_EQ(available, std::future_status::ready);
        const auto audit = fixture.scheduler.Audit();
        EXPECT_EQ(audit.scheduled, 1u);
        EXPECT_EQ(audit.pending, stop ? 0u : 1u);
        EXPECT_EQ(audit.cancelled, stop ? 1u : 0u);
        EXPECT_EQ(fixture.completion.results, (std::vector<int32_t>{kLegacyLocalPointerNull}));
    }
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
    for (size_t i=0;i<2;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
    ASSERT_EQ(f.radio.requests.size(),4u);
    for (size_t i=2;i<4;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0,true)); }
    ASSERT_EQ(f.radio.requests.size(),6u);
    EXPECT_FALSE(f.StopAndDrain());
    EXPECT_TRUE(f.service->HasPendingDeviceEffects());
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
    for (size_t i=0;i<2;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedNotApplied,kLegacyLocalPointerNull)); }
    ASSERT_EQ(f.radio.requests.size(),4u);
    EXPECT_FALSE(f.service->StopAcceptingAndCancelScheduledRetries());
    for (size_t i=2;i<4;++i) { f.service->AcceptRadioStep(Result(f.radio.requests[i],
        EffectKnowledge::ConfirmedApplied,0)); }
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
    for (size_t i=0;i<2;++i) {
        ASSERT_TRUE(f.scheduler.PostOwner([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
        }));
    }
    ASSERT_TRUE(f.scheduler.WaitIdle());
    ASSERT_EQ(f.radio.requests.size(),4u);
    for (size_t i=2;i<4;++i) {
        f.scheduler.OwnerCall([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0,true));
        });
    }
    ASSERT_EQ(f.radio.requests.size(),6u);
    ASSERT_TRUE(f.scheduler.PostOwner([service=f.service] {
        EXPECT_FALSE(service->StopAcceptingAndCancelScheduledRetries());
    }));
    for (size_t i=4;i<6;++i) {
        ASSERT_TRUE(f.scheduler.PostOwner([&, request=f.radio.requests[i], service=f.service] {
            service->AcceptRadioStep(Result(request,EffectKnowledge::ConfirmedApplied,0));
        }));
    }
    ASSERT_TRUE(f.scheduler.WaitIdle());
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
