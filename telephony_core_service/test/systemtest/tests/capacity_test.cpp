#include <gmock/gmock.h>
#include "parameter_mock.h"
#include <chrono>
#include <cstdlib>
#include <thread>
#include "capacity_probe.h"
extern "C" unsigned int SimHostParameterReads();
TEST(SimComponent, CapacityReadOnly)
{
    testing::StrictMock<SimHost::MockParameterService> parameters;
    // Exact counts/order are intentionally unconstrained: queries may repeat.
    EXPECT_CALL(parameters, Read(testing::StrEq("const.telephony.slotCount"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("1", value, length);
        });
    EXPECT_CALL(parameters, Read(testing::StrEq("const.booster.virtual_modem_switch"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("false", value, length);
        });
    EXPECT_CALL(parameters, Read(testing::StrEq("const.product.devicetype"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("phone", value, length);
        });
    SimHost::ScopedParameterService scenario(parameters);
    RecordProperty("scenario_framework", "GoogleMock v1.15.2");
    RecordProperty("case_id", "SIM-COMP-001");
    RecordProperty("level", "B");
    RecordProperty("oracle_id", "O-C11");
    RecordProperty("synthetic_only", "true");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    int observed = SimHost::ObserveCapacity();
    int previous = observed;
    bool stable = false;
    do {
        observed = SimHost::ObserveCapacity();
        stable = observed == previous;
        previous = observed;
        if (stable) break;
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    RecordProperty("converged", stable ? "true" : "false");
    RecordProperty("legacy_capacity", observed);
    RecordProperty("parameter_boundary_observed", SimHostParameterReads() > 0 ? "true" : "false");
    ASSERT_TRUE(stable) << "O-H02: no bounded public convergence";
    const bool negative = std::getenv("SIM_HOST_NEGATIVE_CONTROL") != nullptr;
    RecordProperty("corrupted_evidence", negative ? "true" : "false");
    if (negative) observed = -1; // Test observer corruption only; never changes production.
    ASSERT_GE(observed, 0) << "O-C11: a count of slots cannot be negative";
}
int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
