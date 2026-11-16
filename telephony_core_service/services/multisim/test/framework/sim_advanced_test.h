// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sim_test.h"
#include "host/ril_device.h"
#include "host/ril_fault_controls.h"

namespace SimGuard {
// Explicit transport control for races and protocol failures. Ordinary cases use
// SimTest. No Scenario/Runner/Adapter dispatch is involved.
class SimAdvancedTest : public SimTest {
protected:
    FakeRil &Ril();
    RilFaultControls &Faults();
    void Start(int slots);
    void StartWithRil(int slots, bool available);
    void Stop();
    void Recreate(bool available = true);
    StateSnapshot Observe();
    void Observe(const char *step);
    void Settle();
    void Complete();
    void Change(int slot, DeviceCard card);
    UnlockResult UnlockPin(int slot, const std::string &pin);
    UnlockResult UnlockPuk(int slot, const std::string &newPin, const std::string &puk);
    int Refresh(int slot);
    void TearDown() override;
private:
    std::string trace_;
};
}
