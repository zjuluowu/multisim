/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include <condition_variable>
#include <future>
#include <gtest/gtest.h>
#include "device/resource/resource_radio_runtime.h"
namespace OHOS::Telephony::MultiSim {
namespace {
constexpr RuntimeEpoch epoch{6,2};
const ResourceKey key{"same-physical-modem"};
const RadioSendQualification kOnlyRadioQualification = [](const RadioPermissionRegistration &permit) {
    return permit([] { return true; });
};
struct Sink : RadioResultSink {
    std::vector<uint64_t> deliveries;
    std::function<void()> onDelivery;
    void AcceptRadioStep(const RadioSwitchResult &r) override
    {
        deliveries.push_back(r.step.request.value);
        if (onDelivery) { onDelivery(); }
    }
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
    std::function<RadioSubmitResult(const RadioSwitchRequest &, uint64_t)> onSubmit;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override { return {slot}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t token,
        const RadioSendPermit &permit) override
    {
        if (!permit()) { return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt}; }
        if (onSubmit) { return onSubmit(request, token); }
        return {RadioSubmitDisposition::HandoffAccepted, static_cast<int32_t>(token)};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override {}
};
struct InlineDispatch final : RadioOwnerDispatchPort {
    bool PostRadioOwnerTask(std::function<void()> task) override { task(); return true; }
    bool RadioOwnerCallbacksDrained() const override { return true; }
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
} // namespace

TEST(TargetRadioReceipt, RefusedPostDoesNotRemoveAnotherRequestsReceipt)
{
    ResourceBarrier barrier{epoch};
    Device device;
    auto sink = std::make_shared<Sink>();
    auto dispatch = std::make_shared<Dispatcher>();
    ResourceRadioRuntime runtime{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    ASSERT_TRUE(barrier.RegisterForInitialCheck(key));
    ASSERT_TRUE(runtime.Initialize(key, {epoch, true, true}));
    runtime.BindResultSink(sink);
    runtime.BindOwnerDispatch(dispatch);
    runtime.SwitchPrimaryRadio(Request(1), kOnlyRadioQualification);
    auto first = std::async(std::launch::async, [&]{ return runtime.AcceptDeviceResult(Reply(1)); });
    {
        std::unique_lock<std::mutex> lock(dispatch->mutex);
        if (!dispatch->changed.wait_for(lock,std::chrono::seconds(5),[&]{ return dispatch->posts == 1; })) {
            dispatch->releaseFirst = true;
            dispatch->changed.notify_all();
            FAIL() << "The first completion did not reach the dispatch boundary";
        }
    }
    runtime.SwitchPrimaryRadio(Request(2), kOnlyRadioQualification);
    const bool acceptedSecond = runtime.AcceptDeviceResult(Reply(2));
    std::function<void()> postedSecond;
    {
        std::lock_guard<std::mutex> lock(dispatch->mutex);
        dispatch->releaseFirst = true;
        postedSecond = std::move(dispatch->second);
    }
    dispatch->changed.notify_all();
    EXPECT_FALSE(first.get());
    ASSERT_TRUE(postedSecond);
    EXPECT_TRUE(acceptedSecond);
    postedSecond();
    EXPECT_TRUE(runtime.FlushOwnerDeliveries());
    EXPECT_EQ(sink->deliveries, (std::vector<uint64_t> {2, 1}));
}

TEST(TargetRadioReceipt, FlushCannotRepostWhileOriginalPostIsStillInFlight)
{
    ResourceBarrier barrier{epoch};
    Device device;
    auto sink = std::make_shared<Sink>();
    auto dispatch = std::make_shared<Dispatcher>();
    ResourceRadioRuntime runtime{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    ASSERT_TRUE(barrier.RegisterForInitialCheck(key));
    ASSERT_TRUE(runtime.Initialize(key, {epoch, true, true}));
    runtime.BindResultSink(sink);
    runtime.BindOwnerDispatch(dispatch);
    ASSERT_TRUE(runtime.SwitchPrimaryRadio(Request(1), kOnlyRadioQualification));

    auto accept = std::async(std::launch::async, [&] { return runtime.AcceptDeviceResult(Reply(1)); });
    {
        std::unique_lock<std::mutex> lock(dispatch->mutex);
        if (!dispatch->changed.wait_for(lock, std::chrono::seconds(5), [&] { return dispatch->posts == 1; })) {
            dispatch->releaseFirst = true;
            dispatch->changed.notify_all();
            FAIL() << "Original completion did not reach the dispatch boundary";
        }
    }
    EXPECT_TRUE(runtime.DeviceResourcesDrained());
    EXPECT_FALSE(runtime.OwnerCallbacksDrained());
    EXPECT_FALSE(runtime.FlushOwnerDeliveries()) << "A second thread reposted an in-flight Owner receipt";
    std::function<void()> reposted;
    {
        std::lock_guard<std::mutex> lock(dispatch->mutex);
        EXPECT_EQ(dispatch->posts, 1u);
        reposted = std::move(dispatch->second);
        dispatch->releaseFirst = true;
    }
    dispatch->changed.notify_all();
    EXPECT_FALSE(accept.get());
    if (reposted) { reposted(); }

    EXPECT_TRUE(runtime.FlushOwnerDeliveries());
    std::function<void()> retry;
    {
        std::lock_guard<std::mutex> lock(dispatch->mutex);
        retry = std::move(dispatch->second);
    }
    if (retry) { retry(); }
    EXPECT_EQ(sink->deliveries, std::vector<uint64_t> {1});
    EXPECT_TRUE(runtime.OwnerCallbacksDrained());
    EXPECT_TRUE(runtime.StopAccepting());
}

TEST(TargetRadioReceipt, EarlyDeliveryCannotOverwriteOriginalSubmitResult)
{
    ResourceBarrier barrier{epoch};
    Device device;
    auto sink = std::make_shared<Sink>();
    auto dispatch = std::make_shared<InlineDispatch>();
    ResourceRadioRuntime runtime{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    ASSERT_TRUE(barrier.RegisterForInitialCheck(key));
    ASSERT_TRUE(runtime.Initialize(key, {epoch, true, true}));
    runtime.BindResultSink(sink);
    runtime.BindOwnerDispatch(dispatch);
    device.onSubmit = [&](const RadioSwitchRequest &request, uint64_t token) -> RadioSubmitResult {
        if (request.step.request.value == 2) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        EXPECT_EQ(token, 1u);
        EXPECT_TRUE(runtime.AcceptDeviceResult(Reply(1)));
        return {RadioSubmitDisposition::HandoffAccepted, static_cast<int32_t>(token)};
    };
    sink->onDelivery = [&] { EXPECT_FALSE(runtime.SwitchPrimaryRadio(Request(2), kOnlyRadioQualification)); };

    EXPECT_TRUE(runtime.SwitchPrimaryRadio(Request(1), kOnlyRadioQualification));
    EXPECT_EQ(sink->deliveries, (std::vector<uint64_t>{1}));
    EXPECT_EQ(runtime.SubmittedCommands(), 1u);
    EXPECT_EQ(runtime.ReadResourceState(key), ResourceState::Available);
}
} // namespace OHOS::Telephony::MultiSim
