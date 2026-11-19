#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "role_assignment/assignment_service.h"

using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr auto kRawIccid = "8900000000000000001";
constexpr auto kEncodedIccid = "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";
// Component-only sentinels used to prove exact Store error propagation; not Legacy compatibility codes.
constexpr auto kUserWriteFailure = 41;
constexpr auto kPrimaryWriteFailure = 42;
constexpr auto kDataWriteFailure = 43;

struct Timeline {
    std::vector<std::string> events;
};

class RecordingStore final : public AssignmentStore {
public:
    explicit RecordingStore(Timeline &timeline) : timeline_(timeline) {}
    int userResult {0};
    int primaryResult {0};
    int dataResult {0};
    bool userFinished {true};
    bool primaryFinished {true};
    bool dataFinished {true};
    bool userPreference {false};
    int primarySlot {0};
    int dataSlot {0};
    std::string primaryIccid {"previous"};
    std::string receivedRawIccid;
    unsigned userWrites {0};
    unsigned primaryWrites {0};
    unsigned dataWrites {0};
    unsigned roleWrites {0};
    std::function<void()> onFirstUserWrite;
    AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) override
    {
        ++userWrites;
        timeline_.events.emplace_back("write:user");
        if (userResult == 0) { userPreference = userSet; }
        if (onFirstUserWrite && userWrites == 1) { auto action = std::move(onFirstUserWrite); action(); }
        return {userResult, userFinished};
    }
    AssignmentParameterResult WritePrimaryParameters(SlotId slot, const std::string &rawIccid) override
    {
        ++primaryWrites;
        receivedRawIccid = rawIccid;
        timeline_.events.emplace_back("write:primary");
        if (primaryResult == 0) {
            primarySlot = slot.legacySlotId;
            // Persistence adapter boundary: SHA-256/raw encoding belongs here, not AssignmentService.
            primaryIccid = rawIccid == kRawIccid ? kEncodedIccid : "";
        }
        return {primaryResult, primaryFinished};
    }
    AssignmentParameterResult WriteDefaultData(SlotId slot) override
    {
        ++dataWrites;
        timeline_.events.emplace_back("write:data");
        if (dataResult == 0) { dataSlot = slot.legacySlotId; }
        return {dataResult, dataFinished};
    }
    RoleStoreResult WriteDefaultRole(const SetRoleCommand &) override
    { ++roleWrites; return {-1, false, true}; }
    PrimaryParameterSnapshot ReadPrimaryParameters() override
    { return {std::to_string(primarySlot), primaryIccid}; }
    AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &) override
    { return {-1, true}; }
private:
    Timeline &timeline_;
};

class RecordingOutput final : public AssignmentOutputPort {
public:
    explicit RecordingOutput(Timeline &timeline) : timeline_(timeline) {}
    std::vector<std::string> notifications;
    void PublishPrimaryPhase(bool, bool) override { notifications.emplace_back("phase"); }
    void PublishMainCardChanged(SlotId slot) override
    {
        timeline_.events.emplace_back("notify:primary");
        notifications.emplace_back("primary:" + std::to_string(slot.legacySlotId));
    }
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) override
    {
        timeline_.events.emplace_back("notify:data");
        notifications.emplace_back("data:" + std::to_string(slot.legacySlotId) + ":" +
            std::to_string(subscription.legacySimId));
    }
private:
    Timeline &timeline_;
};

class ControlledTopology final : public PrimaryTopologyPort {
public:
    bool present {true};
    bool active {true};
    BindingRef binding {{1}, {1}, 4};
    std::string rawIccid {kRawIccid};
    std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
    {
        if (!present || !(slot == SlotId {1})) { return std::nullopt; }
        return PrimaryTargetFacts {binding, rawIccid, active};
    }
    RequestId BeginRouteMapping(const StepRef &, SlotTarget) override
    { ADD_FAILURE() << "C0 must not route-map"; return {}; }
};

class ControlledRadio final : public RadioExecutionPort {
public:
    bool known {true};
    bool stale {false};
    int modem {0};
    unsigned reads {0};
    unsigned switches {0};
    unsigned retries {0};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    {
        ++const_cast<ControlledRadio *>(this)->reads;
        return {slot, known ? std::optional<int32_t>{modem} : std::nullopt, false, false,
            stale ? 0u : 9u};
    }
    void SwitchPrimaryRadio(const RadioSwitchRequest &) override { ++switches; }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++switches; }
};

