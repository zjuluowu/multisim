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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §7.1、§14.2～14.3、§22.1 G2a。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_HOST_PORTS_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_HOST_PORTS_H

#include <memory>
#include "compatibility/legacy_boundary_ports.h"
#include "topology/ports/number_query_host_port.h"

namespace OHOS::Telephony { class ITelRilManager; }
namespace OHOS::Telephony::MultiSim {
// H的Core侧实现保留在 services/core/，本文件仅是SIM私有工厂所需契约。
// Core创建并注册一个共享RIL，原搜网/IMS等消费者继续使用同一实例。
struct SimHostPorts {
    std::shared_ptr<ITelRilManager> sharedRil;
    LegacyIngressPort &legacyIngress;
    CallbackHostPort &callbacks;
    CompletionHostPort &completions;
    NumberQueryHostPort &numberQuery;
};
// sharedRil只保留引用，不在SIM Init/Stop共享RIL；Core端口的代码/对象必须活到SIM排空结束。
// EXT/HiSysEvent/IMS/卫星单例唯一定义处留Core或原所属库，不在SIM重新编译一份。
// 未来卫星/厂商换路/宿主反查按实际切片补窄端口，不创建万能HostContext。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_HOST_PORTS_H
