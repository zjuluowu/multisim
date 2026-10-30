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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_PRIMARY_EXECUTION_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_API_PRIMARY_EXECUTION_H
#include "foundation/slot_catalog.h"
namespace OHOS::Telephony::MultiSim {
enum class PrimarySwitchMode { ThreePhaseProtocol, WithoutModemRestart, AlreadyPrimary, VendorRouteMapping };
enum class ParameterWritePosition { BeforeDevice, AfterDevice, WithoutDevice };
struct PrimaryDecisionFacts {
    SlotDescriptor target;
    bool userSet;
    bool dedicatedPrimaryRilSupported;
    bool alreadyOnPrimaryModem;
    bool vendorMappingRequired;
}; // R真实能力/观察由应用提供；未知不默认false或M0，不从目录数量推断。
} // namespace OHOS::Telephony::MultiSim
#endif
