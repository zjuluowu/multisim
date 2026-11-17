// SPDX-License-Identifier: Apache-2.0
#include "sim_test.h"
using SimComponent=SimGuard::SimTest;
#include <chrono>
#include <cstdlib>
#include <thread>
TEST_F(SimComponent, CapacityReadOnly)
{
    RecordProperty("scenario_framework", "GoogleMock v1.15.2");
    RecordProperty("case_id", "SIM-COMP-001");
    RecordProperty("level", "B");
    RecordProperty("oracle_id", "O-C11");
    RecordProperty("synthetic_only", "true");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    int observed = ReadCapacity();
    int previous = observed;
    bool stable = false;
    do {
        observed = ReadCapacity();
        stable = observed == previous;
        previous = observed;
        if (stable) break;
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    RecordProperty("converged", stable ? "true" : "false");
    RecordProperty("legacy_capacity", observed);
    RecordProperty("parameter_boundary_observed", ParameterBoundaryObserved() ? "true" : "false");
    ASSERT_TRUE(stable) << "O-H02: no bounded public convergence";
    const bool negative = std::getenv("SIM_HOST_NEGATIVE_CONTROL") != nullptr;
    RecordProperty("corrupted_evidence", negative ? "true" : "false");
    if (negative) observed = -1; // Test observer corruption only; never changes production.
    ASSERT_GE(observed, 0) << "O-C11: a count of slots cannot be negative";
}
