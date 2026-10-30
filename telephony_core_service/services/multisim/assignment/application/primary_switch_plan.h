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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_PRIMARY_SWITCH_PLAN_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_APPLICATION_PRIMARY_SWITCH_PLAN_H
#include <optional>
#include <string>
#include <vector>
#include "assignment/api/primary_execution.h"
namespace OHOS::Telephony::MultiSim {
enum class PrimaryAction {
    ValidateOrdinaryGuards, ValidateDedicatedGuards, BeginProgress, BeginSenseSwitch,
    SaveOldParameters, WritePrimaryParameters, PublishStarted, SendSwitchType,
    ExecuteThreePhase, ExecuteDedicatedRil, PublishMainCard, SaveDefaultData,
    RestoreValidOldParameters, FinishOrdinary, FinishDedicated, ScheduleLegacyRetry,
    ResetRetry, RemoveDedicatedTimeout, ClearDedicatedTarget,
    RequestThirdCardRoute, ReturnOriginalResult
};
struct PrimarySwitchPlan {
    PrimaryMode mode;
    uint64_t observationRevision;
    std::vector<PrimaryAction> beforeDevice;
    std::vector<PrimaryAction> onDeviceSuccess;
    std::vector<PrimaryAction> onDeviceFailure;
};
// 兼容步骤属于Application；Domain只保留角色/资源不变量。
PrimarySwitchPlan PlanPrimarySwitch(bool userSet, const PrimaryExecutionObservation &observation);
} // namespace OHOS::Telephony::MultiSim
#endif
