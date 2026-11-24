#include <gtest/gtest.h>

#include <future>
#include <map>
#include <optional>
#include <string>
#include <vector>
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
    AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &) override { return {-1, false, true}; }
    AccountLoadResult LoadAccounts(const AccountLoadRequest &r) override { return {r, -1, false, false, {}, {},}; }
    AccountIdentityMatch MatchAccountByIccid(const std::string &) override { return {-1, true, std::nullopt}; }
    AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &) override
    { return {-1, std::nullopt, false, true, EffectKnowledge::Rejected}; }
    AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &) override { return {-1, false, true}; }
    int32_t QueryImsPreferenceByIccid(const std::string &, int32_t &) override { return -1; }
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
    unsigned loads {0};
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
    AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &) override { return {-1,false,true}; }
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
    int32_t QueryImsPreferenceByIccid(const std::string &, int32_t &) override { return -1; }
};

class Parameters final : public AssignmentParameterPort {
public:
    std::map<std::string, std::string> values {{kPrimary, "0"}, {kData, "0"},
        {kIccid, "previous"}, {kPreference, "false"}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::function<AssignmentParameterResult(const std::string &)> writeResult;
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
        return {slot, 0, false, false, 9};
    }
    void SwitchPrimaryRadio(const RadioSwitchRequest &) override { ++switches; }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++switches; }
};
class Output final : public AssignmentOutputPort {
public:
    std::vector<std::string> events;
    void PublishPrimaryPhase(bool, bool) override {}
    void PublishMainCardChanged(SlotId) override { events.emplace_back("primary"); }
    void PublishDefaultDataChanged(SlotId, SubscriptionId) override { events.emplace_back("data"); }
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

TEST(TargetTopologyIntegration, T1RoleRevisionChangeRejectsOldDiscoveredFragment)
{
    DiscoveryFixture f;
    f.accountStore.beforeStoreReturn=[&](const AccountDiscoveryWritePlan &) {
        const auto revision=f.service.ReadRoleProjectionRevision(false);
        EXPECT_TRUE(f.service.AcceptReloadedRoleProjection(
            {{9},false,false,false,false},1,revision,false));
    };
    const auto acceptance=f.topology.DiscoverAccount(Discovery(1,1,kRaw1));
    EXPECT_FALSE(acceptance.bindingAccepted);
    EXPECT_FALSE(f.topology.FindBinding({1}).has_value());
    EXPECT_FALSE(f.topology.ReadAccount({41},AccountReadView::Current).has_value());
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
    unsigned commands {0};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    { return {slot, requiresDevice ? 1 : 0, requiresDevice, false, 7}; }
    RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &, uint64_t) override
    {
        ++commands;
        return {RadioSubmitDisposition::ConfirmedNotSent, std::nullopt};
    }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++commands; }
};

class ModuleDispatch final : public RetrySchedulerPort, public RadioOwnerDispatchPort {
public:
    bool defer {false};
    std::function<void()> pending;
    unsigned seals {0};
    unsigned cancels {0};
    std::optional<uint64_t> ScheduleRetry(const ScheduledRetryEvent &, uint64_t,
        const std::weak_ptr<ScheduledRetrySink> &) override { return std::nullopt; }
    bool CancelRetry(uint64_t) override { return false; }
    size_t CancelAllForOwner(RuntimeEpoch, RequestOwner) override { ++cancels; return 0; }
    void StopAcceptingAndCancel() override { ++seals; }
    RetrySchedulerAudit Audit() const override { return {}; }
    bool PostRadioOwnerTask(std::function<void()> task) override
    {
        if (defer) { pending = std::move(task); } else { task(); }
        return true;
    }
    bool RadioOwnerCallbacksDrained() const override { return !pending; }
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
    CoreSimModulePorts Ports()
    {
        // The adapter borrows the parameter port; keep the pair in one shared control block.
        struct ParameterAdapter {
            explicit ParameterAdapter(std::shared_ptr<Parameters> port) : parameters(std::move(port)), store(*parameters)
            {}
            std::shared_ptr<Parameters> parameters;
            AssignmentStoreAdapter store;
        };
        const auto adapter = std::make_shared<ParameterAdapter>(parameters);
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
    auto pending = std::move(input.dispatch->pending);
    ASSERT_TRUE(pending);
    pending();
    pending = {};
    input.dispatch->pending = {};
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
struct LiveDiscoveryFixture {
    ModuleInputs input;
    std::shared_ptr<ControlledDiscoveryStore> store = std::make_shared<ControlledDiscoveryStore>();
    CoreSimModuleHandle module;

    LiveDiscoveryFixture()
    {
        store->persistRows = true;
        input.accountStore = store;
        module = std::get<CoreSimModuleHandle>(CreateCoreSimModule(input.Ports(), input.catalog, {}));
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
    bool AcceptReloadedRoleProjectionSet(const std::vector<StoredAccountRoleFragment> &rows,
        uint64_t attempt, uint64_t expected, bool historical) override
    { return owner_.AcceptReloadedRoleProjectionSet(rows, attempt, expected, historical); }
    bool AcceptReloadedRoleProjection(const StoredAccountRoleFragment &row, uint64_t attempt,
        uint64_t expected, bool historical) override
    { return owner_.AcceptReloadedRoleProjection(row, attempt, expected, historical); }
private:
    AssignmentService &owner_;
};
} // namespace

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
