// Host protocol tests: no production card/RIL/provider is created or replaced.
#include <algorithm>
#include <cassert>
#include "assignment/application/primary_switch_plan.h"
#include "assignment/application/account_role_protocol.h"
#include "application/card_access/local_card_requests.h"
#include "compatibility/completion_cell.h"
using namespace OHOS::Telephony::MultiSim;
using A = PrimaryAction;
static bool Has(const std::vector<A> &actions, A value)
{ return std::find(actions.begin(), actions.end(), value) != actions.end(); }
static size_t At(const std::vector<A> &actions, A value)
{ auto p = std::find(actions.begin(), actions.end(), value); assert(p != actions.end()); return p-actions.begin(); }
static void PrimaryModes()
{
    auto normal = PlanPrimarySwitch(true, {false, false, 1, 7});
    assert(normal.mode == PrimaryMode::ThreePhase && normal.observationRevision == 7);
    assert(At(normal.beforeDevice,A::WritePrimaryParameters) < At(normal.beforeDevice,A::ExecuteThreePhase));
    assert(At(normal.onDeviceSuccess,A::PublishMainCard) < At(normal.onDeviceSuccess,A::SaveDefaultData));
    assert(At(normal.onDeviceFailure,A::RestoreValidOldParameters) < At(normal.onDeviceFailure,A::FinishOrdinary));
    assert(At(normal.onDeviceFailure,A::FinishOrdinary) < At(normal.onDeviceFailure,A::ScheduleLegacyRetry));
    auto dedicated = PlanPrimarySwitch(true, {false, true, 0, 8});
    assert(dedicated.mode == PrimaryMode::WithoutModemReboot); // precedence over M0.
    assert(!Has(dedicated.beforeDevice,A::WritePrimaryParameters));
    assert(Has(dedicated.beforeDevice,A::ExecuteDedicatedRil));
    assert(At(dedicated.onDeviceSuccess,A::WritePrimaryParameters) < At(dedicated.onDeviceSuccess,A::PublishMainCard));
    assert(!Has(dedicated.onDeviceFailure,A::RestoreValidOldParameters));
    assert(!Has(dedicated.onDeviceFailure,A::ScheduleLegacyRetry));
    auto already = PlanPrimarySwitch(false, {false, true, 0, 9});
    assert(already.mode == PrimaryMode::AlreadyM0 && already.beforeDevice.size() == 1);
    assert(Has(already.onDeviceSuccess,A::SaveDefaultData) && !Has(already.onDeviceSuccess,A::FinishOrdinary));
    auto third = PlanPrimarySwitch(true, {true, true, 0, 10});
    assert(third.mode == PrimaryMode::ThirdCardRoute);
    assert(third.beforeDevice == std::vector<A>{A::RequestThirdCardRoute});
    assert(third.onDeviceSuccess == std::vector<A>{A::ReturnOriginalResult});
    // Missing modem observation must not manufacture AlreadyM0.
    auto missingController=PlanPrimarySwitch(false,{false,false,std::nullopt,11});
    assert(missingController.mode == PrimaryMode::ThreePhase);
    assert(At(missingController.beforeDevice,A::WritePrimaryParameters) <
        At(missingController.beforeDevice,A::ExecuteThreePhase)); // 原controller缺失仍先预写再失败。
}
static void CompletionBoundaries()
{
    CompletionCell immediate;
    assert(immediate.Deliver(-31));
    assert(immediate.Result() == -31); // 完成先于宿主Await也保留原结果。
    assert(!immediate.Deliver(0));
    assert(!immediate.EndWait(WaitState::TimedOut));
    CompletionCell pending;
    assert(!pending.Result());
    assert(pending.Deliver(17));
    assert(pending.Result() == 17);
    CompletionCell timeout;
    assert(timeout.EndWait(WaitState::TimedOut));
    assert(!timeout.Result() && !timeout.Deliver(0));
    CompletionCell dead;
    assert(dead.EndWait(WaitState::CallerDied));
    assert(!dead.Deliver(0) && !dead.Result());
}
static void JointRowsAndReload()
{
    AccountRoleProtocol roles;
    DiscoveryRoleContext first {{0}, "A", std::nullopt, 1};
    auto plan = roles.Prepare(first, true);
    assert(!plan.subscription && plan.includeInStorage && plan.voice && plan.sms && plan.mainCardColumn && plan.dataCardColumn);
    assert(roles.AcceptStorage(plan,SubscriptionId{3},EffectKnowledge::ConfirmedApplied) == RoleFragmentAcceptance::ReadyForOwnerReload);
    roles.NoteLocalRoleMutation();
    assert(!roles.IsCurrent(plan));
    assert(roles.AcceptStorage(plan,SubscriptionId{3},EffectKnowledge::ConfirmedApplied) == RoleFragmentAcceptance::NeedsReconciliation);
    assert(roles.AcceptStorage(plan,std::nullopt,EffectKnowledge::ConfirmedNotApplied) == RoleFragmentAcceptance::NoEffect);
    auto restore = roles.Prepare({{0},"A",SubscriptionId{3},2},true);
    assert(restore.includeInStorage && restore.voice && restore.sms && restore.mainCardColumn && restore.dataCardColumn);
    auto multiRestore = roles.Prepare({{0},"A",SubscriptionId{3},2},false);
    assert(!multiRestore.includeInStorage); // 多卡旧卡保留角色，单卡UpdateDataByIccId重写MAIN。
    auto row = roles.Prepare(first,false); row.subscription=SubscriptionId{3};
    auto current=roles.BeginReload(AccountReadView::Current,1);
    auto all=roles.BeginReload(AccountReadView::Historical,1);
    assert(roles.AcceptReload(current,{row},true));
    assert(!roles.AcceptReload(all,{row},false));
    assert(roles.Projection(AccountReadView::Current).size() == 1);
    assert(roles.Projection(AccountReadView::Historical).empty());
    auto duplicate=roles.BeginReload(AccountReadView::Current,2);
    assert(!roles.AcceptReload(duplicate,{row,row},true));
    auto unidentified=row; unidentified.subscription.reset();
    assert(!roles.AcceptReload(duplicate,{unidentified},true));
    assert(roles.Projection(AccountReadView::Current).size() == 1);
    auto old=roles.BeginReload(AccountReadView::Current,3);
    roles.NoteLocalRoleMutation();
    assert(!roles.AcceptReload(old,{},true)); // reload cannot erase newer local role change.
}
struct ScriptedRuntime final : LocalCardPort {
    std::vector<int> &trace;
    LocalProfileRuntimeResult result;
    SwitchProfileRequest saved;
    EffectKnowledge ended {EffectKnowledge::NotSent};
    ScriptedRuntime(std::vector<int> &t, LocalProfileRuntimeResult r, SwitchProfileRequest request)
        : trace(t),result(r),saved(request) {}
    void VerifyPin(const VerifyPinRequest &) override { assert(false); }
    void ReadElementaryFile(const ReadEfRequest &) override { assert(false); }
    std::optional<ExpectedProfileToken> BeginExpectedProfileTransition(const SwitchProfileRequest &r) override
    { trace.push_back(1);
      if (!result.fileReturned) { return std::nullopt; } // 原slot/File校验失败，不登记。
      return ExpectedProfileToken{r.context.step,*r.context.sentSession}; }
    LocalProfileRuntimeResult SwitchLocalProfile(const SwitchProfileRequest &r) override
    { trace.push_back(2); saved=r; return result; }
    void EndProfileRequestWait(const ExpectedProfileToken &,EffectKnowledge effect) override
    { trace.push_back(4); ended=effect; }
};
struct PreferenceRecorder final : LocalProfilePreferencePort {
    std::vector<int> &trace;
    explicit PreferenceRecorder(std::vector<int> &t) : trace(t) {}
    void AcceptLocalProfileReturnPreference() override { trace.push_back(3); }
};
static void ProfileParametersAndResults()
{
    SessionRef old {{{1,2}},{{0},"usim"},4};
    RequestContext context {{{21},0,0},SlotTarget{{0}},old,{{1},false,CompletionKind::InternalReceipt}};
    SwitchProfileRequest request {context,{{0},"usim"},"A",7,true,std::nullopt,1000};
    for (auto fixture : std::vector<LocalProfileRuntimeResult>{
        {{-9,std::nullopt},false,EffectKnowledge::NotSent,false}, // invalid slot/File: no hint.
        {{0,-55},true,EffectKnowledge::ConfirmedNotApplied,true}, // card failure still hints.
        {{0,-55},true,EffectKnowledge::Unknown,false}, // APDU timeout: possible card effect.
        {{0,0},true,EffectKnowledge::ConfirmedApplied,false}}) { // card OK, close unknown.
        std::vector<int> trace;
        ScriptedRuntime runtime(trace,fixture,request); PreferenceRecorder preference(trace);
        LocalCardRequests useCase(runtime,preference);
        auto result=useCase.SwitchLocalProfile(request);
        assert(result.outerLegacyResult == fixture.legacy.outerLegacyResult && result.cardEnumResult == fixture.legacy.cardEnumResult);
        assert(runtime.saved.legacyPortIndex == 7 && runtime.saved.forceDisableProfile && !runtime.saved.verifiedRoute);
        assert(runtime.ended == (fixture.fileReturned ? fixture.profileEffect : EffectKnowledge::NotSent));
        assert(trace == (fixture.fileReturned ? std::vector<int>{1,2,3,4} : std::vector<int>{1,2}));
    }
}
int main() { PrimaryModes(); CompletionBoundaries(); JointRowsAndReload(); ProfileParametersAndResults(); }
