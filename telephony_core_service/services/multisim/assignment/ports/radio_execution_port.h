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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§9.3、§11.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H

#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct RadioSwitchRequest { StepRef step; SlotId target; RadioDomainId domain; };
struct RadioSwitchResult {
    StepRef step;
    SlotId memberSlot;
    uint32_t protocolSession;
    uint32_t phase;
    EffectKnowledge effect;
    int32_t legacyResult;
};
// A 使用；R CardRuntime 实现并复用 RadioProtocolController，不在此新造设备协议。
class RadioExecutionPort {
public:
    virtual ~RadioExecutionPort() = default;
    virtual void SwitchPrimaryRadio(const RadioSwitchRequest &request) = 0;
    virtual void EndLegacySenseSwitch(const RadioDomainId &domain) = 0;
};
// A 旧广播/阶段出口；I AssignmentOutputAdapter 实现，阶段结束不是成功确认。
class AssignmentOutputPort {
public:
    virtual ~AssignmentOutputPort() = default;
    virtual void PublishPrimaryPhase(bool setDone, bool isUserSet) = 0;
    virtual void PublishMainCardChanged(SlotId slot) = 0;
    virtual void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_RADIO_EXECUTION_PORT_H
