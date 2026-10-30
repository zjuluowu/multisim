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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_LEGACY_ENTRY_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_LEGACY_ENTRY_H

#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
class SetShowNumberUseCase;
class ShowNumberQuery;
class ActivateSimUseCase;
// C sees only this delegation seam; the original business method remains declaration-only.
// H owns the concrete object; this API adds no runtime object or business state.
RequestId StartLegacyOrdinaryActivation(ActivateSimUseCase &entry, const ActivateSimCommand &command);
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_LEGACY_ENTRY_H
