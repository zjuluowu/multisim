// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
#include <gtest/gtest.h>
#include <sstream>
namespace SimGuard {
inline const char *StateName(State state)
{
    switch (state) {
        case State::Unknown: return "UNKNOWN";
        case State::Absent: return "ABSENT";
        case State::Locked: return "LOCKED";
        case State::NotReady: return "NOT_READY";
        case State::Ready: return "READY";
    }
    return "UNMAPPED";
}
inline void PrintTo(State state, std::ostream *out) { *out << StateName(state); }
inline void ExpectCard(const CardObservation &actual, int slot, State state, CardKind kind)
{
    EXPECT_EQ(actual.slot, slot);
    EXPECT_EQ(actual.present, state != State::Absent);
    EXPECT_EQ(actual.state, state);
    EXPECT_EQ(actual.kind, kind);
}
inline void ExpectNotification(const std::vector<StateNotification> &events, int slot, State state, Lock reason)
{
    bool found = false;
    for (const auto &event : events)
        if (event.slot == slot && event.state == state && event.reason == reason) found = true;
    EXPECT_TRUE(found) << "missing public state notification slot=" << slot << " state=" << static_cast<int>(state);
}
inline std::string Describe(const StateSnapshot &snapshot)
{
    std::ostringstream out;
    for (const auto &card : snapshot.cards)
        out << "slot=" << card.slot << ",present=" << card.present << ",state=" << StateName(card.state)
            << ",kind=" << static_cast<int>(card.kind) << ";";
    return out.str();
}
}
