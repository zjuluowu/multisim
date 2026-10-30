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
#include "assembly/manager_publication.h"
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
    EsimManager *euicc; // 原唯一Manager；内部两File视图，不是两个Manager或两个设备域。
};
// 同SO迁移附着图；所有引用由Core/原生产对象持有，需逐WU停止旧writer后才能接管。
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
// 迁移附着接缝只接受已有对象，不是独立SO最终创建边界。
// 只有契约，无工厂/销毁定义，无生产调用者；不会创建第二套活动对象。
// H未来构建并校验SlotCatalog、提供新epoch、沿原初始化发布时点创建模块；
// 返回失败须报告具体初始化阶段，不能用空句柄伪装成功。
SimModuleCreationResult AttachExistingSimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog,
    RuntimeEpoch epoch, const ExistingSimBindings &existing);
// 独立SO最终边界：Core只提供HostPorts/目录/epoch，SIM侧创建并拥有原Manager、
// Controller/store及运行对象；不再要求Core取得这些内部对象或链接其具体构造符号。
// 此工厂与附着工厂按迁移阶段择一使用，不能为同一活动写权同时创建两套执行者。
class OwnedSimModule final {
public:
    // 保留原步骤：eSIM OnInit -> Core将返回manager发布CMI -> 确认 -> SIM OnInit。
    // 实现须创建原具体对象、OnInit，并RecordEsimInitialization；不得提前创建/初始化SIM。
    EsimManagerInitialization InitializeEsimAtLegacyPoint(int32_t legacySlotCount);
    bool ConfirmEsimPublished(const std::shared_ptr<IEsimManager> &published)
    {
        return publication_.ConfirmEsimPublished(published);
    }
    // 实现只在EsimPublished阶段创建/初始化SIM，再RecordSimInitialization；返回原同一控制块。
    SimManagerInitialization InitializeSimAtLegacyPoint(int32_t legacySlotCount);
    bool ConfirmSimPublished(const std::shared_ptr<ISimManager> &published)
    {
        return publication_.ConfirmSimPublished(published);
    }
    // 句柄为原生产具体对象向原接口转换的同一shared_ptr控制块；无虚析构接口不裸delete。
    const ManagerPublicationSequence &Publication() const { return publication_; }
    LegacySimFacade &LegacyEntry();
private:
    friend struct OwnedSimModuleDeleter;
    friend struct OwnedSimModuleFactoryAccess;
    explicit OwnedSimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch,
        bool hasEsim);
    ~OwnedSimModule(); // 定义留SIM产物，失败清理/匹配具体对象销毁亦由SIM负责。
    struct ProductionObjects; // 原SimManager、唯一EsimManager、Controller/store及同一RIL引用。
    std::unique_ptr<ProductionObjects> production_;
    ManagerPublicationSequence publication_;
};
struct OwnedSimModuleDeleter { void operator()(OwnedSimModule *module) const; };
using OwnedSimModuleHandle = std::unique_ptr<OwnedSimModule, OwnedSimModuleDeleter>;
using OwnedSimModuleCreationResult = std::variant<OwnedSimModuleHandle, SimModuleInitFailure>;
OwnedSimModuleCreationResult CreateOwnedSimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog,
    RuntimeEpoch epoch);
// 真实创建/初始化/导出/失败排空实现仍待G2b；上述接口和发布协议不是生产完成证明。
// 新增排空协议不属于机械拆SO已具备能力：失败不卸载，Core最后才释放共享RIL。
// 不裸delete ISimManager/IEsimManager，不给现有公共虚表追加析构。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_SIM_MODULE_FACTORY_H
