#include "sim_test.h"
using namespace SimGuard;
TEST(ResourceModel, FourIndependentResourcesDoNotImplyLegacyFourCardSupport)
{
    ResourceCatalog resources;
    resources.Declare("A", {ResourceForm::Removable, 0, std::nullopt, "", 3});
    resources.Declare("B", {ResourceForm::Removable, 3, std::nullopt, "", 0});
    resources.Declare("C", {ResourceForm::EmbeddedProfile, std::nullopt, 2, "profile-C", 1});
    resources.Declare("D", {ResourceForm::Virtual, std::nullopt, std::nullopt, "", 2, "endpoint-D"});
    EXPECT_EQ(resources.Size(), 4u);
    EXPECT_EQ(resources.At("B").location.physicalSlot, 3);
    EXPECT_EQ(resources.At("C").location.port, 2);
    EXPECT_FALSE(resources.At("C").location.physicalSlot.has_value());
    EXPECT_EQ(resources.At("C").location.profile,"profile-C");
    EXPECT_TRUE(resources.At("C").location.virtualEndpoint.empty());
    EXPECT_EQ(resources.At("D").location.modem, 2);
    EXPECT_EQ(resources.At("D").location.virtualEndpoint, "endpoint-D");
    EXPECT_TRUE(resources.At("D").location.profile.empty());
    EXPECT_THROW(resources.Declare("A", {ResourceForm::Virtual, {}, {}, "", {}}), std::invalid_argument);
    RecordProperty("verification_level", "resource_model_only_not_Legacy_business");
}
