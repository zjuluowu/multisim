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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H

#include <optional>
#include "application/orchestration/request_context.h"
#include "assignment/api/primary_execution.h"

namespace OHOS::Telephony::MultiSim {
struct PrimaryRadioObservation {
    SlotId slot;
    std::optional<int32_t> currentModemId;
    bool dedicatedPrimaryRilSupported;
    bool vendorMappingRequired;
    uint64_t observationRevision;
};
struct RadioSwitchRequest {
    StepRef step;
    SlotId target;
    RadioDomainId domain;
    PrimarySwitchMode mode;
    std::optional<BindingRef> expectedBinding;
};
struct RadioSwitchResult {
    StepRef step;
    SlotId memberSlot;
    uint32_t protocolSession;
    uint32_t phase;
    EffectKnowledge effect;
    int32_t legacyResult;
};
class RadioResultSink {
public:
    virtual ~RadioResultSink() = default;
    virtual void AcceptRadioStep(const RadioSwitchResult &result) = 0;
    virtual void AcceptLateRadioEvidence(const RadioSwitchResult &result) = 0;
};
// A消费；R复用三阶段控制器或原共享RIL专用命令，不反调旧混合Controller。
class RadioExecutionPort {
public:
    virtual ~RadioExecutionPort() = default;
    virtual PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId slot) const = 0;
    virtual void SwitchPrimaryRadio(const RadioSwitchRequest &request) = 0;
    virtual void EndLegacySenseSwitch(const StepRef &owner, const RadioDomainId &domain) = 0;
};
class AssignmentOutputPort {
public:
    virtual ~AssignmentOutputPort() = default;
    virtual void PublishPrimaryPhase(bool setDone, bool isUserSet) = 0;
    virtual void PublishMainCardChanged(SlotId slot) = 0;
    virtual void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) = 0;
}; // setDone为旧阶段结束，不冒充设备成功。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H
