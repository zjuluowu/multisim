#include "../../../framework/internal/common/primary_observation.h"
#include "../../../framework/sim_test.h"
#include <gtest/gtest.h>
#include "telephony_errors.h"
#include "../../shared/account_metadata_scenario.h"
#include "../../shared/default_call_message_scenario.h"
#include "../../shared/ims_preference_scenario.h"
#include "../../shared/cellular_selection_scenario.h"
#include "../../shared/show_number_scenario.h"
#include "../../shared/account_reload_scenario.h"
#include "../../shared/unlock_pin_scenario.h"
#include "core_service_errors.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "account/account_store_adapter.h"
#include "account/topology_service.h"
#include "infrastructure/assembly/sim_module.h"
#include "role_assignment/assignment_service.h"
#include "role_assignment/assignment_store_adapter.h"

using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr auto kRaw0 = "8900000000000000000";
constexpr auto kRaw1 = "8900000000000000001";
constexpr auto kEncoded1 = "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";
constexpr auto kPrimary = "persist.telephony.MainSlotId";
constexpr auto kIccid = "persist.telephony.MainCard.Iccid";
constexpr auto kData = "persist.telephony.MainCellularDataSlotId";
constexpr auto kPreference = "persist.telephony.userPreferPrimarySlot";

class UnsupportedAccountStore final : public AccountStore {
public:
    AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &) override { return {-1, false, true}; }
    AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &) override { return {-1, false, true}; }
    AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &) override { return {-1, false, true}; }
    AccountStoreWriteResult WriteLabel(const AccountLabelPatch &) override { return {-1, false, true}; }
    AccountLoadResult LoadAccounts(const AccountLoadRequest &r) override { return {r, -1, false, false, {}, {},}; }
    AccountIdentityMatch MatchAccountByIccid(const std::string &) override { return {-1, true, std::nullopt}; }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &) override
    { return {-1, std::nullopt, false, true, EffectKnowledge::Rejected}; }
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &) override { return {-1, false, true}; }
};

class ControlledDiscoveryStore final : public AccountStore {
public:
    std::optional<AccountStorageRow> matched;
    int32_t matchResult {0};
    bool matchFinished {true};
    AccountDiscoveryStoreResult stored {0, SubscriptionId{41}, true, true, EffectKnowledge::ConfirmedApplied};
    std::function<void(const AccountDiscoveryWritePlan &)> beforeStoreReturn;
    std::function<AccountDiscoveryStoreResult(const AccountDiscoveryWritePlan &)> produceStore;
    unsigned matches {0};
    unsigned writes {0};
    std::function<AccountLoadResult(const AccountLoadRequest &)> load;
    std::atomic<unsigned> loads {0};
    std::optional<AccountDiscoveryWritePlan> lastPlan;
    std::optional<DiscoveryCallRef> lastMatchCall;
    std::optional<DiscoveryCallRef> returnedMatch;
    std::map<uint64_t, AccountDiscoveryStoreResult> originalEvidence;
    std::mutex callsMutex;
    bool persistRows {false};
    size_t inserts {0};
    std::map<std::string, AccountStorageRow> rows;
    std::function<void(DiscoveryCallRef)> beforeMatchReturn;

    void RecordWrite(const AccountDiscoveryWritePlan &plan, SubscriptionId assigned)
    {
        if (!persistRows) { return; }
        std::lock_guard<std::mutex> guard(callsMutex);
        if (!plan.existingSubscription) { ++inserts; }
        auto &row = rows[plan.accountFields.iccid];
        row.subscription = assigned;
        row.iccid = plan.accountFields.iccid;
        row.legacyPosition = plan.accountFields.legacyPosition;
        row.active = plan.accountFields.active;
        if (plan.accountFields.legacyLabel) { row.legacyLabel = plan.accountFields.legacyLabel; }
    }
    AccountIdentityMatch MatchDiscoveryAccount(DiscoveryCallRef call, const std::string &iccid) override
    {
        {
            std::lock_guard<std::mutex> guard(callsMutex);
            lastMatchCall = call;
        }
        const auto match = MatchAccountByIccid(iccid);
        if (beforeMatchReturn) { beforeMatchReturn(call); }
        return match;
    }
    bool DiscoveryMatchReturned(DiscoveryCallRef call) override { return returnedMatch == call; }
    std::optional<AccountDiscoveryStoreResult> ReadDiscoveryEvidence(DiscoveryCallRef call) override
    {
        const auto found = originalEvidence.find(call.admission);
        return found == originalEvidence.end() ? std::nullopt : std::make_optional(found->second);
    }
    void ProveApplied(const AccountDiscoveryWritePlan &plan, SubscriptionId assigned)
    {
        RecordWrite(plan, assigned);
        originalEvidence.insert_or_assign(plan.call.admission, AccountDiscoveryStoreResult {
            0, assigned, true, true, EffectKnowledge::ConfirmedApplied, DiscoveryWriteEvidence {plan.call, plan.Columns(), 0}});
    }
    AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &) override { return {-1,false,true}; }
    AccountStoreWriteResult WriteLabel(const AccountLabelPatch &) override { return {-1,false,true}; }
    AccountLoadResult LoadAccounts(const AccountLoadRequest &r) override
    {
        ++loads;
        return load ? load(r) : AccountLoadResult{r,-1,false,false,{}, {}};
    }
    AccountIdentityMatch MatchAccountByIccid(const std::string &iccid) override
    {
        std::lock_guard<std::mutex> guard(callsMutex);
        ++matches;
        const auto row = rows.find(iccid);
        return {matchResult, matchFinished, row == rows.end() ? matched : std::make_optional(row->second)};
    }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &plan) override
    {
        auto result = stored;
        const auto produce = produceStore;
        const auto beforeReturn = beforeStoreReturn;
        {
            std::lock_guard<std::mutex> guard(callsMutex);
            ++writes;
            lastPlan = plan;
        }
        if (beforeReturn) { beforeReturn(plan); }
        if (produce) { result = produce(plan); }
        if (!result.evidence && result.writeConfirmed && result.effect == EffectKnowledge::ConfirmedApplied) {
            if (result.assignedSubscription) { RecordWrite(plan, *result.assignedSubscription); }
            result.evidence = DiscoveryWriteEvidence {plan.call, plan.Columns(), 0};
        }
        if (!result.evidence && result.effect == EffectKnowledge::ConfirmedNotApplied) {
            result.evidence = DiscoveryWriteEvidence {plan.call, 0, plan.Columns()};
        }
        return result;
    }
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &) override { return {-1,false,true}; }
};

class Parameters final : public AssignmentParameterPort {
public:
    std::map<std::string, std::string> values {{kPrimary, "0"}, {kData, "0"},
        {kIccid, "previous"}, {kPreference, "false"}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::function<AssignmentParameterResult(const std::string &)> writeResult;
    std::vector<C0ParameterOperation> originals;
    std::optional<C0ParameterEvidence> evidence;
    std::function<C0ParameterOutcome(const C0ParameterOperation &, AssignmentParameterResult)> classify;
    size_t evidenceReads {0};
    C0ParameterOutcome WriteC0Parameter(const C0ParameterOperation &operation, const std::string &key,
        const std::string &value) override
    {
        originals.push_back(operation);
        if (classify) { return classify(operation, Write(key, value)); }
        return AssignmentParameterPort::WriteC0Parameter(operation, key, value);
    }
    std::optional<C0ParameterEvidence> ReadC0ParameterEvidence(const C0ParameterOperation &) override
    {
        ++evidenceReads;
        return evidence;
    }
    AssignmentParameterResult Read(const std::string &key, const std::string &fallback,
        std::string &value) override
    { value = values.count(key) ? values[key] : fallback; return {0, true}; }
    AssignmentParameterResult Write(const std::string &key, const std::string &value) override
    {
        writes.emplace_back(key, value);
        const auto result = writeResult ? writeResult(key) : AssignmentParameterResult {0, true};
        if (result.callFinished && result.parameterResult == 0) { values[key] = value; }
        return result;
    }
};
class Radio final : public RadioExecutionPort {
public:
    unsigned switches {0};
    mutable std::function<void()> beforeRead;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    {
        if (beforeRead) { const_cast<Radio *>(this)->beforeRead(); beforeRead = {}; }
        return {slot, slot == SlotId {1} ? 0 : 1, false, false, 9, true,
            PrimaryProtocolRouting {slot == SlotId {0} ? 100u : 200u,
                {{100, {0}, 8, RadioProtocolCapability {1, 0x22}},
                 {200, {1}, 9, RadioProtocolCapability {0, 0x11}}}}};
    }
    RadioSubmitDisposition SwitchPrimaryRadio(const RadioSwitchRequest &, const RadioSendQualification &) override
    { ++switches; return RadioSubmitDisposition::HandoffAccepted; }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++switches; }
};
class RoleProvider final : public AssignmentRoleProviderPort {
public:
    bool independent {true};
    std::function<RoleProviderOutcome(const RoleProviderOperation &)> control;
    std::optional<RoleProviderEvidence> originalEvidence;
    std::mutex mutex;
    std::vector<std::pair<DefaultRole, int32_t>> writes;
    std::map<DefaultRole, int32_t> persisted;

    bool HasIndependentRoleTransactions() const override { return independent; }
    RoleProviderOutcome UpdateDefaultCard(const RoleProviderOperation &operation, std::string_view uri,
        const RoleProviderValues &values) override
    {
        EXPECT_EQ(uri, "datashare:///com.ohos.simability/sim/sim_info/set_card");
        EXPECT_EQ(values[0].first, "sim_id");
        EXPECT_EQ(values[1].first, "card_type");
        EXPECT_EQ(values[1].second, operation.role == DefaultRole::Voice ? 1 : 2);
        {
            std::lock_guard<std::mutex> lock(mutex);
            writes.emplace_back(operation.role, values[0].second);
        }
        const auto outcome = control ? control(operation) : RoleProviderOutcome {0, true, RoleProviderEffect::Applied};
        if (outcome.effect == RoleProviderEffect::Applied) {
            std::lock_guard<std::mutex> lock(mutex);
            persisted[operation.role] = values[0].second;
        }
        return outcome;
    }
    std::optional<RoleProviderEvidence> ReadRoleProviderEvidence(const RoleProviderOperation &) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        return originalEvidence;
    }
};
class Output final : public AssignmentOutputPort {
public:
    std::vector<std::string> events;
    std::vector<std::pair<SlotId, SubscriptionId>> mainChanges;
    std::vector<std::pair<SlotId, SubscriptionId>> dataChanges;
    void PublishPrimaryPhase(bool, bool) override {}
    void PublishMainCardChanged(SlotId) override { events.emplace_back("primary"); }
    void PublishMainCardChanged(SlotId slot, SubscriptionId subscription) override
    {
        mainChanges.emplace_back(slot, subscription);
        PublishMainCardChanged(slot);
    }
    std::vector<std::pair<DefaultRole, std::optional<SubscriptionId>>> roleChanges;
    bool publishRole {true};
    bool PublishDefaultRoleChanged(DefaultRole role, std::optional<SubscriptionId> subscription) override
    {
        roleChanges.emplace_back(role, subscription);
        events.emplace_back(role == DefaultRole::Voice ? "voice" : "sms");
        return publishRole;
    }
    std::function<void(SubscriptionId)> onDataChanged;
    std::function<void(SlotId)> onDataBehavior;
    void ReportDefaultDataSelection(SlotId slot) override
    {
        events.emplace_back("data-behavior");
        dataBehavior.push_back(slot);
        if (onDataBehavior) { onDataBehavior(slot); }
    }
    std::vector<SlotId> dataBehavior;
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) override
    {
        dataChanges.emplace_back(slot, subscription);
        events.emplace_back("data");
        if (onDataChanged) { onDataChanged(subscription); }
    }
};
class Completion final : public LegacyIntCompletionPort {
public:
    std::vector<int32_t> results;
    std::function<void()> callback;
    void CompleteLegacyInt(const CompletionPlan &, int32_t result) override
    {
        results.push_back(result);
        if (callback) { callback(); }
    }
};

AccountRecord Account(SubscriptionId subscription, const std::string &iccid, bool active = true)
{
    AccountProjection current;
    current.active = active;
    current.legacyPosition = SlotId {subscription.legacySimId};
    AccountProjection historical = current;
    return {subscription, iccid, 1, current, historical};
}
CardDiscoveryObservation Discovery(int slot, uint64_t generation, const std::string &iccid)
{ return {{RuntimeEpoch {5, 9}, CardApplicationTarget {SlotId {slot}, "USIM"}, generation},
          generation + 10, iccid}; }
AccountDiscoveryStoreResult Stored(SubscriptionId subscription)
{ return {0, subscription, true, true, EffectKnowledge::ConfirmedApplied}; }

struct DiscoveryFixture {
    SlotCatalog catalog {{
        {SlotId {0}, EndpointKind::Physical, 1, RadioDomainId {"modem-1"}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2}};
    ControlledDiscoveryStore accountStore;
    TopologyService topology {catalog, accountStore, RuntimeEpoch {5, 9}};
    Parameters parameters;
    AssignmentStoreAdapter store {parameters};
    Radio radio;
    Output output;
    Completion completion;
    AssignmentService service {catalog, store, radio, output, topology, completion, RuntimeEpoch {5, 9}};
    DiscoveryFixture()
    {
        topology.BindRolePlanner(service);
        EXPECT_TRUE(topology.AcceptSlotAssembly({1},true));
    }
};

struct Fixture {
    SlotCatalog catalog {{
        {SlotId {0}, EndpointKind::Physical, 1, RadioDomainId {"modem-1"}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2}};
    UnsupportedAccountStore accountStore;
    TopologyService topology {catalog, accountStore, RuntimeEpoch {5, 9}};
    Parameters parameters;
    AssignmentStoreAdapter store {parameters};
    Radio radio;
    Output output;
    Completion completion;
    AssignmentService service {catalog, store, radio, output, topology, completion, RuntimeEpoch {5, 9}};
    uint64_t discoveryGeneration {0};
    BindingRef seed(int slot, SubscriptionId subscription, const std::string &iccid, bool active = true)
    {
        EXPECT_TRUE(topology.AcceptSlotAssembly(SlotId {slot}, true));
        auto binding = topology.AcceptStoredDiscovery(Discovery(slot, ++discoveryGeneration, iccid),
            Account(subscription, iccid, active), Stored(subscription));
        EXPECT_TRUE(binding.has_value());
        return binding.value_or(BindingRef {});
    }
};

SwitchPrimaryCommand Command(const BindingRef &binding, bool withBinding = true)
{
    return {SlotTarget {binding.slot}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {31}}, false}, std::nullopt,
        withBinding ? std::optional<BindingRef>{binding} : std::nullopt};
}
} // namespace

TEST(TargetTopologyIntegration, T1NewDiscoveryExecutesOneProviderWriteAndAcceptsTARoles)
{
    DiscoveryFixture f;
    auto observation=Discovery(1,1,kRaw1);
    observation.legacyLabel=2;
    observation.isEsim=true;
    const auto acceptance=f.topology.DiscoverAccount(observation);
    EXPECT_TRUE(acceptance.bindingAccepted);
    EXPECT_EQ(f.accountStore.matches,1u);
    EXPECT_EQ(f.accountStore.writes,1u);
    ASSERT_TRUE(f.accountStore.lastPlan.has_value());
    EXPECT_TRUE(f.accountStore.lastPlan->roleFields.includeInStorage);
    EXPECT_EQ(f.accountStore.lastPlan->accountFields.legacyLabel,std::optional<int32_t>{2});
    EXPECT_TRUE(f.accountStore.lastPlan->accountFields.isEsim);
    EXPECT_FALSE(f.accountStore.lastPlan->existingSubscription.has_value());
    const auto binding=f.topology.FindBinding({1});
    ASSERT_TRUE(binding.has_value());
    EXPECT_EQ(binding->subscription.legacySimId,41);
    ASSERT_TRUE(f.topology.ReadAccount({41},AccountReadView::Current).has_value());
    EXPECT_EQ(f.topology.ReadPrimaryTarget({1})->iccid,kRaw1);
}

TEST(TargetTopologyIntegration, T1ProviderIoDoesNotHoldTLockAcrossAReadWrite)
{
    DiscoveryFixture f;
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    auto zero=f.topology.AcceptStoredDiscovery(Discovery(0,1,kRaw0),
        Account({2},kRaw0),Stored({2}));
    ASSERT_TRUE(zero.has_value());
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        auto primary=std::async(std::launch::async,[&] {
            return f.service.StartPrimarySwitch(Command(*zero));
        });
        ASSERT_EQ(primary.wait_for(std::chrono::seconds(3)),std::future_status::ready);
        (void)primary.get(); // C1 may remain in device pending; readiness proves no T/A lock cycle.
    };
    EXPECT_TRUE(f.topology.DiscoverAccount(Discovery(1,2,kRaw1)).bindingAccepted);
}

TEST(TargetTopologyIntegration, T1KnownIccidReusesIdentityWithoutNewInsertSemantics)
{
    DiscoveryFixture f;
    f.accountStore.matched=AccountStorageRow{{7},kRaw1,SlotId{0},u"old",u"100",true,3,0};
    f.accountStore.stored={0,SubscriptionId{7},true,true,EffectKnowledge::ConfirmedApplied};
    const auto acceptance=f.topology.DiscoverAccount(Discovery(1,2,kRaw1));
    EXPECT_TRUE(acceptance.bindingAccepted);
    EXPECT_EQ(f.topology.FindBinding({1})->subscription.legacySimId,7);
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1KnownAccountAlreadyBoundCannotShadowMove)
{
    DiscoveryFixture f;
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    f.accountStore.matched=AccountStorageRow{{7},kRaw1,SlotId{0},u"",u"",true,std::nullopt,0};
    f.accountStore.stored={0,SubscriptionId{7},true,true,EffectKnowledge::ConfirmedApplied};
    EXPECT_TRUE(f.topology.DiscoverAccount(Discovery(0,1,kRaw1)).bindingAccepted);
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(1,2,kRaw1)).bindingAccepted);
    ASSERT_TRUE(f.topology.FindBinding({0}).has_value());
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1DuplicateObservationDoesNotRepeatStore)
{
    DiscoveryFixture f;
    const auto observation=Discovery(1,1,kRaw1);
    EXPECT_TRUE(f.topology.DiscoverAccount(observation).bindingAccepted);
    const auto duplicate=f.topology.DiscoverAccount(observation);
    EXPECT_FALSE(duplicate.bindingAccepted);
    EXPECT_EQ(f.accountStore.matches,1u);
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1ObservationInvalidatedDuringWriteKeepsEffectWithoutBinding)
{
    DiscoveryFixture f;
    const auto observation=Discovery(1,1,kRaw1);
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        f.topology.AcceptCardSessionInvalidation({observation.session,observation.observationSequence+1});
    };
    const auto acceptance=f.topology.DiscoverAccount(observation);
    EXPECT_FALSE(acceptance.bindingAccepted);
    ASSERT_TRUE(acceptance.storedSubscription.has_value());
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1ConcurrentInvalidationRejectsBlockedProviderReturn)
{
    DiscoveryFixture f;
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    const auto observation=Discovery(1,1,kRaw1);
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        entered.set_value(); released.wait();
    };
    auto discover=std::async(std::launch::async,[&] { return f.topology.DiscoverAccount(observation); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    f.topology.AcceptCardSessionInvalidation({observation.session,observation.observationSequence+1});
    release.set_value();
    const auto accepted=discover.get();
    EXPECT_FALSE(accepted.bindingAccepted);
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1UnknownStoreEffectIsNotRetriedByDuplicateObservation)
{
    DiscoveryFixture f;
    f.accountStore.stored={-1,SubscriptionId{41},false,true,EffectKnowledge::Unknown};
    const auto observation=Discovery(1,1,kRaw1);
    EXPECT_FALSE(f.topology.DiscoverAccount(observation).bindingAccepted);
    EXPECT_FALSE(f.topology.DiscoverAccount(observation).bindingAccepted);
    EXPECT_EQ(f.accountStore.writes,1u);
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
}

TEST(TargetTopologyIntegration, T1ConfirmedNotAppliedDoesNotCreateBinding)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,std::nullopt,false,true,EffectKnowledge::ConfirmedNotApplied};
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1PendingDiscoveryPreventsReloadedRoleReplacement)
{
    DiscoveryFixture f;
    ASSERT_TRUE(f.topology.AcceptSlotAssembly({1}, true));
    f.accountStore.beforeStoreReturn = [&](const AccountDiscoveryWritePlan &) {
        const auto reload = f.topology.PrepareAccountReload(AccountReadView::Current, 1);
        const auto result = f.topology.AcceptAccountReload(reload, {reload.storeRequest, 0, true, true, {}, {}});
        EXPECT_NE(result.roles, RoleReloadDisposition::Accepted);
    };
    const auto acceptance = f.topology.DiscoverAccount(Discovery(1, 1, kRaw1));
    EXPECT_TRUE(acceptance.bindingAccepted);
    EXPECT_TRUE(f.topology.FindBinding({1}));
    EXPECT_TRUE(f.topology.ReadAccount({41}, AccountReadView::Current));
}

TEST(TargetTopologyIntegration, T1ProviderIdentityMismatchForKnownIccidIsRejected)
{
    DiscoveryFixture f;
    f.accountStore.matched=AccountStorageRow{{7},kRaw1,SlotId{0},u"",u"",true,std::nullopt,0};
    f.accountStore.stored={0,SubscriptionId{9},true,true,EffectKnowledge::ConfirmedApplied};
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1MatchFailureBeforeWriteDoesNotReserveProviderEffect)
{
    DiscoveryFixture f;
    f.accountStore.matchFinished=false;
    f.accountStore.matchResult=-1;
    const auto result=f.topology.DiscoverAccount(Discovery(1,1,kRaw1));
    EXPECT_FALSE(result.bindingAccepted);
    EXPECT_EQ(f.accountStore.matches,1u);
    EXPECT_EQ(f.accountStore.writes,0u);
}

TEST(TargetTopologyIntegration, T2AcceptedRoleReservationSurvivesLaterUnrelatedRoleMutation)
{
    AccountRoleProtocol roles;
    const auto planned=roles.Prepare({std::nullopt,{1},false});
    ASSERT_TRUE(planned.has_value());
    ASSERT_TRUE(roles.AcceptDiscovered(*planned,{41}));
    roles.NoteLocalRoleMutation();
    EXPECT_TRUE(roles.StillOwns(*planned));
    EXPECT_TRUE(roles.Finalize(*planned,{true,EffectKnowledge::ConfirmedApplied,true}));
}

TEST(TargetTopologyIntegration, T2StaleAppliedCanReconcileWithoutRestoringOldBinding)
{
    DiscoveryFixture f;
    auto observation=Discovery(1,1,kRaw1);
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        f.topology.AcceptCardSessionInvalidation({observation.session,observation.observationSequence+1});
    };
    EXPECT_FALSE(f.topology.DiscoverAccount(observation).bindingAccepted);
    EXPECT_FALSE(f.topology.ReconcileDiscovery(observation)); // Confirmed stale effect was already disposed.
    EXPECT_FALSE(f.topology.FindBinding({1}));
    EXPECT_FALSE(f.topology.ReadAccount({41},AccountReadView::Current));
    EXPECT_TRUE(f.topology.ReadAccount({41},AccountReadView::Historical));
    EXPECT_EQ(f.service.PendingDiscoveryRoles(), 0u);
}

TEST(TargetTopologyIntegration, T2RecoveryDoesNotReleaseRoleForMissingProviderRow)
{
    DiscoveryFixture f;
    f.accountStore.stored={-1,SubscriptionId{41},false,true,EffectKnowledge::Unknown};
    const auto observation=Discovery(1,1,kRaw1);
    EXPECT_FALSE(f.topology.DiscoverAccount(observation).bindingAccepted);
    EXPECT_FALSE(f.topology.ReconcileDiscovery(observation));
    f.accountStore.matched=AccountStorageRow{{41},kRaw1,SlotId{1},u"",u"",true,std::nullopt,0};
    EXPECT_FALSE(f.topology.ReconcileDiscovery(observation)); // A matching row cannot attribute the original write.
    ASSERT_TRUE(f.accountStore.lastPlan);
    f.accountStore.ProveApplied(*f.accountStore.lastPlan, {41});
    EXPECT_TRUE(f.topology.ReconcileDiscovery(observation));
    EXPECT_TRUE(f.topology.FindBinding({1}));
    EXPECT_EQ(f.service.PendingDiscoveryRoles(), 0u);
}

TEST(TargetTopologyIntegration, T1ProviderReconnectRequiresFreshObservationNotCachedRetry)
{
    DiscoveryFixture f;
    f.accountStore.matchFinished=false;
    f.accountStore.matchResult=-1;
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    EXPECT_EQ(f.accountStore.writes,0u);
    f.accountStore.matchFinished=true;
    f.accountStore.matchResult=0;
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    EXPECT_EQ(f.accountStore.writes,0u);
    ASSERT_TRUE(f.accountStore.lastMatchCall);
    f.accountStore.returnedMatch = f.accountStore.lastMatchCall;
    EXPECT_TRUE(f.topology.ReconcileDiscovery(*f.accountStore.lastMatchCall));
    EXPECT_TRUE(f.topology.DiscoverAccount(Discovery(1,2,kRaw1)).bindingAccepted);
    EXPECT_EQ(f.accountStore.writes,1u);
}

TEST(TargetTopologyIntegration, T1SameSlotNewCardDoesNotInheritOldCompletedWrite)
{
    DiscoveryFixture f;
    const auto old=Discovery(1,1,"old-card");
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        f.topology.AcceptCardSessionInvalidation({old.session,old.observationSequence+1});
    };
    EXPECT_FALSE(f.topology.DiscoverAccount(old).bindingAccepted);
    f.accountStore.beforeStoreReturn={};
    f.accountStore.stored={0,SubscriptionId{42},true,true,EffectKnowledge::ConfirmedApplied};
    auto replacement = Discovery(1,2,"new-card");
    replacement.observationSequence = old.observationSequence + 2; // After the old source's revocation.
    EXPECT_TRUE(f.topology.DiscoverAccount(replacement).bindingAccepted);
    ASSERT_TRUE(f.topology.FindBinding({1}));
    EXPECT_EQ(f.topology.FindBinding({1})->subscription.legacySimId, 42);
    EXPECT_FALSE(f.topology.ReadAccount({41},AccountReadView::Current));
    EXPECT_TRUE(f.topology.ReadAccount({41},AccountReadView::Historical));
}

TEST(TargetTopologyIntegration, T2SameIccidDifferentSlotIsRejectedBeforeProviderWrite)
{
    DiscoveryFixture f;
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &plan) {
        if (plan.accountFields.legacyPosition.legacySlotId==1) {
            entered.set_value(); released.wait();
        }
    };
    auto first=std::async(std::launch::async,[&] { return f.topology.DiscoverAccount(Discovery(1,1,kRaw1)); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    EXPECT_FALSE(f.topology.DiscoverAccount(Discovery(0,2,kRaw1)).bindingAccepted);
    EXPECT_EQ(f.accountStore.writes,1u);
    release.set_value();
    EXPECT_TRUE(first.get().bindingAccepted);
}

TEST(TargetTopologyIntegration, T2C1InFlightDoesNotBlockUnrelatedDiscoveryReservation)
{
    DiscoveryFixture f;
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    const auto zero=f.topology.AcceptStoredDiscovery(Discovery(0,1,kRaw0),
        Account({2},kRaw0),Stored({2}));
    ASSERT_TRUE(zero.has_value());
    f.service.StartPrimarySwitch(Command(*zero));
    EXPECT_GT(f.radio.switches,0u);
    EXPECT_TRUE(f.topology.DiscoverAccount(Discovery(1,2,kRaw1)).bindingAccepted);
    EXPECT_TRUE(f.topology.FindBinding({0}).has_value());
    EXPECT_TRUE(f.topology.FindBinding({1}).has_value());
}

TEST(TargetTopologyIntegration, T2UnrelatedDiscoveryDoesNotSharePendingRoleReservation)
{
    DiscoveryFixture f;
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &plan) {
        if (plan.accountFields.legacyPosition.legacySlotId==1) {
            entered.set_value(); released.wait();
        }
    };
    auto first=std::async(std::launch::async,[&] { return f.topology.DiscoverAccount(Discovery(1,1,kRaw1)); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    f.accountStore.produceStore=[](const AccountDiscoveryWritePlan &plan) {
        return AccountDiscoveryStoreResult{0,
            SubscriptionId{plan.accountFields.legacyPosition.legacySlotId==0 ? 42 : 41},
            true,true,EffectKnowledge::ConfirmedApplied};
    };
    auto second=f.topology.DiscoverAccount(Discovery(0,2,"other-card"));
    EXPECT_TRUE(second.bindingAccepted);
    release.set_value();
    EXPECT_TRUE(first.get().bindingAccepted);
}

TEST(TargetTopologyIntegration, T2ReloadCurrentAndHistoricalAcceptIndependentAttempts)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,SubscriptionId{41},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    const auto current=f.topology.PrepareAccountReload(AccountReadView::Current,91);
    const auto historical=f.topology.PrepareAccountReload(AccountReadView::Historical,92);
    const AccountStorageRow row {{41},kRaw1,SlotId{1},u"loaded",u"100",true,2,0};
    f.accountStore.load=[&](const AccountLoadRequest &r) {
        return AccountLoadResult{r,0,true,r.view==AccountReadView::Current,{row},
            {{ {41}, true,false,true,true }}};
    };
    const auto historicalLoad=f.accountStore.LoadAccounts(historical.storeRequest);
    const auto currentLoad=f.accountStore.LoadAccounts(current.storeRequest);
    f.topology.AcceptAccountReload(historical,historicalLoad);
    f.topology.AcceptAccountReload(current,currentLoad);
    EXPECT_EQ(f.topology.ReadAccount({41},AccountReadView::Current)->displayName,u"loaded");
    EXPECT_EQ(f.topology.ReadAccount({41},AccountReadView::Historical)->displayName,u"loaded");
}

TEST(TargetTopologyIntegration, T2FailedViewDoesNotClearOtherOrSameView)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,SubscriptionId{41},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    const auto original=f.topology.ReadAccount({41},AccountReadView::Current);
    const auto current=f.topology.PrepareAccountReload(AccountReadView::Current,1);
    f.topology.AcceptAccountReload(current,{current.storeRequest,-1,false,false,{}, {}});
    ASSERT_TRUE(f.topology.ReadAccount({41},AccountReadView::Current).has_value());
    EXPECT_EQ(f.topology.ReadAccount({41},AccountReadView::Current)->identityRevision,
        original->identityRevision);
    const auto historical=f.topology.PrepareAccountReload(AccountReadView::Historical,2);
    const AccountStorageRow row {{41},kRaw1,SlotId{1},u"history",u"200",true,2,0};
    f.topology.AcceptAccountReload(historical,{historical.storeRequest,0,true,true,{row},
        {{{41},false,false,false,false}}});
    EXPECT_EQ(f.topology.ReadAccount({41},AccountReadView::Historical)->displayName,u"history");
}

