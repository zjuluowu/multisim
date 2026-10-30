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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACTIVATION_HOOK_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACTIVATION_HOOK_ADAPTER_H

#include "topology/api/activation_runtime_port.h"

namespace OHOS::Telephony::MultiSim {
// 厂商执行与通知分开；仅T激活用例可决定原Hook节点，不把Hook当业务完成。
class ActivationHookAdapter final : public ActivationHookPort {
public:
    void ApplyLegacySubStateHook(SlotId slot, bool enable) override;
    void ApplyLegacyActiveCompleteHook(SlotId slot, bool enable) override;
};
// 仍仅声明：按T的原节点复用UpdateSubState/SetActiveSimFunc厂商Hook；
// 原进度清零仍交T接纳，不持T可写状态，不修改Hook ABI或决定业务完成。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_ADAPTERS_ACTIVATION_HOOK_ADAPTER_H
