// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "internal/common/resource_catalog.h"
#include "internal/common/sim_types.h"
#include <gtest/gtest.h>
#include <memory>

namespace SimGuard
{
class StateProtocol;
class PrimaryProtocol;
// A card is a test resource, not a subscription/slot/simId mapping model.
struct Card
{
    std::string name;
    CardKind kind;
    std::string iccid;

  private:
    friend class SimTest;
    friend class PrimaryProtocol;
    int physicalSlot;
    Card(std::string label, int endpoint, CardKind type, std::string identity)
        : name(std::move(label)), kind(type), iccid(std::move(identity)), physicalSlot(endpoint)
    {
    }
};
class SimTest : public testing::Test
{
  public:
    SimTest();
    ~SimTest() override;

  protected:
    void SetUp() override;
    void TearDown() override;
    Card AddCard(std::string name, CardKind kind, State state, Lock lock = Lock::None);
    int CardCapacity();
    Card ExistingCard(int physicalSlot = 0);
    void ExpectReadableCard(const Card &card);
    State ReadCardState(const Card &card);
    int ReadCapacity();
    bool ParameterBoundaryObserved();
    void Start();
    // Real Primary slice: bootstrap the requested loaded card, then call Core.
    void StartPrimary(const Card &card);
    int SelectPrimary(const Card &card);
    void ExpectPrimary(const Card &card, const std::string &persistedIdentity = "");
    void ExpectPrimaryChanged(const Card &card);
    void ExpectPrimaryResult(bool success);
    void ExpectPrimaryIdle();
    void ChangeCard(const Card &card, State state, Lock lock = Lock::None);
    UnlockResult UnlockPuk(const Card &card, const std::string &newPin, const std::string &puk);
    UnlockResult UnlockPin(const Card &card, const std::string &pin);
    void ExpectCardState(const Card &card, State state);
    void ExpectStateNotification(const Card &card, State state, Lock lock = Lock::None);

  private:
    Card AddCard(std::string name, int physicalSlot, CardKind kind, State state, Lock lock);
    friend class StateProtocol;
    friend class PrimaryProtocol;
    struct Environment;
    std::unique_ptr<Environment> environment_;
};
} // namespace SimGuard