TEST(TargetTopologyIntegration, T2OlderReloadCannotOverwriteFieldUpdatedAfterAttempt)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,SubscriptionId{41},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    auto before=f.topology.ReadAccount({41},AccountReadView::Current);
    const auto reload=f.topology.PrepareAccountReload(AccountReadView::Current,1);
    EXPECT_EQ(f.topology.AcceptActivationProjection({41},AccountReadView::Current,
        before->fields.active,false),AccountAcceptance::Accepted);
    const AccountStorageRow oldRow {{41},kRaw1,SlotId{1},u"name",u"old",true,2,0};
    f.topology.AcceptAccountReload(reload,{reload.storeRequest,0,true,true,{oldRow},
        {{{41},false,false,false,false}}});
    EXPECT_FALSE(f.topology.ReadAccount({41},AccountReadView::Current)->activeProjection);
}

TEST(TargetTopologyIntegration, T2OutOfOrderAttemptsUseLatestOnly)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,SubscriptionId{41},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    auto first=f.topology.PrepareAccountReload(AccountReadView::Current,1);
    auto second=f.topology.PrepareAccountReload(AccountReadView::Current,2);
    const AccountStorageRow row {{41},kRaw1,SlotId{1},u"latest",u"1",true,2,0};
    f.topology.AcceptAccountReload(second,{second.storeRequest,0,true,true,{row},
        {{{41},false,false,false,false}}});
    f.topology.AcceptAccountReload(first,{first.storeRequest,0,true,true,{row},
        {{{41},false,false,false,false}}});
    EXPECT_EQ(f.topology.ReadAccount({41},AccountReadView::Current)->displayName,u"latest");
}

TEST(TargetTopologyIntegration, T2ReloadDoesNotRemovePostAttemptNewAccount)
{
    DiscoveryFixture f;
    f.accountStore.stored={0,SubscriptionId{41},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1,1,kRaw1)).bindingAccepted);
    const auto reload=f.topology.PrepareAccountReload(AccountReadView::Current,1);
    EXPECT_TRUE(f.topology.AcceptSlotAssembly({0},true));
    f.accountStore.stored={0,SubscriptionId{42},true,true,EffectKnowledge::ConfirmedApplied};
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(0,2,"second-card")).bindingAccepted);
    const AccountStorageRow oldOnly {{41},kRaw1,SlotId{1},u"old",u"",true,2,0};
    f.topology.AcceptAccountReload(reload,{reload.storeRequest,0,true,true,{oldOnly},
        {{{41},false,false,false,false}}});
    EXPECT_TRUE(f.topology.ReadAccount({42},AccountReadView::Current).has_value());
}

TEST(TargetTopologyIntegration, T1OldEpochCannotEnterNewTopology)
{
    DiscoveryFixture f;
    CardDiscoveryObservation old {{RuntimeEpoch{4,9},CardApplicationTarget{{1},"USIM"},1},11,kRaw1};
    EXPECT_FALSE(f.topology.DiscoverAccount(old).bindingAccepted);
    EXPECT_EQ(f.accountStore.matches,0u);
    EXPECT_EQ(f.accountStore.writes,0u);
}

TEST(TargetTopologyIntegration, C0UsesRealRegistryBindingIndexAndTopologyService)
{
    Fixture f;
    const auto zero = f.seed(0, {2}, kRaw0);
    const auto target = f.seed(1, {1}, kRaw1);
    const auto facts = f.topology.ReadPrimaryTarget(SlotId {1});
    ASSERT_TRUE(facts.has_value());
    EXPECT_TRUE(facts->binding.subscription == target.subscription);
    EXPECT_EQ(facts->binding.bindingRevision, target.bindingRevision);
    EXPECT_EQ(facts->iccid, kRaw1);
    EXPECT_TRUE(facts->active);
    f.service.StartPrimarySwitch(Command(target));
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {0}));
    EXPECT_EQ(f.parameters.writes, (std::vector<std::pair<std::string, std::string>> {
        {kPreference, "true"}, {kPrimary, "1"}, {kIccid, kEncoded1}, {kData, "1"}}));
    EXPECT_EQ(f.output.events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(f.radio.switches, 0u);
    EXPECT_TRUE(zero.subscription == SubscriptionId {2});
    RecordProperty("validation_level", "topology_integration");
    RecordProperty("binding_revision", std::to_string(target.bindingRevision));
    RecordProperty("target_business_path", "AccountRegistry/BindingIndex/TopologyService/AssignmentService/AssignmentStoreAdapter");
}

TEST(TargetTopologyIntegration, C0RejectsUnboundInactiveChangedAndStaleIdentity)
{
    Fixture f;
    f.topology.AcceptSlotAssembly(SlotId {1}, true);
    const auto wrongSubscription = f.topology.AcceptStoredDiscovery(Discovery(1, 1, kRaw1),
        Account({1}, kRaw1), Stored({9}));
    EXPECT_FALSE(wrongSubscription.has_value());
    EXPECT_FALSE(f.topology.ReadPrimaryTarget(SlotId {1}).has_value());
    const auto inactive = f.seed(1, {1}, kRaw1, false);
    EXPECT_FALSE(f.topology.ReadPrimaryTarget(SlotId {1})->active);
    auto oldEpoch = Command(inactive);
    oldEpoch.parent = StepRef {{8, RuntimeEpoch {4, 8}, RequestOwner::Activation}, 0, 0};
    f.service.StartPrimarySwitch(oldEpoch);
    EXPECT_EQ(f.completion.results.size(), 1u);
    EXPECT_NE(f.completion.results[0], 0);

    f.topology.AcceptCardRemoval(inactive);
    EXPECT_FALSE(f.topology.ReadPrimaryTarget(SlotId {1}).has_value());
    const auto replacement = f.seed(1, {3}, "new-card", true);
    EXPECT_GT(replacement.bindingRevision, inactive.bindingRevision);
    f.service.StartPrimarySwitch(Command(inactive));
    EXPECT_EQ(f.parameters.writes.size(), 0u);
    f.service.StartPrimarySwitch(Command(replacement));
    EXPECT_EQ(f.completion.results.back(), 0);
}

TEST(TargetTopologyIntegration, BindingAbaAndAccountViewLifetimeProtectC0)
{
    Fixture f;
    auto first = f.seed(1, {1}, kRaw1);
    f.topology.AcceptCardRemoval(first);
    auto same = f.seed(1, {1}, kRaw1);
    EXPECT_GT(same.bindingRevision, first.bindingRevision);
    EXPECT_NE(f.topology.ReadPrimaryTarget(SlotId {1})->binding.bindingRevision, first.bindingRevision);
    f.service.StartPrimarySwitch(Command(first));
    EXPECT_NE(f.completion.results.back(), 0);
    EXPECT_TRUE(f.parameters.writes.empty());

    const auto current = f.topology.ReadAccount({1}, AccountReadView::Current);
    const auto historical = f.topology.ReadAccount({1}, AccountReadView::Historical);
    ASSERT_TRUE(current.has_value());
    ASSERT_TRUE(historical.has_value());
    EXPECT_EQ(f.topology.AcceptActivationProjection({1}, AccountReadView::Current,
        current->fields.active, false), AccountAcceptance::Accepted);
    EXPECT_TRUE(f.topology.ReadAccount({1}, AccountReadView::Historical)->activeProjection);
    EXPECT_FALSE(f.topology.ReadPrimaryTarget(SlotId {1})->active);
    f.completion.results.clear();
    f.service.StartPrimarySwitch(Command(same));
    EXPECT_NE(f.completion.results.back(), 0);

    f.topology.AcceptCardRemoval(same);
    EXPECT_FALSE(f.topology.ReadPrimaryTarget(SlotId {1}).has_value());
    const auto wrongIdentity = f.topology.AcceptStoredDiscovery(Discovery(1, ++f.discoveryGeneration, kRaw1),
        Account({9}, kRaw1), Stored({9}));
    EXPECT_FALSE(wrongIdentity.has_value());
    auto rebound = f.seed(1, {1}, kRaw1);
    EXPECT_GT(rebound.bindingRevision, same.bindingRevision);
    EXPECT_NE(f.topology.ReadPrimaryTarget(SlotId {1})->binding.bindingRevision, same.bindingRevision);
}

TEST(TargetTopologyIntegration, BindingChangeDuringAssignmentReadRejectsOldRequest)
{
    Fixture f;
    const auto old = f.seed(1, {1}, kRaw1);
    f.radio.beforeRead = [&] {
        f.topology.AcceptCardRemoval(old);
        f.seed(1, {3}, "replacement-card");
    };
    f.service.StartPrimarySwitch(Command(old));
    ASSERT_EQ(f.completion.results.size(), 1u);
    EXPECT_NE(f.completion.results[0], 0);
    EXPECT_TRUE(f.parameters.values[kPrimary] == "0");
    EXPECT_TRUE(f.output.events.empty());
    EXPECT_EQ(f.radio.switches, 0u);
}

namespace {
class ModuleRadio final : public RadioPlatformPort {
public:
    bool requiresDevice {false};
    SlotId primaryEndpoint {1};
    unsigned commands {0};
    RadioSubmitDisposition disposition {RadioSubmitDisposition::HandoffAccepted};
    std::optional<PrimaryProtocolRouting> routing;
    std::optional<std::pair<RadioSwitchRequest, uint64_t>> sent;
    std::vector<std::pair<RadioSwitchRequest, uint64_t>> history;
    std::function<void(const RadioSwitchRequest &)> beforeSendPermit;
    std::function<void()> beforeSubmitReturn;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    { return {slot, slot == primaryEndpoint ? 0 : 1, requiresDevice, false, 7, true, routing}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &request, uint64_t token,
        const RadioSendPermit &permit) override
    {
        if (beforeSendPermit) { beforeSendPermit(request); }
        if (!routing || !request.member || std::none_of(routing->members.begin(), routing->members.end(),
            [&](const auto &member) { return member.MatchesRoute(*request.member); }) || !permit()) {
            return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
        }
        ++commands;
        sent.emplace(request, token);
        history.emplace_back(request, token);
        if (beforeSubmitReturn) { beforeSubmitReturn(); }
        return {disposition, static_cast<int32_t>(token)};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++commands; }
};

class ModuleDispatch final : public RetrySchedulerPort, public RadioOwnerDispatchPort {
public:
    bool defer {false};
    bool rejectRetryRegistration {false};
    bool failNextSeal {false};
    mutable bool failNextOwnerAudit {false};
    std::function<void()> afterAudit;
    unsigned seals {0};
    unsigned cancels {0};
    std::optional<uint64_t> ScheduleRetry(const ScheduledRetryEvent &event, uint64_t delay,
        const std::weak_ptr<ScheduledRetrySink> &sink) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!audit_.accepting || (rejectRetryRegistration && event.purpose == ScheduledRetryEvent::Purpose::Retry)) {
            return std::nullopt;
        }
        const auto id = nextTask_++;
        scheduled_.push_back({id, clock_ + delay, event, sink});
        ++audit_.scheduled;
        return id;
    }
    bool CancelRetry(uint64_t id) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        const auto found = std::find_if(scheduled_.begin(), scheduled_.end(), [id](const auto &task) {
            return task.id == id;
        });
        if (found == scheduled_.end()) { return false; }
        scheduled_.erase(found);
        ++audit_.cancelled;
        return true;
    }
    size_t CancelAllForOwner(RuntimeEpoch epoch, RequestOwner owner) override
    {
        std::lock_guard<std::mutex> guard(lock_);
        ++cancels;
        const auto old = scheduled_.size();
        scheduled_.erase(std::remove_if(scheduled_.begin(), scheduled_.end(), [&](const auto &task) {
            return task.event.request.epoch == epoch && task.event.request.owner == owner;
        }), scheduled_.end());
        audit_.cancelled += old - scheduled_.size();
        return old - scheduled_.size();
    }
    void StopAcceptingAndCancel() override
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (std::exchange(failNextSeal, false)) { throw std::runtime_error("scheduler seal not completed"); }
        ++seals;
        audit_.accepting = false;
        audit_.cancelled += scheduled_.size();
        scheduled_.clear();
    }
    RetrySchedulerAudit Audit() const override
    {
        RetrySchedulerAudit audit;
        {
            std::lock_guard<std::mutex> guard(lock_);
            audit = audit_;
            audit.pending = scheduled_.size();
        }
        if (afterAudit) { afterAudit(); }
        return audit;
    }
    void Advance(uint64_t milliseconds)
    {
        std::unique_lock<std::mutex> guard(lock_);
        clock_ += milliseconds;
        for (;;) {
            const auto due = std::find_if(scheduled_.begin(), scheduled_.end(), [&](const auto &task) {
                return task.due <= clock_;
            });
            if (due == scheduled_.end()) { return; }
            const auto task = *due;
            scheduled_.erase(due);
            ++audit_.running;
            guard.unlock();
            const auto sink = task.sink.lock();
            const bool delivered = sink && sink->AcceptScheduledC1Retry(task.event);
            guard.lock();
            --audit_.running;
            if (delivered) { ++audit_.delivered; } else { ++audit_.rejected; }
        }
    }
    bool PostRadioOwnerTask(std::function<void()> task) override
    {
        if (defer) { ownerTasks_.push_back(std::move(task)); } else { task(); }
        return true;
    }
    bool RadioOwnerCallbacksDrained() const override
    {
        if (std::exchange(failNextOwnerAudit, false)) { throw std::runtime_error("Owner audit unavailable"); }
        return ownerTasks_.empty() && ownerRunning_ == 0;
    }
    size_t PendingOwnerTasks() const { return ownerTasks_.size(); }
    void RunOwnerTasks()
    {
        while (!ownerTasks_.empty()) {
            auto task = std::move(ownerTasks_.front());
            ownerTasks_.pop_front();
            ++ownerRunning_;
            task();
            --ownerRunning_;
        }
    }
private:
    struct Task {
        uint64_t id;
        uint64_t due;
        ScheduledRetryEvent event;
        std::weak_ptr<ScheduledRetrySink> sink;
    };
    mutable std::mutex lock_;
    std::vector<Task> scheduled_;
    RetrySchedulerAudit audit_;
    uint64_t nextTask_ {1};
    uint64_t clock_ {0};
    std::deque<std::function<void()>> ownerTasks_;
    size_t ownerRunning_ {0};
};

struct ModuleInputs {
    std::shared_ptr<SlotCatalog> catalog = std::make_shared<SlotCatalog>(std::vector<SlotDescriptor> {
        {{0}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false},
        {{1}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false}},
        LegacyCapacityMeasures {2, 2, 2, 2, 2});
    std::shared_ptr<Parameters> parameters = std::make_shared<Parameters>();
    std::shared_ptr<ModuleRadio> radio = std::make_shared<ModuleRadio>();
    std::shared_ptr<Output> output = std::make_shared<Output>();
    std::shared_ptr<Completion> completion = std::make_shared<Completion>();
    std::shared_ptr<ModuleDispatch> dispatch = std::make_shared<ModuleDispatch>();
    std::shared_ptr<AccountStore> accountStore = std::make_shared<UnsupportedAccountStore>();
    CoreSimModulePorts Ports(std::shared_ptr<RoleProvider> provider = {})
    {
        // The adapter borrows the parameter port; keep the pair in one shared control block.
        struct ParameterAdapter {
            ParameterAdapter(std::shared_ptr<Parameters> port, std::shared_ptr<RoleProvider> database)
                : parameters(std::move(port)), provider(std::move(database)), store(*parameters, provider.get()) {}
            std::shared_ptr<Parameters> parameters;
            std::shared_ptr<RoleProvider> provider;
            AssignmentStoreAdapter store;
        };
        const auto adapter = std::make_shared<ParameterAdapter>(parameters, std::move(provider));
        return {accountStore, std::shared_ptr<AssignmentStore>(adapter, &adapter->store),
            output, completion, radio, dispatch, dispatch, [](const ResourceKey &key, RuntimeEpoch epoch) {
                EXPECT_EQ(key.verifiedPhysicalDomain, "primary-shared");
                return InitialRecoveryEvidence {epoch, true, true};
            }};
    }
    const std::vector<ConfirmedSlotAccount> accounts {
        {{{0}, "USIM"}, 1, Account({40}, kRaw0), Stored({40})},
        {{{1}, "USIM"}, 1, Account({41}, kRaw1), Stored({41})}};
};
} // namespace

TEST(AF2Module, FactoryPublishesRealTopologyAndRunsC0)
{
    ModuleInputs input;
    auto result = CreateCoreSimModule(input.Ports(), input.catalog, input.accounts);
    ASSERT_TRUE(std::holds_alternative<CoreSimModuleHandle>(result));
    auto module = std::get<CoreSimModuleHandle>(std::move(result));
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    EXPECT_EQ(binding->subscription.legacySimId, 41);
    ASSERT_TRUE(module->ReadAccount({41}));
    EXPECT_EQ(module->Catalog().Slots().size(), 2u);
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    const auto request = module->StartPrimarySwitch(Command(*binding));
    ASSERT_TRUE(request);
    EXPECT_EQ(request->epoch, module->Epoch());
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(module->ReadAssignment().lastData, SlotId {1});
    EXPECT_EQ(input.parameters->values.at(kIccid), kEncoded1);
    EXPECT_EQ(input.output->events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(input.completion->results, std::vector<int32_t> {0});
    EXPECT_EQ(input.radio->commands, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2Module, AlreadyPrimaryUsesLiveModemMappingAfterEndpointRemap)
{
    ModuleInputs input;
    input.radio->primaryEndpoint = {0};
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    const auto current = module->FindBinding({0});
    const auto previous = module->FindBinding({1});
    ASSERT_TRUE(current && previous);
    ASSERT_EQ(module->Catalog().Find({0})->rilInstance, 1);
    ASSERT_EQ(module->Catalog().Find({1})->rilInstance, 0);

    ASSERT_TRUE(module->StartPrimarySwitch(Command(*current)));
    ASSERT_TRUE(module->StartPrimarySwitch(Command(*previous)));

    EXPECT_EQ(input.completion->results, (std::vector<int32_t> {0, -1}));
    EXPECT_EQ(input.radio->commands, 0u);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {0});
    EXPECT_EQ(input.parameters->values.at(kPrimary), "0");
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Module, StopRetriesAReportedSchedulerSealFailureBeforePostingOwnerClose)
{
    ModuleInputs input;
    input.dispatch->failNextSeal = true;
    input.dispatch->defer = true;
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    using Drain = SimModule::DrainResult;

    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    EXPECT_FALSE(module->StartPrimarySwitch(Command(*module->FindBinding({1}))));
    EXPECT_EQ(input.dispatch->seals, 0u);
    EXPECT_EQ(input.dispatch->PendingOwnerTasks(), 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    EXPECT_EQ(input.dispatch->seals, 1u);
    EXPECT_EQ(input.dispatch->PendingOwnerTasks(), 1u);
    input.dispatch->RunOwnerTasks();
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
    EXPECT_EQ(input.dispatch->seals, 1u);
    EXPECT_EQ(input.dispatch->cancels, 1u);
}

TEST(AF2Module, StopRetainsAcknowledgedSchedulerSealWhenRadioAuditFails)
{
    ModuleInputs input;
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    input.dispatch->failNextOwnerAudit = true;
    using Drain = SimModule::DrainResult;

    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    EXPECT_EQ(input.dispatch->seals, 1u);
    EXPECT_EQ(input.dispatch->cancels, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
    EXPECT_EQ(input.dispatch->seals, 1u);
    EXPECT_EQ(input.dispatch->cancels, 1u);
}

TEST(AF2Module, MissingPortAndBadEvidenceNeverPublishAndReleasePartialGraph)
{
    using Stage = SimModuleInitFailure::Stage;
    ModuleInputs input;
    auto ports = input.Ports();
    ports.output.reset();
    EXPECT_EQ(std::get<SimModuleInitFailure>(CreateCoreSimModule(std::move(ports), input.catalog, {})).stage,
        Stage::BindHostPorts);
    ports = input.Ports();
    ports.accountStore = std::make_shared<UnsupportedAccountStore>();
    const std::weak_ptr<AccountStore> released = ports.accountStore;
    ports.initialEvidence = [](const ResourceKey &, RuntimeEpoch epoch) {
        ++epoch[1];
        return InitialRecoveryEvidence {epoch, true, true};
    };
    EXPECT_EQ(std::get<SimModuleInitFailure>(CreateCoreSimModule(std::move(ports), input.catalog, {})).stage,
        Stage::InitializeResources);
    EXPECT_TRUE(released.expired());

    ports = input.Ports();
    ports.accountStore = std::make_shared<UnsupportedAccountStore>();
    const std::weak_ptr<AccountStore> seedFailed = ports.accountStore;
    auto accounts = input.accounts;
    accounts.back().stored.writeConfirmed = false;
    EXPECT_EQ(std::get<SimModuleInitFailure>(CreateCoreSimModule(std::move(ports), input.catalog, accounts)).stage,
        Stage::SeedAccount);
    EXPECT_TRUE(seedFailed.expired());
    EXPECT_TRUE(input.parameters->writes.empty());
}

TEST(AF2Module, DeviceBranchesAreRejectedBeforeAnyWriteOrCommand)
{
    ModuleInputs input;
    input.radio->requiresDevice = true;
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    ASSERT_TRUE(module->StartPrimarySwitch(Command(*binding)));
    EXPECT_TRUE(input.parameters->writes.empty());
    EXPECT_TRUE(input.output->events.empty());
    EXPECT_EQ(input.radio->commands, 0u);
    EXPECT_EQ(input.completion->results, std::vector<int32_t> {-1});
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    ModuleInputs nextInput;
    auto next = std::get<CoreSimModuleHandle>(CreateCoreSimModule(nextInput.Ports(), nextInput.catalog, {}));
    EXPECT_NE(next->Epoch(), module->Epoch());
    EXPECT_EQ(next->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Module, ConcurrentStopDuringCompletionSealsOldEntryAndKeepsOriginalFrame)
{
    ModuleInputs input;
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    const auto command = Command(*binding);
    std::promise<void> entered;
    auto notification = entered.get_future();
    std::promise<void> release;
    auto released = release.get_future();
    input.completion->callback = [&] {
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
        const auto receipt = module->ReadC0Receipt();
        EXPECT_TRUE(receipt && receipt->outstanding == AssignmentService::C0Stage::Completion);
        entered.set_value();
        released.wait();
    };
    auto running = std::async(std::launch::async, [&] { return module->StartPrimarySwitch(command); });
    notification.wait();
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_FALSE(module->StartPrimarySwitch(command));
    EXPECT_EQ(input.dispatch->seals, 0u);
    release.set_value();
    EXPECT_TRUE(running.get());
    EXPECT_FALSE(module->ReadC0Receipt());
    input.dispatch->defer = true;
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    ASSERT_EQ(input.dispatch->PendingOwnerTasks(), 1u);
    input.dispatch->RunOwnerTasks();
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_EQ(input.dispatch->seals, 1u);
    EXPECT_EQ(input.dispatch->cancels, 1u);
    EXPECT_EQ(input.completion->results, std::vector<int32_t> {0});
}

TEST(AF2Module, UnknownParameterEffectRetainsOriginalResponsibility)
{
    ModuleInputs input;
    input.parameters->writeResult = [](const std::string &key) {
        return key == kData ? AssignmentParameterResult {-1, false} : AssignmentParameterResult {0, true};
    };
    auto module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, input.accounts));
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    const auto request = module->StartPrimarySwitch(Command(*binding));
    ASSERT_TRUE(request);
    const auto receipt = module->ReadC0Receipt();
    ASSERT_TRUE(receipt);
    EXPECT_EQ(receipt->request, *request);
    EXPECT_EQ(receipt->outstanding, AssignmentService::C0Stage::DataParameters);
    EXPECT_TRUE(input.completion->results.empty());
    EXPECT_EQ(input.output->events, std::vector<std::string> {"primary"});
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_FALSE(module->StartPrimarySwitch(Command(*binding)));
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_FALSE(weak.expired());
    // A process-lifetime host holds the deliberately unresolved graph for the test process.
    static const auto *const retained = new CoreSimModuleHandle(weak.lock());
    EXPECT_TRUE(*retained);
}

TEST(AF2Module, SharedResourceAllowsOnlyOneCompetingOwner)
{
    const RuntimeEpoch epoch {9, 3};
    const ResourceKey shared {"shared"};
    const ResourceKey independent {"independent"};
    ResourceBarrier barrier(epoch);
    ASSERT_TRUE(barrier.RegisterForInitialCheck(shared));
    ASSERT_TRUE(barrier.RegisterForInitialCheck(independent));
    ASSERT_TRUE(barrier.AcceptInitialRecoveryEvidence(shared, {epoch, true, true}));
    ASSERT_TRUE(barrier.AcceptInitialRecoveryEvidence(independent, {epoch, true, true}));
    const StepRef first {{1, epoch, RequestOwner::Assignment}, 1, 0};
    const StepRef second {{1, epoch, RequestOwner::ShowNumber}, 1, 0};
    std::promise<void> start;
    auto begin = start.get_future().share();
    auto caller = [&](StepRef owner) {
        begin.wait();
        return barrier.Acquire({shared}, owner);
    };
    auto radio = std::async(std::launch::async, caller, first);
    auto card = std::async(std::launch::async, caller, second);
    start.set_value();
    auto radioLease = radio.get();
    auto cardLease = card.get();
    ASSERT_NE(radioLease.has_value(), cardLease.has_value());
    const auto other = barrier.Acquire({independent}, radioLease ? second : first);
    ASSERT_TRUE(other);
    EXPECT_TRUE(barrier.CancelConfirmedNotSent(*other));
    EXPECT_TRUE(barrier.CancelConfirmedNotSent(radioLease ? *radioLease : *cardLease));
}

namespace {
class MetadataProvider final : public AccountMetadataProviderPort {
public:
    struct Write {
        AccountMetadataOperation operation;
        std::string selector;
        std::string predicate;
        std::string field;
        AccountMetadataValue value;
    };

    bool independent {true};
    std::function<AccountMetadataOutcome(const Write &)> respond;
    std::optional<AccountMetadataEvidence> evidence;
    std::function<ImsPreferenceRead(const std::string &)> readIms;

    bool HasIndependentMetadataFields() const override { return independent; }
    AccountMetadataOutcome UpdateAccountField(const AccountMetadataOperation &operation, std::string_view uri,
        std::string_view selector, const std::string &predicate, std::string_view field,
        const AccountMetadataValue &value) override
    {
        EXPECT_EQ(uri, "datashare:///com.ohos.simability/sim/sim_info");
        Write write {operation, std::string(selector), predicate, std::string(field), value};
        {
            std::lock_guard<std::mutex> guard(lock_);
            writes_.push_back(write);
        }
        const auto outcome = respond ? respond(write) :
            AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
        if (field == "ims_switch" && outcome.callFinished && outcome.effect == EffectKnowledge::ConfirmedApplied) {
            SetImsValue(predicate, std::get<int32_t>(value));
        }
        return outcome;
    }
    ImsPreferenceRead QueryAccountField(std::string_view uri, std::string_view selector,
        const std::string &predicate, std::string_view field) override
    {
        EXPECT_EQ(uri, "datashare:///com.ohos.simability/sim/sim_info");
        EXPECT_EQ(selector, "icc_id");
        EXPECT_EQ(field, "ims_switch");
        {
            std::lock_guard<std::mutex> guard(lock_);
            reads_.push_back(predicate);
        }
        return readIms ? readIms(predicate) : StoredImsValue(predicate);
    }
    ImsPreferenceRead StoredImsValue(const std::string &iccid)
    {
        std::lock_guard<std::mutex> guard(lock_);
        const auto row = ims_.find(iccid);
        return row == ims_.end() ? ImsPreferenceRead {ImsPreferenceReadStatus::NoRow} :
            ImsPreferenceRead {ImsPreferenceReadStatus::Value, row->second};
    }
    void SetImsValue(const std::string &iccid, int32_t value)
    {
        std::lock_guard<std::mutex> guard(lock_);
        ims_.insert_or_assign(iccid, value);
    }
    std::vector<std::string> Reads()
    {
        std::lock_guard<std::mutex> guard(lock_);
        return reads_;
    }
    std::optional<AccountMetadataEvidence> ReadMetadataEvidence(const AccountMetadataOperation &) override
    { return evidence; }
    std::vector<Write> Writes()
    {
        std::lock_guard<std::mutex> guard(lock_);
        return writes_;
    }
private:
    std::mutex lock_;
    std::vector<Write> writes_;
    std::vector<std::string> reads_;
    std::map<std::string, int32_t> ims_;
};

struct MetadataStorePorts {
    std::shared_ptr<ControlledDiscoveryStore> discovery;
    std::shared_ptr<MetadataProvider> provider;
    AccountStoreAdapter adapter;
};

struct LiveDiscoveryFixture {
    ModuleInputs input;
    std::shared_ptr<ControlledDiscoveryStore> store = std::make_shared<ControlledDiscoveryStore>();
    CoreSimModuleHandle module;

    explicit LiveDiscoveryFixture(std::shared_ptr<LegacyPrimaryHostPort> primaryHost = {},
        std::shared_ptr<SlotCatalog> catalog = {}, std::shared_ptr<RoleProvider> provider = {},
        std::shared_ptr<MetadataProvider> metadata = {},
        std::shared_ptr<LegacyNumberHostPort> numberHost = {}, std::shared_ptr<MsisdnPlatformPort> msisdn = {},
        std::shared_ptr<EfIoPort> efIo = {}, std::shared_ptr<PinIoPort> pinIo = {},
        std::shared_ptr<LegacyPinHostPort> pinHost = {})
    {
        if (catalog) { input.catalog = std::move(catalog); }
        store->persistRows = true;
        input.accountStore = store;
        if (metadata) {
            auto owner = std::make_shared<MetadataStorePorts>(MetadataStorePorts {
                store, metadata, AccountStoreAdapter(*store, *metadata)});
            input.accountStore = std::shared_ptr<AccountStore>(owner, &owner->adapter);
        }
        auto ports = input.Ports(std::move(provider));
        ports.primaryHost = std::move(primaryHost);
        ports.numberHost = std::move(numberHost);
        ports.msisdn = std::move(msisdn);
        ports.efIo = std::move(efIo);
        ports.pinIo = std::move(pinIo);
        ports.pinHost = std::move(pinHost);
        if (ports.efIo || ports.pinIo) {
            ports.initialEvidence = [](const ResourceKey &key, RuntimeEpoch epoch) {
                EXPECT_TRUE(key.verifiedPhysicalDomain == "primary-shared" ||
                    key.verifiedPhysicalDomain == "other-card-ef" || key.verifiedPhysicalDomain == "same-card-gid1");
                return InitialRecoveryEvidence {epoch, true, true};
            };
        }
        module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(std::move(ports), input.catalog, {}));
    }
};

class LostRoleReceipt final : public AccountRolePlanningPort {
public:
    explicit LostRoleReceipt(AssignmentService &owner) : owner_(owner) {}
    unsigned finalizations {0};

    std::optional<AccountRoleFragment> PrepareAccountRoleFragment(const AccountRolePlanningInput &input) override
    { return owner_.PrepareAccountRoleFragment(input); }
    bool StillOwnsInitialRoleReservation(const AccountRoleFragment &plan) const override
    { return owner_.StillOwnsInitialRoleReservation(plan); }
    bool AcceptDiscoveredRoleProjection(const AccountRoleFragment &plan, SubscriptionId assigned) override
    { return owner_.AcceptDiscoveredRoleProjection(plan, assigned); }
    bool FinalizeInitialRoleReservation(const AccountRoleFragment &plan,
        const InitialRoleFinalizationEvidence &evidence) override
    {
        const bool applied = owner_.FinalizeInitialRoleReservation(plan, evidence);
        return ++finalizations > 1 && applied;
    }
    void ConfirmInitialRoleReceipt(const AccountRoleFragment &plan,
        const InitialRoleFinalizationEvidence &evidence) override
    { owner_.ConfirmInitialRoleReceipt(plan, evidence); }
    uint64_t ReadRoleProjectionRevision(bool historical) const override
    { return owner_.ReadRoleProjectionRevision(historical); }
    void AcceptReloadedRoleProjectionSet(const RoleReloadDelivery &delivery) override
    { owner_.AcceptReloadedRoleProjectionSet(delivery); }
private:
    AssignmentService &owner_;
};
} // namespace

TEST(AF2Discovery, ApplicationPrecedesIdentityAndStalePromotionCannotCreateBinding)
{
    LiveDiscoveryFixture f;
    const CardApplicationTarget application {{1}, "USIM"};
    const auto first = f.module->ObserveCardApplication(application, 1);
    ASSERT_TRUE(first);
    EXPECT_EQ(f.module->ObserveCardApplication(application, 1), first);
    EXPECT_FALSE(f.module->ReadAccount({41}));
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.store->writes, 0u);

    const auto replacement = f.module->ObserveCardApplication(application, 2);
    ASSERT_TRUE(replacement);
    EXPECT_FALSE(f.module->ObserveVerifiedCard(application, 1, kRaw1));
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.store->writes, 0u);

    EXPECT_EQ(f.module->ObserveVerifiedCard(application, 2, kRaw1), replacement);
    ASSERT_TRUE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->FindBinding({1})->subscription, SubscriptionId {41});
    EXPECT_EQ(f.store->writes, 1u);
    EXPECT_FALSE(f.module->ObserveVerifiedCard(application, 2, kRaw1));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_FALSE(f.module->ObserveCardApplication(application, 3));
}

