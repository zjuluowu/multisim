#include <gtest/gtest.h>

#include <string>
#include <vector>
#include "role_assignment/assignment_service.h"

using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr auto kIccid = "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";

class RecordingStore final : public AssignmentStore {
public:
    explicit RecordingStore(std::vector<std::string> &timeline) : timeline_(timeline) {}
    std::vector<std::string> writes;
    std::vector<std::pair<std::string, SlotId>> ordered;
    AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) override
    {
        writes.emplace_back(userSet ? "user:true" : "user:false");
        timeline_.emplace_back("write:user");
        ordered.emplace_back("user", SlotId { -1 });
        return {0, true};
    }
    AssignmentParameterResult WritePrimaryParameters(SlotId slot, const std::string &iccid) override
    {
        writes.emplace_back("primary:" + std::to_string(slot.legacySlotId) + ":" + iccid);
        timeline_.emplace_back("write:primary");
        ordered.emplace_back("primary", slot);
        return {0, true};
    }
    AssignmentParameterResult WriteDefaultData(SlotId slot) override
    {
        writes.emplace_back("data:" + std::to_string(slot.legacySlotId));
        timeline_.emplace_back("write:data");
        ordered.emplace_back("data", slot);
        return {0, true};
    }
    RoleStoreResult WriteDefaultRole(const SetRoleCommand &) override { return {-1, false, true}; }
    PrimaryParameterSnapshot ReadPrimaryParameters() override { return {"0", "previous"}; }
    AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &) override
    { return {-1, true}; }
private:
    std::vector<std::string> &timeline_;
};

class RecordingOutput final : public AssignmentOutputPort {
public:
    explicit RecordingOutput(std::vector<std::string> &timeline) : timeline_(timeline) {}
    std::vector<std::string> notifications;
    void PublishPrimaryPhase(bool, bool) override { notifications.emplace_back("phase"); }
    void PublishMainCardChanged(SlotId slot) override
    {
        timeline_.emplace_back("notify:primary");
        notifications.emplace_back("primary:" + std::to_string(slot.legacySlotId));
    }
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) override
    {
        timeline_.emplace_back("notify:data");
        notifications.emplace_back("data:" + std::to_string(slot.legacySlotId) + ":" +
            std::to_string(subscription.legacySimId));
    }
private:
    std::vector<std::string> &timeline_;
};

class ControlledTopology final : public PrimaryTopologyPort {
public:
    std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
    {
        if (!(slot == SlotId {1})) { return std::nullopt; }
        return PrimaryTargetFacts {{SubscriptionId {1}, SlotId {1}, 4}, kIccid, true};
    }
    RequestId BeginRouteMapping(const StepRef &, SlotTarget) override
    { ADD_FAILURE() << "C0 must not route-map"; return {}; }
};

class ControlledRadio final : public RadioExecutionPort {
public:
    explicit ControlledRadio(bool known = true) : known_(known) {}
    unsigned reads {0};
    unsigned switches {0};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    {
        ++const_cast<ControlledRadio *>(this)->reads;
        return {slot, known_ ? std::optional<int32_t>{0} : std::nullopt, false, false, 9};
    }
    void SwitchPrimaryRadio(const RadioSwitchRequest &) override { ++switches; }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++switches; }
private:
    bool known_;
};

class RecordingCompletion final : public LegacyIntCompletionPort {
public:
    explicit RecordingCompletion(std::vector<std::string> &timeline) : timeline_(timeline) {}
    std::vector<int32_t> results;
    void CompleteLegacyInt(const CompletionPlan &, int32_t legacyResult) override
    {
        timeline_.emplace_back("complete");
        results.push_back(legacyResult);
    }
private:
    std::vector<std::string> &timeline_;
};

