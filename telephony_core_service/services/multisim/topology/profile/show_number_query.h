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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§8.2、§9.3 SIM-023.c。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SHOW_NUMBER_QUERY_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SHOW_NUMBER_QUERY_H

#include "topology/topology_service.h"
#include "topology/api/show_number_runtime_port.h"
#include "topology/api/number_query_host_port.h"

namespace OHOS::Telephony::MultiSim {
// 无需统一编排器：只读卡缓存 -> T账户 -> Core IMS IMPU；查询不等异步回写。
class ShowNumberQuery final {
public:
    ShowNumberQuery(TopologyService &topology, ShowNumberRuntimePort &runtime, NumberQueryHostPort &host);
    ShowNumberRead ReadAtLegacyExecutionPoint(SlotTarget origin);
    void RunIndependentBackwrite(const NumberBackwrite &backwrite);
private:
    TopologyService &topology_;
    ShowNumberRuntimePort &runtime_;
    NumberQueryHostPort &host_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SHOW_NUMBER_QUERY_H
