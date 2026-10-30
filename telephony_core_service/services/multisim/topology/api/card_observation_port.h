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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_CARD_OBSERVATION_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_CARD_OBSERVATION_PORT_H

#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
// T应用声明、TopologyService实现，H绑定给R；不让R认识具体应用服务。
class CardObservationSink {
public:
    virtual ~CardObservationSink() = default;
    virtual void AcceptCardDiscoveryObservation(const CardDiscoveryObservation &observation) = 0;
    virtual void AcceptCardSessionInvalidation(const CardSessionInvalidation &invalidation) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_CARD_OBSERVATION_PORT_H
