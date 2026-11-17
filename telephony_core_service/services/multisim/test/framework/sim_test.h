// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "common/sim_types.h"
#include <gtest/gtest.h>
#include <memory>

namespace SimGuard {
class SimAdvancedTest;
class PrimaryProtocol;
// A card is a test resource, not a subscription/slot/simId mapping model.
struct Card {
    std::string name;
    int physicalSlot;
    CardKind kind;
    std::string iccid;
};
class SimTest : public testing::Test {
public:
    SimTest();
    ~SimTest() override;
protected:
    void SetUp() override;
    void TearDown() override;
    Card AddCard(std::string name, int physicalSlot, CardKind kind, State state,
        Lock lock = Lock::None);
    Card AddCard(std::string name, CardKind kind, State state, Lock lock = Lock::None);
    int CardCapacity();
    Card ExistingCard(int physicalSlot);
    State ReadCardState(const Card &card);
    int ReadCapacity();
    bool ParameterBoundaryObserved();
    void Start();
    // Real Primary slice: bootstrap the requested loaded card, then call Core.
    void StartPrimary(const Card &card);
    int SelectPrimary(const Card &card);
    void ExpectPrimary(const Card &card, const std::string &persistedIdentity = "");
    void ExpectPrimaryUnchanged(int previous, const std::string &identity = "");
    void ExpectPrimaryNotifications(bool committed, unsigned requests, bool direct);
    void ExpectPrimaryResult(bool success);
    void ExpectPrimaryIdle();
    void ChangeCard(const Card &card, State state, Lock lock = Lock::None);
    UnlockResult UnlockPin(const Card &card, const std::string &pin);
    void ExpectCardState(const Card &card, State state);
    void ExpectStateNotification(const Card &card, State state, Lock lock = Lock::None);
private:
    friend class SimAdvancedTest;
    friend class PrimaryProtocol;
    struct Environment;
    std::unique_ptr<Environment> environment_;
};
}