namespace {
constexpr auto kIccidMetadata = "0000000A0000040000000000000000";
constexpr auto kFirstRawIccid = "980000000000000000F1";
constexpr auto kGid1Metadata = "000000020000040000000000000000";

class EfDevice final : public EfIoPort {
public:
    explicit EfDevice(bool withGid1 = false) : withGid1_(withGid1) {}

    std::vector<ResourceKey> InitialResources() const override
    {
        std::vector<ResourceKey> keys {{"primary-shared"}, {"other-card-ef"}};
        if (withGid1_) { keys.push_back({"same-card-gid1"}); }
        return keys;
    }

    std::optional<EfIoRoute> ResolveEfRoute(const SessionRef &session, EfField field) override
    {
        if (field == EfField::Gid1) {
            if (!withGid1_) { return std::nullopt; }
            const char *path = session.application.applicationKey == "SIM" ? "3F007F20" : "3F007FFF";
            return EfIoRoute {{{"same-card-gid1"}}, path, routeRevision};
        }
        const char *domain = session.application.slot == SlotId {0} ? "other-card-ef" : "primary-shared";
        return EfIoRoute {{{domain}}, "3F00", routeRevision};
    }

    bool SubmitEfIo(const CardCommandTicket &ticket, const EfIoRoute &route, const EfIoCommand &command) override
    {
        const auto current = ResolveEfRoute(ticket.session, static_cast<EfField>(command.fileId));
        if (!current || current->revision != route.revision || current->path != route.path ||
            command.path != route.path ||
            !std::equal(current->resources.begin(), current->resources.end(), route.resources.begin(),
                route.resources.end(), [](const auto &left, const auto &right) {
                    return left.verifiedPhysicalDomain == right.verifiedPhysicalDomain;
                })) {
            return false;
        }
        commands.emplace_back(ticket, command);
        if (replyInline) {
            auto reply = Reply(commands.size() - 1,
                command.code == EfIoCommand::Code::GetResponse ? kIccidMetadata : kFirstRawIccid);
            replyInline(reply);
        }
        return handoffAccepted;
    }

    bool handoffAccepted {true};
    uint64_t routeRevision {1};
    CardCommandEvidence Reply(size_t index, const std::string &hex) const
    {
        return {commands.at(index).first, EffectKnowledge::ConfirmedApplied, true, true, true, false, hex};
    }

    std::vector<std::pair<CardCommandTicket, EfIoCommand>> commands;
    std::function<void(const CardCommandEvidence &)> replyInline;
private:
    bool withGid1_;
};

struct IccidFixture {
    std::shared_ptr<EfDevice> io = std::make_shared<EfDevice>();
    LiveDiscoveryFixture live {{}, {}, {}, {}, {}, {}, io};
};
} // namespace

TEST(AF2EfRecords, RefreshIccidLeavesSameCardGid1AndNeighbourInFlight)
{
    const auto io = std::make_shared<EfDevice>(true);
    LiveDiscoveryFixture fixture {{}, {}, {}, {}, {}, {}, io};
    const auto module = fixture.module;
    const auto current = module->ObserveCardApplication({{1}, "USIM"}, 1);
    const auto neighbour = module->ObserveCardApplication({{0}, "SIM"}, 1);
    ASSERT_TRUE(current && neighbour);
    fixture.store->produceStore = [](const auto &plan) {
        return Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
    };
    ASSERT_TRUE(module->LoadIccid(*current));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->LoadGid1(*current));
    ASSERT_TRUE(module->AcceptGid1IoEvidence(io->Reply(2, kGid1Metadata)));
    ASSERT_TRUE(module->LoadIccid(*neighbour));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(io->Reply(4, kIccidMetadata)));
    ASSERT_EQ(io->commands.size(), 6u);
    EXPECT_EQ(io->commands[1].first.session, *current);
    EXPECT_EQ(io->commands[3].first.session, *current);
    EXPECT_EQ(io->commands[5].first.session, *neighbour);
    EXPECT_EQ(io->commands[3].second.code, EfIoCommand::Code::ReadBinary);
    EXPECT_EQ(io->commands[3].second.fileId, 0x6f3e);
    EXPECT_EQ(io->commands[3].second.path, "3F007FFF");
    EXPECT_EQ(io->commands[3].second.size, 2u);
    EXPECT_EQ(fixture.store->writes, 0u);

    ASSERT_TRUE(module->RefreshIccid(*current));
    const auto gid = io->Reply(3, "A0fe");
    EXPECT_FALSE(module->AcceptIccidIoEvidence(gid));
    EXPECT_TRUE(module->AcceptGid1IoEvidence(gid));
    EXPECT_EQ(module->ReadGid1Records(*current), "A0fe");
    EXPECT_FALSE(module->ReadGid1Records(*neighbour));
    EXPECT_EQ(fixture.store->writes, 0u);

    EXPECT_TRUE(module->AcceptIccidIoEvidence(io->Reply(5, "980000000000000000F2")));
    ASSERT_TRUE(module->ReadIccidRecords(*neighbour));
    EXPECT_EQ(module->ReadIccidRecords(*neighbour)->decimal, "8900000000000000002");
    EXPECT_TRUE(module->FindBinding({0}));
    const auto obsolete = io->Reply(1, kFirstRawIccid);
    EXPECT_TRUE(module->AcceptIccidIoEvidence(obsolete));
    EXPECT_FALSE(module->AcceptIccidIoEvidence(obsolete));
    EXPECT_FALSE(module->ReadIccidRecords(*current));
    EXPECT_FALSE(module->FindBinding({1}));

    ASSERT_TRUE(module->LoadIccid(*current));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(io->Reply(6, kIccidMetadata)));
    const auto malformed = io->Reply(7, "ZZ");
    EXPECT_TRUE(module->AcceptIccidIoEvidence(malformed));
    EXPECT_FALSE(module->AcceptIccidIoEvidence(malformed));
    EXPECT_FALSE(module->ReadIccidRecords(*current));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(module->ReadGid1Records(*current), "A0fe");

    ASSERT_TRUE(module->LoadIccid(*current));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(io->Reply(8, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(io->Reply(9, kFirstRawIccid)));
    EXPECT_EQ(fixture.store->writes, 2u);
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(module->ReadGid1Records(*current), "A0fe");
    EXPECT_TRUE(module->RefreshGid1(*current));
    EXPECT_FALSE(module->ReadGid1Records(*current));
    EXPECT_TRUE(module->ReadIccidRecords(*current));
    EXPECT_TRUE(module->ReadIccidRecords(*neighbour));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2EfRecords, OriginalRouteRevisionGatesReadBinaryAndGid1FailureClearsItsField)
{
    const auto io = std::make_shared<EfDevice>(true);
    LiveDiscoveryFixture fixture {{}, {}, {}, {}, {}, {}, io};
    const auto module = fixture.module;
    const auto session = module->ObserveCardApplication({{1}, "SIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_TRUE(module->LoadGid1(*session));
    ASSERT_EQ(io->commands.size(), 1u);
    EXPECT_EQ(io->commands.front().second.path, "3F007F20");
    ++io->routeRevision;
    EXPECT_TRUE(module->AcceptGid1IoEvidence(io->Reply(0, kGid1Metadata)));
    EXPECT_EQ(io->commands.size(), 1u);
    EXPECT_FALSE(module->ReadGid1Records(*session));

    ASSERT_TRUE(module->LoadGid1(*session));
    ASSERT_TRUE(module->AcceptGid1IoEvidence(io->Reply(1, kGid1Metadata)));
    ASSERT_TRUE(module->AcceptGid1IoEvidence(io->Reply(2, "ABc0")));
    EXPECT_EQ(module->ReadGid1Records(*session), "ABc0");
    ASSERT_TRUE(module->LoadGid1(*session));
    ASSERT_TRUE(module->AcceptGid1IoEvidence(io->Reply(3, kGid1Metadata)));
    auto failure = io->Reply(4, "");
    failure.effect = EffectKnowledge::ConfirmedNotApplied;
    failure.legacyWaitSucceeded = false;
    EXPECT_TRUE(module->AcceptGid1IoEvidence(failure));
    EXPECT_FALSE(module->AcceptGid1IoEvidence(failure));
    EXPECT_FALSE(module->ReadGid1Records(*session));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(fixture.store->writes, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, PreIdentityApplicationReadsRawEfAndDiscoversOneAccount)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_FALSE(module->FindBinding({1}));
    const auto request = module->LoadIccid(*session);
    ASSERT_TRUE(request);
    ASSERT_EQ(fixture.io->commands.size(), 1u);
    const auto [getTicket, getCommand] = fixture.io->commands.front();
    EXPECT_EQ(getTicket.step.request, *request);
    EXPECT_EQ(getTicket.session, *session);
    EXPECT_EQ(getCommand.code, EfIoCommand::Code::GetResponse);
    EXPECT_EQ(getCommand.fileId, 0x2fe2);
    EXPECT_EQ(getCommand.path, "3F00");
    EXPECT_EQ(getCommand.size, 15);

    const auto metadata = fixture.io->Reply(0, kIccidMetadata);
    ASSERT_TRUE(module->AcceptIccidIoEvidence(metadata));
    ASSERT_EQ(fixture.io->commands.size(), 2u);
    const auto [readTicket, readCommand] = fixture.io->commands.back();
    EXPECT_EQ(readTicket.step.request, *request);
    EXPECT_NE(readTicket.fence, getTicket.fence);
    EXPECT_EQ(readCommand.code, EfIoCommand::Code::ReadBinary);
    EXPECT_EQ(readCommand.size, 10);
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    EXPECT_FALSE(module->AcceptIccidIoEvidence(metadata));

    const auto reply = fixture.io->Reply(1, kFirstRawIccid);
    ASSERT_TRUE(module->AcceptIccidIoEvidence(reply));
    ASSERT_FALSE(module->AcceptIccidIoEvidence(reply));
    const auto record = module->ReadIccidRecords(*session);
    ASSERT_TRUE(record);
    EXPECT_EQ(record->full, "8900000000000000001F");
    EXPECT_EQ(record->decimal, "8900000000000000001");
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    EXPECT_EQ(binding->subscription, SubscriptionId {41});
    EXPECT_EQ(fixture.live.store->rows.count(record->full), 1u);
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, InlineRepliesRetainEachOriginalIoStep)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "SIM"}, 1);
    ASSERT_TRUE(session);
    fixture.io->replyInline = [&](const CardCommandEvidence &reply) {
        EXPECT_TRUE(module->AcceptIccidIoEvidence(reply));
    };
    const auto request = module->LoadIccid(*session);
    ASSERT_TRUE(request);
    ASSERT_EQ(fixture.io->commands.size(), 2u);
    EXPECT_EQ(fixture.io->commands[0].first.step.request, *request);
    EXPECT_EQ(fixture.io->commands[1].first.step.request, *request);
    EXPECT_TRUE(module->ReadIccidRecords(*session));
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, FailedAndMalformedRepliesCannotCreateBinding)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);

    ASSERT_TRUE(module->LoadIccid(*session));
    auto failed = fixture.io->Reply(0, kIccidMetadata);
    failed.effect = EffectKnowledge::ConfirmedNotApplied;
    failed.legacyWaitSucceeded = false;
    EXPECT_TRUE(module->AcceptIccidIoEvidence(failed));
    EXPECT_FALSE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_EQ(fixture.io->commands.size(), 1u);

    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kIccidMetadata)));
    EXPECT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, "980")));
    EXPECT_FALSE(module->ReadIccidRecords(*session));

    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, kIccidMetadata)));
    EXPECT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(4, std::string(20, 'F'))));
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, RefreshRejectsOldReadAndLeavesNeighbourRunning)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto old = module->ObserveCardApplication({{1}, "USIM"}, 1);
    const auto peer = module->ObserveCardApplication({{0}, "SIM"}, 1);
    ASSERT_TRUE(old && peer);
    fixture.live.store->produceStore = [](const auto &plan) {
        return Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
    };
    ASSERT_TRUE(module->LoadIccid(*old));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->LoadIccid(*peer));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, kIccidMetadata)));
    ASSERT_TRUE(module->RefreshIccid(*old));

    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    EXPECT_FALSE(module->ReadIccidRecords(*old));
    EXPECT_FALSE(module->FindBinding({1}));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, "980000000000000000F2")));
    ASSERT_TRUE(module->ReadIccidRecords(*peer));
    EXPECT_EQ(module->ReadIccidRecords(*peer)->decimal, "8900000000000000002");

    ASSERT_TRUE(module->LoadIccid(*old));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(4, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(5, kFirstRawIccid)));
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 2u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, RefreshAfterGetResponseHandoffPreventsOldReadBinary)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->RefreshIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    EXPECT_EQ(fixture.io->commands.size(), 1u);
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    EXPECT_EQ(fixture.live.store->writes, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, RefreshRevokesOriginalProviderMatchBeforeItsWrite)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    fixture.live.store->beforeMatchReturn = [&](DiscoveryCallRef) {
        EXPECT_TRUE(module->RefreshIccid(*session));
    };

    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 0u);

    fixture.live.store->beforeMatchReturn = {};
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, kFirstRawIccid)));
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, FreshReadCanDiscoverAfterPriorMatchReleasesSameIdentity)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    bool reloadedDuringMatch = false;
    fixture.live.store->beforeMatchReturn = [&](DiscoveryCallRef) {
        if (reloadedDuringMatch) { return; }
        reloadedDuringMatch = true;
        ASSERT_TRUE(module->RefreshIccid(*session));
        ASSERT_TRUE(module->LoadIccid(*session));
        ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, kIccidMetadata)));
        ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, kFirstRawIccid)));
        EXPECT_FALSE(module->FindBinding({1}));
    };

    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    EXPECT_TRUE(reloadedDuringMatch);
    EXPECT_FALSE(module->FindBinding({1}));

    fixture.live.store->beforeMatchReturn = {};
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(4, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(5, kFirstRawIccid)));
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, PaddedEfRetainsIdentityAlreadyReadFromCardState)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1);
    ASSERT_TRUE(session);
    const auto bound = module->FindBinding({1});
    ASSERT_TRUE(bound);
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    const auto records = module->ReadIccidRecords(*session);
    ASSERT_TRUE(records);
    EXPECT_EQ(records->full, "8900000000000000001F");
    EXPECT_EQ(records->decimal, kRaw1);
    EXPECT_EQ(module->FindBinding({1})->subscription, bound->subscription);
    EXPECT_EQ(module->FindBinding({1})->bindingRevision, bound->bindingRevision);
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(fixture.live.store->rows.count(kRaw1), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, ConflictingIdentityInOneApplicationCannotReplaceRecordsOrBinding)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    const auto bound = module->FindBinding({1});
    ASSERT_TRUE(bound);
    ASSERT_TRUE(module->RefreshIccid(*session));

    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, "980000000000000000F2")));
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    ASSERT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(module->FindBinding({1})->subscription, bound->subscription);
    EXPECT_EQ(module->FindBinding({1})->bindingRevision, bound->bindingRevision);
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, ContradictoryEarlyReplyKeepsOriginalDeviceResponsibility)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    fixture.io->handoffAccepted = false;
    fixture.io->replyInline = [&](const CardCommandEvidence &reply) {
        EXPECT_TRUE(module->AcceptIccidIoEvidence(reply));
    };

    ASSERT_TRUE(module->LoadIccid(*session));
    EXPECT_EQ(fixture.io->commands.size(), 1u);
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Quarantined);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    auto proof = fixture.io->Reply(0, kIccidMetadata);
    proof.currentStateVerified = true;
    EXPECT_TRUE(module->AcceptIccidIoEvidence(proof));
    EXPECT_EQ(fixture.io->commands.size(), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, ReplacedApplicationCannotPublishOldIdentity)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto original = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(original);
    ASSERT_TRUE(module->LoadIccid(*original));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    const auto replaced = module->ObserveCardApplication({{1}, "USIM"}, 2);
    ASSERT_TRUE(replaced);

    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(1, kFirstRawIccid)));
    EXPECT_FALSE(module->ReadIccidRecords(*original));
    EXPECT_FALSE(module->ReadIccidRecords(*replaced));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 0u);

    ASSERT_TRUE(module->LoadIccid(*replaced));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(2, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(3, "980000000000000000F2")));
    ASSERT_TRUE(module->ReadIccidRecords(*replaced));
    EXPECT_EQ(module->ReadIccidRecords(*replaced)->decimal, "8900000000000000002");
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, LateProofAfterUnknownReadClosesResourceWithoutPublishing)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_TRUE(module->LoadIccid(*session));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    auto early = fixture.io->Reply(1, "");
    early.effect = EffectKnowledge::Unknown;
    early.commandEnded = false;
    early.resourceReusable = false;
    early.legacyWaitSucceeded = false;
    ASSERT_TRUE(module->AcceptIccidIoEvidence(early));
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Quarantined);
    EXPECT_FALSE(module->ReadIccidRecords(*session));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);

    auto late = fixture.io->Reply(1, kFirstRawIccid);
    EXPECT_FALSE(module->AcceptIccidIoEvidence(late));
    late.currentStateVerified = true;
    EXPECT_TRUE(module->AcceptIccidIoEvidence(late));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(fixture.live.store->writes, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Iccid, StopRetainsOriginalGetResponseAndRejectsContinuation)
{
    IccidFixture fixture;
    auto &module = fixture.live.module;
    const auto session = module->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(session);
    ASSERT_TRUE(module->LoadIccid(*session));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    EXPECT_FALSE(module->LoadIccid(*session));
    EXPECT_TRUE(module->AcceptIccidIoEvidence(fixture.io->Reply(0, kIccidMetadata)));
    EXPECT_EQ(fixture.io->commands.size(), 1u);
    EXPECT_EQ(fixture.live.store->writes, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, LiveCardDiscoveryCreatesAccountAndBinding)
{
    LiveDiscoveryFixture f;
    ASSERT_FALSE(f.module->FindBinding({1}));
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    EXPECT_EQ(f.store->writes, 1u);
    EXPECT_EQ(f.store->rows.at(kRaw1).subscription.legacySimId, 41);
    EXPECT_EQ(f.store->inserts, 1u);
    ASSERT_TRUE(f.module->ReadAccount({41}));
    ASSERT_TRUE(f.module->ReadAccount({41}, AccountReadView::Historical));
    ASSERT_TRUE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->FindBinding({1})->subscription.legacySimId, 41);
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
    const auto roles = f.module->ReadConfirmedDiscoveryRoles({41});
    ASSERT_TRUE(roles);
    EXPECT_TRUE(roles->call == f.store->lastPlan->call);
    EXPECT_FALSE(roles->stored.mainCardColumn);
    EXPECT_FALSE(roles->historicalOnly);
    EXPECT_FALSE(f.module->ReadDiscoveryReceipt(f.store->lastPlan->call));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, KnownCardReinsertKeepsIdentityAndRejectsDuplicateAndOldSource)
{
    LiveDiscoveryFixture f;
    f.store->rows.emplace(kRaw1, AccountStorageRow {{77}, kRaw1, SlotId {1}, u"known", u"22", true, 3, {}});
    f.store->stored.assignedSubscription = SubscriptionId {77};
    auto first = f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1);
    ASSERT_TRUE(first);
    const auto old = f.module->FindBinding({1});
    ASSERT_TRUE(old);
    EXPECT_TRUE(f.module->ReadAccount({77})->displayName == u"known");
    EXPECT_FALSE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    EXPECT_FALSE(f.module->ObserveVerifiedCard({{1}, "ISIM"}, 1, "other-application"));
    EXPECT_TRUE(f.module->InvalidateVerifiedCard({f.module->Epoch(), {{1}, "ISIM"}, 1}));
    EXPECT_EQ(f.module->FindBinding({1})->bindingRevision, old->bindingRevision);
    EXPECT_TRUE(f.module->InvalidateVerifiedCard(*first));
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_TRUE(f.module->ReadAccount({77}, AccountReadView::Historical));
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 2, kRaw1));
    ASSERT_TRUE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->FindBinding({1})->bindingRevision, old->bindingRevision + 2);
    EXPECT_FALSE(f.module->InvalidateVerifiedCard(*first));
    EXPECT_EQ(f.module->FindBinding({1})->subscription.legacySimId, 77);
    EXPECT_EQ(f.store->writes, 2u);
    EXPECT_EQ(f.store->inserts, 0u);
    EXPECT_FALSE(f.store->lastPlan->roleFields.includeInStorage);
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, WrongProofAndPartialEvidenceKeepOriginalUntilAllColumnsAreProven)
{
    LiveDiscoveryFixture f;
    f.store->stored = {-1, {}, false, true, EffectKnowledge::Unknown};
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    const auto original = *f.store->lastPlan;
    ASSERT_TRUE(f.module->ReadDiscoveryReceipt(original.call));
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 1u);
    f.store->rows.emplace(kRaw1, AccountStorageRow {{41}, kRaw1, SlotId {1}, u"", u"", true, {}, {}});
    EXPECT_FALSE(f.module->ReconcileDiscovery(original.call)); // Same-value row does not attribute.
    auto wrong = original.call;
    ++wrong.epoch[1];
    f.store->originalEvidence[original.call.admission] = {0, SubscriptionId {41}, true, true,
        EffectKnowledge::ConfirmedApplied, DiscoveryWriteEvidence {wrong, original.Columns(), 0}};
    EXPECT_FALSE(f.module->ReconcileDiscovery(original.call));
    f.store->originalEvidence[original.call.admission] = {0, SubscriptionId {41}, false, true,
        EffectKnowledge::Unknown, DiscoveryWriteEvidence {original.call, DiscoveryIccid, 0}};
    EXPECT_FALSE(f.module->ReconcileDiscovery(original.call));
    ASSERT_TRUE(f.module->ReadDiscoveryReceipt(original.call));
    EXPECT_EQ(f.module->ReadDiscoveryReceipt(original.call)->applied, DiscoveryIccid);
    EXPECT_FALSE(f.module->ReadAccount({41}));
    EXPECT_FALSE(f.module->FindBinding({1}));
    const auto remaining = static_cast<uint16_t>(original.Columns() & ~DiscoveryIccid);
    f.store->originalEvidence[original.call.admission] = {0, SubscriptionId {41}, true, true,
        EffectKnowledge::ConfirmedApplied, DiscoveryWriteEvidence {original.call, remaining, 0}};
    EXPECT_TRUE(f.module->ReconcileDiscovery(original.call));
    ASSERT_TRUE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->FindBinding({1})->bindingRevision, 1u);
    EXPECT_TRUE(f.module->ReadAccount({41}));
    EXPECT_FALSE(f.module->ReadDiscoveryReceipt(original.call));
    EXPECT_FALSE(f.module->ReconcileDiscovery(original.call));
    EXPECT_EQ(f.store->writes, 1u);
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, SameSlotOriginalsRecoverIndependentlyInBothResultOrders)
{
    for (const bool newerFirst : {false, true}) {
        LiveDiscoveryFixture f;
        f.store->stored = {-1, {}, false, true, EffectKnowledge::Unknown};
        const auto old = f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw0);
        ASSERT_TRUE(old);
        const auto first = *f.store->lastPlan;
        ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 2, kRaw1));
        const auto second = *f.store->lastPlan;
        EXPECT_FALSE(first.call == second.call);
        EXPECT_EQ(f.module->PendingDiscoveryRoles(), 2u);
        EXPECT_FALSE(f.module->InvalidateVerifiedCard(*old));
        EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
        EXPECT_FALSE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 3, "third-card"));
        f.store->ProveApplied(first, {40});
        f.store->ProveApplied(second, {41});
        if (newerFirst) {
            EXPECT_TRUE(f.module->ReconcileDiscovery(second.call));
            ASSERT_TRUE(f.module->FindBinding({1}));
            EXPECT_EQ(f.module->FindBinding({1})->subscription.legacySimId, 41);
            EXPECT_TRUE(f.module->ReadDiscoveryReceipt(first.call));
            EXPECT_TRUE(f.module->ReconcileDiscovery(first.call));
        } else {
            EXPECT_TRUE(f.module->ReconcileDiscovery(first.call));
            EXPECT_FALSE(f.module->FindBinding({1}));
            EXPECT_TRUE(f.module->ReadDiscoveryReceipt(second.call));
            EXPECT_TRUE(f.module->ReconcileDiscovery(second.call));
        }
        ASSERT_TRUE(f.module->FindBinding({1}));
        EXPECT_EQ(f.module->FindBinding({1})->subscription.legacySimId, 41);
        EXPECT_FALSE(f.module->ReadAccount({40}));
        EXPECT_TRUE(f.module->ReadAccount({40}, AccountReadView::Historical));
        const auto oldRoles = f.module->ReadConfirmedDiscoveryRoles({40});
        ASSERT_TRUE(oldRoles);
        EXPECT_TRUE(oldRoles->historicalOnly);
        EXPECT_TRUE(oldRoles->call == first.call);
        EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
        EXPECT_EQ(f.store->writes, 2u);
        EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
        const std::weak_ptr<SimModule> remained = f.module;
        f.module.reset();
        EXPECT_TRUE(remained.expired());
    }
}