class RecordingCompletion final : public LegacyIntCompletionPort {
public:
    explicit RecordingCompletion(Timeline &timeline) : timeline_(timeline) {}
    std::vector<int32_t> results;
    void CompleteLegacyInt(const CompletionPlan &, int32_t legacyResult) override
    {
        timeline_.events.emplace_back("complete");
        results.push_back(legacyResult);
    }
private:
    Timeline &timeline_;
};

const SlotCatalog &Catalog()
{
    static const SlotCatalog catalog({
        {SlotId {0}, EndpointKind::Physical, 1, RadioDomainId {"modem-1"}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2});
    return catalog;
}

SwitchPrimaryCommand C0Command(BindingRef binding = {{1}, {1}, 4})
{
    return {SlotTarget {SlotId {1}}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {17}}, false}, std::nullopt, binding};
}

std::string JsonInt(int value) { return std::to_string(value); }
std::string Observation(const RecordingStore &store, const RecordingOutput &output,
    const ControlledRadio &radio, const RecordingCompletion &completion, const AssignmentView &view)
{
    const int primaryRole = view.lastPrimary ? view.lastPrimary->legacySlotId : store.primarySlot;
    const int dataRole = view.lastData ? view.lastData->legacySlotId : store.dataSlot;
    std::string notifications = "[";
    for (size_t i = 0; i < output.notifications.size(); ++i) {
        const auto &notification = output.notifications[i];
        notifications += (i ? "," : "") + std::string(notification.compare(0, 7, "primary") == 0 ?
            "\"primary\"" : notification.compare(0, 4, "data") == 0 ? "\"data\"" : "\"phase\"");
    }
    notifications += "]";
    return std::string("{\"api_result\":") + JsonInt(completion.results.empty() ? -99 : completion.results.back()) +
        ",\"primary_role\":" + JsonInt(primaryRole) + ",\"default_data_role\":" + JsonInt(dataRole) +
        ",\"parameters\":{\"user_preference\":" + (store.userPreference ? "true" : "false") +
        ",\"primary_slot\":" + JsonInt(store.primarySlot) + ",\"primary_iccid\":\"" + store.primaryIccid +
        "\",\"default_data_slot\":" + JsonInt(store.dataSlot) + "},\"notifications\":" + notifications +
        ",\"device_switch_commands\":" + JsonInt(radio.switches) +
        ",\"retry_count\":" + JsonInt(radio.retries) + ",\"switching\":" +
        (view.primarySwitchInProgress ? "true" : "false") + "}";
}

struct Fixture {
    Timeline timeline;
    RecordingStore store {timeline};
    RecordingOutput output {timeline};
    ControlledTopology topology;
    ControlledRadio radio;
    RecordingCompletion completion {timeline};
    AssignmentService service {Catalog(), store, radio, output, topology, completion, RuntimeEpoch {8, 3}};
};
} // namespace

TEST(TargetPrimary, C0AssignmentServiceAlreadyPrimary)
{
    Fixture f;
    const RequestId request = f.service.StartPrimarySwitch(C0Command());
    const auto view = f.service.ReadLegacyAssignment();
    EXPECT_EQ(request.epoch, RuntimeEpoch ({8, 3}));
    EXPECT_EQ(request.owner, RequestOwner::Assignment);
    ASSERT_EQ(f.completion.results, (std::vector<int32_t> {0}));
    EXPECT_EQ(f.store.receivedRawIccid, kRawIccid);
    EXPECT_EQ(f.store.primaryIccid, kEncodedIccid);
    EXPECT_EQ(f.timeline.events, (std::vector<std::string> {
        "write:user", "write:primary", "notify:primary", "write:data", "notify:data", "complete"}));
    EXPECT_EQ(f.output.notifications, (std::vector<std::string> {"primary:1", "data:1:1"}));
    EXPECT_EQ(f.radio.reads, 1u);
    EXPECT_EQ(f.radio.switches, 0u);
    EXPECT_FALSE(view.primarySwitchInProgress);
    ASSERT_TRUE(view.lastPrimary.has_value());
    ASSERT_TRUE(view.lastData.has_value());
    EXPECT_EQ(view.lastPrimary->legacySlotId, 1);
    EXPECT_EQ(view.lastData->legacySlotId, 1);
    RecordProperty("scenario_id", "SIM028-LEGACY-C0-DUAL-PRIMARY0");
    RecordProperty("primary_observation", Observation(f.store, f.output, f.radio, f.completion, view));
    RecordProperty("target_business_path", "AssignmentService/PlanPrimarySwitch/AssignmentAggregate");
}

