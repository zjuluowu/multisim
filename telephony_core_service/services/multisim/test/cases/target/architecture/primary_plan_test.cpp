#include <algorithm>
#include <gtest/gtest.h>

#include "role_assignment/primary/primary_switch_plan.h"

using namespace OHOS::Telephony::MultiSim;

TEST(TargetArchitecture, C0PrimaryPlanUsesNoDeviceSwitch)
{
    const SlotDescriptor target {SlotId {1}, EndpointKind::Physical, 0,
        RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false};
    const auto plan = PlanPrimarySwitch({target, true, false, true, false});
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->mode, PrimarySwitchMode::AlreadyPrimary);
    EXPECT_EQ(plan->parameterWrite, ParameterWritePosition::WithoutDevice);
    EXPECT_TRUE(plan->followWithDefaultData);
    EXPECT_EQ(plan->beforeDevice,
        (std::vector<PrimaryAction> {PrimaryAction::ValidateOrdinaryGuards}));
    EXPECT_EQ(plan->onDeviceSuccess.front(), PrimaryAction::WritePrimaryParameters);
    EXPECT_EQ(plan->onDeviceSuccess.back(), PrimaryAction::ReturnOriginalResult);
    EXPECT_EQ(std::count(plan->onDeviceSuccess.begin(), plan->onDeviceSuccess.end(),
        PrimaryAction::ExecuteThreePhase), 0);
}