TEST(AF2Discovery, UnreturnedMatchKeepsOriginalAndNeverAuthorizesWrite)
{
    LiveDiscoveryFixture f;
    f.store->matchFinished = false;
    f.store->matchResult = -1;
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    const auto original = *f.store->lastMatchCall;
    ASSERT_TRUE(f.module->ReadDiscoveryReceipt(original));
    EXPECT_EQ(f.module->ReadDiscoveryReceipt(original)->phase, TopologyService::DiscoveryPhase::WaitingMatch);
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_FALSE(f.module->ReconcileDiscovery(original));
    f.store->returnedMatch = original;
    EXPECT_TRUE(f.module->ReconcileDiscovery(original));
    EXPECT_EQ(f.store->writes, 0u);
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, StopBeforeSendGrantSealsNewWork)
{
    LiveDiscoveryFixture f;
    f.store->beforeMatchReturn = [&](DiscoveryCallRef) {
        EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    };
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    EXPECT_EQ(f.store->writes, 0u);
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, StopWhileWriteExecutesWaitsForRealReturnAndAllowsOriginalDisposition)
{
    LiveDiscoveryFixture f;
    std::promise<void> entered, release;
    auto enteredSignal = entered.get_future();
    auto resume = release.get_future();
    f.store->beforeStoreReturn = [&](const AccountDiscoveryWritePlan &) { entered.set_value(); resume.wait(); };
    auto writing = std::async(std::launch::async, [&] { return f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1); });
    if (enteredSignal.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        FAIL() << "Store call did not enter";
    }
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_FALSE(f.module->ReconcileDiscovery(f.store->lastPlan->call));
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_FALSE(f.module->ObserveVerifiedCard({{0}, "USIM"}, 1, kRaw0));
    release.set_value();
    EXPECT_TRUE(writing.get());
    EXPECT_TRUE(f.module->FindBinding({1}));
    EXPECT_EQ(f.store->writes, 1u);
    EXPECT_EQ(f.module->PendingDiscoveryRoles(), 0u);
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Discovery, LostRoleReceiptIsReReadWithoutReplayingPublishOrWrite)
{
    DiscoveryFixture f;
    LostRoleReceipt port(f.service);
    f.topology.BindRolePlanner(port);
    ASSERT_TRUE(f.topology.DiscoverAccount(Discovery(1, 1, kRaw1)).bindingAccepted);
    const auto ticket = f.accountStore.lastPlan->call;
    ASSERT_TRUE(f.topology.ReadDiscoveryReceipt(ticket));
    const auto receipt = *f.topology.ReadDiscoveryReceipt(ticket);
    EXPECT_EQ(receipt.phase, TopologyService::DiscoveryPhase::Finalizing);
    ASSERT_TRUE(receipt.published);
    EXPECT_EQ(f.service.PendingDiscoveryRoles(), 1u);
    EXPECT_TRUE(f.topology.ReconcileDiscovery(ticket));
    EXPECT_EQ(port.finalizations, 2u);
    EXPECT_EQ(f.accountStore.writes, 1u);
    EXPECT_EQ(f.topology.FindBinding({1})->bindingRevision, receipt.published->bindingRevision);
    EXPECT_FALSE(f.topology.ReconcileDiscovery(ticket));
    EXPECT_EQ(f.service.PendingDiscoveryRoles(), 0u);
}

TEST(AF2Discovery, LiveBindingsFeedExistingC0AndStaleTargetCannotAdvance)
{
    LiveDiscoveryFixture f;
    f.store->produceStore = [](const AccountDiscoveryWritePlan &plan) {
        return Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
    };
    ASSERT_TRUE(f.module->ObserveVerifiedCard({{0}, "USIM"}, 1, kRaw0));
    auto second = f.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1);
    ASSERT_TRUE(second);
    ASSERT_TRUE(f.module->FindBinding({0}));
    const auto binding = f.module->FindBinding({1});
    ASSERT_TRUE(binding);
    ASSERT_TRUE(f.module->StartPrimarySwitch(Command(*binding)));
    EXPECT_EQ(f.module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(f.module->ReadAssignment().lastData, SlotId {1});
    EXPECT_TRUE(f.module->ReadAccount({40}));
    EXPECT_TRUE(f.module->ReadAccount({41}));
    EXPECT_EQ(f.input.radio->commands, 0u);
    const auto before = f.input.parameters->writes.size();
    f.input.parameters->writeResult = [&](const std::string &key) {
        if (key == kPreference) { EXPECT_TRUE(f.module->InvalidateVerifiedCard(*second)); }
        return AssignmentParameterResult {0, true};
    };
    ASSERT_TRUE(f.module->StartPrimarySwitch(Command(*binding)));
    EXPECT_EQ(f.input.parameters->writes.size(), before + 1); // Only the original admitted preference.
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.input.completion->results, (std::vector<int32_t> {0, -1}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

namespace {
class PrimaryHost final : public LegacyPrimaryHostPort {
public:
    bool system {true};
    bool permitted {true};
    bool slotSupported {true};
    bool readable {true};
    unsigned accountsChanged {0};
    std::vector<std::string> calls;
    std::function<void()> onAccountChanged;

    bool IsSystemCaller(LegacyCallerRef) override { calls.emplace_back("system"); return system; }
    bool CanSetTelephonyState(LegacyCallerRef) override { calls.emplace_back("permission"); return permitted; }
    bool CanGetTelephonyState(LegacyCallerRef) override { calls.emplace_back("read-permission"); return readable; }
    bool SupportsPrimarySlot(SlotId) override { calls.emplace_back("slot"); return slotSupported; }
    void NotifyOriginalAccountChanged() override
    {
        ++accountsChanged;
        if (onAccountChanged) { onAccountChanged(); }
    }
    int32_t PrimaryError(LegacyPrimaryError error) const override
    {
        using namespace OHOS::Telephony;
        switch (error) {
            case LegacyPrimaryError::Unavailable: return TELEPHONY_ERR_LOCAL_PTR_NULL;
            case LegacyPrimaryError::NonSystemCaller: return TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API;
            case LegacyPrimaryError::PermissionDenied: return TELEPHONY_ERR_PERMISSION_ERR;
            case LegacyPrimaryError::InvalidSlot: return TELEPHONY_ERR_SLOTID_INVALID;
            case LegacyPrimaryError::NoCard: return TELEPHONY_ERR_NO_SIM_CARD;
            case LegacyPrimaryError::Failed: return TELEPHONY_ERR_FAIL;
            case LegacyPrimaryError::InactiveCard: return CORE_SERVICE_SIM_CARD_IS_NOT_ACTIVE;
            case LegacyPrimaryError::DatabaseWriteFailed: return TELEPHONY_ERR_DATABASE_WRITE_FAIL;
            case LegacyPrimaryError::InvalidArgument: return TELEPHONY_ERR_ARGUMENT_INVALID;
            case LegacyPrimaryError::HistoricalMissing: return TELEPHONY_ERR_ARRAY_OUT_OF_BOUNDS;
            case LegacyPrimaryError::BroadcastFailed: return TELEPHONY_ERR_PUBLISH_BROADCAST_FAIL;
        }
        return TELEPHONY_ERR_FAIL;
    }
};

class PrimaryWait final : public LegacyPrimaryWaitPort {
public:
    uint64_t expectedBudget {LEGACY_DEDICATED_PRIMARY_WAIT_MS};
    bool AwaitPrimaryResult(SynchronousWaitRef waiter, std::optional<uint64_t> budget,
        const std::function<bool()> &ready) override
    {
        std::unique_lock<std::mutex> lock(lock_);
        if (budget) {
            EXPECT_EQ(*budget, expectedBudget);
            awaiting_ = waiter;
            changed_.notify_all();
        }
        const bool signalled = changed_.wait_for(lock, std::chrono::seconds(5),
            [&] { return ready() || (budget && expired_); });
        EXPECT_TRUE(signalled) << "Host wait received neither the original completion nor the controlled deadline";
        return ready();
    }
    void NotifyPrimaryResult(SynchronousWaitRef waiter) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (awaiting_) { EXPECT_EQ(waiter.value, awaiting_->value); }
        changed_.notify_all();
    }
    bool WaitUntilCallerIsWaiting()
    {
        std::unique_lock<std::mutex> lock(lock_);
        return changed_.wait_for(lock, std::chrono::seconds(3), [&] { return awaiting_.has_value(); });
    }
    void Expire()
    {
        std::lock_guard<std::mutex> lock(lock_);
        expired_ = true;
        changed_.notify_all();
    }
private:
    std::mutex lock_;
    std::condition_variable changed_;
    std::optional<SynchronousWaitRef> awaiting_;
    bool expired_ {false};
};

struct DedicatedPrimaryFixture {
    ModuleInputs input;
    std::shared_ptr<PrimaryHost> host = std::make_shared<PrimaryHost>();
    std::shared_ptr<PrimaryWait> wait = std::make_shared<PrimaryWait>();
    std::shared_ptr<RoleProvider> roles = std::make_shared<RoleProvider>();
    CoreSimModuleHandle module;

    explicit DedicatedPrimaryFixture(bool threePhase = false, const std::shared_ptr<EfDevice> &iccid = {})
    {
        input.radio->requiresDevice = !threePhase;
        input.radio->routing = PrimaryProtocolRouting {200,
            {{100, {0}, 7, RadioProtocolCapability {0, 0x11}},
             {200, {1}, 7, RadioProtocolCapability {1, 0x22}}}};
        if (threePhase) {
            input.radio->primaryEndpoint = {0};
            wait->expectedBudget = 60000;
            input.catalog = std::make_shared<SlotCatalog>(std::vector<SlotDescriptor> {
                {{0}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
                {{1}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false},
                {{2}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
                {{3}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false}},
                LegacyCapacityMeasures {2, 2, 2, 2, 2});
        }
        auto ports = input.Ports(roles);
        ports.primaryHost = host;
        ports.primaryWait = wait;
        if (iccid) {
            ports.efIo = iccid;
            ports.initialEvidence = [](const ResourceKey &key, RuntimeEpoch epoch) -> std::optional<InitialRecoveryEvidence> {
                if (key.verifiedPhysicalDomain != "primary-shared" && key.verifiedPhysicalDomain != "other-card-ef") {
                    return std::nullopt;
                }
                return InitialRecoveryEvidence {epoch, true, true};
            };
        }
        module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(std::move(ports), input.catalog, input.accounts));
    }
    RadioDeviceResult Reply(EffectKnowledge effect, int32_t result, bool currentStateVerified = false) const
    {
        const auto &[request, token] = input.radio->sent.value();
        return {token, {request.step, request.target, request.expectedBinding, request.protocolSession,
            request.phase, false, effect, result}, true, true, currentStateVerified, static_cast<int32_t>(token)};
    }
    RadioDeviceResult ReplyAt(size_t index, bool notify = false) const
    {
        const auto &[request, token] = input.radio->history.at(index);
        return {token, {request.step, request.target, request.expectedBinding, request.protocolSession,
            request.phase, notify, EffectKnowledge::ConfirmedApplied, 0}, true, true, false, static_cast<int32_t>(token)};
    }
    void FinishFailedCheck()
    {
        ASSERT_EQ(input.radio->history.size(), 2u);
        for (size_t index = 0; index < 2; ++index) {
            auto reply = ReplyAt(index);
            reply.result.effect = EffectKnowledge::ConfirmedNotApplied;
            reply.result.legacyResult = OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL;
            EXPECT_TRUE(module->AcceptRadioDeviceResult(reply));
        }
        ASSERT_EQ(input.radio->history.size(), 4u);
        for (size_t index = 2; index < 4; ++index) {
            const auto &request = input.radio->history[index].first;
            ASSERT_TRUE(request.payload);
            EXPECT_EQ(request.payload->status, RadioProtocolStatus::Fail);
            EXPECT_TRUE(module->AcceptRadioDeviceResult(ReplyAt(index)));
        }
    }
};

struct PrimaryFixture {
    std::shared_ptr<PrimaryHost> host = std::make_shared<PrimaryHost>();
    explicit PrimaryFixture(std::shared_ptr<SlotCatalog> catalog = {}, std::shared_ptr<RoleProvider> provider = {},
        std::shared_ptr<MetadataProvider> metadata = {})
        : live(host, std::move(catalog), std::move(provider), std::move(metadata)) {}
    LiveDiscoveryFixture live;

    void DiscoverBoth()
    {
        live.store->produceStore = [](const AccountDiscoveryWritePlan &plan) {
            return Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
        };
        ASSERT_TRUE(live.module->ObserveVerifiedCard({{0}, "USIM"}, 1, kRaw0));
        ASSERT_TRUE(live.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    }
};
} // namespace

TEST(AF2Primary, FormalThreePhaseUsesTwoNativeMembersAndOriginalWait)
{
    DedicatedPrimaryFixture fixture(true);
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 entry did not retain its compatibility waiter";
    }
    EXPECT_EQ(module->Catalog().Slots().size(), 4u);
    const auto &commands = fixture.input.radio->history;
    ASSERT_EQ(commands.size(), 2u);
    const auto original = commands.front().first.step.request;
    const auto session = commands.front().first.protocolSession;
    auto stale = fixture.ReplyAt(0);
    ++stale.result.protocolSession;
    EXPECT_FALSE(module->AcceptRadioDeviceResult(stale));
    EXPECT_EQ(commands.size(), 2u);
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 1u);

    for (const uint32_t phase : {1u, 2u, 4u}) {
        const size_t first = phase == 1 ? 0u : phase == 2 ? 2u : 4u;
        ASSERT_EQ(commands.size(), first + 2);
        for (size_t member = 0; member < 2; ++member) {
            const auto request = commands[first + member].first;
            EXPECT_EQ(request.step.request, original);
            EXPECT_EQ(request.protocolSession, session);
            EXPECT_EQ(request.mode, PrimarySwitchMode::ThreePhaseProtocol);
            EXPECT_EQ(request.phase, phase);
            ASSERT_TRUE(request.member);
            EXPECT_EQ(request.member->nativeMember, member == 0 ? 100u : 200u);
            ASSERT_TRUE(request.payload);
            EXPECT_EQ(request.payload->status, phase == 4 ? RadioProtocolStatus::Success : RadioProtocolStatus::None);
            const int32_t modem = static_cast<int32_t>(phase == 1 ? member : 1 - member);
            EXPECT_EQ(request.payload->capability, (RadioProtocolCapability {modem, modem == 0 ? 0x11 : 0x22}));
            EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.ReplyAt(first + member, phase == 2)));
        }
    }
    EXPECT_EQ(caller.get(), 0);
    EXPECT_EQ(commands.size(), 6u);
    EXPECT_EQ(fixture.input.dispatch->Audit().cancelled, 1u);
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 0u);
    EXPECT_EQ(fixture.host->accountsChanged, 1u);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(module->ReadAssignment().lastData, SlotId {1});
    EXPECT_FALSE(module->ReadC0Receipt());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalC1RetainsSharedCardResourceAcrossOwnerStageGaps)
{
    const auto iccid = std::make_shared<EfDevice>();
    DedicatedPrimaryFixture fixture(true, iccid);
    const auto module = fixture.module;
    const auto shared = module->ObserveCardApplication({{1}, "USIM"}, 1);
    const auto independent = module->ObserveCardApplication({{0}, "USIM"}, 1);
    ASSERT_TRUE(shared && independent);
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 entry did not retain its compatibility waiter";
    }
    fixture.input.dispatch->defer = true;
    auto releaseEf = [&](const SessionRef &session, size_t index) {
        EXPECT_TRUE(module->RefreshIccid(session));
        EXPECT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(index, kIccidMetadata)));
    };
    for (const uint32_t phase : {1u, 2u, 4u}) {
        SCOPED_TRACE(phase);
        const size_t first = phase == 1 ? 0u : phase == 2 ? 2u : 4u;
        ASSERT_EQ(fixture.input.radio->history.size(), first + 2);
        EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.ReplyAt(first, phase == 2)));
        EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.ReplyAt(first + 1, phase == 2)));
        EXPECT_EQ(fixture.input.dispatch->PendingOwnerTasks(), 2u);
        EXPECT_EQ(fixture.input.radio->history.size(), first + 2);
        EXPECT_EQ(module->ReadPrimaryResourceState(), phase == 4 ? ResourceState::Available : ResourceState::InFlight);

        const auto before = iccid->commands.size();
        EXPECT_TRUE(module->LoadIccid(*shared));
        EXPECT_EQ(iccid->commands.size(), before + (phase == 4 ? 1 : 0));
        if (iccid->commands.size() == before + 1) { releaseEf(*shared, before); }
        if (phase == 1) {
            const auto independentBefore = iccid->commands.size();
            EXPECT_TRUE(module->LoadIccid(*independent));
            ASSERT_EQ(iccid->commands.size(), independentBefore + 1);
            releaseEf(*independent, independentBefore);
        }
        fixture.input.dispatch->RunOwnerTasks();
    }
    fixture.input.dispatch->defer = false;
    EXPECT_EQ(caller.get(), 0);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalThreePhaseDeadlineAndLegacyWaitDenyAPausedOriginalSend)
{
    for (const bool protocol : {true, false}) {
        SCOPED_TRACE(protocol);
        DedicatedPrimaryFixture fixture(true);
        const auto module = fixture.module;
        const auto previous = fixture.input.parameters->values;
        std::promise<RequestId> reached;
        auto original = reached.get_future();
        std::promise<void> release;
        auto permitCanRun = release.get_future();
        fixture.input.radio->beforeSendPermit = [&](const RadioSwitchRequest &request) {
            reached.set_value(request.step.request);
            permitCanRun.wait();
        };
        auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
        if (original.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            release.set_value();
            FAIL() << "The first C1 CHECK never reached the original platform permission boundary";
        }
        const auto request = original.get();
        EXPECT_FALSE(module->AcceptPrimarySwitchTimeout(request, 45000));
        EXPECT_FALSE(module->AcceptPrimarySwitchTimeout(request, 59999));
        if (protocol) {
            fixture.input.dispatch->Advance(44999);
            EXPECT_EQ(fixture.input.dispatch->Audit().delivered, 0u);
            EXPECT_EQ(fixture.input.dispatch->Audit().pending, 1u);
            fixture.input.dispatch->Advance(1);
            EXPECT_EQ(fixture.input.dispatch->Audit().delivered, 1u);
        } else {
            EXPECT_TRUE(module->AcceptPrimarySwitchTimeout(request, 60000));
            EXPECT_EQ(fixture.input.dispatch->Audit().cancelled, 1u);
        }
        EXPECT_FALSE(module->AcceptPrimarySwitchTimeout(request, 60000));
        release.set_value();
        EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
        EXPECT_TRUE(fixture.input.radio->history.empty());
        EXPECT_EQ(fixture.input.parameters->values.at(kPrimary), previous.at(kPrimary));
        EXPECT_EQ(fixture.input.parameters->values.at(kIccid), previous.at(kIccid));
        EXPECT_EQ(fixture.input.parameters->values.at(kData), previous.at(kData));
        EXPECT_EQ(fixture.host->accountsChanged, 0u);
        EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    }
}

TEST(AF2Primary, FormalC1WaitFallbackKeepsOriginalCheckGroupUntilRecoveryAfterStop)
{
    DedicatedPrimaryFixture fixture(true);
    const auto module = fixture.module;
    const auto previous = fixture.input.parameters->values;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 caller did not enter its original wait";
    }
    ASSERT_EQ(fixture.input.radio->history.size(), 2u);
    fixture.wait->Expire();
    EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(fixture.input.dispatch->Audit().cancelled, 1u);
    EXPECT_EQ(fixture.input.dispatch->Audit().delivered, 0u);
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Quarantined);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    for (size_t index = 0; index < 2; ++index) {
        auto reply = fixture.ReplyAt(index);
        reply.result.effect = EffectKnowledge::ConfirmedNotApplied;
        reply.result.legacyResult = OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL;
        EXPECT_FALSE(module->AcceptRadioDeviceResult(reply));
        reply.currentStateVerified = true;
        EXPECT_TRUE(module->AcceptRadioDeviceResult(reply));
        EXPECT_FALSE(module->AcceptRadioDeviceResult(reply));
        if (index == 0) {
            EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
            EXPECT_EQ(fixture.input.parameters->values.at(kPrimary), "1");
        }
    }
    EXPECT_EQ(fixture.input.parameters->values.at(kPrimary), previous.at(kPrimary));
    EXPECT_EQ(fixture.input.parameters->values.at(kIccid), previous.at(kIccid));
    EXPECT_EQ(fixture.input.radio->history.size(), 2u);
    EXPECT_EQ(fixture.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalC1BackgroundSuccessNotifiesOnceAfterOriginalCallerFails)
{
    DedicatedPrimaryFixture fixture(true);
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 caller did not enter its original wait";
    }
    ASSERT_NO_FATAL_FAILURE(fixture.FinishFailedCheck());
    EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(fixture.host->accountsChanged, 0u);
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 1u);
    fixture.input.radio->requiresDevice = true;
    fixture.input.dispatch->Advance(999);
    const auto &commands = fixture.input.radio->history;
    ASSERT_EQ(commands.size(), 4u);
    const auto initial = commands.front().first;
    fixture.input.dispatch->Advance(1);
    for (const uint32_t phase : {1u, 2u, 4u}) {
        const size_t first = phase == 1 ? 4u : phase == 2 ? 6u : 8u;
        ASSERT_EQ(commands.size(), first + 2);
        for (size_t member = 0; member < 2; ++member) {
            const auto request = commands[first + member].first;
            EXPECT_EQ(request.mode, PrimarySwitchMode::ThreePhaseProtocol);
            EXPECT_EQ(request.phase, phase);
            EXPECT_EQ(request.step.request, initial.step.request);
            EXPECT_EQ(request.step.attempt, 1u);
            EXPECT_NE(request.protocolSession, initial.protocolSession);
            EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.ReplyAt(first + member, phase == 2)));
        }
    }
    EXPECT_EQ(fixture.host->accountsChanged, 1u);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(module->ReadAssignment().lastData, SlotId {1});
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalC1RejectedRetryLeavesConfirmedOriginalRoundDrained)
{
    DedicatedPrimaryFixture fixture(true);
    fixture.input.dispatch->rejectRetryRegistration = true;
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 caller did not enter its original wait";
    }
    ASSERT_NO_FATAL_FAILURE(fixture.FinishFailedCheck());
    EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 0u);
    EXPECT_EQ(fixture.input.dispatch->Audit().scheduled, 1u);
    fixture.input.dispatch->Advance(60000);
    EXPECT_EQ(fixture.input.radio->history.size(), 4u);
    EXPECT_EQ(fixture.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalC1ProtocolDeadlineCannotConsumeTheOriginalStoreRestore)
{
    DedicatedPrimaryFixture fixture(true);
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal C1 caller did not enter its original wait";
    }
    ASSERT_NO_FATAL_FAILURE(fixture.FinishFailedCheck());
    EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    fixture.input.dispatch->Advance(1000);
    ASSERT_EQ(fixture.input.radio->history.size(), 6u);
    for (size_t index = 4; index < 6; ++index) {
        auto reply = fixture.ReplyAt(index);
        reply.result.effect = EffectKnowledge::ConfirmedNotApplied;
        reply.result.legacyResult = OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL;
        EXPECT_TRUE(module->AcceptRadioDeviceResult(reply));
    }
    ASSERT_EQ(fixture.input.radio->history.size(), 8u);
    EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.ReplyAt(6)));

    std::promise<void> entered;
    auto restoring = entered.get_future();
    std::promise<void> release;
    auto writeCanReturn = release.get_future();
    bool blocked = false;
    fixture.input.parameters->writeResult = [&](const std::string &key) {
        if (key == kPrimary && !blocked) {
            blocked = true;
            entered.set_value();
            writeCanReturn.wait();
        }
        return AssignmentParameterResult {0, true};
    };
    const auto end = fixture.ReplyAt(7);
    auto response = std::async(std::launch::async, [&] { return module->AcceptRadioDeviceResult(end); });
    if (restoring.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        FAIL() << "The final C1 response never reached the original Store restoration";
    }
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 0u);
    fixture.input.dispatch->Advance(45000);
    EXPECT_EQ(fixture.input.dispatch->Audit().delivered, 1u);
    release.set_value();
    EXPECT_TRUE(response.get());
    EXPECT_EQ(fixture.input.radio->history.size(), 8u);
    EXPECT_EQ(fixture.input.parameters->values.at(kPrimary), "0");
    EXPECT_EQ(fixture.input.dispatch->Audit().pending, 1u);
    EXPECT_EQ(fixture.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, DedicatedSwitchAwaitsOriginalCompletionThroughFormalModule)
{
    DedicatedPrimaryFixture fixture;
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal compatible Primary entry never awaited its original device result";
    }
    ASSERT_TRUE(fixture.input.radio->sent);
    const auto &request = fixture.input.radio->sent->first;
    EXPECT_EQ(request.mode, PrimarySwitchMode::WithoutModemRestart);
    EXPECT_EQ(request.step.request.epoch, module->Epoch());
    ASSERT_TRUE(request.member);
    EXPECT_EQ(request.member->nativeMember, 200u);
    EXPECT_EQ(request.target, SlotId {1});
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::InFlight);
    EXPECT_FALSE(module->ReadAssignment().lastPrimary);
    EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_FAIL);
    EXPECT_EQ(module->SetDefaultVoiceSlotId(0, {7}), 0);
    EXPECT_EQ(module->ReadAssignment().defaultVoice, SubscriptionId {40});
    const auto reply = fixture.Reply(EffectKnowledge::ConfirmedApplied, 0);
    EXPECT_TRUE(module->AcceptRadioDeviceResult(reply));
    EXPECT_EQ(caller.get(), 0);

    EXPECT_EQ(fixture.host->accountsChanged, 2u);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(module->ReadAssignment().lastData, SlotId {1});
    EXPECT_EQ(fixture.input.parameters->writes, (std::vector<std::pair<std::string, std::string>> {
        {kPreference, "true"}, {kPrimary, "1"}, {kIccid, kEncoded1}, {kData, "1"}}));
    EXPECT_FALSE(module->AcceptRadioDeviceResult(reply));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalDedicatedSwitchRetainsStoreReceiptDiscoveredAfterStartReturns)
{
    DedicatedPrimaryFixture fixture;
    const auto module = fixture.module;
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    fixture.input.parameters->writeResult = [](const std::string &key) {
        return key == kIccid ? AssignmentParameterResult {-1, false} : AssignmentParameterResult {0, true};
    };
    const auto original = module->StartPrimarySwitch(Command(*binding));
    ASSERT_TRUE(original);
    EXPECT_FALSE(module->ReadC0Receipt());
    ASSERT_TRUE(fixture.input.radio->sent);
    EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.Reply(EffectKnowledge::ConfirmedApplied, 0)));
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);

    const auto receipt = module->ReadC0Receipt();
    ASSERT_TRUE(receipt);
    EXPECT_EQ(receipt->request, *original);
    EXPECT_EQ(receipt->outstanding, AssignmentService::C0Stage::PrimaryParameters);
    const auto write = receipt->writes[static_cast<size_t>(C0ParameterKey::PrimaryIccid)];
    ASSERT_TRUE(write);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_FALSE(module->StartPrimarySwitch(Command(*binding)));
    EXPECT_FALSE(module->ReconcilePrimaryParameter(write->operation));

    fixture.input.parameters->values[kIccid] = kEncoded1;
    fixture.input.parameters->evidence = C0ParameterEvidence {write->operation,
        {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(module->ReconcilePrimaryParameter(write->operation));
    EXPECT_FALSE(module->ReadC0Receipt());
    EXPECT_TRUE(fixture.input.output->events.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalDedicatedSwitchHandlesReplyBeforeSubmitReturns)
{
    DedicatedPrimaryFixture fixture;
    const auto module = fixture.module;
    fixture.input.radio->beforeSubmitReturn = [&] {
        EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.Reply(EffectKnowledge::ConfirmedApplied, 0)));
        EXPECT_EQ(fixture.input.parameters->writes.size(), 1u);
        EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::InFlight);
    };

    EXPECT_EQ(module->SetPrimarySlotId(1, {7}), 0);
    EXPECT_EQ(fixture.host->accountsChanged, 1u);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, FormalDedicatedTimeoutClosesOnlyAfterOriginalDeviceRecovery)
{
    DedicatedPrimaryFixture fixture;
    fixture.input.radio->disposition = RadioSubmitDisposition::HandoffUnknown;
    const auto module = fixture.module;
    auto caller = std::async(std::launch::async, [&] { return module->SetPrimarySlotId(1, {7}); });
    if (!fixture.wait->WaitUntilCallerIsWaiting()) {
        fixture.wait->Expire();
        FAIL() << "The formal Primary entry never awaited the still unclassified device effect";
    }
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Quarantined);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    fixture.wait->Expire();
    EXPECT_EQ(caller.get(), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);

    ASSERT_TRUE(fixture.input.radio->sent);
    const auto status = OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL;
    EXPECT_FALSE(module->AcceptRadioDeviceResult(fixture.Reply(EffectKnowledge::ConfirmedNotApplied, status)));
    EXPECT_TRUE(module->AcceptRadioDeviceResult(fixture.Reply(EffectKnowledge::ConfirmedNotApplied, status, true)));
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_EQ(fixture.host->accountsChanged, 0u);
    EXPECT_EQ(fixture.input.parameters->writes.size(), 1u);
}

