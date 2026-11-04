// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
#include "fakes/ril_device.h"
#include "oracle/card_oracle.h"
#include <gtest/gtest.h>
namespace SimGuard {
class ContractDriver : public testing::Test {
protected:
    FakeRil ril;
    std::unique_ptr<ISimSutAdapter> sut;
    std::string trace;
    void SetUp() override { sut = CreateSimSut(ril); }
    void TearDown() override
    {
        RecordProperty("observations", trace);
        RecordProperty("pending_responses", std::to_string(ril.Pending()));
        std::string calls;
        for (const auto &call : ril.Calls()) calls += call + ";";
        RecordProperty("external_calls", calls);
        if (sut) sut->Stop();
    }
    void Observe(const char *step)
    {
        trace += std::string(step) + ":" + Describe(sut->Observe()) + "\n";
        RecordProperty("observations", trace); // preserve trace before a later assertion/exception
    }
    void Change(int slot, DeviceCard card)
    {
        ril.Change(slot, card);
        sut->Settle();
        ASSERT_GT(ril.Pending(), 0u) << "SUT did not issue a real RIL status request";
        Complete();
        Observe("card-event");
    }
    void Complete()
    {
        // Host convergence bound, not a required business request count/order.
        // Additional legal queries receive the same external device semantics.
        for (unsigned round = 0; round < 32; ++round) {
            sut->Settle();
            if (!ril.Pending()) return;
            ril.RespondAll();
        }
        FAIL() << "status transport failed to converge; pending=" << ril.Pending();
    }
};
}
