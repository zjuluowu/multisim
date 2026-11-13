// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
namespace SimGuard {
class RilFaultControls;
// Additive lifecycle/security capability; the v2 business contract remains unchanged.
class ISimSessionAdapter : public ISimSutAdapter {
public:
    virtual void StartWithRil(int physicalSlots, bool available) = 0;
    virtual void Recreate(bool rilAvailable = true) = 0;
    virtual int Refresh(int slot) = 0;
    virtual UnlockResult UnlockPuk(int slot, const std::string &newPin, const std::string &puk) = 0;
};
std::unique_ptr<ISimSessionAdapter> CreateSimSessionSut(FakeRil &ril, RilFaultControls &faults);
}