namespace {
class PinHost final : public LegacyPinHostPort {
public:
    struct Result {
        uint64_t callback;
        int32_t status;
        std::optional<int32_t> unlockResult;
        int32_t remaining;
    };
    std::optional<CompletionId> RetainPinCallback(LegacyCallbackRef callback) override
    {
        return callback.token == 0 ? std::nullopt : std::optional<CompletionId> {{callback.token}};
    }
    void ReleasePinCallback(CompletionId) override { ++released; }
    bool PostPinTask(std::function<void()> task) override
    {
        if (!acceptTasks) { return false; }
        tasks.push_back(std::move(task));
        return true;
    }
    int32_t PinError(LegacyPinError error) const override
    {
        using namespace OHOS::Telephony;
        switch (error) {
            case LegacyPinError::Unavailable: return TELEPHONY_ERR_LOCAL_PTR_NULL;
            case LegacyPinError::RilFailure: return TELEPHONY_ERR_RIL_CMD_FAIL;
            case LegacyPinError::WaitExpired: return CORE_ERR_SIM_CARD_UPDATE_FAILED;
        }
        return TELEPHONY_ERR_FAIL;
    }
    void TransferOriginalPinResult(CompletionId callback, int32_t status,
        std::optional<int32_t> unlockResult, int32_t remaining) override
    { results.push_back({callback.value, status, unlockResult, remaining}); }
    void NotifyPinStateChanged(const SessionRef &session) override { changed.push_back(session); }
    void RunNext()
    {
        ASSERT_FALSE(tasks.empty());
        auto task = std::move(tasks.front());
        tasks.erase(tasks.begin());
        task();
    }

    bool acceptTasks {true};
    size_t released {0};
    std::vector<std::function<void()>> tasks;
    std::vector<Result> results;
    std::vector<SessionRef> changed;
};

class PinDevice final : public PinIoPort {
public:
    std::vector<ResourceKey> InitialResources() const override
    { return {{"primary-shared"}, {"other-card-ef"}}; }
    std::optional<PinIoRoute> ResolvePinRoute(SlotId slot) override
    {
        return PinIoRoute {{slot, "USIM"}, {{slot == SlotId {1} ? "primary-shared" : "other-card-ef"}}};
    }
    bool SubmitUnlockPin(const CardCommandTicket &ticket, const std::u16string &pin) override
    {
        commands.emplace_back(ticket, pin);
        if (onSubmit) { onSubmit(Reply(commands.size() - 1, PinReplyStatus::Unlocked, 3)); }
        return true;
    }
    CardCommandEvidence Reply(size_t index, PinReplyStatus status, int32_t remaining) const
    {
        auto effect = status == PinReplyStatus::Unlocked ? EffectKnowledge::ConfirmedApplied :
            status == PinReplyStatus::TimedOut ? EffectKnowledge::Unknown : EffectKnowledge::ConfirmedNotApplied;
        const bool ended = status != PinReplyStatus::TimedOut;
        return {commands.at(index).first, effect, ended, ended, ended, false, PinReply {status, remaining}};
    }
    std::vector<std::pair<CardCommandTicket, std::u16string>> commands;
    std::function<void(const CardCommandEvidence &)> onSubmit;
};

struct PinFixture {
    std::shared_ptr<PrimaryHost> permission = std::make_shared<PrimaryHost>();
    std::shared_ptr<PinHost> host = std::make_shared<PinHost>();
    std::shared_ptr<PinDevice> io = std::make_shared<PinDevice>();
    LiveDiscoveryFixture live;

    explicit PinFixture(std::shared_ptr<EfIoPort> iccid = {})
        : live(permission, {}, {}, {}, {}, {}, std::move(iccid), io, host) {}

    std::optional<SessionRef> ObserveLocked(SlotId slot, uint64_t generation = 1)
    {
        auto session = live.module->ObserveCardApplication({slot, "USIM"}, generation);
        EXPECT_TRUE(session && live.module->ObservePinLocked(*session));
        return session;
    }
};
} // namespace

TEST(AF2UnlockPin, WrongThenSuccessfulUsesOriginalApiContract)
{
    using namespace SimGuard;
    PinFixture fixture;
    auto &module = fixture.live.module;
    const auto session = fixture.ObserveLocked({0});
    ASSERT_TRUE(session);
    size_t call = 0;
    ExerciseWrongThenSuccessfulPin([&](const char *pin) {
        const std::string value(pin);
        EXPECT_EQ(module->UnlockPin(0, std::u16string(value.begin(), value.end()), {7}, {40 + call}), 0);
        fixture.host->RunNext();
        const auto status = call == 0 ? PinReplyStatus::Incorrect : PinReplyStatus::Unlocked;
        EXPECT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(call, status, call == 0 ? 2 : 3)));
        const auto &result = fixture.host->results.at(call++);
        const auto legacy = !result.unlockResult ? Unlock::Failure :
            *result.unlockResult == 0 ? Unlock::Ok : Unlock::Incorrect;
        return UnlockResult {result.status, legacy, result.remaining};
    }, [&] { EXPECT_EQ(module->ReadLockState(*session), CardLockState::Locked); });
    EXPECT_EQ(module->ReadLockState(*session), CardLockState::NotReady);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2CardComposition, PinAndEfUseSharedResourceAndOriginalRequestNamespace)
{
    auto iccid = std::make_shared<EfDevice>();
    PinFixture fixture(iccid);
    auto &module = fixture.live.module;
    const auto locked = fixture.ObserveLocked({1});
    const auto peer = module->ObserveCardApplication({{0}, "USIM"}, 1);
    ASSERT_TRUE(locked && peer);
    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), 0);
    fixture.host->RunNext();
    ASSERT_EQ(fixture.io->commands.size(), 1u);
    const auto pinReply = fixture.io->Reply(0, PinReplyStatus::Unlocked, 3);
    ASSERT_TRUE(module->LoadIccid(*locked));
    EXPECT_TRUE(iccid->commands.empty());
    const auto peerRequest = module->LoadIccid(*peer);
    ASSERT_TRUE(peerRequest);
    ASSERT_EQ(iccid->commands.size(), 1u);
    EXPECT_NE(pinReply.ticket.step.request.value, peerRequest->value);
    EXPECT_FALSE(module->AcceptIccidIoEvidence(pinReply));
    EXPECT_FALSE(module->AcceptPinIoEvidence(iccid->Reply(0, kIccidMetadata)));

    EXPECT_TRUE(module->RefreshIccid(*peer));
    EXPECT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(0, kIccidMetadata)));
    EXPECT_EQ(iccid->commands.size(), 1u);
    EXPECT_TRUE(fixture.host->results.empty());
    EXPECT_TRUE(module->AcceptPinIoEvidence(pinReply));
    ASSERT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(module->ReadLockState(*locked), CardLockState::NotReady);

    ASSERT_TRUE(module->LoadIccid(*locked));
    ASSERT_EQ(iccid->commands.size(), 2u);
    ASSERT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(1, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(2, kFirstRawIccid)));
    EXPECT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2CardComposition, StopSeparatesPinDeviceAndIccidStoreEffects)
{
    auto iccid = std::make_shared<EfDevice>();
    PinFixture fixture(iccid);
    auto &module = fixture.live.module;
    const auto locked = fixture.ObserveLocked({1});
    const auto peer = module->ObserveCardApplication({{0}, "USIM"}, 1);
    ASSERT_TRUE(locked && peer);
    fixture.live.store->stored = {-1, {}, false, true, EffectKnowledge::Unknown};

    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), 0);
    fixture.host->RunNext();
    ASSERT_EQ(fixture.io->commands.size(), 1u);
    ASSERT_TRUE(module->LoadIccid(*peer));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(0, kIccidMetadata)));
    ASSERT_TRUE(module->AcceptIccidIoEvidence(iccid->Reply(1, kFirstRawIccid)));
    ASSERT_TRUE(fixture.live.store->lastPlan);
    const auto original = *fixture.live.store->lastPlan;
    EXPECT_FALSE(module->FindBinding({0}));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);

    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(0, PinReplyStatus::Unlocked, 3)));
    EXPECT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    fixture.live.store->ProveApplied(original, {41});
    EXPECT_TRUE(module->ReconcileDiscovery(original.call));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2CardComposition, OriginalPinCompletionDuringCloseInvalidatesFirstReceipt)
{
    PinFixture fixture;
    auto &module = fixture.live.module;
    const auto &dispatch = fixture.live.input.dispatch;
    ASSERT_TRUE(fixture.ObserveLocked({1}));
    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), 0);
    fixture.host->RunNext();
    ASSERT_EQ(fixture.io->commands.size(), 1u);
    const auto reply = fixture.io->Reply(0, PinReplyStatus::Unlocked, 3);
    unsigned audits = 0;
    dispatch->afterAudit = [&] {
        if (++audits == 1) {
            EXPECT_TRUE(module->AcceptPinIoEvidence(reply));
            EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
        }
    };

    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_FALSE(module->AcceptPinIoEvidence(reply));
    ASSERT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(fixture.host->results.front().callback, 31u);
    const auto auditsBeforeSecondClose = audits;
    bool duplicateReachedSecondObservation = false;
    dispatch->afterAudit = [&] {
        if (++audits == auditsBeforeSecondClose + 2) {
            duplicateReachedSecondObservation = true;
            EXPECT_FALSE(module->AcceptPinIoEvidence(reply));
        }
    };
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_TRUE(duplicateReachedSecondObservation);
    EXPECT_EQ(fixture.host->results.size(), 1u);

    const auto auditsBeforeRetry = audits;
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_GE(audits - auditsBeforeRetry, 2u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    EXPECT_EQ(dispatch->seals, 1u);
    EXPECT_EQ(dispatch->cancels, 1u);
}

