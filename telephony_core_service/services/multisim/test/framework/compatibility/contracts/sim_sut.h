// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace SimGuard {
enum class State { Unknown, Absent, Locked, NotReady, Ready };
enum class CardKind { Unknown, Sim, Usim };
enum class Lock { None, Pin, Puk };
enum class Unlock { Ok, Incorrect, Failure };
struct CardObservation {
    int slot;
    bool present;
    State state;
    CardKind kind;
    std::string iccid;
};
struct StateNotification { int slot; State state; CardKind kind; Lock reason; };
struct UnlockResult { int error; Unlock result; int remaining; };
struct StateSnapshot {
    std::vector<CardObservation> cards;
    // No inferred slot->subscription identity or profile/registration/role state.
    // These independent business concepts require separate real query bindings.
    bool profilesObserved = false;
    bool subscriptionsObserved = false;
    bool rolesObserved = false;
};
class ISimSutAdapter {
public:
    virtual ~ISimSutAdapter() = default;
    virtual void Start(int physicalSlots) = 0;
    virtual void Stop() = 0;
    virtual StateSnapshot Observe() = 0;
    virtual std::vector<StateNotification> Notifications() = 0;
    virtual UnlockResult UnlockPin(int slot, const std::string &pin) = 0;
    virtual void Settle() = 0; // bounded completion of already dispatched events, never manufactures state
};
class FakeRil;
// Only this factory and its GN binding change when a real replacement becomes available.
std::unique_ptr<ISimSutAdapter> CreateSimSut(FakeRil &ril);
}