std::string Observation(const RecordingStore &store, const RecordingOutput &output,
    const ControlledRadio &radio, const RecordingCompletion &completion,
    const AssignmentView &view)
{
    return std::string("{\"api_result\":") + std::to_string(completion.results.front()) +
        ",\"primary_role\":1,\"default_data_role\":1,\"parameters\":{\"user_preference\":true,"
        "\"primary_slot\":1,\"primary_iccid\":\"" + store.writes[1].substr(10) +
        "\",\"default_data_slot\":1},\"notifications\":[\"primary\",\"data\"],"
        "\"device_switch_commands\":" + std::to_string(radio.switches) +
        ",\"retry_count\":0,\"switching\":" + (view.primarySwitchInProgress ? "true" : "false") +
        ",\"primary_projection\":" + std::to_string(view.lastPrimary->legacySlotId) +
        ",\"data_projection\":" + std::to_string(view.lastData->legacySlotId) + "}";
}
} // namespace

TEST(TargetPrimary, C0AssignmentServiceAlreadyPrimary)
{
    const SlotCatalog catalog({
        {SlotId {0}, EndpointKind::Physical, 1, RadioDomainId {"modem-1"}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2});
    std::vector<std::string> timeline;
    RecordingStore store(timeline);
    RecordingOutput output(timeline);
    ControlledTopology topology;
    ControlledRadio radio;
    RecordingCompletion completion(timeline);
    AssignmentService service(catalog, store, radio, output, topology, completion, RuntimeEpoch {8, 3});

    const RequestId request = service.StartPrimarySwitch({SlotTarget {SlotId {1}}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {17}}, false}, std::nullopt});
    const auto view = service.ReadLegacyAssignment();

    EXPECT_EQ(request.epoch, RuntimeEpoch ({8, 3}));
    EXPECT_EQ(request.owner, RequestOwner::Assignment);
    ASSERT_EQ(completion.results, (std::vector<int32_t> {0}));
    ASSERT_EQ(store.ordered.size(), 3u);
    EXPECT_EQ(store.ordered[0].first, "user");
    EXPECT_EQ(store.ordered[1].first, "primary");
    EXPECT_EQ(store.ordered[2].first, "data");
    ASSERT_EQ(output.notifications, (std::vector<std::string> {"primary:1", "data:1:1"}));
    EXPECT_EQ(timeline, (std::vector<std::string> {
        "write:user", "write:primary", "notify:primary", "write:data", "notify:data", "complete"}));
    EXPECT_EQ(radio.reads, 1u);
    EXPECT_EQ(radio.switches, 0u);
    EXPECT_FALSE(view.primarySwitchInProgress);
    ASSERT_TRUE(view.lastPrimary.has_value());
    ASSERT_TRUE(view.lastData.has_value());
    EXPECT_EQ(view.lastPrimary->legacySlotId, 1);
    EXPECT_EQ(view.lastData->legacySlotId, 1);
    RecordProperty("scenario_id", "SIM028-LEGACY-C0-DUAL-PRIMARY0");
    RecordProperty("primary_observation", Observation(store, output, radio, completion, view));
    RecordProperty("target_business_path", "AssignmentService/PlanPrimarySwitch/AssignmentAggregate");

    std::vector<std::string> unknownTimeline;
    RecordingStore unknownStore(unknownTimeline);
    RecordingOutput unknownOutput(unknownTimeline);
    ControlledRadio unknownRadio(false);
    RecordingCompletion unknownCompletion(unknownTimeline);
    AssignmentService unknownService(catalog, unknownStore, unknownRadio, unknownOutput, topology,
        unknownCompletion, RuntimeEpoch {8, 4});
    unknownService.StartPrimarySwitch({SlotTarget {SlotId {1}}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {18}}, false}, std::nullopt});
    EXPECT_EQ(unknownCompletion.results, (std::vector<int32_t> {-1}));
    EXPECT_TRUE(unknownStore.writes.empty());
    EXPECT_TRUE(unknownOutput.notifications.empty());
    EXPECT_EQ(unknownRadio.switches, 0u);
    EXPECT_EQ(unknownTimeline, (std::vector<std::string> {"complete"}));
}
