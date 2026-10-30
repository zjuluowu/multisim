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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§8.2、§9.3、§14.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_NUMBER_QUERY_HOST_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_NUMBER_QUERY_HOST_PORT_H

#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
struct ImpuNumberResult { int32_t legacyResult; std::u16string number; };
struct NumberBackwrite {
    BindingRef binding;
    SessionRef sourceSession;
    uint64_t expectedIdentityRevision;
    NumberFieldRevisions expectedNumberRevisions;
    std::u16string number;
    NumberSource source;
};
// T ShowNumberQuery 所需的窄宿主端口。Core 实现 IMS 查询及既有队列投递，H 注入。
// IMPU 是回退来源，不是 T/A 持有 IMS 注册状态；投递返回不代表存储完成。
class NumberQueryHostPort {
public:
    virtual ~NumberQueryHostPort() = default;
    virtual ImpuNumberResult ReadImpuNumber(SlotId slot) = 0;
    virtual void PostIndependentNumberBackwrite(const NumberBackwrite &backwrite) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_NUMBER_QUERY_HOST_PORT_H
