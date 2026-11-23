// SPDX-License-Identifier: Apache-2.0
#include "internal/host/primary_entry_fixture.h"
#include <future>
namespace PrimarySlotHost {
TEST_F(PrimaryEntry, StopSealsOldHandleBeforeParameterAndManagerWrite)
{
    core.StopAccepting();
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_EQ(core.SetActiveSim(1,1), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_TRUE(core.AcceptedCallsDrained());
}

TEST_F(PrimaryEntry, StopDoesNotCrossAlreadyAcceptedParameterWrite)
{
    Permit();
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(0)));
    std::promise<void> entered, release;
    auto released=release.get_future().share();
    EXPECT_CALL(*manager, IsSimActive(1)).WillOnce([&] {
        entered.set_value(); released.wait(); return true;
    });
    EXPECT_CALL(identity, WriteParameter(USER_PREFER_PRIMARY_SLOT, "true")).WillOnce(Return(0));
    EXPECT_CALL(*manager, SetPrimarySlotId(1,true)).WillOnce(Return(TELEPHONY_ERR_SUCCESS));
    auto accepted=std::async(std::launch::async,[&] { return core.SetPrimarySlotId(1); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    core.StopAccepting();
    EXPECT_FALSE(core.AcceptedCallsDrained());
    EXPECT_EQ(accepted.wait_for(std::chrono::milliseconds(0)),std::future_status::timeout);
    release.set_value();
    EXPECT_EQ(accepted.get(),TELEPHONY_ERR_SUCCESS);
    EXPECT_TRUE(core.WaitForAcceptedCallsDrained(std::chrono::seconds(3)));
}

TEST_F(PrimaryEntry, ManagerReplacementWaitsForAcceptedCallAndGetReturnsSnapshot)
{
    Permit();
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(0)));
    std::promise<void> entered, release;
    auto released=release.get_future().share();
    EXPECT_CALL(*manager, IsSimActive(1)).WillOnce([&] {
        entered.set_value(); released.wait(); return true;
    });
    EXPECT_CALL(identity, WriteParameter(USER_PREFER_PRIMARY_SLOT, "true")).WillOnce(Return(0));
    EXPECT_CALL(*manager, SetPrimarySlotId(1,true)).WillOnce(Return(TELEPHONY_ERR_SUCCESS));
    auto accepted=std::async(std::launch::async,[&] { return core.SetPrimarySlotId(1); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    auto replacement=std::make_shared<testing::StrictMock<ManagerSeam>>();
    auto replace=std::async(std::launch::async,[&] { core.SetSimManager(replacement); });
    EXPECT_EQ(replace.wait_for(std::chrono::milliseconds(20)),std::future_status::timeout);
    const auto oldSnapshot=core.GetSimManager();
    EXPECT_EQ(oldSnapshot.get(),manager.get());
    release.set_value();
    EXPECT_EQ(accepted.get(),TELEPHONY_ERR_SUCCESS);
    replace.get();
    const auto newSnapshot=core.GetSimManager();
    EXPECT_EQ(newSnapshot.get(),replacement.get());
    EXPECT_EQ(oldSnapshot.get(),manager.get());
}

TEST_F(PrimaryEntry, StopDuringReplacementDoesNotReopenOldCloseGeneration)
{
    Permit();
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(0)));
    std::promise<void> entered, release;
    auto released=release.get_future().share();
    EXPECT_CALL(*manager, IsSimActive(1)).WillOnce([&] {
        entered.set_value(); released.wait(); return true;
    });
    EXPECT_CALL(identity, WriteParameter(USER_PREFER_PRIMARY_SLOT, "true")).WillOnce(Return(0));
    EXPECT_CALL(*manager, SetPrimarySlotId(1,true)).WillOnce(Return(TELEPHONY_ERR_SUCCESS));
    auto accepted=std::async(std::launch::async,[&] { return core.SetPrimarySlotId(1); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(3)),std::future_status::ready);
    auto replacement=std::make_shared<testing::StrictMock<ManagerSeam>>();
    auto replace=std::async(std::launch::async,[&] { core.SetSimManager(replacement); });
    EXPECT_EQ(replace.wait_for(std::chrono::milliseconds(20)),std::future_status::timeout);
    core.StopAccepting();
    release.set_value();
    EXPECT_EQ(accepted.get(),TELEPHONY_ERR_SUCCESS);
    replace.get();
    EXPECT_EQ(core.GetSimManager().get(),replacement.get());
    EXPECT_EQ(core.SetPrimarySlotId(1),TELEPHONY_ERR_LOCAL_PTR_NULL);
}

TEST_F(PrimaryEntry, StopReopensOnlyOnNewManagerGeneration)
{
    core.StopAccepting();
    EXPECT_EQ(core.SetPrimarySlotId(1),TELEPHONY_ERR_LOCAL_PTR_NULL);
    auto replacement=std::make_shared<testing::StrictMock<ManagerSeam>>();
    manager=replacement;
    core.SetSimManager(manager);
    Permit();
    Ready(1);
    Delegate(1,TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.SetPrimarySlotId(1),TELEPHONY_ERR_SUCCESS);
}