TEST(TargetPrimary, C0RejectsMissingInactiveOrChangedTopologyFacts)
{
    Fixture missing;
    missing.topology.present = false;
    missing.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(missing.store.userWrites, 0u);
    ASSERT_EQ(missing.completion.results.size(), 1u);
    EXPECT_NE(missing.completion.results[0], 0);

    Fixture inactive;
    inactive.topology.active = false;
    inactive.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(inactive.store.userWrites, 0u);
    ASSERT_EQ(inactive.completion.results.size(), 1u);
    EXPECT_NE(inactive.completion.results[0], 0);

    Fixture changed;
    changed.topology.binding.bindingRevision = 3; // Identity revision no longer matches the C0 request.
    changed.service.StartPrimarySwitch(C0Command({{1}, {1}, 4}));
    EXPECT_EQ(changed.store.userWrites, 0u);
    EXPECT_TRUE(changed.output.notifications.empty());
    ASSERT_EQ(changed.completion.results.size(), 1u);
    EXPECT_NE(changed.completion.results[0], 0);
    EXPECT_EQ(changed.radio.switches, 0u);

    Fixture omitted;
    auto command = C0Command();
    command.expectedBinding.reset();
    omitted.service.StartPrimarySwitch(command);
    EXPECT_EQ(omitted.store.userWrites, 0u);
    ASSERT_EQ(omitted.completion.results.size(), 1u);
    EXPECT_NE(omitted.completion.results[0], 0);
}

TEST(TargetPrimary, C0RejectsUnknownStaleOrDifferentRadioObservation)
{
    for (int mode = 0; mode < 2; ++mode) {
        Fixture f;
        f.radio.known = mode != 0;
        f.radio.stale = mode == 1;
        f.radio.modem = 0;
        f.service.StartPrimarySwitch(C0Command());
        EXPECT_EQ(f.store.userWrites, 0u);
        EXPECT_TRUE(f.output.notifications.empty());
        EXPECT_EQ(f.radio.switches, 0u);
        ASSERT_EQ(f.completion.results.size(), 1u);
        EXPECT_NE(f.completion.results[0], 0);
    }
}

