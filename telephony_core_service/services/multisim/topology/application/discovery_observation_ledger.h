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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_DISCOVERY_OBSERVATION_LEDGER_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_DISCOVERY_OBSERVATION_LEDGER_H

#include <map>
#include <utility>
#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
// T私有接纳资格，不拥有R设备会话；所有观察和失效在同一T短提交域排序。
class DiscoveryObservationLedger final {
public:
    explicit DiscoveryObservationLedger(RuntimeEpoch epoch) : epoch_(epoch) {}
    bool AcceptDiscovery(const CardDiscoveryObservation &observation);
    bool AcceptInvalidation(const CardSessionInvalidation &invalidation);
    bool StillCurrent(const CardDiscoveryObservation &observation) const;
private:
    struct Entry { SessionRef session; uint64_t sequence; bool valid; };
    using Key = std::pair<SlotId, std::string>;
    RuntimeEpoch epoch_;
    std::map<Key, Entry> accepted_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_DISCOVERY_OBSERVATION_LEDGER_H
