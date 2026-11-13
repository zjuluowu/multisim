// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "fakes/ril_device.h"
namespace SimGuard {
struct Scenario { const char *id; int slots; std::vector<DeviceCard> sequence; };
// Version 1: public physical card state/type semantics, independent of implementation.
inline const std::vector<Scenario> LIFECYCLES = {
    {"LIFE_001", 1, {{State::Absent}, {State::Locked, CardKind::Usim, "fixture-A", Lock::Pin},
        {State::Ready, CardKind::Usim, "fixture-A"}, {State::Absent}}},
    {"LIFE_002", 1, {{State::Ready, CardKind::Sim, "fixture-B"}, {State::Absent},
        {State::Ready, CardKind::Usim, "fixture-C"}}},
    {"PUK_001", 1, {{State::Locked, CardKind::Usim, "fixture-D", Lock::Puk}, {State::Absent}}},
};
}
