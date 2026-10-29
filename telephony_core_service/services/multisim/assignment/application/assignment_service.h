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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§9.3、§12.5。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_ASSIGNMENT_SERVICE_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_ASSIGNMENT_SERVICE_H

#include "application/orchestration/runtime_operation_store.h"
#include "assignment/api/assignment_requests.h"
#include "assignment/domain/assignment_aggregate.h"
#include "assignment/ports/assignment_store.h"
#include "assignment/ports/radio_execution_port.h"

namespace OHOS::Telephony::MultiSim {
class TopologyService;
// A 唯一应用写入口/切换主编排。O 不再代写 A 参数或发起业务重试。
class AssignmentService final {
public:
    AssignmentService(const SlotCatalog &catalog, AssignmentStore &store, RadioExecutionPort &radio,
        AssignmentOutputPort &output, TopologyService &topology);
    int32_t SetDefaultRole(const SetRoleCommand &command);
    RequestId StartPrimarySwitch(const SwitchPrimaryCommand &command);
    void AcceptRadioStep(const RadioSwitchResult &result);
    void RetryPrimaryAfterReconciliation(RequestId request); // 唯一业务重试授权者。
    AssignmentView ReadLegacyAssignment() const;
    ActivationAdmission AdmitActivationBeforeDevice(SlotId slot);
    void AcceptEntryPrimaryPreference(bool userSet);
    void RecheckPrimaryAfterAccountChange(const PrimaryRecheck &cause);
    AccountRoleFragment PrepareAccountRoleFragment(SubscriptionId subscription) const;
private:
    const SlotCatalog &catalog_;
    AssignmentAggregate assignment_;
    RuntimeOperationStore operations_;
    AssignmentStore &store_;
    RadioExecutionPort &radio_;
    AssignmentOutputPort &output_;
    TopologyService &topology_; // 只调用 T 正式应用入口，不握 T 可写聚合。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_ASSIGNMENT_SERVICE_H
