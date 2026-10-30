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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_ASSIGNMENT_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_ASSIGNMENT_PORT_H

#include "assignment/api/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
// T激活只依赖A正式应用能力，不握AssignmentService具体类型或A聚合。
class ActivationAssignmentPort {
public:
    virtual ~ActivationAssignmentPort() = default;
    virtual LegacyActivationWaitResult WaitForLegacyPrimaryBeforeActivation(SlotId slot) = 0;
    virtual std::optional<ActivationConflictLease> AdmitActivationBeforeDevice(
        const ActivationConflictRequest &request) = 0;
    virtual bool ReleaseActivationConflict(const ActivationConflictLease &lease,
        const ActivationConflictEndEvidence &evidence) = 0;
    virtual bool TransferActivationConflictToReconciliation(const ActivationConflictLease &lease,
        EffectKnowledge effect) = 0;
    // G3按完整请求登记/释放；G2仅保留旧等待，禁止用查询bool冒充准入或因进度false释放。
    virtual void AcceptEntryPrimaryPreference(bool userSet) = 0;
    virtual void RecheckPrimaryAfterAccountChange(const PrimaryRecheck &cause) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_ASSIGNMENT_PORT_H
