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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.2、§7.1、§14.2～14.3、§22.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H

#include "assembly/sim_module_factory.h"
#include "runtime/card_runtime.h"
#include "topology/adapters/account_store_adapter.h"
#include "assignment/adapters/assignment_store_adapter.h"
#include "compatibility/output/legacy_output_adapter.h"
#include "compatibility/output/legacy_completion.h"
#include "compatibility/legacy_sim_facade.h"

#include "assignment/adapters/assignment_output_adapter.h"
#include "topology/adapters/activation_hook_adapter.h"
#include "topology/topology_service.h"
#include "assignment/assignment_service.h"
#include "topology/account_profile/set_show_number_use_case.h"
#include "topology/account_profile/show_number_query.h"
#include "topology/activation/activate_sim_use_case.h"

namespace OHOS::Telephony::MultiSim {
struct ExistingSimBindings {
    SimRdbHelper &accountStore;
    MultiSimMonitor &monitor;
    RadioProtocolController &radio;
    std::vector<ProductionSlotBindings> slots;
    EsimManager *euicc; // 一个原Manager内部拥有普通/低优先级两组EsimFile。
};
// 目标SIM侧装配图；所有引用由Core/原生产对象持有，需逐WU停止旧writer后才能接管。
// 声明顺序=依赖创建顺序；反序销毁，但反序本身不等于已排空异步回调。
class SimModule final {
public:
    LegacySimFacade &LegacyEntry() { return compatibility_; }
    enum class DrainResult { Drained, CallbacksStillInFlight, DeviceEffectsUnresolved };
    DrainResult StopAcceptingAndDrain(); // G2c新增协议，仅声明，机械拆分不默认具备。
private:
    friend struct SimModuleFactoryAccess;
    friend struct SimModuleDeleter;
    SimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch,
        const ExistingSimBindings &existing);
    ~SimModule() = default;
    SimHostPorts host_;
    std::shared_ptr<const SlotCatalog> catalog_;
    AccountStoreAdapter accountStore_;
    AssignmentStoreAdapter assignmentStore_;
    LegacyOutputAdapter output_;
    AssignmentOutputAdapter assignmentOutput_;
    ActivationHookAdapter activationHooks_;
    LegacyCompletion completion_;
    CardRuntime runtime_;
    TopologyService topology_;
    AssignmentService assignment_;
    SetShowNumberUseCase numbers_;
    ShowNumberQuery numberQuery_;
    ActivateSimUseCase activation_;
    LocalCardRequests localCards_;
    LegacySimFacade compatibility_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_H
