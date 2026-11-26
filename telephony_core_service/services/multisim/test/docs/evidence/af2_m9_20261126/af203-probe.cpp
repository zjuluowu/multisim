#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>
#include "device/resource/resource_radio_runtime.h"
using namespace OHOS::Telephony::MultiSim;
constexpr RuntimeEpoch epoch{6,2};
const ResourceKey key{"same-physical-modem"};
struct Sink : RadioResultSink {
    void AcceptRadioStep(const RadioSwitchResult &) override {}
    void AcceptLateRadioEvidence(const RadioSwitchResult &) override {}
};
struct Dispatcher : RadioOwnerDispatchPort {
    bool PostRadioOwnerTask(std::function<void()> f) override { f(); return true; }
    bool RadioOwnerCallbacksDrained() const override { return true; }
};
struct Device : RadioPlatformPort {
    std::mutex mutex;
    std::condition_variable changed;
    bool pEntered = false;
    bool releaseP = false;
    unsigned pCalls = 0, qCalls = 0, otherCalls = 0;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override { return {slot}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t) override
    {
        std::unique_lock<std::mutex> guard(mutex);
        if (request.step.request.value == 2) {
            ++otherCalls;
            return {RadioSubmitDisposition::HandoffAccepted, 82};
        }
        if (request.target.legacySlotId == 1) {
            ++qCalls;
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        if (++pCalls == 1) {
            pEntered = true;
            changed.notify_all();
            changed.wait(guard, [&]{ return releaseP; });
        }
        return {RadioSubmitDisposition::HandoffAccepted, 81};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override {}
};
struct Fixture {
    ResourceBarrier barrier{epoch};
    Device device;
    std::shared_ptr<Sink> sink = std::make_shared<Sink>();
    std::shared_ptr<Dispatcher> dispatch = std::make_shared<Dispatcher>();
    ResourceRadioRuntime runtime{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    Fixture()
    {
        if (!barrier.RegisterForInitialCheck(key) || !runtime.Initialize(key, {epoch, true, true})) { std::abort(); }
        runtime.BindResultSink(sink);
        runtime.BindOwnerDispatch(dispatch);
    }
    bool DuringSubmit(const std::function<void()> &f)
    {
        auto p = std::async(std::launch::async, [this]{ runtime.SwitchPrimaryRadio(Request(1, 0)); });
        {
            std::unique_lock<std::mutex> lock(device.mutex);
            if (!device.changed.wait_for(lock,std::chrono::seconds(5),[&]{ return device.pEntered; })) {
                device.releaseP = true;
                device.changed.notify_all();
                return false;
            }
        }
        f();
        {
            std::lock_guard<std::mutex> lock(device.mutex);
            device.releaseP = true;
        }
        device.changed.notify_all();
        p.get();
        return true;
    }
    static RadioSwitchRequest Request(uint64_t id, int slot)
    {
        return {{{id, epoch, RequestOwner::Assignment},1,1},SlotId{slot},RadioDomainId{"same-domain"},
            PrimarySwitchMode::ThreePhaseProtocol,std::nullopt,8,1,id == 1 ? 2u : 1u};
    }
};
int main()
{
    Fixture duplication;
    if (!duplication.DuringSubmit([&] {
        duplication.runtime.SwitchPrimaryRadio(Fixture::Request(1, 0));
    })) { return 2; }
    Fixture release;
    bool released = false;
    if (!release.DuringSubmit([&] {
        release.runtime.SwitchPrimaryRadio(Fixture::Request(1, 1));
        released = release.barrier.ReadState(key) == ResourceState::Available;
        release.runtime.SwitchPrimaryRadio(Fixture::Request(2, 0));
    })) { return 2; }
    const bool retained = release.runtime.ReadTransportSerial(1) == 81;
    std::cout << "same-member platform calls=" << duplication.device.pCalls
        << ", Q calls=" << release.device.qCalls << ", lease prematurely Available=" << released
        << ", rival platform calls=" << release.device.otherCalls << ", P serial retained=" << retained << '\n';
    return duplication.device.pCalls == 1 && release.device.qCalls == 1 && !released &&
        release.device.otherCalls == 0 && retained ? 0 : 1;
}
