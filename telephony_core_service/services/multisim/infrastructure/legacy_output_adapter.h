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

#ifndef OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_LEGACY_OUTPUT_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_LEGACY_OUTPUT_ADAPTER_H

#include "compatibility/legacy_boundary_ports.h"
#include "assignment/ports/radio_execution_port.h"
#include "topology/ports/activation_runtime_port.h"

namespace OHOS::Telephony { class MultiSimMonitor; }
namespace OHOS::Telephony::MultiSim {
class LegacyOutputAdapter final : public LegacyOutputPort {
public:
    LegacyOutputAdapter(MultiSimMonitor &monitor, CallbackHostPort &callbacks)
        : monitor_(monitor), callbacks_(callbacks) {}
    void NotifyOriginalAccountChanged() override;
    void TransferOriginalShowNumberResult(CompletionId completion, int32_t legacyResult) override;
    void DeliverOriginalInt(const CompletionRoute &route, int32_t legacyResult) override;
private:
    MultiSimMonitor &monitor_;
    CallbackHostPort &callbacks_;
};
// 同文件是因为同属I旧输出技术；分别实现C/A的窄端口，不合成业务事件总线。
class AssignmentOutputAdapter final : public AssignmentOutputPort {
public:
    void PublishPrimaryPhase(bool setDone, bool isUserSet) override;
    void PublishMainCardChanged(SlotId slot) override;
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId subscription) override;
};
// 厂商执行与通知分开；仅T激活用例可决定原Hook节点，不把Hook当业务完成。
class ActivationHookAdapter final : public ActivationHookPort {
public:
    void ApplyLegacySubStateHook(SlotId slot, bool enable) override;
    void ApplyLegacyActiveCompleteHook(SlotId slot, bool enable) override;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_LEGACY_OUTPUT_ADAPTER_H
