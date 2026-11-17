// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <gmock/gmock.h>
#include "core_service_sim.h"
#include "multi_sims_capability_manager.h"
#include "core_service_errors.h"
#include "telephony_permission.h"
#include "internal/host/primary/security_boundary.h"
#include "internal/host/primary/unscripted_sim_manager.h"
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
}