TEST(AF2CardComposition, RebuildRequiresPredecessorProofAndRejectsItsOldEfEvidence)
{
    IccidFixture previous;
    auto &old = previous.live.module;
    const auto oldSession = old->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(oldSession);
    ASSERT_TRUE(old->LoadIccid(*oldSession));
    ASSERT_TRUE(old->AcceptIccidIoEvidence(previous.io->Reply(0, kIccidMetadata)));
    const auto oldReply = previous.io->Reply(1, kFirstRawIccid);
    ASSERT_EQ(old->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);

    const auto io = std::make_shared<EfDevice>();
    const auto rebuild = [&] {
        auto ports = previous.live.input.Ports();
        ports.efIo = io;
        ports.initialEvidence = [&](const ResourceKey &, RuntimeEpoch epoch) {
            const bool predecessorEnded = old->StopAcceptingAndDrain() == SimModule::DrainResult::Drained;
            return InitialRecoveryEvidence {epoch, predecessorEnded, predecessorEnded};
        };
        return CreateCoreSimModule(std::move(ports), previous.live.input.catalog, {});
    };

    const auto blocked = rebuild();
    ASSERT_TRUE(std::holds_alternative<SimModuleInitFailure>(blocked));
    EXPECT_EQ(std::get<SimModuleInitFailure>(blocked).stage, SimModuleInitFailure::Stage::InitializeResources);
    EXPECT_TRUE(io->commands.empty());
    EXPECT_EQ(old->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    ASSERT_TRUE(old->AcceptIccidIoEvidence(oldReply));
    ASSERT_EQ(old->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);

    auto created = rebuild();
    ASSERT_TRUE(std::holds_alternative<CoreSimModuleHandle>(created));
    auto current = std::get<CoreSimModuleHandle>(std::move(created));
    ASSERT_NE(current->Epoch(), old->Epoch());
    const auto currentSession = current->ObserveCardApplication({{1}, "USIM"}, 1);
    ASSERT_TRUE(currentSession);
    ASSERT_TRUE(current->LoadIccid(*currentSession));
    ASSERT_TRUE(current->AcceptIccidIoEvidence(io->Reply(0, kIccidMetadata)));
    const auto reply = io->Reply(1, "980000000000000000F2");
    EXPECT_EQ(reply.ticket.fence, oldReply.ticket.fence);
    EXPECT_EQ(reply.ticket.step.request.value, oldReply.ticket.step.request.value);
    EXPECT_FALSE(current->AcceptIccidIoEvidence(oldReply));
    EXPECT_EQ(current->ReadPrimaryResourceState(), ResourceState::InFlight);
    EXPECT_FALSE(current->ReadIccidRecords(*currentSession));
    EXPECT_FALSE(current->FindBinding({1}));

    ASSERT_TRUE(current->AcceptIccidIoEvidence(reply));
    const auto record = current->ReadIccidRecords(*currentSession);
    ASSERT_TRUE(record);
    EXPECT_EQ(record->decimal, "8900000000000000002");
    EXPECT_TRUE(current->FindBinding({1}));
    EXPECT_EQ(current->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2UnlockPin, PreIdentityApplicationCompletesInlineAndMovesOnlyToNotReady)
{
    PinFixture fixture;
    auto &module = fixture.live.module;
    const auto session = fixture.ObserveLocked({1});
    ASSERT_TRUE(session);
    ASSERT_FALSE(module->FindBinding({1}));
    fixture.io->onSubmit = [&](const CardCommandEvidence &reply) {
        EXPECT_TRUE(module->AcceptPinIoEvidence(reply));
    };

    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {30}), 0);
    EXPECT_TRUE(fixture.io->commands.empty());
    fixture.host->RunNext();
    ASSERT_EQ(fixture.io->commands.size(), 1u);
    EXPECT_EQ(fixture.io->commands.front().first.session, *session);
    EXPECT_EQ(fixture.io->commands.front().first.kind, CardCommandKind::UnlockPin);
    EXPECT_EQ(fixture.io->commands.front().second, u"1234");

    ASSERT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(fixture.host->results[0].callback, 30u);
    EXPECT_EQ(fixture.host->results[0].status, 0);
    EXPECT_EQ(fixture.host->results[0].unlockResult, 0);
    EXPECT_EQ(fixture.host->results[0].remaining, 3);
    EXPECT_EQ(fixture.host->changed, std::vector<SessionRef> {*session});
    EXPECT_EQ(module->ReadLockState(*session), CardLockState::NotReady);
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_FALSE(module->AcceptPinIoEvidence(fixture.io->Reply(0, PinReplyStatus::Unlocked, 3)));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2UnlockPin, WrongPasswordPreservesSuccessfulApiAndRemainingAttemptsAcrossTwoCards)
{
    PinFixture fixture;
    auto &module = fixture.live.module;
    const auto first = fixture.ObserveLocked({0});
    const auto second = fixture.ObserveLocked({1});
    ASSERT_TRUE(first && second);
    ASSERT_EQ(module->UnlockPin(0, u"5678", {7}, {31}), 0);
    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {32}), 0);
    fixture.host->RunNext();
    fixture.host->RunNext();
    ASSERT_EQ(fixture.io->commands.size(), 2u);

    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(1, PinReplyStatus::Unlocked, 3)));
    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(0, PinReplyStatus::Incorrect, 2)));
    ASSERT_EQ(fixture.host->results.size(), 2u);
    const auto wrong = fixture.host->results[1];
    EXPECT_EQ(wrong.callback, 31u);
    EXPECT_EQ(wrong.status, 0);
    EXPECT_EQ(wrong.unlockResult, -1);
    EXPECT_EQ(wrong.remaining, 2);
    EXPECT_EQ(module->ReadLockState(*first), CardLockState::Locked);
    EXPECT_EQ(module->ReadLockState(*second), CardLockState::NotReady);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2UnlockPin, TransportFailureAndWaitExpirationUseDistinctOriginalErrors)
{
    using namespace OHOS::Telephony;
    PinFixture fixture;
    auto &module = fixture.live.module;
    const auto session = fixture.ObserveLocked({1});
    ASSERT_TRUE(session);
    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), 0);
    fixture.host->RunNext();
    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(0, PinReplyStatus::Failed, 0)));
    ASSERT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(fixture.host->results[0].status, TELEPHONY_ERR_RIL_CMD_FAIL);
    EXPECT_FALSE(fixture.host->results[0].unlockResult);

    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {32}), 0);
    fixture.host->RunNext();
    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(1, PinReplyStatus::TimedOut, 0)));
    ASSERT_EQ(fixture.host->results.size(), 2u);
    EXPECT_EQ(fixture.host->results[1].status, CORE_ERR_SIM_CARD_UPDATE_FAILED);
    EXPECT_FALSE(fixture.host->results[1].unlockResult);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    auto late = fixture.io->Reply(1, PinReplyStatus::Unlocked, 3);
    EXPECT_FALSE(module->AcceptPinIoEvidence(late));
    late.currentStateVerified = true;
    EXPECT_TRUE(module->AcceptPinIoEvidence(late));
    EXPECT_EQ(fixture.host->results.size(), 2u);
    EXPECT_TRUE(fixture.host->changed.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2UnlockPin, ReplacedApplicationKeepsItsOwnLockedState)
{
    using namespace OHOS::Telephony;
    PinFixture fixture;
    auto &module = fixture.live.module;
    ASSERT_TRUE(fixture.ObserveLocked({1}));
    ASSERT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), 0);
    fixture.host->RunNext();
    const auto replacement = fixture.ObserveLocked({1}, 2);
    ASSERT_TRUE(replacement);

    ASSERT_TRUE(module->AcceptPinIoEvidence(fixture.io->Reply(0, PinReplyStatus::Unlocked, 3)));
    ASSERT_EQ(fixture.host->results.size(), 1u);
    EXPECT_EQ(fixture.host->results[0].status, TELEPHONY_ERR_RIL_CMD_FAIL);
    EXPECT_EQ(module->ReadLockState(*replacement), CardLockState::Locked);
    EXPECT_TRUE(fixture.host->changed.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2UnlockPin, PermissionsAndQueueRejectionPrecedeTransport)
{
    using namespace OHOS::Telephony;
    PinFixture fixture;
    auto &module = fixture.live.module;
    fixture.permission->system = false;
    EXPECT_EQ(module->UnlockPin(-1, u"1234", {7}, {0}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    fixture.permission->system = true;
    fixture.permission->permitted = false;
    EXPECT_EQ(module->UnlockPin(-1, u"1234", {7}, {0}), TELEPHONY_ERR_PERMISSION_ERR);
    fixture.permission->permitted = true;
    EXPECT_EQ(module->UnlockPin(-1, u"1234", {7}, {0}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(module->UnlockPin(-1, u"1234", {7}, {31}), TELEPHONY_ERR_SLOTID_INVALID);
    fixture.host->acceptTasks = false;
    EXPECT_EQ(module->UnlockPin(1, u"1234", {7}, {31}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(fixture.host->released, 1u);
    EXPECT_TRUE(fixture.io->commands.empty());
    EXPECT_TRUE(fixture.host->results.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, LiveDualSimSetPrimaryThroughLegacyEntry)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    ASSERT_TRUE(f.live.module->ReadAccount({40}));
    ASSERT_TRUE(f.live.module->ReadAccount({41}));
    const auto binding = f.live.module->FindBinding({1});
    ASSERT_TRUE(binding);
    EXPECT_EQ(binding->subscription.legacySimId, 41);
    f.host->onAccountChanged = [&] {
        EXPECT_EQ(f.live.input.output->events, (std::vector<std::string> {"primary", "data"}));
    };
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(f.live.module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(f.live.module->ReadAssignment().lastData, SlotId {1});
    EXPECT_EQ(f.live.input.parameters->writes, (std::vector<std::pair<std::string, std::string>> {
        {kPreference, "true"}, {kPrimary, "1"}, {kIccid, kEncoded1}, {kData, "1"}}));
    const std::vector<std::pair<SlotId, SubscriptionId>> change {{{1}, {41}}};
    EXPECT_EQ(f.live.input.output->mainChanges, change);
    EXPECT_EQ(f.live.input.output->dataChanges, change);
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_TRUE(f.live.input.completion->results.empty());
    EXPECT_EQ(f.live.input.radio->commands, 0u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    std::weak_ptr<SimModule> weak = f.live.module;
    f.live.module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2Primary, EntryPreservesPermissionSlotAndMissingBindingOrder)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f;
    f.host->system = false;
    f.host->permitted = false;
    f.host->slotSupported = false;
    EXPECT_EQ(f.live.module->SetPrimarySlotId(-1, {7}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    EXPECT_EQ(f.host->calls, std::vector<std::string> {"system"});
    f.host->calls.clear();
    f.host->system = true;
    EXPECT_EQ(f.live.module->SetPrimarySlotId(-1, {7}), TELEPHONY_ERR_PERMISSION_ERR);
    EXPECT_EQ(f.host->calls, (std::vector<std::string> {"system", "permission"}));
    f.host->permitted = true;
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), TELEPHONY_ERR_SLOTID_INVALID);
    f.host->slotSupported = true;
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_TRUE(f.live.input.parameters->writes.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    f.host->calls.clear();
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_TRUE(f.host->calls.empty());
}

TEST(AF2Primary, ReplacementDuringPrimarySlotWriteRevokesRemainingKeys)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    const auto old = f.live.module->FindBinding({1});
    ASSERT_TRUE(old);
    f.live.input.parameters->writeResult = [&](const std::string &key) {
        if (key == kPrimary) {
            f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
            EXPECT_TRUE(f.live.module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
        }
        return AssignmentParameterResult {0, true};
    };
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_FAIL);
    const auto replacement = f.live.module->FindBinding({1});
    ASSERT_TRUE(replacement);
    EXPECT_EQ(replacement->subscription.legacySimId, 56);
    EXPECT_NE(old->bindingRevision, replacement->bindingRevision);
    EXPECT_EQ(f.live.input.parameters->writes, (std::vector<std::pair<std::string, std::string>> {
        {kPreference, "true"}, {kPrimary, "1"}}));
    EXPECT_FALSE(f.live.module->ReadAssignment().lastPrimary);
    EXPECT_FALSE(f.live.module->ReadAssignment().lastData);
    EXPECT_TRUE(f.live.input.output->events.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, InterruptedIccidWriteRetainsPrefixAndOnlyAcceptsOriginalEvidenceAfterStop)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &parameters = *f.live.input.parameters;
    parameters.writeResult = [](const std::string &key) {
        return key == kIccid ? AssignmentParameterResult {-1, false} : AssignmentParameterResult {0, true};
    };
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_FAIL);
    const auto receipt = f.live.module->ReadC0Receipt();
    ASSERT_TRUE(receipt);
    ASSERT_TRUE(receipt->writes[0] && receipt->writes[1] && receipt->writes[2]);
    EXPECT_EQ(receipt->writes[0]->outcome.effect, EffectKnowledge::ConfirmedApplied);
    EXPECT_EQ(receipt->writes[1]->outcome.effect, EffectKnowledge::ConfirmedApplied);
    EXPECT_EQ(receipt->writes[2]->outcome.effect, EffectKnowledge::Unknown);
    EXPECT_FALSE(receipt->writes[2]->outcome.call.callFinished);
    EXPECT_FALSE(receipt->writes[3]);
    EXPECT_FALSE(f.live.module->ReadAssignment().lastPrimary);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    const auto original = receipt->writes[2]->operation;
    auto wrong = original;
    ++wrong.original.attempt;
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(wrong));
    EXPECT_EQ(parameters.evidenceReads, 0u);
    auto wrongEpoch = original;
    wrongEpoch.original.request.epoch = {0, 0};
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(wrongEpoch));
    auto wrongOwner = original;
    wrongOwner.original.request.owner = RequestOwner::LocalCard;
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(wrongOwner));
    parameters.values[kIccid] = kEncoded1;
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(original));
    parameters.evidence = C0ParameterEvidence {wrong, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(original));
    parameters.evidence = C0ParameterEvidence {original, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(f.live.module->ReconcilePrimaryParameter(original));
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(original));
    EXPECT_FALSE(f.live.module->ReadC0Receipt());
    EXPECT_EQ(parameters.writes.size(), 3u);
    EXPECT_TRUE(f.live.input.output->events.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, OriginalStoreReceiptCanBeReconciledDuringCloseObservation)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    auto &parameters = *f.live.input.parameters;
    parameters.writeResult = [](const std::string &key) {
        return key == kIccid ? AssignmentParameterResult {-1, false} : AssignmentParameterResult {0, true};
    };
    EXPECT_EQ(module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_FAIL);
    const auto receipt = module->ReadC0Receipt();
    ASSERT_TRUE(receipt && receipt->writes[2]);
    const auto original = receipt->writes[2]->operation;
    parameters.evidence = C0ParameterEvidence {original, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    bool called = false;
    f.live.input.dispatch->afterAudit = [&] {
        if (!std::exchange(called, true)) { EXPECT_TRUE(module->ReconcilePrimaryParameter(original)); }
    };

    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_TRUE(called);
    EXPECT_FALSE(module->ReadC0Receipt());
    EXPECT_FALSE(module->ReconcilePrimaryParameter(original));
    EXPECT_TRUE(f.live.input.output->events.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, CompletedLegacyParameterErrorsReturnSuccessAndRetainOnlyIndeterminateEffects)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &parameters = *f.live.input.parameters;
    parameters.writeResult = [](const std::string &key) {
        return key == kPrimary || key == kIccid || key == kData ? AssignmentParameterResult {-1, true} :
            AssignmentParameterResult {0, true};
    };
    parameters.classify = [](const C0ParameterOperation &operation, AssignmentParameterResult call) {
        if (operation.key == C0ParameterKey::PrimarySlot) {
            return C0ParameterOutcome {call, EffectKnowledge::ConfirmedNotApplied};
        }
        return C0ParameterOutcome {call, call.parameterResult == 0 ?
            EffectKnowledge::ConfirmedApplied : EffectKnowledge::Unknown};
    };
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    const auto receipt = f.live.module->ReadC0Receipt();
    ASSERT_TRUE(receipt && receipt->writes[1] && receipt->writes[2] && receipt->writes[3]);
    EXPECT_TRUE(receipt->businessSucceeded);
    EXPECT_EQ(receipt->writes[1]->outcome.effect, EffectKnowledge::ConfirmedNotApplied);
    EXPECT_EQ(receipt->writes[2]->outcome.effect, EffectKnowledge::Unknown);
    EXPECT_TRUE(receipt->writes[2]->outcome.call.callFinished);
    EXPECT_EQ(f.live.module->ReadAssignment().lastPrimary, SlotId {1});
    EXPECT_EQ(f.live.module->ReadAssignment().lastData, SlotId {1});
    EXPECT_TRUE(f.live.module->InvalidateVerifiedCard({f.live.module->Epoch(), {{1}, "USIM"}, 1}));
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(receipt->writes[1]->operation));
    const auto iccid = receipt->writes[2]->operation;
    const auto data = receipt->writes[3]->operation;
    parameters.evidence = C0ParameterEvidence {data, {{-1, true}, EffectKnowledge::ConfirmedNotApplied}};
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(iccid));
    EXPECT_TRUE(f.live.module->ReconcilePrimaryParameter(data));
    EXPECT_FALSE(f.live.module->ReconcilePrimaryParameter(data));
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    parameters.evidence = C0ParameterEvidence {iccid, {{0, true}, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(f.live.module->ReconcilePrimaryParameter(iccid));
    EXPECT_FALSE(f.live.module->ReadC0Receipt());
    EXPECT_EQ(parameters.writes.size(), 4u);
    EXPECT_EQ(f.live.input.output->events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Primary, StopInFlightAndDuringOriginalNotificationRetainsCallUntilReturn)
{
    using Drain = SimModule::DrainResult;
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    std::promise<void> inStore;
    std::promise<void> release;
    auto continueStore = release.get_future();
    f.live.input.parameters->writeResult = [&](const std::string &key) {
        if (key == kPrimary) {
            inStore.set_value();
            continueStore.wait();
        }
        return AssignmentParameterResult {0, true};
    };
    f.host->onAccountChanged = [&] {
        EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    };
    auto running = std::async(std::launch::async, [&] { return f.live.module->SetPrimarySlotId(1, {7}); });
    inStore.get_future().wait();
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_LOCAL_PTR_NULL);
    release.set_value();
    EXPECT_EQ(running.get(), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), Drain::Drained);
    EXPECT_EQ(f.live.input.dispatch->seals, 1u);
}


TEST(AF2Primary, IndependentRepeatedSelectionPreservesLegacyMainAnnouncementDeduplication)
{
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(f.live.module->SetPrimarySlotId(1, {7}), OHOS::Telephony::TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(f.live.input.parameters->writes.size(), 8u);
    EXPECT_EQ(f.live.input.output->events, (std::vector<std::string> {"primary", "data", "data"}));
    EXPECT_EQ(f.host->accountsChanged, 2u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

namespace {
std::shared_ptr<SlotCatalog> PrimaryZeroCatalog()
{
    return std::make_shared<SlotCatalog>(std::vector<SlotDescriptor> {
        {{0}, EndpointKind::Physical, 0, RadioDomainId {"m0"}, VisibilityClass::Ordinary, false},
        {{1}, EndpointKind::Physical, 1, RadioDomainId {"m1"}, VisibilityClass::Ordinary, false}},
        LegacyCapacityMeasures {2, 2, 2, 2, 2});
}
} // namespace

TEST(AF2Data, LiveDualSimSetDefaultDataThroughLegacyEntry)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f(PrimaryZeroCatalog());
    f.live.input.radio->primaryEndpoint = {0};
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    ASSERT_TRUE(module->ReadAccount(binding->subscription));
    EXPECT_EQ(binding->subscription, SubscriptionId {41});
    ASSERT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(module->ReadAssignment().defaultData, SubscriptionId {40});

    auto &parameters = *f.live.input.parameters;
    auto &output = *f.live.input.output;
    const auto previous = parameters.values;
    parameters.writes.clear();
    output.events.clear();
    output.dataChanges.clear();
    SimGuard::Primary::Observation observation;
    observation.Begin();
    parameters.writeResult = [&](const std::string &key) {
        observation.Add("parameter_write", {{"key", key}, {"value", parameters.writes.back().second}});
        return AssignmentParameterResult {0, true};
    };
    output.onDataChanged = [&](SubscriptionId subscription) {
        observation.Add("publication", {{"action", "usual.event.SIM.DEFAULT_DATA_SUBSCRIPTION_CHANGED"},
            {"sim", std::to_string(subscription.legacySimId)}});
    };
    output.onDataBehavior = [&](SlotId slot) {
        observation.Add("diagnostic", {{"name", "SET_DEFAULT_CELLULAR_DATA"},
            {"slot", std::to_string(slot.legacySlotId)}});
    };
    f.host->onAccountChanged = [&] {
        observation.Add("diagnostic", {{"name", "ACCOUNT_NOTIFY"}});
        EXPECT_EQ(output.events, (std::vector<std::string> {"data", "data-behavior"}));
        EXPECT_EQ(module->ReadAssignment().defaultData, binding->subscription);
    };
    int result = -1;
    int selectedSlot = -1;
    int selectedSim = -1;
    SimGuard::ExerciseIndependentDataSelection(
        [&](int slot) { return module->SetDefaultCellularDataSlotId(slot, {7}); },
        [&] { return module->ReadAssignment().lastData.value_or(SlotId {-1}).legacySlotId; },
        [&](int &sim) {
            const auto subscription = module->ReadAssignment().defaultData;
            if (!subscription) { return TELEPHONY_ERR_FAIL; }
            sim = subscription->legacySimId;
            return TELEPHONY_ERR_SUCCESS;
        }, binding->subscription.legacySimId, result, selectedSlot, selectedSim);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, SlotId {0});
    EXPECT_EQ(parameters.writes, (std::vector<std::pair<std::string, std::string>> {{kData, "1"}}));
    auto expected = previous;
    expected[kData] = "1";
    EXPECT_EQ(parameters.values, expected);
    EXPECT_EQ(output.dataChanges, (std::vector<std::pair<SlotId, SubscriptionId>> {{{1}, {41}}}));
    EXPECT_EQ(output.dataBehavior, (std::vector<SlotId> {{1}}));
    EXPECT_EQ(output.mainChanges.size(), 1u);
    EXPECT_EQ(f.host->accountsChanged, 2u);
    observation.Add("api_return", {{"result", std::to_string(result)}});
    const auto view = module->ReadAssignment();
    ASSERT_TRUE(view.lastPrimary && view.lastData && view.defaultData);
    RecordProperty("default_data_observation", observation.Snapshot(parameters.values,
        {{"primary", std::to_string(view.lastPrimary->legacySlotId)}, {"data", std::to_string(selectedSlot)},
            {"subscription", std::to_string(selectedSim)}}));
    EXPECT_TRUE(f.live.input.completion->results.empty());
    EXPECT_EQ(f.live.input.radio->commands, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2Data, RepeatedSelectionAndMoveBackEachPublish)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f(PrimaryZeroCatalog());
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    for (const int slot : {1, 1, 0}) {
        EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(slot, {7}), TELEPHONY_ERR_SUCCESS);
    }
    const auto &input = f.live.input;
    EXPECT_EQ(input.parameters->writes, (std::vector<std::pair<std::string, std::string>> {
        {kData, "1"}, {kData, "1"}, {kData, "0"}}));
    EXPECT_EQ(input.output->dataChanges, (std::vector<std::pair<SlotId, SubscriptionId>> {
        {{1}, {41}}, {{1}, {41}}, {{0}, {40}}}));
    EXPECT_EQ(input.output->dataBehavior, (std::vector<SlotId> {{1}, {1}, {0}}));
    EXPECT_EQ(f.host->accountsChanged, 3u);
    EXPECT_EQ(f.live.module->ReadAssignment().defaultData, SubscriptionId {40});
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Data, AuthorizationPrecedesMissingCardAndMissingBinding)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f;
    f.host->system = false;
    f.host->permitted = false;
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(-1, {7}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    EXPECT_EQ(f.host->calls, std::vector<std::string> {"system"});
    f.host->system = true;
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(-1, {7}), TELEPHONY_ERR_PERMISSION_ERR);
    f.host->permitted = true;
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(2, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    f.live.store->stored = {-1, {}, false, true, EffectKnowledge::Unknown};
    ASSERT_TRUE(f.live.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    EXPECT_FALSE(f.live.module->FindBinding({1}));
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SLOTID_INVALID);
    EXPECT_TRUE(f.live.input.parameters->writes.empty());
    EXPECT_TRUE(f.live.input.output->events.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    const auto plan = *f.live.store->lastPlan;
    f.live.store->ProveApplied(plan, {41});
    ASSERT_TRUE(f.live.module->ReconcileDiscovery(plan.call));
    ASSERT_TRUE(f.live.module->InvalidateVerifiedCard({f.live.module->Epoch(), {{1}, "USIM"}, 1}));
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_TRUE(f.live.input.parameters->writes.empty());
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Data, PrimaryAndDataRejectCompetingSharedWriteInBothOrders)
{
    using namespace OHOS::Telephony;
    for (const bool primaryFirst : {true, false}) {
        PrimaryFixture f(PrimaryZeroCatalog());
        f.live.input.radio->primaryEndpoint = {0};
        ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
        auto &module = f.live.module;
        auto &parameters = *f.live.input.parameters;
        std::promise<void> entered;
        std::promise<void> release;
        auto resume = release.get_future();
        parameters.writeResult = [&](const std::string &key) {
            if (key == (primaryFirst ? kPrimary : kData)) {
                entered.set_value();
                resume.wait();
            }
            return AssignmentParameterResult {0, true};
        };
        auto active = std::async(std::launch::async, [&] {
            return primaryFirst ? module->SetPrimarySlotId(0, {7}) : module->SetDefaultCellularDataSlotId(1, {7});
        });
        if (entered.get_future().wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            release.set_value();
            FAIL() << "Original parameter call did not start";
        }
        const auto writes = parameters.writes.size();
        const auto competing = primaryFirst ? module->SetDefaultCellularDataSlotId(1, {7}) :
            module->SetPrimarySlotId(0, {7});
        EXPECT_EQ(competing, TELEPHONY_ERR_FAIL);
        EXPECT_EQ(parameters.writes.size(), writes);
        release.set_value();
        EXPECT_EQ(active.get(), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->ReadAssignment().defaultData, SubscriptionId {primaryFirst ? 40 : 41});
        EXPECT_EQ(f.host->accountsChanged, 1u);
        EXPECT_EQ(f.live.input.radio->commands, 0u);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    }
}

TEST(AF2Data, CompletedParameterFailurePreservesLegacyResultAndKnownDisposition)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f(PrimaryZeroCatalog());
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &parameters = *f.live.input.parameters;
    parameters.writeResult = [](const std::string &) { return AssignmentParameterResult {-9, true}; };
    parameters.classify = [](const C0ParameterOperation &, AssignmentParameterResult call) {
        return C0ParameterOutcome {call, EffectKnowledge::ConfirmedNotApplied};
    };
    EXPECT_EQ(f.live.module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(parameters.values[kData], "0");
    EXPECT_EQ(f.live.module->ReadAssignment().defaultData, SubscriptionId {41});
    EXPECT_FALSE(f.live.module->ReadDataParameterReceipt());
    EXPECT_EQ(f.live.input.output->events, (std::vector<std::string> {"data", "data-behavior"}));
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Data, UnknownRetainsSharedWriteUntilOriginalEvidenceAfterStop)
{
    using namespace OHOS::Telephony;
    using Drain = SimModule::DrainResult;
    for (const bool returned : {false, true}) {
        PrimaryFixture f(PrimaryZeroCatalog());
        ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
        auto &module = f.live.module;
        auto &parameters = *f.live.input.parameters;
        parameters.writeResult = [=](const std::string &) { return AssignmentParameterResult {-9, returned}; };
        EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}),
            returned ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
        const auto receipt = module->ReadDataParameterReceipt();
        ASSERT_TRUE(receipt);
        EXPECT_EQ(receipt->outcome.effect, EffectKnowledge::Unknown);
        EXPECT_EQ(receipt->outcome.call.callFinished, returned);
        EXPECT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetDefaultCellularDataSlotId(0, {7}), TELEPHONY_ERR_FAIL);
        EXPECT_TRUE(module->InvalidateVerifiedCard({module->Epoch(), {{1}, "USIM"}, 1}));
        EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::BusinessEffectsUnresolved);
        EXPECT_EQ(module->SetDefaultCellularDataSlotId(0, {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
        const auto original = receipt->operation;
        auto wrong = original;
        ++wrong.original.attempt;
        EXPECT_FALSE(module->ReconcileDataParameter(wrong));
        EXPECT_EQ(parameters.evidenceReads, 0u);
        parameters.values[kData] = "1";
        EXPECT_FALSE(module->ReconcileDataParameter(original));
        parameters.evidence = C0ParameterEvidence {wrong, {{0, true}, EffectKnowledge::ConfirmedApplied}};
        EXPECT_FALSE(module->ReconcileDataParameter(original));
        parameters.evidence = C0ParameterEvidence {original, {{0, true}, EffectKnowledge::ConfirmedApplied}};
        EXPECT_TRUE(module->ReconcileDataParameter(original));
        EXPECT_FALSE(module->ReconcileDataParameter(original));
        EXPECT_FALSE(module->ReadDataParameterReceipt());
        EXPECT_EQ(parameters.writes.size(), 1u);
        EXPECT_EQ(module->ReadAssignment().defaultData, returned ? std::optional<SubscriptionId>(SubscriptionId {41}) : std::nullopt);
        EXPECT_EQ(f.live.input.output->events.size(), returned ? 2u : 0u);
        EXPECT_EQ(f.host->accountsChanged, returned ? 1u : 0u);
        EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
    }
}

TEST(AF2Data, ReplacementDuringOriginalWritePreventsProjectionToNewBinding)
{
    using namespace OHOS::Telephony;
    PrimaryFixture f(PrimaryZeroCatalog());
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const auto old = module->FindBinding({1});
    ASSERT_TRUE(old);
    f.live.input.parameters->writeResult = [&](const std::string &) {
        f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
        EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
        return AssignmentParameterResult {0, true};
    };
    EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_FAIL);
    const auto replacement = module->FindBinding({1});
    ASSERT_TRUE(replacement);
    EXPECT_NE(old->bindingRevision, replacement->bindingRevision);
    EXPECT_EQ(replacement->subscription, SubscriptionId {56});
    EXPECT_EQ(f.live.input.parameters->values[kData], "1");
    EXPECT_FALSE(module->ReadAssignment().defaultData);
    EXPECT_TRUE(f.live.input.output->events.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_FALSE(module->ReadDataParameterReceipt());
    f.live.input.parameters->writeResult = {};
    EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(module->ReadAssignment().defaultData, replacement->subscription);
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Data, StopWaitsForParameterAndAccountNotification)
{
    using namespace OHOS::Telephony;
    using Drain = SimModule::DrainResult;
    PrimaryFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    std::promise<void> entered;
    std::promise<void> release;
    auto resume = release.get_future();
    f.live.input.parameters->writeResult = [&](const std::string &) {
        entered.set_value();
        resume.wait();
        return AssignmentParameterResult {0, true};
    };
    f.host->onAccountChanged = [&] { EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight); };
    auto active = std::async(std::launch::async, [&] { return module->SetDefaultCellularDataSlotId(1, {7}); });
    if (entered.get_future().wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        FAIL() << "Original parameter call did not start";
    }
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::CallbacksStillInFlight);
    EXPECT_EQ(module->SetDefaultCellularDataSlotId(0, {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    const auto receipt = module->ReadDataParameterReceipt();
    EXPECT_TRUE(receipt);
    if (receipt) { EXPECT_FALSE(module->ReconcileDataParameter(receipt->operation)); }
    release.set_value();
    EXPECT_EQ(active.get(), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), Drain::Drained);
}

TEST(AF2VoiceSms, LiveDualSimSelectsFourRolesThroughLegacyEntries)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider);
    f.live.input.radio->primaryEndpoint = {0};
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    ASSERT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    const auto parameters = f.live.input.parameters->values;
    ASSERT_EQ(module->SetDefaultVoiceSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultSmsSlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    const auto first = module->FindBinding({0});
    const auto second = module->FindBinding({1});
    ASSERT_TRUE(first && second && module->ReadAccount(first->subscription) && module->ReadAccount(second->subscription));
    const auto roles = module->ReadAssignment();
    EXPECT_EQ(roles.lastPrimary, SlotId {0});
    EXPECT_EQ(roles.defaultData, second->subscription);
    EXPECT_EQ(roles.defaultVoice, second->subscription);
    EXPECT_EQ(roles.defaultSms, first->subscription);
    EXPECT_EQ(provider->persisted[DefaultRole::Voice], 41);
    EXPECT_EQ(provider->persisted[DefaultRole::Sms], 40);
    EXPECT_EQ(f.live.input.parameters->values, parameters);
    EXPECT_EQ(f.live.input.output->events,
        (std::vector<std::string> {"primary", "data", "data", "data-behavior", "voice", "sms"}));
    EXPECT_EQ(f.host->accountsChanged, 4u);
    EXPECT_EQ(f.live.input.radio->commands, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

namespace {
void VerifyTargetCallMessageRole(DefaultRole role)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const bool voice = role == DefaultRole::Voice;
    const auto setRole = [&](int slot) {
        return voice ? module->SetDefaultVoiceSlotId(slot, {7}) : module->SetDefaultSmsSlotId(slot, {7});
    };
    const auto getSlot = [&] {
        const auto assignment = module->ReadAssignment();
        const auto identity = voice ? assignment.defaultVoice : assignment.defaultSms;
        for (const int slot : {0, 1}) {
            const auto binding = module->FindBinding({slot});
            if (binding && identity == binding->subscription) { return slot; }
        }
        return -1;
    };
    const auto [selected, cleared, clearResult] = SimGuard::ExerciseDefaultCallMessage(setRole, getSlot,
        [&](bool fail) {
            if (fail) {
                provider->control = [](const auto &) {
                    return RoleProviderOutcome {-1, true, RoleProviderEffect::NotWritten};
                };
            } else {
                provider->control = {};
            }
        }, [&] { f.live.input.output->publishRole = false; }, voice ? -1 : 0);
    const auto assignment = module->ReadAssignment();
    EXPECT_FALSE(voice ? assignment.voicePreference : assignment.smsPreference);
    const auto &events = f.live.input.output->roleChanges;
    std::vector<std::pair<DefaultRole, int32_t>> expected;
    for (const auto simId : SimGuard::DefaultCallMessageWrites(41)) { expected.emplace_back(role, simId); }
    EXPECT_EQ(provider->writes, expected);
    EXPECT_EQ(provider->persisted[role], 0);
    EXPECT_EQ(events, (std::vector<std::pair<DefaultRole, std::optional<SubscriptionId>>> {
        {role, SubscriptionId {41}}, {role, std::nullopt}}));
    EXPECT_EQ(f.host->accountsChanged, 3u);
    EXPECT_TRUE(f.live.input.parameters->writes.empty());
    using SimGuard::Primary::Json;
    testing::Test::RecordProperty("default_call_message_observation", Json({
        {"selected", std::to_string(selected)}, {"cleared", std::to_string(cleared)},
        {"clearResult", std::to_string(clearResult)}, {"writes", std::to_string(provider->writes.size())},
        {"announcements", std::to_string(events.size())},
        {"accountNotifications", std::to_string(f.host->accountsChanged)},
        {"identity", provider->writes == expected ? "true" : "false"}
    }));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}
} // namespace

TEST(AF2VoiceSms, DefaultVoiceProviderAndNotificationContract)
{
    VerifyTargetCallMessageRole(DefaultRole::Voice);
}

TEST(AF2VoiceSms, DefaultSmsProviderAndNotificationContract)
{
    VerifyTargetCallMessageRole(DefaultRole::Sms);
}

TEST(AF2VoiceSms, InFlightVoiceOrdersOnlyIntersectingWriters)
{
    using namespace OHOS::Telephony;
    for (const bool independent : {true, false}) {
        auto provider = std::make_shared<RoleProvider>();
        provider->independent = independent;
        PrimaryFixture f(PrimaryZeroCatalog(), provider);
        f.live.input.radio->primaryEndpoint = {0};
        ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
        auto &module = f.live.module;
        std::promise<void> entered;
        std::promise<void> release;
        auto resume = release.get_future();
        provider->control = [&](const RoleProviderOperation &operation) {
            if (operation.role == DefaultRole::Voice) { entered.set_value(); resume.wait(); }
            return RoleProviderOutcome {0, true, RoleProviderEffect::Applied};
        };
        auto voice = std::async(std::launch::async, [&] { return module->SetDefaultVoiceSlotId(1, {7}); });
        if (entered.get_future().wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            release.set_value();
            FAIL() << "Voice Provider call did not start";
        }
        EXPECT_EQ(module->SetDefaultVoiceSlotId(0, {7}), TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetDefaultSmsSlotId(0, {7}), independent ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
        EXPECT_EQ(module->SetDefaultSmsSlotId(0, {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
        const auto pending = module->ReadRoleProviderReceipt(DefaultRole::Voice);
        EXPECT_TRUE(pending);
        if (pending) { EXPECT_FALSE(module->ReconcileRoleProvider(pending->operation)); }
        release.set_value();
        EXPECT_EQ(voice.get(), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(provider->writes.size(), independent ? 2u : 1u);
        EXPECT_EQ(module->ReadAssignment().defaultVoice, SubscriptionId {41});
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    }
}

TEST(AF2VoiceSms, StaleUnknownVoiceEffectRetainsOnlyItsOriginalWriteSet)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const auto prior = module->FindBinding({1});
    ASSERT_TRUE(prior);
    provider->control = [&](const RoleProviderOperation &operation) {
        if (operation.role == DefaultRole::Voice) {
            EXPECT_TRUE(module->InvalidateVerifiedCard({module->Epoch(), {{1}, "USIM"}, 1}));
            return RoleProviderOutcome {0, false, RoleProviderEffect::Unknown};
        }
        return RoleProviderOutcome {0, true, RoleProviderEffect::Applied};
    };
    EXPECT_EQ(module->SetDefaultVoiceSlotId(1, {7}), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    const auto receipt = module->ReadRoleProviderReceipt(DefaultRole::Voice);
    ASSERT_TRUE(receipt);
    EXPECT_EQ(receipt->operation.subscription, prior->subscription);
    EXPECT_EQ(module->SetDefaultVoiceSlotId(0, {7}), TELEPHONY_ERR_FAIL);
    EXPECT_EQ(module->SetDefaultSmsSlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(module->SetDefaultCellularDataSlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    provider->persisted[DefaultRole::Voice] = prior->subscription.legacySimId;
    EXPECT_FALSE(module->ReconcileRoleProvider(receipt->operation)); // a plausible current value is not original evidence
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    auto wrong = receipt->operation;
    ++wrong.original.attempt;
    provider->originalEvidence = {wrong, {0, true, RoleProviderEffect::Applied}};
    EXPECT_FALSE(module->ReconcileRoleProvider(receipt->operation));
    provider->originalEvidence = {receipt->operation, {0, true, RoleProviderEffect::Applied}};
    EXPECT_TRUE(module->ReconcileRoleProvider(receipt->operation));
    EXPECT_FALSE(module->ReconcileRoleProvider(receipt->operation));
    EXPECT_FALSE(module->ReadAssignment().defaultVoice);
    EXPECT_EQ(provider->writes.size(), 2u);
    EXPECT_EQ(f.live.input.output->roleChanges.size(), 1u);
    EXPECT_EQ(f.host->accountsChanged, 2u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2VoiceSms, KnownPartialAndStaleAppliedNeverCommitCurrentRoles)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    provider->control = [](const auto &) { return RoleProviderOutcome {0, true, RoleProviderEffect::Partial}; };
    EXPECT_EQ(module->SetDefaultSmsSlotId(1, {7}), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_FALSE(module->ReadRoleProviderReceipt(DefaultRole::Sms));
    EXPECT_FALSE(module->ReadAssignment().smsPreference);
    EXPECT_EQ(module->ReadAssignment().defaultSms, SubscriptionId {40});
    const auto old = module->FindBinding({1});
    ASSERT_TRUE(old);
    provider->control = [&](const auto &) {
        EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, kRaw0));
        return RoleProviderOutcome {0, true, RoleProviderEffect::Applied};
    };
    EXPECT_EQ(module->SetDefaultVoiceSlotId(1, {7}), TELEPHONY_ERR_FAIL);
    EXPECT_FALSE(module->ReadAssignment().defaultVoice);
    EXPECT_TRUE(f.live.input.output->roleChanges.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2VoiceSms, PermissionsAndBindingRejectBeforeProvider)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider);
    f.host->system = false;
    EXPECT_EQ(f.live.module->SetDefaultVoiceSlotId(-1, {7}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    f.host->system = true;
    f.host->permitted = false;
    EXPECT_EQ(f.live.module->SetDefaultSmsSlotId(-1, {7}), TELEPHONY_ERR_PERMISSION_ERR);
    f.host->permitted = true;
    EXPECT_EQ(f.live.module->SetDefaultSmsSlotId(1, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    f.live.store->stored = {-1, {}, false, true, EffectKnowledge::Unknown};
    ASSERT_TRUE(f.live.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    EXPECT_EQ(f.live.module->SetDefaultVoiceSlotId(1, {7}), CORE_SERVICE_SIM_CARD_IS_NOT_ACTIVE);
    EXPECT_EQ(f.live.module->SetDefaultSmsSlotId(-1, {7}), TELEPHONY_ERR_FAIL); // shared discovery columns
    EXPECT_TRUE(provider->writes.empty());
    const auto plan = *f.live.store->lastPlan;
    f.live.store->ProveApplied(plan, {41});
    EXPECT_TRUE(f.live.module->ReconcileDiscovery(plan.call));
    EXPECT_EQ(f.live.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2VoiceSms, CurrentRoleVersionRejectsPreMutationReload)
{
    AccountRoleProtocol protocol;
    const StoredAccountRoleFragment old {{40}, true, false, false, false};
    ASSERT_TRUE(protocol.AcceptReloadSet({old}, 1, 0, false));
    ASSERT_TRUE(protocol.AcceptReloadSet({old}, 1, 0, true));
    const auto previousCurrent = protocol.Revision(false);
    const auto previousHistorical = protocol.Revision(true);
    protocol.NoteLocalRoleMutation(DefaultRole::Voice, std::nullopt);
    EXPECT_EQ(protocol.Revision(false), previousCurrent + 1);
    EXPECT_FALSE(protocol.AcceptReloadSet({old}, 2, previousCurrent, false));
    EXPECT_FALSE(protocol.Projection(false).at({40}).voice);
    EXPECT_EQ(protocol.Revision(true), previousHistorical);
    EXPECT_TRUE(protocol.Projection(true).at({40}).voice);
}

TEST(AF2AccountMetadata, LiveDualSimMetadataAndFourRoles)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    auto roleProvider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), roleProvider, provider);
    f.live.input.radio->primaryEndpoint = {0};
    auto &module = f.live.module;
    ASSERT_FALSE(module->FindBinding({0}));
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    const auto first = module->FindBinding({0});
    const auto second = module->FindBinding({1});
    ASSERT_TRUE(first && second);
    ASSERT_EQ(first->subscription.legacySimId, 40);
    ASSERT_EQ(second->subscription.legacySimId, 41);
    const auto before = module->ReadAssignment();
    ASSERT_EQ(module->SetShowName(0, u"个人", {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetShowName(1, u"Work", {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetSimLabelIndexByIccId(kRaw0, 3), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetSimLabelIndexByIccId(kRaw1, 4), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, before.lastPrimary);
    EXPECT_EQ(module->ReadAssignment().defaultData, before.defaultData);
    EXPECT_EQ(module->ReadAssignment().defaultVoice, before.defaultVoice);
    EXPECT_EQ(module->ReadAssignment().defaultSms, before.defaultSms);
    const auto left = module->ReadAccount(first->subscription);
    const auto right = module->ReadAccount(second->subscription);
    ASSERT_TRUE(left && right);
    std::u16string name;
    EXPECT_EQ(module->GetShowName(0, name, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(name, u"个人");
    EXPECT_EQ(module->GetShowName(1, name, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(name, u"Work");
    EXPECT_EQ(left->legacyLabel, 3);
    EXPECT_EQ(right->legacyLabel, 4);
    const auto writes = provider->Writes();
    ASSERT_EQ(writes.size(), 4u);
    EXPECT_EQ(writes[0].selector, "sim_id");
    EXPECT_EQ(writes[0].predicate, "40");
    EXPECT_EQ(writes[0].field, "show_name");
    EXPECT_EQ(std::get<std::u16string>(writes[0].value), u"个人");
    EXPECT_EQ(writes[3].selector, "icc_id");
    EXPECT_EQ(writes[3].predicate, kRaw1);
    EXPECT_EQ(writes[3].field, "sim_label_index");
    EXPECT_EQ(std::get<int32_t>(writes[3].value), 4);
    ASSERT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultVoiceSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultSmsSlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    const auto roles = module->ReadAssignment();
    EXPECT_EQ(roles.lastPrimary, SlotId {0});
    EXPECT_EQ(roles.defaultData, second->subscription);
    EXPECT_EQ(roles.defaultVoice, second->subscription);
    EXPECT_EQ(roles.defaultSms, first->subscription);
    for (const auto &account : {*left, *right}) {
        const auto after = module->ReadAccount(account.subscription);
        const auto historical = module->ReadAccount(account.subscription, AccountReadView::Historical);
        ASSERT_TRUE(after && historical);
        EXPECT_EQ(after->displayName, account.displayName);
        EXPECT_EQ(after->legacyLabel, account.legacyLabel);
        EXPECT_EQ(after->fields.name, account.fields.name);
        EXPECT_EQ(after->fields.label, account.fields.label);
        EXPECT_EQ(historical->displayName, account.displayName);
        EXPECT_EQ(historical->legacyLabel, account.legacyLabel);
    }
    EXPECT_EQ(f.host->accountsChanged, 8u);
    EXPECT_EQ(f.live.input.radio->commands, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2AccountMetadata, LegacyFieldContract)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    std::optional<uint64_t> namedRevision;
    std::optional<uint64_t> labeledRevision;
    std::u16string observedName;
    SimGuard::ExerciseAccountMetadata(
        [&](const std::u16string &name) {
            const auto result = module->SetShowName(0, name, {7});
            if (name == u"Host account") {
                const auto account = module->ReadAccount({40});
                EXPECT_TRUE(account);
                if (account) {
                    if (namedRevision) { EXPECT_EQ(account->fields.name, namedRevision); }
                    else { namedRevision = account->fields.name; }
                }
            }
            return result;
        },
        [&](std::u16string &name) {
            const auto result = module->GetShowName(0, name, {7});
            observedName = name;
            return result;
        },
        [&](int32_t label) {
            const auto result = module->SetSimLabelIndexByIccId(kRaw0, label);
            if (label == 3) {
                const auto account = module->ReadAccount({40});
                EXPECT_TRUE(account);
                if (account) {
                    if (labeledRevision) { EXPECT_EQ(account->fields.label, labeledRevision); }
                    else { labeledRevision = account->fields.label; }
                }
            }
            return result;
        },
        [&] { provider->respond = [](const auto &) {
            return AccountMetadataOutcome {-1, true, EffectKnowledge::Rejected};
        }; });
    const auto account = module->ReadAccount({40});
    ASSERT_TRUE(account && account->legacyLabel);
    EXPECT_TRUE(observedName.empty());
    EXPECT_EQ(*account->legacyLabel, 3);
    EXPECT_EQ(f.host->accountsChanged, 5u);
    EXPECT_EQ(provider->Writes().size(), 7u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::string observation = "{\"accountNotifications\":\"" + std::to_string(f.host->accountsChanged) +
        "\",\"emptyName\":\"" + (observedName.empty() ? "true" : "false") + "\",\"identity\":\"" +
        (account->subscription.legacySimId == 40 ? "true" : "false") + "\",\"label\":\"" +
        std::to_string(*account->legacyLabel) + "\",\"providerCalls\":\"" +
        std::to_string(provider->Writes().size()) + "\"}";
    RecordProperty("account_metadata_observation", observation);
}

TEST(AF2AccountMetadata, PermissionAndHistoricalIdentityWithoutBinding)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    auto &module = f.live.module;
    auto session = module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1);
    ASSERT_TRUE(session);
    std::u16string name = u"unmodified";
    f.host->system = false;
    f.host->permitted = false;
    f.host->readable = false;
    EXPECT_EQ(module->SetShowName(-1, u"name", {7}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    EXPECT_EQ(module->GetShowName(-1, name, {7}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    f.host->system = true;
    EXPECT_EQ(module->GetShowName(1, name, {7}), TELEPHONY_ERR_PERMISSION_ERR);
    EXPECT_EQ(module->SetShowName(1, u"name", {7}), TELEPHONY_ERR_PERMISSION_ERR);
    f.host->readable = true;
    f.host->permitted = true;
    f.host->slotSupported = false;
    EXPECT_EQ(module->GetShowName(1, name, {7}), TELEPHONY_ERR_SLOTID_INVALID);
    f.host->slotSupported = true;
    EXPECT_EQ(module->SetShowName(3, u"name", {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    ASSERT_TRUE(module->InvalidateVerifiedCard(*session));
    EXPECT_FALSE(module->FindBinding({1}));
    EXPECT_EQ(module->SetShowName(1, u"name", {7}), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_EQ(module->GetShowName(1, name, {7}), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_EQ(name, u"unmodified");
    EXPECT_EQ(module->SetSimLabelIndexByIccId("", 3), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(module->SetSimLabelIndexByIccId("unseen", 3), TELEPHONY_ERR_ARGUMENT_INVALID);
    EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw1, 8), TELEPHONY_ERR_SUCCESS);
    const auto old = module->ReadAccount({41}, AccountReadView::Historical);
    ASSERT_TRUE(old);
    EXPECT_EQ(old->legacyLabel, 8);
    EXPECT_TRUE(old->displayName.empty());
    ASSERT_EQ(provider->Writes().size(), 1u);
    EXPECT_EQ(provider->Writes()[0].operation.subscription.legacySimId, 41);
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2AccountMetadata, FieldAndRowIsolationWhileOriginalWriteIsInFlight)
{
    using namespace OHOS::Telephony;
    for (const bool independent : {true, false}) {
        auto provider = std::make_shared<MetadataProvider>();
        provider->independent = independent;
        PrimaryFixture f(PrimaryZeroCatalog(), std::make_shared<RoleProvider>(), provider);
        ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
        auto &module = f.live.module;
        std::promise<void> entered;
        std::promise<void> release;
        auto enteredWait = entered.get_future();
        auto releaseWait = release.get_future();
        provider->respond = [&](const auto &write) {
            if (write.value == AccountMetadataValue {u"first"}) {
                entered.set_value();
                releaseWait.wait();
            }
            return AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
        };
        auto running = std::async(std::launch::async, [&] { return module->SetShowName(0, u"first", {7}); });
        enteredWait.wait();
        EXPECT_EQ(module->SetShowName(0, u"conflict", {7}), TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetShowName(1, u"independent account", {7}), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw0, 8), independent ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->SetDefaultVoiceSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
        EXPECT_EQ(module->SetShowName(1, u"closed", {7}), TELEPHONY_ERR_LOCAL_PTR_NULL);
        release.set_value();
        EXPECT_EQ(running.get(), TELEPHONY_ERR_SUCCESS);
        EXPECT_EQ(module->ReadAccount({40})->displayName, u"first");
        EXPECT_EQ(module->ReadAccount({41})->displayName, u"independent account");
        if (independent) { EXPECT_EQ(module->ReadAccount({40})->legacyLabel, 8); }
        EXPECT_EQ(provider->Writes().size(), independent ? 3u : 2u);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    }
}

TEST(AF2AccountMetadata, UnknownRequiresExactOriginalEvidenceAfterStop)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    provider->respond = [](const auto &write) {
        return write.field == "sim_label_index" ? AccountMetadataOutcome {0, false, EffectKnowledge::Unknown} :
            AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
    };
    EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw1, 8), TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    ASSERT_EQ(provider->Writes().size(), 1u);
    const auto original = provider->Writes()[0].operation;
    EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw1, 8), TELEPHONY_ERR_FAIL);
    EXPECT_EQ(module->SetShowName(1, u"independent", {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_FALSE(module->ReconcileAccountMetadata(original)); // Reading the same value supplies no original evidence.
    auto wrong = original;
    ++wrong.original.attempt;
    provider->evidence = AccountMetadataEvidence {wrong, {0, true, EffectKnowledge::ConfirmedApplied}};
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw1, 9), TELEPHONY_ERR_LOCAL_PTR_NULL);
    provider->evidence = AccountMetadataEvidence {original, {0, true, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(module->ReconcileAccountMetadata(original));
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_NE(module->ReadAccount({41})->legacyLabel, 8);
    EXPECT_EQ(module->ReadAccount({41})->displayName, u"independent");
    EXPECT_EQ(f.host->accountsChanged, 1u);
    EXPECT_EQ(provider->Writes().size(), 2u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2AccountMetadata, ReplacementDoesNotReceiveTheOriginalSlotWrite)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    provider->respond = [&](const auto &write) {
        EXPECT_EQ(write.operation.subscription.legacySimId, 41);
        EXPECT_EQ(write.predicate, "41");
        f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
        EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
        return AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
    };
    EXPECT_EQ(module->SetShowName(1, u"issued for old identity", {7}), TELEPHONY_ERR_FAIL);
    ASSERT_TRUE(module->FindBinding({1}));
    EXPECT_EQ(module->FindBinding({1})->subscription.legacySimId, 56);
    std::u16string name;
    EXPECT_EQ(module->GetShowName(1, name, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_TRUE(name.empty());
    EXPECT_TRUE(module->ReadAccount({41}, AccountReadView::Historical)->displayName.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(provider->Writes().size(), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ImsPreference, LiveDualSimSavesAndQueriesProviderPreference)
{
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const auto a = module->FindBinding({0});
    const auto b = module->FindBinding({1});
    ASSERT_TRUE(a && b);
    ASSERT_EQ(a->subscription.legacySimId, 40);
    ASSERT_EQ(b->subscription.legacySimId, 41);
    const auto before = module->ReadAccount(a->subscription);
    ASSERT_TRUE(before);
    f.host->system = false;
    f.host->permitted = false;
    f.host->readable = false;

    ASSERT_EQ(module->SaveImsSwitch(0, 21), 0);
    ASSERT_EQ(module->SaveImsSwitch(1, -7), 0);
    ASSERT_EQ(module->SaveImsSwitch(0, 21), 0);
    int32_t output = 99;
    ASSERT_EQ(module->QueryImsSwitch(0, output), 0);
    EXPECT_EQ(output, 21);
    ASSERT_EQ(module->QueryImsSwitch(1, output), 0);
    EXPECT_EQ(output, -7);
    EXPECT_EQ(provider->Reads(), (std::vector<std::string> {kRaw0, kRaw1}));
    const auto writes = provider->Writes();
    ASSERT_EQ(writes.size(), 3u);
    EXPECT_EQ(writes[0].operation.subscription, a->subscription);
    EXPECT_EQ(writes[1].operation.subscription, b->subscription);
    EXPECT_EQ(writes[0].operation.field, AccountMetadataField::ImsPreference);
    EXPECT_EQ(writes[0].field, "ims_switch");
    EXPECT_EQ(writes[0].selector, "icc_id");
    EXPECT_EQ(writes[0].predicate, kRaw0);
    EXPECT_EQ(writes[1].predicate, kRaw1);
    EXPECT_EQ(std::get<int32_t>(writes[1].value), -7);
    const auto after = module->ReadAccount(a->subscription);
    const auto historical = module->ReadAccount(a->subscription, AccountReadView::Historical);
    ASSERT_TRUE(after && historical);
    EXPECT_EQ(after->fields.imsPreference, before->fields.imsPreference + 2);
    EXPECT_EQ(historical->fields.imsPreference, after->fields.imsPreference);
    EXPECT_EQ(after->fields.name, before->fields.name);
    EXPECT_EQ(after->fields.label, before->fields.label);
    EXPECT_EQ(after->displayName, before->displayName);
    EXPECT_EQ(after->legacyLabel, before->legacyLabel);
    EXPECT_TRUE(f.host->calls.empty());
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(f.live.input.radio->commands, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2ImsPreference, LegacyProviderContract)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    int32_t rawWrite = 0;
    provider->respond = [&](const auto &write) {
        EXPECT_EQ(write.predicate, kRaw1);
        EXPECT_EQ(write.field, "ims_switch");
        return AccountMetadataOutcome {rawWrite, true,
            rawWrite >= 0 ? EffectKnowledge::ConfirmedApplied : EffectKnowledge::Rejected};
    };
    const auto [first, repeat, positive, failure, external, empty, failedRead, value] =
        SimGuard::ExerciseBoundImsPreference(
            [&](int slot, int setting) { return module->SaveImsSwitch(slot, setting); },
            [&](int slot, int &setting) { return module->QueryImsSwitch(slot, setting); },
            [&](int result) { rawWrite = result; },
            [&](int setting) { provider->SetImsValue(kRaw1, setting); },
            [&](SimGuard::ImsReadMode mode) {
                provider->readIms = [mode](const auto &) {
                    return mode == SimGuard::ImsReadMode::NoRow ?
                        ImsPreferenceRead {ImsPreferenceReadStatus::NoRow, 77} :
                        ImsPreferenceRead {ImsPreferenceReadStatus::Failed, 88};
                };
            });
    const auto writes = provider->Writes();
    const auto reads = provider->Reads();
    ASSERT_EQ(writes.size(), 4u);
    EXPECT_EQ(std::get<int32_t>(writes[0].value), 17);
    EXPECT_EQ(std::get<int32_t>(writes[1].value), 17);
    EXPECT_EQ(std::get<int32_t>(writes[2].value), -9);
    EXPECT_EQ(std::get<int32_t>(writes[3].value), 123);
    EXPECT_EQ(reads, std::vector<std::string>(6, kRaw1));
    EXPECT_EQ(module->ReadAccount({41})->fields.imsPreference, 3u);
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    RecordProperty("ims_preference_observation", SimGuard::Primary::Json({
        {"first", std::to_string(first)}, {"repeat", std::to_string(repeat)},
        {"positive", std::to_string(positive)}, {"failure", std::to_string(failure)},
        {"externalValue", std::to_string(external)}, {"noRow", std::to_string(empty)},
        {"failedRead", std::to_string(failedRead)}, {"unknown", std::to_string(value)},
        {"writes", std::to_string(writes.size())}, {"reads", std::to_string(reads.size())},
        {"notifications", std::to_string(f.host->accountsChanged)}
    }));
}

TEST(AF2ImsPreference, MissingBindingDoesNotAddressEmptyIccid)
{
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    auto &module = f.live.module;
    ASSERT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    ASSERT_FALSE(module->FindBinding({0}));
    int32_t output = 99;
    const auto save = module->SaveImsSwitch(0, 7);
    const auto read = module->QueryImsSwitch(0, output);
    EXPECT_EQ(save, -1);
    EXPECT_EQ(read, -1);
    EXPECT_EQ(output, -1);
    EXPECT_TRUE(provider->Writes().empty());
    EXPECT_TRUE(provider->Reads().empty());
    RecordProperty("ims_preference_empty_binding", SimGuard::Primary::Json({
        {"save", std::to_string(save)}, {"read", std::to_string(read)}, {"output", std::to_string(output)},
        {"writes", std::to_string(provider->Writes().size())}, {"reads", std::to_string(provider->Reads().size())}
    }));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ImsPreference, FieldIsolationAndStopDuringOriginalWrite)
{
    using namespace OHOS::Telephony;
    for (const bool independent : {true, false}) {
        auto provider = std::make_shared<MetadataProvider>();
        provider->independent = independent;
        PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
        ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
        auto &module = f.live.module;
        std::promise<void> entered;
        std::promise<void> release;
        auto enteredWait = entered.get_future();
        auto releaseWait = release.get_future();
        provider->respond = [&](const auto &write) {
            if (write.field == "ims_switch" && write.predicate == kRaw0) {
                entered.set_value();
                releaseWait.wait();
            }
            return AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
        };
        auto running = std::async(std::launch::async, [&] { return module->SaveImsSwitch(0, 23); });
        enteredWait.wait();
        EXPECT_EQ(module->SaveImsSwitch(0, 24), -1);
        EXPECT_EQ(module->SaveImsSwitch(1, 25), 0);
        EXPECT_EQ(module->SetShowName(0, u"same row", {7}), independent ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
        EXPECT_EQ(module->SetSimLabelIndexByIccId(kRaw0, 8), independent ? TELEPHONY_ERR_SUCCESS : TELEPHONY_ERR_FAIL);
        int32_t output = 99;
        EXPECT_EQ(module->QueryImsSwitch(0, output), 0);
        EXPECT_EQ(output, -1);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
        EXPECT_EQ(module->SaveImsSwitch(0, 26), TELEPHONY_ERR_LOCAL_PTR_NULL);
        release.set_value();
        EXPECT_EQ(running.get(), 0);
        EXPECT_EQ(provider->StoredImsValue(kRaw0).value, 23);
        EXPECT_EQ(provider->StoredImsValue(kRaw1).value, 25);
        EXPECT_EQ(provider->Writes().size(), independent ? 4u : 2u);
        EXPECT_EQ(f.host->accountsChanged, independent ? 2u : 0u);
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    }
}

TEST(AF2ImsPreference, ReplacementDuringQueryKeepsOriginalIccidAndOpenCall)
{
    using namespace OHOS::Telephony;
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    ASSERT_EQ(module->SaveImsSwitch(1, 23), 0);
    constexpr auto replacementIccid = "89000000000000000056";
    std::promise<void> entered;
    std::promise<void> release;
    auto enteredWait = entered.get_future();
    auto releaseWait = release.get_future();
    provider->readIms = [&](const auto &iccid) {
        const auto value = provider->StoredImsValue(iccid);
        if (iccid == kRaw1) {
            entered.set_value();
            releaseWait.wait();
        }
        return value;
    };
    auto running = std::async(std::launch::async, [&] {
        int32_t output = 99;
        const auto result = module->QueryImsSwitch(1, output);
        return std::make_pair(result, output);
    });
    enteredWait.wait();
    f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
    EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, replacementIccid));
    EXPECT_EQ(module->SaveImsSwitch(1, 24), 0);
    int32_t output = 99;
    EXPECT_EQ(module->QueryImsSwitch(1, output), 0);
    EXPECT_EQ(output, 24);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_EQ(module->QueryImsSwitch(1, output), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(output, 24);
    release.set_value();
    EXPECT_EQ(running.get(), std::make_pair(0, 23));
    EXPECT_EQ(provider->Reads(), (std::vector<std::string> {kRaw1, replacementIccid}));
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ImsPreference, UnknownKeepsTheExactOriginalWriteUntilEvidenceAfterStop)
{
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    provider->respond = [](const auto &) { return AccountMetadataOutcome {6, true, EffectKnowledge::Unknown}; };
    EXPECT_EQ(module->SaveImsSwitch(1, 29), 6);
    ASSERT_EQ(provider->Writes().size(), 1u);
    const auto original = provider->Writes()[0].operation;
    provider->SetImsValue(kRaw1, 29);
    int32_t output = 99;
    EXPECT_EQ(module->QueryImsSwitch(1, output), 0);
    EXPECT_EQ(output, 29);
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(module->SaveImsSwitch(1, 29), -1);
    f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
    EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
    EXPECT_EQ(module->QueryImsSwitch(1, output), 0);
    EXPECT_EQ(output, -1);
    auto other = original;
    ++other.original.attempt;
    provider->evidence = AccountMetadataEvidence {other, {6, true, EffectKnowledge::ConfirmedApplied}};
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    provider->evidence = AccountMetadataEvidence {original, {6, true, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(module->ReconcileAccountMetadata(original));
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(provider->Writes().size(), 1u);
    EXPECT_EQ(module->ReadAccount({41})->fields.imsPreference, 0u);
    EXPECT_EQ(module->ReadAccount({56})->fields.imsPreference, 0u);
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ImsPreference, ConfirmedWriteAfterReplacementKeepsRawReturnWithoutAdvancingVersions)
{
    auto provider = std::make_shared<MetadataProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), {}, provider);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    provider->respond = [&](const auto &write) {
        EXPECT_EQ(write.operation.subscription.legacySimId, 41);
        EXPECT_EQ(write.predicate, kRaw1);
        f.live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
        EXPECT_TRUE(module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
        return AccountMetadataOutcome {2, true, EffectKnowledge::ConfirmedApplied};
    };
    EXPECT_EQ(module->SaveImsSwitch(1, 31), 2);
    EXPECT_EQ(module->ReadAccount({41})->fields.imsPreference, 0u);
    EXPECT_EQ(module->ReadAccount({56})->fields.imsPreference, 0u);
    EXPECT_EQ(provider->StoredImsValue(kRaw1).value, 31);
    EXPECT_EQ(f.host->accountsChanged, 0u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

namespace {
class NumberHost final : public LegacyNumberHostPort {
public:
    std::vector<std::function<void()>> queue;
    std::set<uint64_t> retained;
    std::vector<std::pair<uint64_t, int32_t>> writes;
    std::vector<std::pair<uint64_t, ShowNumberRead>> reads;
    std::vector<std::string> events;
    ImpuNumberResult impu {0, {}};
    bool acceptingTasks {true};
    unsigned impuReads {};

    std::optional<CompletionId> RetainNumberCallback(LegacyCallbackRef callback) override
    {
        if (!retained.insert(callback.token).second) { return std::nullopt; }
        return CompletionId {callback.token};
    }
    void ReleaseNumberCallback(CompletionId callback) override { EXPECT_EQ(retained.erase(callback.value), 1u); }
    bool PostNumberTask(std::function<void()> task) override
    {
        if (!acceptingTasks) { return false; }
        queue.push_back(std::move(task));
        return true;
    }
    void RunNext()
    {
        ASSERT_FALSE(queue.empty());
        auto task = std::move(queue.front());
        queue.erase(queue.begin());
        task();
    }
    ImpuNumberResult ReadImpuNumber(SlotId) override { ++impuReads; return impu; }
    void NotifyOriginalAccountChanged() override { events.push_back("notify"); }
    void TransferOriginalShowNumberResult(CompletionId completion, int32_t result) override
    {
        ReleaseNumberCallback(completion);
        writes.emplace_back(completion.value, result);
        events.push_back("callback");
    }
    void TransferOriginalNumberRead(CompletionId completion, const ShowNumberRead &result) override
    {
        ReleaseNumberCallback(completion);
        reads.emplace_back(completion.value, result);
    }
    void DeliverOriginalInt(const CompletionRoute &, int32_t) override { ADD_FAILURE(); }
};

class NumberDevice final : public MsisdnPlatformPort {
public:
    struct Command {
        MsisdnTransportTicket ticket;
        std::u16string tag;
        std::u16string number;
    };
    std::function<void(SlotId)> onRouteLookup;
    std::optional<ResourceKey> ResourceFor(SlotId slot) const override
    {
        if (onRouteLookup) { onRouteLookup(slot); }
        return ResourceKey {"primary-shared"};
    }
    std::optional<CardNumberView> ReadCachedNumber(const SessionRef &session) const override
    {
        auto it = std::find_if(records.begin(), records.end(), [&](const auto &view) { return view.session == session; });
        return it != records.end() ? *it : CardNumberView {session, {}, u"line"};
    }
    void AcceptWrittenNumber(const CardNumberView &view) override
    {
        auto it = std::find_if(records.begin(), records.end(), [&](const auto &item) { return item.session == view.session; });
        if (it == records.end()) { records.push_back(view); }
        else { *it = view; }
    }
    bool SubmitNumberWrite(const MsisdnTransportTicket &ticket, const std::u16string &alphaTag,
        const std::u16string &number) override
    {
        commands.push_back({ticket, alphaTag, number});
        if (onSubmit) { onSubmit(commands.back()); }
        return true;
    }
    MsisdnTransportEvidence Applied(size_t index)
    {
        const auto &command = commands.at(index);
        return {command.ticket, EffectKnowledge::ConfirmedApplied, true, true, true, true};
    }

    std::vector<CardNumberView> records;
    std::vector<Command> commands;
    std::function<void(const Command &)> onSubmit;
};

struct NumberFixture {
    std::shared_ptr<NumberHost> host = std::make_shared<NumberHost>();
    std::shared_ptr<PrimaryHost> permissions = std::make_shared<PrimaryHost>();
    std::shared_ptr<NumberDevice> device = std::make_shared<NumberDevice>();
    std::shared_ptr<MetadataProvider> db = std::make_shared<MetadataProvider>();
    LiveDiscoveryFixture live {permissions, PrimaryZeroCatalog(), {}, db, host, device};

    void DiscoverBoth()
    {
        live.store->produceStore = [](const AccountDiscoveryWritePlan &plan) {
            return Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
        };
        ASSERT_TRUE(live.module->ObserveVerifiedCard({{0}, "USIM"}, 1, kRaw0));
        ASSERT_TRUE(live.module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1));
    }
    void ReplaceSecond()
    {
        live.store->produceStore = [](const AccountDiscoveryWritePlan &) { return Stored({56}); };
        EXPECT_TRUE(live.module->ObserveVerifiedCard({{1}, "USIM"}, 2, "89000000000000000056"));
    }
};
} // namespace

TEST(AF2ShowNumber, ReplacementBeforeCardSendPermissionDoesNotIssueOriginalCommand)
{
    using namespace OHOS::Telephony;
    NumberFixture fixture;
    ASSERT_NO_FATAL_FAILURE(fixture.DiscoverBoth());
    auto &module = fixture.live.module;
    fixture.device->onRouteLookup = [&](SlotId slot) {
        EXPECT_EQ(slot, SlotId {1});
        fixture.ReplaceSecond();
    };

    ASSERT_EQ(module->SetShowNumber(1, u"111", {7}, {31}), TELEPHONY_ERR_SUCCESS);
    fixture.host->RunNext();
    if (!fixture.device->commands.empty()) {
        ADD_FAILURE() << "Revoked application issued an MSISDN command";
        EXPECT_TRUE(module->AcceptMsisdnEvidence(fixture.device->Applied(0)));
        EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
        return;
    }
    EXPECT_TRUE(fixture.db->Writes().empty());
    EXPECT_EQ(fixture.host->writes,
        (std::vector<std::pair<uint64_t, int32_t>> {{31, TELEPHONY_ERR_FAIL}}));
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    const auto replacement = module->FindBinding({1});
    ASSERT_TRUE(replacement);
    EXPECT_EQ(replacement->subscription, SubscriptionId {56});

    fixture.device->onRouteLookup = {};
    ASSERT_EQ(module->SetShowNumber(1, u"222", {7}, {32}), TELEPHONY_ERR_SUCCESS);
    fixture.host->RunNext();
    ASSERT_EQ(fixture.device->commands.size(), 1u);
    EXPECT_EQ(fixture.device->commands.front().ticket.session.bindingGeneration, 2u);
    EXPECT_TRUE(module->AcceptMsisdnEvidence(fixture.device->Applied(0)));
    ASSERT_EQ(fixture.db->Writes().size(), 1u);
    EXPECT_EQ(fixture.db->Writes().front().operation.subscription, SubscriptionId {56});
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, LiveCardNumberWriteUpdatesAccountAndCompletes)
{
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    ASSERT_EQ(module->SetShowNumber(1, u"123+456", {7}, {31}), 0);
    EXPECT_TRUE(f.device->commands.empty());
    EXPECT_TRUE(f.host->writes.empty());
    f.host->RunNext();
    ASSERT_EQ(f.device->commands.size(), 1u);
    const auto &command = f.device->commands.front();
    EXPECT_EQ(command.tag, u"line");
    EXPECT_EQ(command.number, u"123+456");
    EXPECT_EQ(command.ticket.session.application.slot, SlotId {1});
    EXPECT_EQ(command.ticket.step.request.owner, RequestOwner::ShowNumber);
    EXPECT_GT(command.ticket.fence, 0u);
    EXPECT_TRUE(f.db->Writes().empty());
    const auto reply = f.device->Applied(0);
    auto wrongFence = reply;
    ++wrongFence.ticket.fence;
    EXPECT_FALSE(module->AcceptMsisdnEvidence(wrongFence));
    auto wrongApplication = reply;
    ++wrongApplication.ticket.session.bindingGeneration;
    EXPECT_FALSE(module->AcceptMsisdnEvidence(wrongApplication));
    EXPECT_TRUE(f.host->writes.empty());
    EXPECT_TRUE(f.db->Writes().empty());
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::InFlight);
    EXPECT_TRUE(module->AcceptMsisdnEvidence(reply));
    EXPECT_FALSE(module->AcceptMsisdnEvidence(reply));
    ASSERT_EQ(f.db->Writes().size(), 1u);
    const auto write = f.db->Writes().front();
    EXPECT_EQ(write.operation.subscription.legacySimId, 41);
    EXPECT_EQ(write.selector, "sim_id");
    EXPECT_EQ(write.predicate, "41");
    EXPECT_EQ(write.field, "phone_number");
    EXPECT_EQ(std::get<std::u16string>(write.value), u"123+456");
    EXPECT_EQ(module->ReadAccount({41})->displayNumber, u"123+456");
    EXPECT_EQ(module->ReadAccount({41}, AccountReadView::Historical)->displayNumber, u"123+456");
    EXPECT_EQ(f.host->events, (std::vector<std::string> {"notify", "callback"}));
    EXPECT_EQ(f.host->writes, (std::vector<std::pair<uint64_t, int32_t>> {{31, 0}}));
    ASSERT_EQ(module->GetShowNumber(1, {7}, {32}), 0);
    f.host->RunNext();
    ASSERT_EQ(f.host->reads.size(), 1u);
    EXPECT_EQ(f.host->reads.front().second.number, u"123+456");
    EXPECT_EQ(f.host->reads.front().second.source, NumberSource::CardMsisdnCache);
    EXPECT_TRUE(f.host->queue.empty());
    EXPECT_TRUE(f.host->retained.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2ShowNumber, QueryUsesRealSourcesAndBackwritePinsOriginalAccount)
{
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    f.host->impu = {0, u"555123"};
    ASSERT_EQ(module->GetShowNumber(1, {7}, {31}), 0);
    f.host->RunNext();
    EXPECT_TRUE(f.db->Writes().empty());
    ASSERT_EQ(f.host->reads.size(), 1u);
    EXPECT_EQ(f.host->reads[0].second.source, NumberSource::ImsImpu);
    EXPECT_EQ(f.host->reads[0].second.number, u"555123");
    ASSERT_EQ(f.host->queue.size(), 1u);
    f.host->RunNext();
    ASSERT_EQ(f.db->Writes().size(), 1u);
    ASSERT_EQ(module->GetShowNumber(1, {7}, {32}), 0);
    f.host->RunNext();
    EXPECT_EQ(f.host->impuReads, 1u);
    ASSERT_EQ(f.host->reads.size(), 2u);
    EXPECT_EQ(f.host->reads[1].second.source, NumberSource::AccountDisplay);
    EXPECT_EQ(f.host->reads[1].second.number, u"555123");
    const auto old = f.live.store->lastPlan->observation.session;
    f.device->records.push_back({old, u"666", u"line"});
    ASSERT_EQ(module->GetShowNumber(1, {7}, {33}), 0);
    f.host->RunNext();
    ASSERT_EQ(f.host->reads.size(), 3u);
    EXPECT_EQ(f.host->reads[2].second.source, NumberSource::CardMsisdnCache);
    EXPECT_EQ(f.host->reads[2].second.number, u"666");
    ASSERT_EQ(f.host->queue.size(), 1u);
    f.ReplaceSecond();
    f.host->RunNext();
    EXPECT_EQ(f.db->Writes().size(), 1u);
    EXPECT_TRUE(module->ReadAccount({56})->displayNumber.empty());
    EXPECT_TRUE(f.device->commands.empty());
    EXPECT_TRUE(f.host->events.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, EarlyReplyAndStoreFailurePreserveSeparateEffects)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    f.db->respond = [](const auto &) {
        return AccountMetadataOutcome {-1, true, EffectKnowledge::ConfirmedNotApplied};
    };
    f.device->onSubmit = [&](const auto &) {
        EXPECT_TRUE(module->AcceptMsisdnEvidence(f.device->Applied(0)));
        EXPECT_TRUE(f.host->writes.empty());
        EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::InFlight);
    };
    ASSERT_EQ(module->SetShowNumber(1, u"555", {7}, {31}), 0);
    f.host->RunNext();
    ASSERT_EQ(f.host->writes.size(), 1u);
    EXPECT_EQ(f.host->writes[0].second, TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    EXPECT_EQ(f.host->events, std::vector<std::string> {"callback"});
    EXPECT_TRUE(module->ReadAccount({41})->displayNumber.empty());
    EXPECT_EQ(f.device->records.back().number, u"555");
    EXPECT_EQ(f.device->commands.size(), 1u);
    EXPECT_EQ(f.db->Writes().size(), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, TimeoutQuarantinesOriginalResourcesUntilCorrelatedProofAfterStop)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    ASSERT_EQ(module->SetShowNumber(1, u"777", {7}, {31}), 0);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    f.host->RunNext();
    ASSERT_EQ(f.device->commands.size(), 1u);
    const auto ticket = f.device->commands[0].ticket;
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    EXPECT_EQ(module->SetShowNumber(0, u"8", {7}, {32}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_TRUE(module->AcceptMsisdnEvidence({ticket, EffectKnowledge::Unknown, false, false, false}));
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Quarantined);
    EXPECT_EQ(f.host->writes, (std::vector<std::pair<uint64_t, int32_t>> {{31, TELEPHONY_ERR_FAIL}}));
    auto proof = f.device->Applied(0);
    ++proof.ticket.fence;
    EXPECT_FALSE(module->AcceptMsisdnEvidence(proof));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::DeviceEffectsUnresolved);
    --proof.ticket.fence;
    proof.currentStateVerified = false;
    EXPECT_FALSE(module->AcceptMsisdnEvidence(proof));
    proof.currentStateVerified = true;
    EXPECT_TRUE(module->AcceptMsisdnEvidence(proof));
    EXPECT_FALSE(module->AcceptMsisdnEvidence(proof));
    EXPECT_EQ(f.host->writes.size(), 1u);
    EXPECT_TRUE(f.db->Writes().empty());
    EXPECT_EQ(module->ReadPrimaryResourceState(), ResourceState::Available);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, CardFailureAndReplacementDoNotPersist)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    ASSERT_EQ(module->SetShowNumber(1, u"7", {7}, {31}), 0);
    f.host->RunNext();
    ASSERT_EQ(f.device->commands.size(), 1u);
    ASSERT_TRUE(module->AcceptMsisdnEvidence({f.device->commands[0].ticket,
        EffectKnowledge::ConfirmedNotApplied, true, true, false}));
    ASSERT_EQ(module->SetShowNumber(1, u"8", {7}, {32}), 0);
    f.host->RunNext();
    ASSERT_EQ(f.device->commands.size(), 2u);
    f.ReplaceSecond();
    EXPECT_TRUE(module->AcceptMsisdnEvidence(f.device->Applied(1)));
    EXPECT_TRUE(f.db->Writes().empty());
    EXPECT_TRUE(module->ReadAccount({56})->displayNumber.empty());
    EXPECT_TRUE(module->ReadAccount({41})->displayNumber.empty());
    EXPECT_EQ(f.host->writes, (std::vector<std::pair<uint64_t, int32_t>> {{31, TELEPHONY_ERR_FAIL},
        {32, TELEPHONY_ERR_FAIL}}));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, UnresolvedStoreRetainsNumberFieldAndUsesOriginalEvidence)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    f.db->respond = [](const auto &write) {
        return write.field == "phone_number" ? AccountMetadataOutcome {-1, true, EffectKnowledge::Unknown} :
            AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied};
    };
    ASSERT_EQ(module->SetShowNumber(1, u"333", {7}, {31}), 0);
    f.host->RunNext();
    ASSERT_TRUE(module->AcceptMsisdnEvidence(f.device->Applied(0)));
    ASSERT_EQ(f.db->Writes().size(), 1u);
    EXPECT_EQ(f.host->writes[0].second, TELEPHONY_ERR_DATABASE_WRITE_FAIL);
    const auto original = f.db->Writes()[0].operation;
    ASSERT_EQ(module->GetShowNumber(1, {7}, {32}), 0);
    f.host->RunNext();
    EXPECT_EQ(f.host->reads[0].second.number, u"333");
    f.host->RunNext();
    EXPECT_EQ(f.db->Writes().size(), 1u);
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(module->SetShowName(1, u"disjoint", {7}), 0);
    EXPECT_EQ(f.db->Writes().size(), 2u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::BusinessEffectsUnresolved);
    f.db->evidence = AccountMetadataEvidence {original, {0, true, EffectKnowledge::ConfirmedApplied}};
    EXPECT_TRUE(module->ReconcileAccountMetadata(original));
    EXPECT_FALSE(module->ReconcileAccountMetadata(original));
    EXPECT_EQ(f.host->writes.size(), 1u);
    EXPECT_TRUE(module->ReadAccount({41})->displayNumber.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, QueueAdmissionAndExecutionPointUseLegacyOrder)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    f.permissions->system = false;
    EXPECT_EQ(module->SetShowNumber(-1, u"111", {7}, {0}), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
    f.permissions->system = true;
    f.permissions->permitted = false;
    EXPECT_EQ(module->SetShowNumber(-1, u"111", {7}, {0}), TELEPHONY_ERR_PERMISSION_ERR);
    f.permissions->permitted = true;
    f.host->acceptingTasks = false;
    EXPECT_EQ(module->SetShowNumber(1, u"111", {7}, {31}), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_TRUE(f.host->retained.empty());
    f.host->acceptingTasks = true;
    ASSERT_EQ(module->SetShowNumber(1, u"111", {7}, {31}), 0);
    f.ReplaceSecond();
    f.host->RunNext();
    ASSERT_EQ(f.device->commands.size(), 1u);
    EXPECT_EQ(f.device->commands[0].ticket.session.bindingGeneration, 2u);
    EXPECT_TRUE(module->AcceptMsisdnEvidence(f.device->Applied(0)));
    ASSERT_EQ(f.db->Writes().size(), 1u);
    EXPECT_EQ(f.db->Writes()[0].operation.subscription.legacySimId, 56);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, ProviderAccountFallbackAfterOriginalCardFailure)
{
    using namespace OHOS::Telephony;
    NumberFixture f;
    f.live.store->rows.emplace(kRaw1, AccountStorageRow {{41}, kRaw1, SlotId {1}, {}, SimGuard::kStoredDisplayNumber, true, {}, {}});
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    SimGuard::FailedShowNumberResult observed {};
    ASSERT_NO_FATAL_FAILURE(SimGuard::ExerciseFailedShowNumber(
        [&](int slot, const auto &number, int &result) {
            ASSERT_EQ(module->SetShowNumber(slot, number, {7}, {31}), 0);
            f.host->RunNext();
            ASSERT_EQ(f.device->commands.size(), 1u);
            ASSERT_TRUE(module->AcceptMsisdnEvidence({f.device->commands[0].ticket,
                EffectKnowledge::ConfirmedNotApplied, true, true, false}));
            ASSERT_EQ(f.host->writes.size(), 1u);
            result = f.host->writes[0].second;
            EXPECT_TRUE(f.db->Writes().empty());
        },
        [&](int slot, auto &number, int &result) {
            ASSERT_EQ(module->GetShowNumber(slot, {7}, {32}), 0);
            f.host->RunNext();
            ASSERT_EQ(f.host->reads.size(), 1u);
            const auto &read = f.host->reads[0].second;
            EXPECT_EQ(read.source, NumberSource::AccountDisplay);
            number = read.number;
            result = read.legacyResult;
        }, [&] { return f.device->commands.size(); }, observed));
    RecordProperty("show_number_observation", SimGuard::Primary::Json({
        {"cachedNumber", observed.number == SimGuard::kStoredDisplayNumber ?
            SimGuard::kStoredPhoneNumber : "unexpected"},
        {"readResult", std::to_string(observed.readResult)},
        {"failed", observed.writeResult == TELEPHONY_ERR_FAIL ? "true" : "false"},
        {"commands", std::to_string(observed.commands)}
    }));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2ShowNumber, BackwriteRejectsStaleNumberRevisionOnSameBinding)
{
    NumberFixture f;
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    const auto session = f.live.store->lastPlan->observation.session;
    const auto binding = module->FindBinding({1});
    ASSERT_TRUE(binding);
    f.device->records.push_back({session, u"111", u"line"});
    ASSERT_EQ(module->GetShowNumber(1, {7}, {31}), 0);
    ASSERT_EQ(module->GetShowNumber(1, {7}, {32}), 0);
    f.host->RunNext();
    f.device->records.front().number = u"222";
    f.host->RunNext();
    ASSERT_EQ(f.host->reads.size(), 2u);
    EXPECT_EQ(f.host->reads[0].second.number, u"111");
    EXPECT_EQ(f.host->reads[1].second.number, u"222");
    ASSERT_EQ(f.host->queue.size(), 2u);
    ASSERT_TRUE(f.db->Writes().empty());

    f.host->RunNext();
    f.host->RunNext();
    ASSERT_EQ(f.db->Writes().size(), 1u);
    EXPECT_EQ(std::get<std::u16string>(f.db->Writes().front().value), u"111");
    const auto current = module->FindBinding({1});
    ASSERT_TRUE(current);
    EXPECT_EQ(current->bindingRevision, binding->bindingRevision);
    EXPECT_EQ(module->ReadAccount({41})->displayNumber, u"111");
    EXPECT_EQ(module->ReadAccount({41}, AccountReadView::Historical)->displayNumber, u"111");
    EXPECT_TRUE(f.host->events.empty());
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

namespace {
AccountStorageRow ReloadRow(SubscriptionId id, const std::string &iccid, int32_t position, std::u16string name)
{
    return {id, iccid, SlotId {position}, std::move(name), u"100", true, 2, 0};
}

AccountLoadResult ReloadImage(const AccountLoadRequest &request, uint64_t position,
    std::vector<AccountStorageRow> rows, std::vector<StoredAccountRoleFragment> roles,
    uint8_t fields = ReloadAllFields, bool membership = true, bool roleColumns = true)
{
    return {request, 0, true, true, std::move(rows), std::move(roles),
        AccountLoadImage {request, {8, position}, fields, membership, roleColumns}};
}
} // namespace

namespace {
void CheckAccountReload(bool sparse)
{
    using namespace SimGuard;
    LiveDiscoveryFixture f;
    std::u16string currentName;
    std::u16string historicalName;
    bool reverse = false;
    bool includeHistorical = true;
    const int currentId = sparse ? kSparseCurrentAccount : kCurrentAccount;
    const int historicalId = sparse ? kSparseHistoricalAccount : kHistoricalAccount;
    uint64_t readPosition = 0;
    f.store->load = [&](const AccountLoadRequest &request) {
        std::vector<AccountStorageRow> rows {
            ReloadRow({currentId}, kCurrentReloadIccid, 1, currentName)};
        std::vector<StoredAccountRoleFragment> roles {{{currentId}, false, false, false, false}};
        if (request.view == AccountReadView::Historical && includeHistorical) {
            auto older = ReloadRow({historicalId}, kHistoricalReloadIccid, -1, historicalName);
            older.active = false;
            rows.push_back(older);
            roles.push_back({{historicalId}, false, false, false, false});
        }
        if (reverse) {
            std::reverse(rows.begin(), rows.end());
            std::reverse(roles.begin(), roles.end());
        }
        return ReloadImage(request, ++readPosition, std::move(rows), std::move(roles));
    };
    const auto viewFor = [](ReloadView view) {
        return view == ReloadView::Current ? AccountReadView::Current : AccountReadView::Historical;
    };
    const auto setStorage = [&](const char16_t *current, const char16_t *historical) {
        currentName = current;
        historicalName = historical;
    };
    const auto reload = [&](ReloadView view) {
        return f.module->ReloadAccounts(viewFor(view)).accounts.acceptedAttempt;
    };
    const auto readName = [&](ReloadView view, int id) {
        const auto account = f.module->ReadAccount({id}, viewFor(view));
        return account ? account->displayName : std::u16string{};
    };
    if (sparse) {
        ExerciseSparseAccountReload(
            [&](const char16_t *current, const char16_t *historical, bool order, bool include) {
                reverse = order;
                includeHistorical = include;
                setStorage(current, historical);
            }, reload, readName, true);
    } else {
        ExerciseFreshAccountReload(setStorage, reload, readName);
    }
    EXPECT_FALSE(f.module->FindBinding({1}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}
} // namespace

TEST(AF2Reload, FreshAccountReloadKeepsViewsIndependent)
{
    CheckAccountReload(false);
}

TEST(AF2Reload, SparseAccountReloadKeepsPersistedIdentity)
{
    CheckAccountReload(true);
}

TEST(AF2Reload, LiveAccountsAndRolesSurviveStaleReload)
{
    using namespace OHOS::Telephony;
    const auto metadata = std::make_shared<MetadataProvider>();
    const auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider, metadata);
    f.live.input.radio->primaryEndpoint = {0};
    auto &module = f.live.module;
    auto first = ReloadRow({40}, kRaw0, 0, u"Stored A");
    auto second = ReloadRow({41}, kRaw1, 1, u"Stored B");
    uint64_t visibleThrough = 5;
    std::vector<StoredAccountRoleFragment> roles {{{40}, false, true, false, true},
        {{41}, true, false, true, false}};
    f.live.store->load = [&](const auto &request) { return ReloadImage(request, visibleThrough, {first, second}, roles); };
    for (const auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        const auto loaded = module->ReloadAccounts(view);
        EXPECT_TRUE(loaded.admitted);
        EXPECT_TRUE(loaded.accounts.acceptedAttempt);
        EXPECT_EQ(loaded.roles, RoleReloadDisposition::Accepted);
        EXPECT_EQ(module->ReadAccount({40}, view)->displayName, u"Stored A");
    }
    EXPECT_FALSE(module->FindBinding({0}));
    EXPECT_FALSE(module->FindBinding({1}));
    f.live.store->rows.emplace(kRaw0, first);
    f.live.store->rows.emplace(kRaw1, second);
    f.live.store->produceStore = [](const AccountDiscoveryWritePlan &plan) {
        auto stored = Stored({plan.accountFields.legacyPosition.legacySlotId == 0 ? 40 : 41});
        stored.position = StorePosition {8, 7};
        return stored;
    };
    const auto sessionA = module->ObserveVerifiedCard({{0}, "USIM"}, 1, kRaw0);
    const auto sessionB = module->ObserveVerifiedCard({{1}, "USIM"}, 1, kRaw1);
    ASSERT_TRUE(sessionA && sessionB);
    const auto binding = module->FindBinding({0});
    ASSERT_TRUE(binding);
    metadata->respond = [](const auto &) {
        return AccountMetadataOutcome {0, true, EffectKnowledge::ConfirmedApplied, StorePosition {8, 11}};
    };
    unsigned roleReadsDuringWrite = 0;
    provider->control = [&](const auto &operation) {
        const auto during = module->ReloadAccounts(AccountReadView::Current);
        EXPECT_TRUE(during.accounts.roleMembershipEligible);
        EXPECT_EQ(during.roles, RoleReloadDisposition::Rejected);
        ++roleReadsDuringWrite;
        const uint64_t committedAt = operation.role == DefaultRole::Voice ? 12 : 13;
        return RoleProviderOutcome {0, true, RoleProviderEffect::Applied, StorePosition {8, committedAt}};
    };
    ASSERT_EQ(module->SetShowName(0, u"New A", {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetPrimarySlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultCellularDataSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    visibleThrough = 8;
    ASSERT_EQ(module->SetDefaultVoiceSlotId(0, {7}), TELEPHONY_ERR_SUCCESS);
    ASSERT_EQ(module->SetDefaultSmsSlotId(1, {7}), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(roleReadsDuringWrite, 2u);
    const auto assignment = module->ReadAssignment();
    const auto parameters = f.live.input.parameters->values;
    ASSERT_EQ(assignment.lastPrimary, SlotId {0});
    ASSERT_EQ(assignment.defaultData, SubscriptionId {41});

    visibleThrough = 10;
    first.number = u"200";
    for (const auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        const auto stale = module->ReloadAccounts(view);
        EXPECT_TRUE(stale.accounts.acceptedAttempt);
        EXPECT_NE(stale.accounts.conflictedFields, 0u);
        ASSERT_EQ(stale.accounts.rows.count({40}), 1u);
        EXPECT_EQ(stale.accounts.rows.at({40}).uncovered & ReloadName, ReloadName);
        EXPECT_EQ(stale.accounts.rows.at({40}).accepted & ReloadNumber, ReloadNumber);
        EXPECT_EQ(stale.roles, RoleReloadDisposition::InsufficientCoverage);
        EXPECT_EQ(module->ReadAccount({40}, view)->displayName, u"New A");
        EXPECT_EQ(module->ReadAccount({40}, view)->displayNumber, u"200");
    }
    EXPECT_TRUE(module->ReadStoredAccountRoles({40}, AccountReadView::Current)->voice);
    visibleThrough = 14;
    first.name = u"Later A";
    roles[0].voice = true;
    roles[0].sms = false;
    roles[1].voice = false;
    roles[1].sms = true;
    for (const auto view : {AccountReadView::Current, AccountReadView::Historical}) {
        const auto fresh = module->ReloadAccounts(view);
        EXPECT_EQ(fresh.roles, RoleReloadDisposition::Accepted);
        EXPECT_EQ(module->ReadAccount({40}, view)->displayName, u"Later A");
        EXPECT_TRUE(module->ReadStoredAccountRoles({40}, view)->voice);
        EXPECT_TRUE(module->ReadStoredAccountRoles({41}, view)->sms);
    }
    visibleThrough = 13; // Covers the setters but not the already accepted later image.
    first.name = u"New A";
    EXPECT_EQ(module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Rejected);
    EXPECT_EQ(module->ReadAccount({40})->displayName, u"Later A");
    EXPECT_EQ(module->FindBinding({0})->bindingRevision, binding->bindingRevision);
    EXPECT_EQ(module->FindBinding({0})->subscription, binding->subscription);
    EXPECT_EQ(module->ReadAssignment().lastPrimary, assignment.lastPrimary);
    EXPECT_EQ(module->ReadAssignment().defaultData, assignment.defaultData);
    EXPECT_EQ(f.live.input.parameters->values, parameters);
    EXPECT_TRUE(module->InvalidateVerifiedCard(*sessionA));
    EXPECT_TRUE(module->InvalidateVerifiedCard(*sessionB));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
    const std::weak_ptr<SimModule> weak = module;
    module.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(AF2Reload, IndependentViewsValidateCompleteMembershipAndExactRoleIdentity)
{
    LiveDiscoveryFixture f;
    const auto row = ReloadRow({40}, kRaw0, 0, u"Account");
    const std::vector<StoredAccountRoleFragment> roles {{{40}, true, true, true, true}};
    f.store->load = [&](const auto &request) {
        if (request.view == AccountReadView::Current) {
            return AccountLoadResult {request, -2, false, true, {row}, roles};
        }
        return ReloadImage(request, 1, {row}, roles);
    };
    const auto current = f.module->ReloadAccounts(AccountReadView::Current);
    EXPECT_EQ(current.legacyResult, -2);
    EXPECT_TRUE(current.legacyReturn);
    EXPECT_FALSE(current.accounts.acceptedAttempt);
    EXPECT_FALSE(f.module->ReadAccount({40}));
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Historical).roles, RoleReloadDisposition::Accepted);
    ASSERT_TRUE(f.module->ReadAccount({40}, AccountReadView::Historical));
    f.store->load = [&](const auto &request) { return ReloadImage(request, 2, {row}, roles); };
    ASSERT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Accepted);
    f.store->load = [&](const auto &request) {
        return ReloadImage(request, 3, {row}, {{{41}, false, false, false, false}});
    };
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Rejected);
    EXPECT_TRUE(f.module->ReadStoredAccountRoles({40}, AccountReadView::Current)->voice);
    EXPECT_FALSE(f.module->ReadStoredAccountRoles({41}, AccountReadView::Current));
    f.store->load = [&](const auto &request) { return ReloadImage(request, 4, {row}, roles, ReloadAllFields, true, false); };
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::InsufficientCoverage);
    f.store->load = [&](const auto &request) {
        auto image = ReloadImage(request, 5, {row}, roles);
        image.image->sample.view = AccountReadView::Historical;
        return image;
    };
    EXPECT_FALSE(f.module->ReloadAccounts(AccountReadView::Current).accounts.acceptedAttempt);
    EXPECT_TRUE(f.module->ReadAccount({40}));
    EXPECT_FALSE(f.module->FindBinding({0}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Reload, AbsentMembershipRequiresCoverageAndTombstoneRejectsReappearance)
{
    LiveDiscoveryFixture f;
    const auto row = ReloadRow({40}, kRaw0, 0, u"Original");
    const std::vector<StoredAccountRoleFragment> roles {{{40}, true, true, true, true}};
    uint64_t visibleThrough = 10;
    bool present = true;
    f.store->load = [&](const auto &request) {
        return present ? ReloadImage(request, visibleThrough, {row}, roles) :
            ReloadImage(request, visibleThrough, {}, {});
    };
    ASSERT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Accepted);
    ASSERT_EQ(f.module->ReloadAccounts(AccountReadView::Historical).roles, RoleReloadDisposition::Accepted);
    present = false;
    visibleThrough = 9;
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Rejected);
    ASSERT_TRUE(f.module->ReadAccount({40}));
    const auto original = f.module->ReadAccount({40})->fields;
    visibleThrough = 11;
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Accepted);
    EXPECT_FALSE(f.module->ReadAccount({40}));
    EXPECT_FALSE(f.module->ReadStoredAccountRoles({40}, AccountReadView::Current));
    EXPECT_TRUE(f.module->ReadAccount({40}, AccountReadView::Historical));
    present = true;
    visibleThrough = 10;
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Rejected);
    EXPECT_FALSE(f.module->ReadAccount({40}));
    visibleThrough = 12;
    EXPECT_EQ(f.module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Accepted);
    ASSERT_TRUE(f.module->ReadAccount({40}));
    EXPECT_GT(f.module->ReadAccount({40})->fields.number, original.number);
    EXPECT_FALSE(f.module->FindBinding({0}));
    EXPECT_EQ(f.module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Reload, LaterAttemptAndStopPreventAnInFlightReadFromPublishing)
{
    LiveDiscoveryFixture f;
    auto &module = f.module;
    std::promise<void> entered, release;
    auto ready = entered.get_future();
    auto resume = release.get_future().share();
    f.store->load = [&](const auto &request) {
        if (request.loadAttempt == 1) { entered.set_value(); resume.wait(); }
        const auto name = request.loadAttempt == 1 ? u"Old" : u"New";
        return ReloadImage(request, request.loadAttempt, {ReloadRow({40}, kRaw0, 0, name)},
            {{{40}, true, false, false, true}});
    };
    auto old = std::async(std::launch::async, [&] { return module->ReloadAccounts(AccountReadView::Current); });
    ready.wait();
    EXPECT_EQ(module->ReloadAccounts(AccountReadView::Current).roles, RoleReloadDisposition::Accepted);
    release.set_value();
    EXPECT_FALSE(old.get().accounts.acceptedAttempt);
    ASSERT_EQ(module->ReadAccount({40})->displayName, u"New");

    std::promise<void> enteredStop, releaseStop;
    auto stopReady = enteredStop.get_future();
    auto stopResume = releaseStop.get_future().share();
    f.store->load = [&](const auto &request) {
        enteredStop.set_value();
        stopResume.wait();
        return ReloadImage(request, 12, {}, {});
    };
    auto inFlight = std::async(std::launch::async, [&] { return module->ReloadAccounts(AccountReadView::Current); });
    stopReady.wait();
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::CallbacksStillInFlight);
    EXPECT_FALSE(module->ReloadAccounts(AccountReadView::Historical).admitted);
    releaseStop.set_value();
    EXPECT_EQ(inFlight.get().roles, RoleReloadDisposition::Revoked);
    EXPECT_TRUE(module->ReadAccount({40}));
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}

TEST(AF2Reload, RoleReceiptCanBeReadAgainWithoutReapplying)
{
    AccountRoleProtocol owner;
    const RoleReloadDelivery batch {{1, 2}, 1, 0, false, StorePosition {8, 4},
        {{{40}, false, true, false, true}}};
    owner.AcceptReloadSet(batch);
    ASSERT_EQ(batch.receipt.load(), RoleReloadDisposition::Accepted);
    const auto revision = owner.Revision(false);
    owner.AcceptReloadSet(batch);
    EXPECT_EQ(batch.receipt.load(), RoleReloadDisposition::Accepted);
    EXPECT_EQ(owner.Revision(false), revision);

    const RoleReloadDelivery revoked {{1, 2}, 2, revision, false, StorePosition {8, 5}, {}};
    revoked.Revoke();
    owner.AcceptReloadSet(revoked);
    EXPECT_EQ(revoked.receipt.load(), RoleReloadDisposition::Revoked);
    EXPECT_EQ(owner.Revision(false), revision);
    EXPECT_TRUE(owner.Projection(false).at({40}).sms);
}

TEST(AF2Reload, ReadWhileMetadataWriteIsUnresolved)
{
    const auto metadata = std::make_shared<MetadataProvider>();
    const auto provider = std::make_shared<RoleProvider>();
    PrimaryFixture f(PrimaryZeroCatalog(), provider, metadata);
    ASSERT_NO_FATAL_FAILURE(f.DiscoverBoth());
    auto &module = f.live.module;
    auto first = ReloadRow({40}, kRaw0, 0, u"Stored");
    auto second = ReloadRow({41}, kRaw1, 1, u"Other");
    f.live.store->load = [&](const auto &request) {
        // The discovery port provides no comparable membership evidence in this fixture.
        return ReloadImage(request, 9, {first, second},
            {{{40}, true, false, false, false}, {{41}, false, true, false, false}});
    };
    metadata->respond = [](const auto &) {
        return AccountMetadataOutcome {0, true, EffectKnowledge::Unknown};
    };
    const auto before = module->ReadAccount({40})->displayName;
    (void)module->SetShowName(0, u"Unconfirmed", {7});
    ASSERT_EQ(metadata->Writes().size(), 1u);
    const auto operation = metadata->Writes().front().operation;
    const auto read = module->ReloadAccounts(AccountReadView::Current);
    EXPECT_TRUE(read.accounts.acceptedAttempt);
    ASSERT_EQ(read.accounts.rows.count({40}), 1u);
    EXPECT_EQ(read.accounts.rows.at({40}).unresolved & ReloadName, ReloadName);
    EXPECT_EQ(module->ReadAccount({40})->displayName, before);
    EXPECT_EQ(module->ReadAccount({40})->displayNumber, u"100");

    metadata->evidence = AccountMetadataEvidence {operation,
        {0, true, EffectKnowledge::ConfirmedApplied, StorePosition {8, 10}}};
    EXPECT_TRUE(module->ReconcileAccountMetadata(operation));
    EXPECT_FALSE(module->ReconcileAccountMetadata(operation));
    EXPECT_TRUE(module->ReloadAccounts(AccountReadView::Current).accounts.acceptedAttempt);
    EXPECT_EQ(module->ReadAccount({40})->displayName, before);
    EXPECT_EQ(metadata->Writes().size(), 1u);
    EXPECT_EQ(module->StopAcceptingAndDrain(), SimModule::DrainResult::Drained);
}
