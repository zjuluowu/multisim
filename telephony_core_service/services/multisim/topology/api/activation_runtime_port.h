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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_RUNTIME_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_RUNTIME_PORT_H

#include "foundation/request_ref.h"

namespace OHOS::Telephony::MultiSim {
struct ActivationDeviceRequest { StepRef step; SessionRef session; bool enable; };
struct ActivationDeviceResult { StepRef step; SessionRef sentSession; EffectKnowledge effect; int32_t legacyResult; };
class ActivationResultSink {
public:
    virtual ~ActivationResultSink() = default;
    virtual void AcceptDeviceResult(const ActivationDeviceResult &result) = 0;
};
// T使用，R实现。保留原activeSimMutex_跨等待串行及PrimaryDone同锁关系；
// ctx_在cv.wait期间释放，不能将串行错误归因为全程持有ctx_或按槽拆分。
class ActivationRuntimePort {
public:
    virtual ~ActivationRuntimePort() = default;
    virtual void SetOrdinarySimActive(const ActivationDeviceRequest &request) = 0;
};
// T激活用例所需的既有厂商执行节点，T专属Adapter实现；T在原节点自行接纳进度清零。
// void只保持原Hook契约，不等于设备确认；不修改厂商ABI。
class ActivationHookPort {
public:
    virtual ~ActivationHookPort() = default;
    virtual void ApplyLegacySubStateHook(SlotId slot, bool enable) = 0;
    virtual void ApplyLegacyActiveCompleteHook(SlotId slot, bool enable) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_ACTIVATION_RUNTIME_PORT_H