TEST_F(PrimaryEntry, NonSystemCallerPrecedesPermissionAndNullManager)
{
    core.SetSimManager(nullptr);
    EXPECT_CALL(identity, TokenType(101)).WillOnce(Return(TOKEN_HAP));
    EXPECT_CALL(identity, SystemApp(0x100000065ULL)).WillOnce(Return(false));
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API);
}
TEST_F(PrimaryEntry, PermissionDenialPrecedesNullManager)
{
    core.SetSimManager(nullptr);
    Permit(TOKEN_HAP, false);
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_PERMISSION_ERR);
}
TEST_F(PrimaryEntry, NullManagerPrecedesCapabilityRecheck)
{
    core.SetSimManager(nullptr);
    Permit();
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_LOCAL_PTR_NULL);
}
TEST_F(PrimaryEntry, ChangedExternalSystemAppDecisionRejectsExtendedSlot)
{
    // Explicit policy-service fault/revocation between two checks, not a normal
    // stable-caller scenario: both real component guards consult the same API.
    EXPECT_CALL(identity, TokenType(101)).Times(3).WillRepeatedly(Return(TOKEN_HAP));
    EXPECT_CALL(identity, SystemApp(0x100000065ULL)).WillOnce(Return(true)).WillOnce(Return(false));
    EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE)).WillOnce(Return(PERMISSION_GRANTED));
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_SLOTID_INVALID);
}
TEST_F(PrimaryEntry, MissingCardStopsBeforeActiveAndPreference)
{
    Permit();
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(DoAll(SetArgReferee<1>(false), Return(0)));
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_NO_SIM_CARD);
}
TEST_F(PrimaryEntry, InactiveCardStopsBeforePreferenceAndSwitch)
{
    Permit();
    testing::InSequence order;
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(0)));
    EXPECT_CALL(*manager, IsSimActive(1)).WillOnce(Return(false));
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_SLOTID_INVALID);
}
TEST_F(PrimaryEntry, ReadyCardWritesPreferenceBeforeDelegatingExactTarget)
{
    Permit(TOKEN_HAP);
    testing::InSequence order;
    Ready(1);
    Delegate(1, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
}
TEST_F(PrimaryEntry, ManagerFailureReturnedUnchangedAfterPreferenceWrite)
{
    Permit();
    testing::InSequence order;
    Ready(0);
    Delegate(0, TELEPHONY_ERR_RIL_CMD_FAIL);
    EXPECT_EQ(core.SetPrimarySlotId(0), TELEPHONY_ERR_RIL_CMD_FAIL);
}
TEST_F(PrimaryEntry, FailedPreferenceWriteStillDelegatesCurrentBehavior)
{
    RecordProperty("oracle", "legacy_characterization_SetParameter_return_is_ignored");
    Permit();
    testing::InSequence order;
    Ready(1);
    Delegate(1, TELEPHONY_ERR_SUCCESS, -1);
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
}
TEST_F(PrimaryEntry, PresenceQueryFailureWithFalseOutputReturnsNoCard)
{
    RecordProperty("oracle", "legacy_characterization_HasSimCard_return_is_ignored");
    Permit();
    EXPECT_CALL(*manager, HasSimCard(1, _)).WillOnce(Return(TELEPHONY_ERR_RIL_CMD_FAIL));
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_NO_SIM_CARD);
}
TEST_F(PrimaryEntry, PresenceQueryFailureWithTrueOutputStillDelegates)
{
    RecordProperty("oracle", "legacy_characterization_HasSimCard_return_is_ignored");
    Permit();
    testing::InSequence order;
    Ready(1, TELEPHONY_ERR_RIL_CMD_FAIL);
    Delegate(1, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
}
TEST_F(PrimaryEntry, StableSystemAppPassesExtendedSlotCapability)
{
    EXPECT_CALL(identity, TokenType(101)).Times(3).WillRepeatedly(Return(TOKEN_HAP));
    EXPECT_CALL(identity, SystemApp(0x100000065ULL)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE)).WillOnce(Return(PERMISSION_GRANTED));
    testing::InSequence order;
    Ready(SLOT_3_INDEX);
    Delegate(SLOT_3_INDEX, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_SUCCESS);
}
class NativeOrShell : public PrimaryEntry, public testing::WithParamInterface<ATokenTypeEnum> {};
TEST_P(NativeOrShell, SystemAppAndExtendedCapabilityUseRealTokenRules)
{
    EXPECT_CALL(identity, TokenType(101)).Times(3).WillRepeatedly(Return(GetParam()));
    EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE)).WillOnce(Return(PERMISSION_GRANTED));
    testing::InSequence order;
    Ready(SLOT_3_INDEX);
    Delegate(SLOT_3_INDEX, TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(core.SetPrimarySlotId(SLOT_3_INDEX), TELEPHONY_ERR_SUCCESS);
}
INSTANTIATE_TEST_SUITE_P(CallerKinds, NativeOrShell, testing::Values(TOKEN_NATIVE, TOKEN_SHELL));
}
