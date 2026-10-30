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

#include "assembly/sim_module.h"
#include <utility>

namespace OHOS::Telephony::MultiSim {
// 此构造函数只让IDE看清实际类型/注入关系；私有且工厂无定义，无法进入生产。
SimModule::SimModule(SimHostPorts host, std::shared_ptr<const SlotCatalog> catalog, RuntimeEpoch epoch,
    const ExistingSimBindings &existing)
    : host_(std::move(host)), catalog_(std::move(catalog)), accountStore_(existing.accountStore),
      assignmentStore_(existing.accountStore), output_(existing.monitor, host_.callbacks), completion_(output_),
      runtime_(*catalog_, epoch, existing.slots, existing.radio, *host_.sharedRil, existing.euicc),
      topology_(*catalog_, accountStore_, epoch),
      assignment_(*catalog_, assignmentStore_, runtime_, assignmentOutput_, topology_, completion_, epoch),
      numbers_(topology_, runtime_, completion_, epoch), numberQuery_(topology_, runtime_, host_.numberQuery),
      activation_(topology_, assignment_, runtime_, activationHooks_, completion_, epoch),
      localCards_(runtime_, assignment_),
      compatibility_(*catalog_, host_.legacyIngress, host_.callbacks, numbers_, numberQuery_, activation_,
          assignment_, localCards_)
{
    // 附着工厂须先验证catalog/sharedRil与原对象身份非空且唯一；本构造仍无生产调用者。
    topology_.BindRolePlanner(assignment_); // 构造后绑定窄入口，避免A/T构造环；未绑定不得发布。
    runtime_.BindResultSinks(numbers_, activation_, assignment_, topology_);
    // 只绑定模块内部引用，不注册第二套真实事件，不新建State/File/Records/Esim对象。
}
/* AttachSimModuleToExisting/SimModuleDeleter 仅声明：
 * Core 侧创建SlotCatalog与共享RIL，保留Core原RIL注册 -> eSIM OnInit(可能访问66250)
 * -> SIM OnInit/注入CoreServiceSim -> 搜网/CMI的发布时点；不得调用本构造旁路双装配。
 * G2b CreateOwnedSimServices在SIM内创建原对象，以SimServicePublicationPort交Core发布，
 * 不接收ExistingSimBindings，不强制目录；按原返回传播规则记录阶段，旧未传播OnInit false不升级错误；
 * 按既有实际能力处理失败路径，不能宣称已经安全卸载或撤回已发布对象。
 * 原共享控制块/匹配deleter的代码必须活到最后句柄释放；CardFileDetectManager当前先于Controller创建的
 * weak_ptr风险单独评审，不把本骨架作为已修正事实。
 * 关闭增强：停止受理 -> epoch失效 -> 反注册 -> 结束等待/保留未知效果 -> 排空回调/Hook
 * -> 确认代码退出 -> 匹配销毁。未排空不得卸载SO；SIM不得Stop共享RIL。
 */
} // namespace OHOS::Telephony::MultiSim
