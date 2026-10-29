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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.2、§14.2～14.3、§22.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "assembly/sim_module_factory.h"
#include <utility>

namespace OHOS::Telephony::MultiSim {
// 此构造函数只让IDE看清实际类型/注入关系；私有且工厂无定义，无法进入生产。
SimModule::SimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch,
    const ExistingSimBindings &existing)
    : host_(std::move(host)), catalog_(std::move(catalog)), accountStore_(existing.accountStore),
      assignmentStore_(existing.accountStore), output_(existing.monitor, host_.callbacks), completion_(output_),
      runtime_(*catalog_, epoch, existing.slots, existing.radio, existing.ordinaryEuicc, existing.lowPriorityEuicc),
      topology_(*catalog_, accountStore_), assignment_(*catalog_, assignmentStore_, runtime_, assignmentOutput_, topology_),
      numbers_(topology_, runtime_, completion_), numberQuery_(topology_, runtime_, host_.numberQuery),
      activation_(topology_, assignment_, runtime_, activationHooks_), localCards_(runtime_),
      compatibility_(*catalog_, host_.legacyIngress, host_.callbacks, numbers_, numberQuery_, activation_,
          assignment_, localCards_)
{
    // 未来在发布前 runtime_.BindResultOwners(numbers_, activation_, assignment_)；
    // 本批不调用、不注册真实事件、不新建State/File/Records/Esim对象。
}
/* CreateSimModule/SimModuleDeleter 仅声明：
 * Core 侧创建SlotCatalog与共享RIL，保留Core原RIL注册 -> eSIM OnInit(可能访问66250)
 * -> SIM OnInit/注入CoreServiceSim -> 搜网/CMI的发布时点；不得调用本构造旁路双装配。
 * 机械拆分时仍沿原对象控制块/deleter；CardFileDetectManager当前先于Controller创建的
 * weak_ptr风险单独评审，不把本骨架作为已修正事实。
 * 关闭增强：停止受理 -> epoch失效 -> 反注册 -> 结束等待/保留未知效果 -> 排空回调/Hook
 * -> 确认代码退出 -> 匹配销毁。未排空不得卸载SO；SIM不得Stop共享RIL。
 */
} // namespace OHOS::Telephony::MultiSim
