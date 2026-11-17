#include "internal/common/capability.h"
namespace
{
class SkipFlow : public testing::Test
{
    void TearDown() override { RecordProperty("cleanup_after_skip", "PASS"); }

  protected:
    int Requires(const char *reason) { SimGuard::SkipCapability(reason); }
};
TEST_F(SkipFlow, MissingMutationIsUnsupported)
{
    EXPECT_EQ(Requires("UNSUPPORTED: controlled PIN resource is absent"), 0);
    ADD_FAILURE() << "A skipped helper must abort the business test body";
}
TEST_F(SkipFlow, MissingServiceIsBlocked)
{
    EXPECT_EQ(Requires("BLOCKED: product SA is not ready"), 0);
    ADD_FAILURE() << "A blocked helper must abort the business test body";
}
} // namespace
