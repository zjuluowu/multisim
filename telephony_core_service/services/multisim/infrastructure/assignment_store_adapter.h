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

#ifndef OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ASSIGNMENT_STORE_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ASSIGNMENT_STORE_ADAPTER_H

#include "assignment/ports/assignment_store.h"

namespace OHOS::Telephony { class SimRdbHelper; }
namespace OHOS::Telephony::MultiSim {
class AssignmentStoreAdapter final : public AssignmentStore {
public:
    explicit AssignmentStoreAdapter(SimRdbHelper &legacyStore) : legacyStore_(legacyStore) {}
    RoleStoreResult WriteDefaultRole(const SetRoleCommand &command) override;
    PrimaryParameterSnapshot ReadPrimaryParameters() override;
    AssignmentParameterResult PrewritePrimary(SlotId slot, const std::string &encodedIccid) override;
    AssignmentParameterResult RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &old) override;
    AssignmentParameterResult WriteDefaultData(SlotId slot) override;
    AssignmentParameterResult WriteUserPrimaryPreference(bool userSet) override;
private:
    SimRdbHelper &legacyStore_;
    // WriteDefaultRole沿原默认卡URI协议；其余沿SetParameter原键与编码。
    // ResumePrimaryCardInfo对旧参数空/非法时不恢复的条件保持；不发设备回滚。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ASSIGNMENT_STORE_ADAPTER_H
