/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include <functional>
#include <iostream>
#include "device/resource/resource_radio_runtime.h"
using namespace OHOS::Telephony::MultiSim;

class Sink final : public RadioResultSink {
public:
    std::function<void()> onDelivery;
    void AcceptRadioStep(const RadioSwitchResult &) override { onDelivery(); }
    void AcceptLateRadioEvidence(const RadioSwitchResult &) override {}
};

class InlineDispatch final : public RadioOwnerDispatchPort {
public:
    bool PostRadioOwnerTask(std::function<void()> task) override { task(); return true; }
    bool RadioOwnerCallbacksDrained() const override { return true; }
};

class Device final : public RadioPlatformPort {
public:
    std::function<bool(const RadioDeviceResult &)> accept;
    bool buffered = false;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override { return {slot}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t transport) override
    {
        if (request.step.request.value == 2) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        const auto serial = static_cast<int32_t>(transport);
        buffered = accept({transport, {request.step, request.target, request.expectedBinding,
            request.protocolSession, request.phase, false, EffectKnowledge::ConfirmedApplied, 0},
            true, true, true, serial});
        return {RadioSubmitDisposition::HandoffAccepted, serial};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override {}
};

int main()
{
    const RuntimeEpoch epoch{6, 2};
    const ResourceKey key{"same-device"};
    ResourceBarrier barrier(epoch);
    Device device;
    ResourceRadioRuntime runtime(barrier, device, {key}, ResourceRadioRuntime::Mode::Execute);
    if (!barrier.RegisterForInitialCheck(key) || !runtime.Initialize(key, {epoch, true, true})) {
        return 2;
    }
    auto sink = std::make_shared<Sink>();
    auto dispatch = std::make_shared<InlineDispatch>();
    runtime.BindResultSink(sink);
    runtime.BindOwnerDispatch(dispatch);
    device.accept = [&](const auto &result) { return runtime.AcceptDeviceResult(result); };
    constexpr RequestOwner owner = RequestOwner::Assignment;
    const RadioSwitchRequest first{{{1, epoch, owner}, 1, 1}, {0}, {"shared"},
        PrimarySwitchMode::WithoutModemRestart, std::nullopt, 1, 0, 1};
    auto second = first;
    second.step.request.value = 2;
    const auto submit = [&](const RadioSwitchRequest &request) {
#ifdef BASELINE
        runtime.SwitchPrimaryRadio(request);
        return runtime.WasLastSubmissionAccepted(request);
#else
        return runtime.SwitchPrimaryRadio(request);
#endif
    };
    bool secondAccepted = true;
    unsigned deliveries = 0;
    sink->onDelivery = [&] { ++deliveries; secondAccepted = submit(second); };
    const bool firstAccepted = submit(first);
    std::cout << "first=" << firstAccepted << " second=" << secondAccepted
              << " buffered=" << device.buffered << " deliveries=" << deliveries << '\n';
    return firstAccepted && !secondAccepted && device.buffered && deliveries == 1 &&
        runtime.ReadResourceState(key) == ResourceState::Available ? 0 : 1;
}
