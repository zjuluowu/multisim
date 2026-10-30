/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// TARGET ARCHITECTURE SKELETON / PROPOSED: 尚未接入生产或构建，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3 WU-03/04、§9.3、§13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_ASSIGNMENT_STORE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_ASSIGNMENT_STORE_H

#include <string>
#include "assignment/api/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
struct PrimaryParameterSnapshot { std::string slotText; std::string mainIccidText; };
struct AssignmentParameterResult { int32_t parameterResult; bool callFinished; };
struct RoleStoreResult { int32_t legacyResult; bool writeConfirmed; bool callFinished; };
// A 专属字段/参数协议。I AssignmentStoreAdapter 实现，保持原 URI/键，非新 schema。
class AssignmentStore {
public:
    virtual ~AssignmentStore() = default;
    virtual RoleStoreResult WriteDefaultRole(const SetRoleCommand &command) = 0;
    virtual PrimaryParameterSnapshot ReadPrimaryParameters() = 0;
    virtual AssignmentParameterResult WritePrimaryParameters(SlotId slot, const std::string &encodedIccid) = 0;
    virtual AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &old) = 0;
    virtual AssignmentParameterResult WriteDefaultData(SlotId slot) = 0;
    virtual AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_ASSIGNMENT_STORE_H
