#include <gmock/gmock.h>
#include "core_service_sim.h"
#include "multi_sims_capability_manager.h"
#include "core_service_errors.h"
#include "telephony_permission.h"
#include "security_boundary.h"
#include "unscripted_sim_manager.h"
namespace PrimarySlotHost {
using namespace OHOS::Telephony;
using namespace OHOS::Security::AccessToken;
using testing::_;
using testing::Return;
using testing::DoAll;
using testing::SetArgReferee;
class ManagerSeam : public UnscriptedSimManager {
public:
    MOCK_METHOD(int32_t, HasSimCard, (int32_t, bool &), (override));
    MOCK_METHOD(bool, IsSimActive, (int32_t), (override));
    MOCK_METHOD(int32_t, SetPrimarySlotId, (int32_t, bool), (override));
};
class PrimaryEntry : public testing::Test {
protected:
    testing::StrictMock<ExternalIdentity> identity;
    IdentityScope installed{identity};
    std::shared_ptr<testing::StrictMock<ManagerSeam>> manager =
        std::make_shared<testing::StrictMock<ManagerSeam>>();
    CoreServiceSim core;
    void SetUp() override
    {
        core.SetSimManager(manager);
        RecordProperty("scope", "real_CoreServiceSim_Permission_Capability_internal_ISimManager_seam");
        RecordProperty("whole_core_service_chain", "BLOCKED_not_executed");
    }
    void Permit(ATokenTypeEnum type = TOKEN_NATIVE, bool granted = true)
    {
        EXPECT_CALL(identity, TokenType(101)).Times(2).WillRepeatedly(Return(type));
        if (type == TOKEN_HAP) EXPECT_CALL(identity, SystemApp(0x100000065ULL)).WillOnce(Return(true));
        EXPECT_CALL(identity, Verify(101, Permission::SET_TELEPHONY_STATE))
            .WillOnce(Return(granted ? PERMISSION_GRANTED : PERMISSION_DENIED));
    }
    void Ready(int slot, int hasResult = TELEPHONY_ERR_SUCCESS)
    {
        EXPECT_CALL(*manager, HasSimCard(slot, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(hasResult)));
        EXPECT_CALL(*manager, IsSimActive(slot)).WillOnce(Return(true));
    }
    void Delegate(int slot, int result, int parameterResult = 0)
    {
        EXPECT_CALL(identity, WriteParameter(USER_PREFER_PRIMARY_SLOT, "true")).WillOnce(Return(parameterResult));
        EXPECT_CALL(*manager, SetPrimarySlotId(slot, true)).WillOnce(Return(result));
    }
};
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
int main(int argc, char **argv)
{
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
