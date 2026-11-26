#include <iostream>
#include "device/resource/resource_radio_runtime.h"
#include "role_assignment/assignment_service.h"

using namespace OHOS::Telephony::MultiSim;
constexpr RuntimeEpoch epoch{6, 2};
struct Dispatch : RadioOwnerDispatchPort {
    bool PostRadioOwnerTask(std::function<void()> f) override { f(); return true; }
    bool RadioOwnerCallbacksDrained() const override { return true; }
};
struct Device : RadioPlatformPort {
    ResourceRadioRuntime *runtime {};
    unsigned calls {};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override { return {slot, 1, false, false, 1}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &q, uint64_t token) override
    {
        ++calls;
        const bool update = q.phase == 2;
        (void)runtime->AcceptDeviceResult({token, {q.step, q.target, q.expectedBinding, q.protocolSession, q.phase,
            update, EffectKnowledge::ConfirmedApplied, 0}, true, true, true, static_cast<int32_t>(token)});
        return {RadioSubmitDisposition::HandoffAccepted, static_cast<int32_t>(token)};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override {}
};
struct Store : AssignmentStore {
    RoleStoreResult WriteDefaultRole(const SetRoleCommand &) override { return {0, true, true}; }
    PrimaryParameterSnapshot ReadPrimaryParameters() override { return {"0", "123"}; }
    AssignmentParameterResult WritePrimaryParameters(SlotId, const std::string &) override { return {0, true}; }
    AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &) override { return {0, true}; }
    AssignmentParameterResult WriteDefaultData(SlotId) override { return {0, true}; }
    AssignmentParameterResult WriteUserPrimaryPreference(bool) override { return {0, true}; }
};
struct Out : AssignmentOutputPort {
    void PublishPrimaryPhase(bool, bool) override {}
    void PublishMainCardChanged(SlotId) override {}
    void PublishDefaultDataChanged(SlotId, SubscriptionId) override {}
    void ReportDefaultDataSelection(SlotId) override {}
};
struct Topo : PrimaryTopologyPort {
    std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
    { return PrimaryTargetFacts {{SubscriptionId{2}, slot, 5}, "89000000000000000001", true}; }
    RequestId BeginRouteMapping(const StepRef &, SlotTarget) override { return {0, epoch}; }
};
struct Complete : LegacyIntCompletionPort {
    unsigned calls {};
    void CompleteLegacyInt(const CompletionPlan &, int32_t result) override
    {
        std::cerr << "Completion result=" << result << '\n';
        ++calls;
    }
};
int main()
{
    SlotCatalog catalog {{
      {SlotId{0},EndpointKind::Physical,0,RadioDomainId{"m0"},VisibilityClass::Ordinary,false},
      {SlotId{1},EndpointKind::Physical,1,RadioDomainId{"m1"},VisibilityClass::Ordinary,false}}, {2,2,2,2,2}};
    ResourceBarrier barrier{epoch};
    Device device;
    auto dispatch = std::make_shared<Dispatch>();
    ResourceKey key{"m0"};
    ResourceRadioRuntime radio{barrier, device, {key}, ResourceRadioRuntime::Mode::Execute};
    device.runtime = &radio;
    if (!barrier.RegisterForInitialCheck(key) || !radio.Initialize(key,{epoch,true,true})) { return 2; }
    Store store;
    Out output;
    Topo topology;
    Complete completion;
    auto service = std::make_shared<AssignmentService>(catalog,store,radio,output,topology,completion,epoch);
    radio.BindResultSink(service);
    radio.BindOwnerDispatch(dispatch);
    service->StartPrimarySwitch({SlotTarget{SlotId{1}},true,
       {SynchronousCompletionRoute{SynchronousWaitRef{44}},false}, std::nullopt,
       topology.ReadPrimaryTarget(SlotId{1})->binding});
    std::cerr << "device calls=" << device.calls << ", completion calls=" << completion.calls << '\n';
    return device.calls == 6 && completion.calls == 1 ? 0 : 1;
}
