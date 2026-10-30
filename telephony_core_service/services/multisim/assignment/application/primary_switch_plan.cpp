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

#include "assignment/application/primary_switch_plan.h"
namespace OHOS::Telephony::MultiSim {
PrimarySwitchPlan PlanPrimarySwitch(bool userSet, const PrimaryExecutionObservation &o)
{
    using A = PrimaryAction;
    if (o.thirdCardRequiresRoute) {
        // 旧NeedSwitchSimCard的int是换路/受理结果，后续Primary有独立完成。
        return {PrimaryMode::ThirdCardRoute, o.observationRevision,
            {A::RequestThirdCardRoute}, {A::ReturnOriginalResult}, {A::ReturnOriginalResult}};
    }
    if (userSet && o.dedicatedPrimaryRilSupported) {
        return {PrimaryMode::WithoutModemReboot, o.observationRevision,
            {A::ValidateDedicatedGuards, A::BeginProgress, A::PublishStarted, A::ExecuteDedicatedRil},
            {A::WritePrimaryParameters, A::PublishMainCard, A::SaveDefaultData, A::FinishDedicated,
                A::RemoveDedicatedTimeout, A::ClearDedicatedTarget, A::ReturnOriginalResult},
            {A::FinishDedicated, A::ReturnOriginalResult}};
    }
    if (o.currentModemId && *o.currentModemId == 0) {
        return {PrimaryMode::AlreadyM0, o.observationRevision,
            {A::ValidateOrdinaryGuards},
            {A::WritePrimaryParameters, A::PublishMainCard, A::SaveDefaultData, A::ResetRetry,
                A::ReturnOriginalResult}, {A::ReturnOriginalResult}};
    }
    return {PrimaryMode::ThreePhase, o.observationRevision,
        {A::ValidateOrdinaryGuards, A::BeginProgress, A::BeginSenseSwitch, A::SaveOldParameters,
            A::WritePrimaryParameters, A::PublishStarted, A::SendSwitchType, A::ExecuteThreePhase},
        {A::PublishMainCard, A::SaveDefaultData, A::FinishOrdinary, A::ResetRetry, A::ReturnOriginalResult},
        {A::RestoreValidOldParameters, A::FinishOrdinary, A::ScheduleLegacyRetry, A::ReturnOriginalResult}};
}
} // namespace OHOS::Telephony::MultiSim
