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

#include "topology/application/topology_service.h"

namespace OHOS::Telephony::MultiSim {
TopologyService::TopologyService(const SlotCatalog &catalog, AccountStore &store, RuntimeEpoch epoch)
    : bindings_(catalog), observations_(epoch), store_(store), activationProgress_(epoch) {}

void TopologyService::BindRolePlanner(AccountRolePlanningPort &roles)
{
    roles_ = &roles; // H仅在发布前绑定一次；它和TopologyService不互相拥有生命周期。
}

/* 业务方法仅声明，以下是锁外I/O与最终接纳的协议，不是可调用实现：
 *
 * AcceptNumberAfterCardWrite / AcceptIndependentNumberBackwrite：
 * 1. T短提交域核查binding/session、身份及号码字段版本，ReserveNumberField。
 *    显示名、IMS、label、active改变不使号码资格失效；同号码冲突资格覆盖整个调用。
 * 2. 锁外WriteDisplayNumber；返回后复核并依旧节点分别接纳Current/Historical，
 *    每投影使用自己的numberRevision，当前成功/历史失败保留原部分效果。
 * 3. 调用已结束且结果已接纳后才释放；已写但目标失效进入核对，不盲重发写卡/写库。
 *
 * AcceptCardDiscoveryObservation：
 * 1. 与AcceptCardSessionInvalidation同一T接纳序列，observations_.AcceptDiscovery。
 *    保存旧/新绑定键版本并预留资格；序列/epoch失效立即停止尚未发出的步骤。
 * 2. 锁外MatchAccountByIccid；返回后复核观察及绑定键。向roles_请求角色片段，输入
 *    可以只有位置/既有策略，existingSubscription为空不强求插入前未知的新simId。
 *    父子关联、不嵌套等待；跨Owner冲突集合一次登记，失败不得持一个Owner资格等另一个。
 * 3. 经roles_->StillOwnsInitialRoleReservation复核A角色预留及T受影响键资格后，
 *    锁外WriteDiscoveredAccount一次旧Insert/Update。
 * 4. 返回后observations_.StillCurrent、绑定键和roles_->StillOwnsInitialRoleReservation分别复核。
 *    provider返回身份交T，
 *    角色片段经roles_->AcceptDiscoveredRoleProjection交A；不整行回灌或恢复旧快照。
 *    若已写而观察失效，保存assignedSubscription/部分效果进入核对，不绑定到新卡。
 *    通过FinalizeInitialRoleReservation(planned,{callFinished,storageEffect,projectionsAcceptedOrReconciled})
 *    交A核验释放；DB已写但观察失效时先核对，不以callFinished单独释放；T资格遵守相同结束/核对条件。
 * AcceptCardSessionInvalidation：按同一序列失效观察，再以原BindingRef撤销相关键；
 *    R合法观察校验不替代本T序列及锁外I/O后的复核，禁止回包时补贴当前会话。
 *
 * PrepareAccountReload / AcceptAccountReload：Current与Historical分别发起LoadAccounts，
 * 保存本次view/attempt、相关身份/各字段版本及A角色投影版本。失败只保留该view旧值并
 * 沿原预算重试，不阻止另一view成功；同次读取的storedRoles交roles_正式接纳入口。
 * 各T字段用AcceptReloadProjection按相关版本接纳，旧读取不覆盖新写的号码等字段；
 * 同一行较新名称可以保留，而未冲突号码/active仍可接受。不要从A当前态重造旧DB角色。
 *
 * BeginActivationProgress/AcceptActivationProgress由activationProgress_按request+session准入，
 * 在原两次Hook及失败出口更新槽进度；false不释放R资源、不证明设备静止。
 *
 * G2仍保留SetShowNumberToDB在该步骤GetTargetSimId(slot)的解析时点及旧错误轨迹；
 * 固定目标/陈旧拒绝为G3增强。名称、IMS、标签、初始化、current/all重载、Monitor、
 * provider/用户重连、卡探测、eSIM插入和普通/force/卫星旧写者全部按WU委托，不能双写。
 */
} // namespace OHOS::Telephony::MultiSim
