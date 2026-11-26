#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "role_assignment/assignment_service.h"
#include "role_assignment/assignment_store_adapter.h"

using namespace OHOS::Telephony::MultiSim;
namespace {
constexpr auto kRawIccid = "8900000000000000001";
constexpr auto kEncodedIccid = "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2";
constexpr auto kPrimary = "persist.telephony.MainSlotId";
constexpr auto kIccid = "persist.telephony.MainCard.Iccid";
constexpr auto kData = "persist.telephony.MainCellularDataSlotId";
constexpr auto kPreference = "persist.telephony.userPreferPrimarySlot";

struct ParameterCall { std::string key; std::string value; AssignmentParameterResult result; };
class ControlledParameters final : public AssignmentParameterPort {
public:
    std::map<std::string, std::string> values {{kPrimary, "0"}, {kIccid, "previous"},
        {kData, "0"}, {kPreference, "false"}};
    std::vector<ParameterCall> calls;
    std::map<std::string, AssignmentParameterResult> next;
    std::map<std::string, bool> applyWhenUnfinished;
    AssignmentParameterResult Read(const std::string &key, const std::string &fallback,
        std::string &value) override
    {
        value = values.count(key) ? values[key] : fallback;
        return {0, true};
    }
    AssignmentParameterResult Write(const std::string &key, const std::string &value) override
    {
        AssignmentParameterResult result = next.count(key) ? next[key] : AssignmentParameterResult {0, true};
        calls.push_back({key, value, result});
        if (result.callFinished && result.parameterResult == 0 || applyWhenUnfinished[key]) {
            values[key] = value;
        }
        next.erase(key);
        applyWhenUnfinished.erase(key);
        return result;
    }
};
class Topology final : public PrimaryTopologyPort {
public:
    std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override
    { return PrimaryTargetFacts {{SubscriptionId {1}, slot, 4}, kRawIccid, true}; }
    RequestId BeginRouteMapping(const StepRef &, SlotTarget) override
    { ADD_FAILURE(); return {}; }
};
class Radio final : public RadioExecutionPort {
public:
    unsigned switches {0};
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const override
    { return {slot, 0, false, false, 9}; }
    bool SwitchPrimaryRadio(const RadioSwitchRequest &) override { ++switches; return true; }
    void EndLegacySenseSwitch(const StepRef &, const RadioDomainId &) override { ++switches; }
};
class Output final : public AssignmentOutputPort {
public:
    std::vector<std::string> events;
    void PublishPrimaryPhase(bool, bool) override {}
    void PublishMainCardChanged(SlotId) override { events.emplace_back("primary"); }
    void ReportDefaultDataSelection(SlotId) override {}
    void PublishDefaultDataChanged(SlotId, SubscriptionId) override { events.emplace_back("data"); }
};
class Completion final : public LegacyIntCompletionPort {
public:
    std::vector<int32_t> results;
    void CompleteLegacyInt(const CompletionPlan &, int32_t result) override { results.push_back(result); }
};
const SlotCatalog &Catalog()
{
    static const SlotCatalog catalog({
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {1, 1, 1, 1, 1});
    return catalog;
}
SwitchPrimaryCommand Command()
{
    return {SlotTarget {SlotId {1}}, true,
        {SynchronousCompletionRoute {SynchronousWaitRef {7}}, false}, std::nullopt,
        BindingRef {{1}, {1}, 4}};
}
struct Fixture {
    ControlledParameters parameters;
    AssignmentStoreAdapter store {parameters};
    Radio radio;
    Output output;
    Topology topology;
    Completion completion;
    AssignmentService service {Catalog(), store, radio, output, topology, completion, RuntimeEpoch {11, 2}};
};
void ExpectCommonSuccess(const Fixture &f)
{
    ASSERT_EQ(f.parameters.calls.size(), 4u);
    EXPECT_EQ(f.parameters.calls[0].key, kPreference);
    EXPECT_EQ(f.parameters.calls[0].value, "true");
    EXPECT_EQ(f.parameters.calls[1].key, kPrimary);
    EXPECT_EQ(f.parameters.calls[1].value, "1");
    EXPECT_EQ(f.parameters.calls[2].key, kIccid);
    EXPECT_EQ(f.parameters.calls[2].value, kEncodedIccid);
    EXPECT_EQ(f.parameters.calls[3].key, kData);
    EXPECT_EQ(f.parameters.calls[3].value, "1");
    EXPECT_EQ(f.output.events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(f.radio.switches, 0u);
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {0}));
}
} // namespace

