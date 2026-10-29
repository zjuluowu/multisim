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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§9.3 SIM-024、§11.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_ACTIVATE_SIM_USE_CASE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_ACTIVATE_SIM_USE_CASE_H

#include "topology/application/topology_service.h"
#include "topology/ports/activation_runtime_port.h"
#include "assignment/application/assignment_service.h"
#include "application/orchestration/runtime_operation_store.h"

namespace OHOS::Telephony::MultiSim {
// 第二条流程的关键入口：T 拥有 active 及激活进行中；A 仅作切换准入/偏好/后续主卡复核。
class ActivateSimUseCase final {
public:
    ActivateSimUseCase(TopologyService &topology, AssignmentService &assignment,
        ActivationRuntimePort &runtime, ActivationHookPort &hooks);
    RequestId StartOrdinaryActivation(const ActivateSimCommand &command);
    void AcceptDeviceResult(const ActivationDeviceResult &result);
    // force/卫星有独立轨迹，仅保留声明，不能套用普通 non-force 的计划。
    RequestId StartForceActivation(const ActivateSimCommand &command);
    RequestId StartSatelliteActivation(const ActivateSimCommand &command);
private:
    TopologyService &topology_;
    AssignmentService &assignment_;
    ActivationRuntimePort &runtime_;
    ActivationHookPort &hooks_;
    RuntimeOperationStore operations_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_ACTIVATE_SIM_USE_CASE_H
