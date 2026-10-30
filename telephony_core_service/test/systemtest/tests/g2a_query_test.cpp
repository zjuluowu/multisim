#include <cstdlib>
#include <string>
#include <gmock/gmock.h>
#include "g2a_query_probe.h"
#include "parameter_mock.h"

TEST(G2aReadOnlyQuery, OriginalManagerAttachment)
{
    const char *configured = std::getenv("SIM_G2A_SLOT_PARAMETER");
    ASSERT_NE(configured, nullptr);
    const int expectedConfiguration = std::atoi(configured); // exact old GetRealMaxSlotCount rule; not GetMaxSlotCount.
    const bool vendor = std::getenv("SIM_G2A_VENDOR_RESULT") != nullptr;
    const int expectedManager = vendor ? std::atoi(std::getenv("SIM_G2A_VENDOR_RESULT")) : expectedConfiguration;
    testing::StrictMock<SimHost::MockParameterService> parameters;
    EXPECT_CALL(parameters, Read(testing::StrEq("const.telephony.slotCount"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([configured](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue(configured, value, length);
        });
    SimHost::ScopedParameterService installed(parameters);
    const auto seen = SimHost::ObserveReadOnlyAttachment(vendor);
    EXPECT_EQ(seen.emptyManagerCount, expectedConfiguration);
    EXPECT_EQ(seen.resetCount, expectedConfiguration);
    EXPECT_EQ(seen.originalCoreCount, expectedManager);
    EXPECT_EQ(seen.directManagerCount, expectedManager);
    EXPECT_EQ(seen.bindingCount, expectedManager);
    EXPECT_EQ(seen.attachedCoreCount, expectedManager);
    EXPECT_TRUE(seen.nullBindingRejected);
    EXPECT_TRUE(seen.attached);
    EXPECT_TRUE(seen.failedAttach);
    EXPECT_TRUE(seen.managerIdentityPreserved);
    EXPECT_TRUE(seen.rilIdentityPreserved);
    EXPECT_TRUE(seen.managerConstructorRetainedRil);
    EXPECT_TRUE(seen.sameAddressDifferentControlBlockRejected);
    EXPECT_TRUE(seen.resetReleasedOriginalManager);
    if (expectedConfiguration == 0) { EXPECT_TRUE(seen.zeroConfigurationWasReRead); }
    if (vendor) {
        EXPECT_EQ(seen.vendorInput, expectedConfiguration);
        EXPECT_GT(seen.vendorCalls, 0); // execution evidence, not an exact call-count business oracle.
    }
    RecordProperty("case_id", "SIM-G2A-READ-001");
    RecordProperty("manager_kind", "production_SimManager");
    RecordProperty("manager_on_init_executed", "false");
    RecordProperty("core_init_executed", "false");
    RecordProperty("card_presence", "not_observed_or_required");
    RecordProperty("parameter", configured);
    RecordProperty("original_configuration", expectedConfiguration);
    RecordProperty("manager_result", seen.attachedCoreCount);
    RecordProperty("host_attachment_initialized", "true");
    RecordProperty("original_manager_control_block", "preserved");
    RecordProperty("original_ril_control_block", "preserved");
    RecordProperty("vendor_boundary", vendor ? "explicit_test_ABI" : "not_loaded");
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