TEST(TargetAdapterIntegration, C0UsesRealAssignmentStoreAdapter)
{
    Fixture f;
    f.service.StartPrimarySwitch(Command());
    ExpectCommonSuccess(f);
    EXPECT_EQ(f.parameters.values[kPreference], "true");
    EXPECT_EQ(f.parameters.values[kPrimary], "1");
    EXPECT_EQ(f.parameters.values[kIccid], kEncodedIccid);
    EXPECT_EQ(f.parameters.values[kData], "1");
    const auto view = f.service.ReadLegacyAssignment();
    ASSERT_TRUE(view.lastPrimary.has_value());
    ASSERT_TRUE(view.lastData.has_value());
    EXPECT_EQ(view.lastPrimary->legacySlotId, 1);
    EXPECT_EQ(view.lastData->legacySlotId, 1);
    EXPECT_FALSE(view.primarySwitchInProgress);
    RecordProperty("validation_level", "adapter_integration");
    RecordProperty("adapter_implementation", "AssignmentStoreAdapter");
    RecordProperty("encoded_iccid", f.parameters.values[kIccid]);
}

TEST(TargetAdapterIntegration, C0PreservesLegacySuccessForCompletedParameterFailures)
{
    Fixture f;
    f.parameters.next[kIccid] = {73, true};
    f.service.StartPrimarySwitch(Command());
    EXPECT_EQ(f.completion.results, (std::vector<int32_t> {0}));
    EXPECT_EQ(f.parameters.values[kPreference], "true");
    EXPECT_EQ(f.parameters.values[kPrimary], "1");
    EXPECT_EQ(f.parameters.values[kIccid], "previous");
    EXPECT_EQ(f.output.events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(f.parameters.values[kData], "1");

    Fixture data;
    data.parameters.next[kData] = {74, true};
    data.service.StartPrimarySwitch(Command());
    EXPECT_EQ(data.completion.results, (std::vector<int32_t> {0}));
    EXPECT_EQ(data.output.events, (std::vector<std::string> {"primary", "data"}));
    EXPECT_EQ(data.parameters.values[kPrimary], "1");
    EXPECT_EQ(data.parameters.values[kIccid], kEncodedIccid);
    EXPECT_EQ(data.parameters.values[kData], "0");
    EXPECT_TRUE(data.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_TRUE(data.service.ReadLegacyAssignment().lastData.has_value());
}

TEST(TargetAdapterIntegration, C0DoesNotCompleteUnconfirmedWrites)
{
    Fixture f;
    f.parameters.next[kIccid] = {0, false};
    f.parameters.applyWhenUnfinished[kIccid] = true;
    f.service.StartPrimarySwitch(Command());
    EXPECT_TRUE(f.completion.results.empty());
    EXPECT_EQ(f.parameters.values[kPrimary], "1");
    EXPECT_EQ(f.parameters.values[kIccid], kEncodedIccid);
    EXPECT_TRUE(f.output.events.empty());
    EXPECT_TRUE(f.service.ReadLegacyAssignment().primarySwitchInProgress);
    EXPECT_FALSE(f.service.ReadLegacyAssignment().lastPrimary.has_value());

    Fixture data;
    data.parameters.next[kData] = {0, false};
    data.parameters.applyWhenUnfinished[kData] = true;
    data.service.StartPrimarySwitch(Command());
    EXPECT_TRUE(data.completion.results.empty());
    EXPECT_EQ(data.output.events, (std::vector<std::string> {"primary"}));
    EXPECT_EQ(data.parameters.values[kData], "1");
    EXPECT_TRUE(data.service.ReadLegacyAssignment().lastPrimary.has_value());
    EXPECT_FALSE(data.service.ReadLegacyAssignment().lastData.has_value());
    EXPECT_TRUE(data.service.ReadLegacyAssignment().primarySwitchInProgress);
}

TEST(TargetAdapterIntegration, RestoreKeepsLegacyValidSlotAndConditionalIccidRules)
{
    ControlledParameters parameters;
    AssignmentStoreAdapter adapter(parameters);
    const auto snapshot = adapter.ReadPrimaryParameters();
    EXPECT_EQ(snapshot.slotText, "0");
    EXPECT_EQ(snapshot.mainIccidText, "previous");
    ASSERT_EQ(adapter.RestorePrimaryIfLegacyValid({"0", "old-encoded"}).parameterResult, 0);
    EXPECT_EQ(parameters.values[kPrimary], "0");
    EXPECT_EQ(parameters.values[kIccid], "old-encoded");
    const auto invalid = adapter.RestorePrimaryIfLegacyValid({"invalid", "ignored"});
    EXPECT_EQ(invalid.parameterResult, 0);
    EXPECT_TRUE(invalid.callFinished);
    EXPECT_EQ(parameters.values[kIccid], "old-encoded");
    const auto writesBefore = parameters.calls.size();
    ASSERT_EQ(adapter.WritePrimaryParameters(SlotId {1}, "").parameterResult, 0);
    ASSERT_EQ(parameters.calls.size(), writesBefore + 1u);
    EXPECT_EQ(parameters.calls.back().key, kPrimary);
    EXPECT_EQ(parameters.values[kIccid], "old-encoded");
}
