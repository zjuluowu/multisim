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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.1、§11～12。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_PRIMARY_EXECUTION_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_PRIMARY_EXECUTION_H
#include <optional>
#include <string>
#include "model/sim_identity.h"
namespace OHOS::Telephony::MultiSim {
enum class PrimaryMode { ThreePhase, WithoutModemReboot, AlreadyM0, ThirdCardRoute };
struct PrimaryExecutionObservation {
    // R/厂商实际能力与设备观察，不从slot数量推导。第三卡分支优先于免重启。
    bool thirdCardRequiresRoute;
    bool dedicatedPrimaryRilSupported; // 必须来自原真实能力输入；未知时R不得以false冒充观察。
    std::optional<int32_t> currentModemId;
    uint64_t observationRevision;
};
} // namespace OHOS::Telephony::MultiSim
#endif