TEST(TargetPrimary, C0PreservesPartialEffectsWhenStoreWriteFails)
{
    Fixture preference;
    preference.store.userResult = kUserWriteFailure;
    preference.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(preference.completion.results, (std::vector<int32_t> {0}));
    EXPECT_FALSE(preference.store.userPreference);
    EXPECT_EQ(preference.store.primaryWrites, 1u);
    EXPECT_TRUE(preference.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_TRUE(preference.service.ReadLegacyAssignment().lastData.has_value());

    Fixture primary;
    primary.store.primaryResult = kPrimaryWriteFailure;
    primary.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(primary.completion.results, (std::vector<int32_t> {0}));
    EXPECT_TRUE(primary.store.userPreference);
    EXPECT_EQ(primary.store.primarySlot, 0); // completed system-parameter failure is ignored by Legacy C0
    EXPECT_EQ(primary.output.notifications, (std::vector<std::string> {"primary:1", "data:1:1"}));
    EXPECT_EQ(primary.store.dataWrites, 1u);
    EXPECT_TRUE(primary.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_TRUE(primary.service.ReadLegacyAssignment().lastData.has_value());

    Fixture data;
    data.store.dataResult = kDataWriteFailure;
    data.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(data.completion.results, (std::vector<int32_t> {0}));
    EXPECT_EQ(data.store.primarySlot, 1);
    EXPECT_EQ(data.store.primaryIccid, kEncodedIccid);
    EXPECT_EQ(data.output.notifications, (std::vector<std::string> {"primary:1", "data:1:1"}));
    EXPECT_EQ(data.store.dataSlot, 0); // Legacy in-memory role still advances after void parameter write.
    const auto view = data.service.ReadLegacyAssignment();
    ASSERT_TRUE(view.lastPrimary.has_value());
    EXPECT_TRUE(view.lastData.has_value());
}

TEST(TargetPrimary, C0RejectsConflictingRequestAndCanRestartAfterFailure)
{
    Fixture f;
    std::vector<int32_t> nestedResults;
    f.store.onFirstUserWrite = [&f, &nestedResults] {
        f.service.StartPrimarySwitch(C0Command());
        nestedResults = f.completion.results;
    };
    const RequestId first = f.service.StartPrimarySwitch(C0Command());
    ASSERT_EQ(f.completion.results.size(), 2u);
    EXPECT_NE(f.completion.results[0], 0); // Same-domain conflict cannot inherit outer success.
    EXPECT_EQ(f.completion.results[1], 0);
    EXPECT_EQ(first.value, 1u);
    EXPECT_EQ(f.store.primaryWrites, 1u);
    EXPECT_EQ(f.output.notifications, (std::vector<std::string> {"primary:1", "data:1:1"}));

    Fixture retry;
    retry.radio.known = false;
    retry.service.StartPrimarySwitch(C0Command());
    retry.radio.known = true;
    const RequestId second = retry.service.StartPrimarySwitch(C0Command());
    EXPECT_GT(second.value, 1u);
    ASSERT_EQ(retry.completion.results.size(), 2u);
    EXPECT_NE(retry.completion.results[0], 0);
    EXPECT_EQ(retry.completion.results[1], 0);
    EXPECT_EQ(retry.store.primarySlot, 1);
    EXPECT_EQ(retry.store.dataSlot, 1);
}

TEST(TargetPrimary, C0DifferentDomainsRejectNestedSharedRoleWrite)
{
    class BothTopology final : public PrimaryTopologyPort {
    public:
        std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
        {
            if (slot.legacySlotId > 1) { return std::nullopt; }
            return PrimaryTargetFacts {BindingRef {{slot.legacySlotId + 1}, slot, 4}, kRawIccid, true};
        }
        RequestId BeginRouteMapping(const StepRef &, SlotTarget) override { return {}; }
    };
    Timeline timeline;
    RecordingStore store {timeline};
    RecordingOutput output {timeline};
    ControlledRadio radio;
    RecordingCompletion completion {timeline};
    BothTopology topology;
    const SlotCatalog catalog({
        {SlotId {0}, EndpointKind::Physical, 0, RadioDomainId {"c0-0"}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"c0-1"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2});
    AssignmentService service(catalog, store, radio, output, topology, completion, RuntimeEpoch {8, 3});
    int nestedData {-99}; int nestedVoice {-99}; int nestedSms {-99};
    store.onFirstUserWrite = [&] {
        service.StartPrimarySwitch(C0Command(BindingRef {{2}, {1}, 4}));
        nestedData = service.SetDefaultRole({DefaultRole::Data, SubscriptionId {2}, SlotId {1}});
        nestedVoice = service.SetDefaultRole({DefaultRole::Voice, SubscriptionId {2}, SlotId {1}});
        nestedSms = service.SetDefaultRole({DefaultRole::Sms, SubscriptionId {2}, SlotId {1}});
    };
    service.StartPrimarySwitch({SlotTarget {SlotId {0}}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {16}}, false}, std::nullopt,
        BindingRef {{1}, {0}, 4}});
    EXPECT_EQ(completion.results, (std::vector<int32_t> {-1, 0}));
    EXPECT_EQ(nestedData, -1);
    EXPECT_EQ(nestedVoice, -1);
    EXPECT_EQ(nestedSms, -1);
    EXPECT_EQ(store.roleWrites, 2u);
    EXPECT_EQ(store.primarySlot, 0);
    EXPECT_EQ(store.dataSlot, 0);
    EXPECT_EQ(radio.switches, 0u);
    EXPECT_EQ(output.notifications, (std::vector<std::string> {"primary:0", "data:0:1"}));
}

TEST(TargetPrimary, C0KeepsUnfinishedWritesUncommittedAndQualified)
{
    Fixture preference;
    preference.store.userFinished = false;
    preference.service.StartPrimarySwitch(C0Command());
    EXPECT_TRUE(preference.completion.results.empty());
    EXPECT_TRUE(preference.store.userPreference);
    EXPECT_EQ(preference.store.primaryWrites, 0u);
    EXPECT_TRUE(preference.service.ReadLegacyAssignment().primarySwitchInProgress);
    preference.service.StartPrimarySwitch(C0Command());
    EXPECT_EQ(preference.completion.results, (std::vector<int32_t> {-1}));
    EXPECT_EQ(preference.store.userWrites, 1u);

    Fixture primary;
    primary.store.primaryFinished = false;
    primary.service.StartPrimarySwitch(C0Command());
    EXPECT_TRUE(primary.completion.results.empty());
    EXPECT_EQ(primary.store.primarySlot, 1);
    EXPECT_TRUE(primary.output.notifications.empty());
    EXPECT_FALSE(primary.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_TRUE(primary.service.ReadLegacyAssignment().primarySwitchInProgress);

    Fixture data;
    data.store.dataFinished = false;
    data.service.StartPrimarySwitch(C0Command());
    EXPECT_TRUE(data.completion.results.empty());
    EXPECT_EQ(data.output.notifications, (std::vector<std::string> {"primary:1"}));
    EXPECT_TRUE(data.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_FALSE(data.service.ReadLegacyAssignment().lastData.has_value());
    EXPECT_TRUE(data.service.ReadLegacyAssignment().primarySwitchInProgress);
}

TEST(TargetPrimary, C0OperationAdmissionRejectsOldEpochStepAndDuplicateCompletion)
{
    RuntimeOperationStore store({8, 3}, RequestOwner::Assignment);
    const auto plan = CompletionPlan {SynchronousCompletionRoute {SynchronousWaitRef {1}}, false};
    const RequestId request = store.Begin(SlotTarget {SlotId {1}}, plan);
    ASSERT_TRUE(store.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt));
    EXPECT_EQ(store.AcceptResult({request, 0, 0}, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::WrongStep);
    const StepRef oldEpoch {{request.value, {8, 2}, RequestOwner::Assignment}, 1, 0};
    EXPECT_EQ(store.AcceptResult(oldEpoch, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::UnknownRequest);
    EXPECT_EQ(store.AcceptResult({request, 1, 0}, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::Accepted);
    EXPECT_EQ(store.AcceptResult({request, 1, 0}, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::Duplicate);
    store.MarkTerminal(request, TerminalState::Succeeded);
    EXPECT_EQ(store.AcceptResult({request, 1, 0}, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::AlreadyTerminal);
    EXPECT_FALSE(store.TryMarkTerminal(request, TerminalState::Failed));
    EXPECT_TRUE(store.ClaimCompletion(request));
    EXPECT_FALSE(store.ClaimCompletion(request));
    EXPECT_FALSE(store.AcceptLateEvidence({request, 1, 0}, EffectKnowledge::ConfirmedNotApplied));
    const auto operation = store.Read(request);
    ASSERT_TRUE(operation.has_value());
    EXPECT_EQ(operation->terminal, TerminalState::Succeeded);
}

TEST(TargetPrimary, C0OperationConsumesNotSentAndKeepsLateEvidenceSeparate)
{
    RuntimeOperationStore store({8, 3}, RequestOwner::Assignment);
    const auto plan = CompletionPlan {SynchronousCompletionRoute {SynchronousWaitRef {2}}, false};
    const RequestId request = store.Begin(SlotTarget {SlotId {1}}, plan);
    ASSERT_TRUE(store.AdvanceStep({request, 0, 0}, 1, 0, std::nullopt));
    EXPECT_EQ(store.AcceptResult({request, 1, 0}, EffectKnowledge::NotSent),
        RuntimeOperationStore::ResultAdmission::Accepted);
    EXPECT_EQ(store.AcceptResult({request, 1, 0}, EffectKnowledge::ConfirmedApplied),
        RuntimeOperationStore::ResultAdmission::Duplicate);
    ASSERT_TRUE(store.AdvanceStep({request, 1, 0}, 2, 0, std::nullopt));
    EXPECT_EQ(store.AcceptResult({request, 2, 0}, EffectKnowledge::ConfirmedNotApplied),
        RuntimeOperationStore::ResultAdmission::Accepted);
    EXPECT_FALSE(store.ClaimCompletion(request));
    ASSERT_TRUE(store.TryMarkTerminal(request, TerminalState::NeedsReconciliation));
    EXPECT_FALSE(store.TryMarkTerminal(request, TerminalState::Succeeded));
    EXPECT_TRUE(store.AcceptLateEvidence({request, 2, 0}, EffectKnowledge::ConfirmedApplied));
    EXPECT_EQ(store.ReadLateEvidence(request), EffectKnowledge::ConfirmedApplied);
    EXPECT_TRUE(store.ClaimCompletion(request));
    EXPECT_FALSE(store.ClaimCompletion(request));
}

TEST(TargetPrimary, SharedWriteSetRejectsDifferentDomainButDisjointRoleAdvances)
{
    AssignmentAggregate assignment;
    const auto &slots = Catalog().Slots();
    ASSERT_TRUE(assignment.AdmitRoleWriteSet(10, kPrimaryRoleWriteSet));
    ASSERT_EQ(assignment.AdmitTransition(slots[0], 10), TransitionAdmission::Admitted);
    EXPECT_FALSE(assignment.AdmitRoleWriteSet(11, kDefaultDataWriteSet));
    EXPECT_TRUE(assignment.AdmitRoleWriteSet(12, kVoiceParameters | kSmsParameters));
    EXPECT_EQ(assignment.AdmitTransition(slots[1], 12), TransitionAdmission::Admitted);
    EXPECT_TRUE(assignment.ReleaseRoleWriteSet(12, kVoiceParameters | kSmsParameters));
    EXPECT_TRUE(assignment.ReleaseRoleWriteSet(10, kPrimaryRoleWriteSet));
    EXPECT_TRUE(assignment.AdmitRoleWriteSet(11, kDefaultDataWriteSet));
}
