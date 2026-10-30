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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3、§9.2～9.5、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/application/topology_service.h"

namespace OHOS::Telephony::MultiSim {
TopologyService::TopologyService(const SlotCatalog &catalog, AccountStore &store, RuntimeEpoch epoch)
    : bindings_(catalog), discovery_(epoch), store_(store) {}
bool TopologyService::FenceRuntimeSession(const SessionRef &session, uint64_t sequence)
{
    return discovery_.Fence(session, sequence);
}
AccountReloadTicket TopologyService::BeginAccountReload(AccountReadView view, uint64_t assignmentRevision)
{
    return accounts_.BeginReload(view, assignmentRevision);
}
AccountReloadAcceptance TopologyService::AcceptAccountReload(const AccountReloadTicket &ticket,
    const AccountLoadResult &result)
{
    if (result.view != ticket.view || result.attempt != ticket.attempt) { return {false, 0, 0}; }
    return accounts_.AcceptReload(ticket, result.projectionReplaceable, result.rows);
}
std::optional<ActivationProgressTicket> TopologyService::BeginActivationProgress(RequestId request, SlotId slot,
    const SessionRef &session)
{
    auto ticket = activationProgress_.Prepare(request, slot, session);
    if (!ticket || !activationProgress_.Begin(*ticket)) { return std::nullopt; }
    return ticket;
}
bool TopologyService::ClearActivationProgress(const ActivationProgressTicket &ticket)
{
    return activationProgress_.Clear(ticket);
}
bool TopologyService::IsActivationInProgress(SlotId slot) const { return activationProgress_.IsInProgress(slot); }


/* 伪代码，AcceptNumberAfterCardWrite / AcceptIndependentNumberBackwrite 尚无定义：
 * 1. 在 T 短提交域复核 target.binding / session / currentNumberVersion / historicalNumberVersion，ReserveNumberField(target)。
 * 2. 锁外 store_.WriteDisplayNumber({target.binding.subscription, number})。
 *    来源 MultiSimController::SetShowNumberToDB -> SimRdbHelper::UpdateDataBySimId。
 * 3. 返回同一提交域复核相关输入，按旧错误依次接纳：
 *    accounts_.AcceptNumberVisibility(..., AccountReadView::Current, number)；
 *    accounts_.AcceptNumberVisibility(..., AccountReadView::Historical, number)。
 *    每一步的可见性独立；DB成功/当前成功/历史失败不得整体回滚或隐藏。
 * 4. 只有调用已结束且结果已接纳才 ReleaseAfterStoreCallFinished；DB已写但目标失效
 *    进入核对，不能只拒绝内存发布就宣称没有效果。未知效果不盲目重写。
 *
 * 这里是固定目标的 G3b 推荐。G2 结构切片须保留旧 SetShowNumberToDB 在此步骤才
 * GetTargetSimId(slot) 的解析时点，并保留 slot 越界返回 false(整数0) 等旧异常轨迹；
 * 这些兼容错误不能从本骨架自造的新枚举推导，修正须独立裁决。
 *
 * 迁移闭包：InitData/InitIccId/InitShowNumber/SetShowName/SaveImsSwitch、
 * GetListFromDataBase/GetAllListFromDataBase、Monitor RefreshData/用户切换/provider重连、
 * ForgetAllData、卡探测、InsertEsimData、force/卫星 active 及查询异步回写全部委托同一入口。
 * 建行/重载的角色字段使用 A 应用提供的片段；T 不持有或重写角色事实。
 *
 * DiscoverCard协议（真实provider及观察桥未接入，非业务成功桩）：
 * T短域 discovery_.Begin(observation, bindings_)固定ticket -> 锁外store_.MatchAccountByIccid。
 * 返回短域复核ticket，保留查询失败/旧卡isEsim不匹配等原错误；新卡matchedSubscription为空。
 * 匹配查询或纯规划失败调用 EndPreparationFailure(ticket,实际callFinished,bindings_)关闭准备，
 * 未结束的查询不释放资格。原Monitor预算重试显式调用RetryAfterKnownNoEffect，
 * 同一仍有效观察/slotrevision发新admission；不强求新R事件，重复观察也不自动发令。
 * 锁外 A.PrepareAccountRoleFragment({slot,iccid,matchedSubscription,sequence})规划角色。
 * 再次短域复核ticket并 A.IsAccountRoleFragmentCurrent；任何失败均不发ApplyAccountRow。
 * 锁外一次原Insert/Rebind：store_.ApplyAccountRow({T字段, A片段仅includeInStorage时})，
 * 新建行按真实配置给初始角色；旧卡多槽rebind不覆盖旧角色，由provider落实身份。
 * 返回后A.AcceptAccountRoleStorage(fragment,storedIdentity,实际EffectKnowledge)处理自己的字段：
 * 角色版本变化只NeedsReconciliation，不能回滚或重写已存片段。
 * T最终短域 CompleteStored(ticket,result.effect,storedIdentity,result.storage.callFinished,bindings_)消费票据并提交绑定。
 * 仅已确认存储、身份已解析且ticket仍有效时发布绑定；已写/未知而资格失效则
 * needsFieldReconciliation，沿原读源分字段核对，不自动重放建行、不把DB效果当成未发生。
 * 只有ConfirmedNotApplied/Rejected/NotSent且调用结束可按原预算显式重试；
 * Unknown/Applied或旧ticket一律不重发，旧失败不能覆盖已重试的新ticket。
 * 新绑定不是current/all缓存最终刷新；后续原各自重载仍独立失败/替换。
 * ObserveCardDiscovery只投递此具名用例，不在R锁内做数据库工作；R会话失效Fence不能排在
 * 旧发现长IO之后等待，必须先撤销T票据后再暴露替换会话。撤销/解绑也先获同序资格，
 * Unbind计划保留原标签/active条件，存后分别核对、按binding revision提交移除。
 *
 * 重载：分别BeginRoleReload/BeginAccountReload -> 一次LoadAccounts(ticket) -> T接纳rows，
 * A.AcceptRoleReload(ticketA,result.roles,result.projectionReplaceable)，不因T冲突抑制A核对。
 * 每个view独立尝试、结果、重试预算；失败保留本view，另一view可继续。
 * G3字段版本保护拒绝旧重载覆盖本地较新字段；G2仍保持原读源、返回、节点与重试。

 */
} // namespace OHOS::Telephony::MultiSim
