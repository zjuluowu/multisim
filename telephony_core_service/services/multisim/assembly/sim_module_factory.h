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

#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_FACTORY_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_FACTORY_H

#include <memory>
#include <variant>
#include "assembly/host_ports.h"
#include "runtime/card_runtime.h"
#include "infrastructure/account_store_adapter.h"
#include "infrastructure/assignment_store_adapter.h"
#include "infrastructure/legacy_output_adapter.h"
#include "compatibility/legacy_completion.h"
#include "compatibility/legacy_sim_facade.h"

namespace OHOS::Telephony::MultiSim {
struct ExistingSimBindings {
    SimRdbHelper &accountStore;
    MultiSimMonitor &monitor;
    RadioProtocolController &radio;
    std::vector<ProductionSlotBindings> slots;
    EsimManager *ordinaryEuicc;
    EsimManager *lowPriorityEuicc;
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
struct SimModuleDeleter { void operator()(SimModule *module) const; };
using SimModuleHandle = std::unique_ptr<SimModule, SimModuleDeleter>;
struct SimModuleInitFailure {
    enum class Stage { ValidateCatalog, BindExistingRuntime, BindHostPorts, RegisterCallbacks };
    Stage stage;
    int32_t legacyResult;
};
using SimModuleCreationResult = std::variant<SimModuleHandle, SimModuleInitFailure>;
// 只有契约，无工厂/销毁定义，无生产调用者；不会创建第二套活动对象。
// H未来构建并校验SlotCatalog、提供新epoch、沿原初始化发布时点创建模块；
// 返回失败须报告具体初始化阶段，不能用空句柄伪装成功。
SimModuleCreationResult CreateSimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog,
    RuntimeEpoch epoch, const ExistingSimBindings &existing);
// 新增排空协议不属于机械拆SO已具备能力：失败不卸载，Core最后才释放共享RIL。
// 不裸delete ISimManager/IEsimManager，不给现有公共虚表追加析构。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_FACTORY_H
