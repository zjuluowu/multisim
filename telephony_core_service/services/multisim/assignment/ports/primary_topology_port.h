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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_PRIMARY_TOPOLOGY_PORT_H
#define OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_PRIMARY_TOPOLOGY_PORT_H

#include <optional>
#include <string>
#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct PrimaryTargetFacts { BindingRef binding; std::string iccid; bool active; };
// A实际需要的T查询/映射入口，T实现，H注入；不暴露TopologyService具体类。
class PrimaryTopologyPort {
public:
    virtual ~PrimaryTopologyPort() = default;
    virtual std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const = 0;
    virtual RequestId BeginRouteMapping(const StepRef &parent, SlotTarget target) = 0;
}; // 子请求以parent关联返回；A不得持角色/设备资格同步等待T。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSIGNMENT_PORTS_PRIMARY_TOPOLOGY_PORT_H
