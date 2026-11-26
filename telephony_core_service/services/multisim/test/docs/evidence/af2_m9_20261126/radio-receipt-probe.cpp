#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>
#include "device/resource/resource_radio_runtime.h"
using namespace OHOS::Telephony::MultiSim;
constexpr RuntimeEpoch epoch{6,2};
const ResourceKey key{"same-physical-modem"};
struct Sink : RadioResultSink {
    std::vector<uint64_t> deliveries;
    void AcceptRadioStep(const RadioSwitchResult &r) override { deliveries.push_back(r.step.request.value); }
    void AcceptLateRadioEvidence(const RadioSwitchResult &) override {}
};
struct Dispatcher : RadioOwnerDispatchPort {
    mutable std::mutex mutex;
    std::condition_variable changed;
    unsigned posts = 0;
    bool releaseFirst = false;
    std::function<void()> second;
    bool PostRadioOwnerTask(std::function<void()> f) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (++posts == 1) {
            changed.notify_all();
            changed.wait(lock,[&] { return releaseFirst; });
            return false;
        }
        if (posts == 2) {
            second = std::move(f);
            return true;
        }
        lock.unlock();
        f();
        return true;
    }
    bool RadioOwnerCallbacksDrained() const override
    {
        std::lock_guard<std::mutex> lock(mutex);
        return !second;
    }
};
struct Device : RadioPlatformPort {
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override { return {slot}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &, uint64_t token) override
    {
        return {RadioSubmitDisposition::HandoffAccepted, static_cast<int32_t>(token)};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override {}
};
RadioSwitchRequest Request(uint64_t id)
{
    return {{{id, epoch, RequestOwner::Assignment}, 1, 1}, SlotId{0}, RadioDomainId{"same-domain"},
        PrimarySwitchMode::WithoutModemRestart, std::nullopt, 1, 0, 1};
}
RadioDeviceResult Reply(uint64_t id)
{
    auto req = Request(id);
    return {id, {req.step, req.target, req.expectedBinding, req.protocolSession, req.phase, false,
        EffectKnowledge::ConfirmedApplied, 0}, true, true, true, static_cast<int32_t>(id)};
}
int main()
{
    ResourceBarrier barrier{epoch};
    Device device;
    auto sink = std::make_shared<Sink>();
    auto dispatch = std::make_shared<Dispatcher>();
    ResourceRadioRuntime runtime{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    if (!barrier.RegisterForInitialCheck(key) || !runtime.Initialize(key,{epoch,true,true})) { return 2; }
    runtime.BindResultSink(sink);
    runtime.BindOwnerDispatch(dispatch);
    runtime.SwitchPrimaryRadio(Request(1));
    auto first = std::async(std::launch::async, [&]{ return runtime.AcceptDeviceResult(Reply(1)); });
    {
        std::unique_lock<std::mutex> lock(dispatch->mutex);
        if (!dispatch->changed.wait_for(lock,std::chrono::seconds(5),[&]{ return dispatch->posts == 1; })) {
            dispatch->releaseFirst = true; dispatch->changed.notify_all(); return 2;
        }
    }
    runtime.SwitchPrimaryRadio(Request(2));
    const bool acceptedSecond = runtime.AcceptDeviceResult(Reply(2));
    std::function<void()> postedSecond;
    {
        std::lock_guard<std::mutex> lock(dispatch->mutex);
        dispatch->releaseFirst = true;
        postedSecond = std::move(dispatch->second);
    }
    dispatch->changed.notify_all();
    if (first.get() || !postedSecond || !acceptedSecond) { return 2; }
    postedSecond();
    if (!runtime.FlushOwnerDeliveries()) { return 2; }
    std::cout << "delivered request IDs:";
    for (auto id : sink->deliveries) { std::cout << ' ' << id; }
    std::cout << '\n';
    return sink->deliveries == std::vector<uint64_t>{2, 1} ? 0 : 1;
}
