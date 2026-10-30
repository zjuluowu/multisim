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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_CARD_DISCOVERY_SINK_H
#define OHOS_TELEPHONY_MULTISIM_CARD_DISCOVERY_SINK_H

#include "topology/api/account_lifecycle.h"
namespace OHOS::Telephony::MultiSim {
// H绑定R->T窄出口；R只认识此端口。R换卡/撤销/替换在暴露新session前同步Fence。
// Fence只做T短状态提交，无DB/RIL/外部回调；失败不发布新session为可接纳观察。
class CardDiscoverySink {
public:
    virtual ~CardDiscoverySink() = default;
    virtual bool FenceRuntimeSession(const SessionRef &session, uint64_t observationSequence) = 0;
    virtual void ObserveCardDiscovery(const CardDiscoveryObservation &observation) = 0;
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_CARD_DISCOVERY_SINK_H
