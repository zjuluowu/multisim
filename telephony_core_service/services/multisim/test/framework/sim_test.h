// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "common/sim_types.h"
#include <gtest/gtest.h>
#include <memory>

namespace SimGuard {
class SimAdvancedTest;
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
    void Start();
    void ChangeCard(const Card &card, State state, Lock lock = Lock::None);
    UnlockResult UnlockPin(const Card &card, const std::string &pin);
    void ExpectCardState(const Card &card, State state);
    void ExpectStateNotification(const Card &card, State state, Lock lock = Lock::None);
private:
    friend class SimAdvancedTest;
    struct Environment;
    std::unique_ptr<Environment> environment_;
};
}
