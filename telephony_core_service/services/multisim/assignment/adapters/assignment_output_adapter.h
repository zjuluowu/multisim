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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §9.3、§14.4、§15。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_OUTPUT_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_OUTPUT_ADAPTER_H

#include "assignment/api/radio_execution_port.h"

namespace OHOS::Telephony::MultiSim {
class AssignmentOutputAdapter final : public AssignmentOutputPort {
public:
    void PublishPrimaryPhase(bool setDone, bool isUserSet) override;
    void PublishMainCardChanged(SlotId slot) override;
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) override;
};
// 仍仅声明：复用原Controller的PublishSetPrimaryEvent/SendMainCardBroadCast/
// AnnounceDefaultCellularDataSlotIdChanged的CES/广播参数和偏序。
// setDone=true是旧阶段结束，失败同样可能发送，不作为设备成功事实。
// A决定原输出节点；Adapter无业务缓存、不补发、不授权设备重试。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_ADAPTERS_ASSIGNMENT_OUTPUT_ADAPTER_H
