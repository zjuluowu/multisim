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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "account/topology_service.h"
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
TopologyService::TopologyService(const SlotCatalog &catalog, AccountStore &store, RuntimeEpoch epoch)
    : bindings_(catalog), observations_(epoch), store_(store),
      useLegacySingleSlotRoles_(catalog.Capacities().configured == 1), activationProgress_(epoch) {}

void TopologyService::BindRolePlanner(AccountRolePlanningPort &roles)
{
    roles_ = &roles; // H仅在发布前绑定一次；它和TopologyService不互相拥有生命周期。
}
bool TopologyService::AcceptSlotAssembly(SlotId slot, bool assembled)
{ return bindings_.AcceptAssemblyState(slot, assembled); }

std::optional<ActivationTransition> TopologyService::PrepareActivationProgress(const ActivationTransition &transition)
{ return activationProgress_.Prepare(transition); }
bool TopologyService::BeginActivationProgress(const ActivationTransition &transition)
{ return activationProgress_.Begin(transition); }
bool TopologyService::AcceptActivationProgress(const StepRef &owner, const SessionRef &session, bool inProgress)
{ return activationProgress_.Accept(owner, session, inProgress); }
bool TopologyService::ReadLegacyActivationProgress(SlotId slot) const
{ return activationProgress_.ReadLegacy(slot); }

RequestId TopologyService::BeginRouteMapping(const StepRef &, SlotTarget)
{ throw std::logic_error("third-card route mapping is outside the Target C0 topology closure"); }

std::optional<PrimaryTargetFacts> TopologyService::ReadPrimaryTarget(SlotId slot) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto binding = bindings_.Find(slot);
    if (!binding || !bindings_.StillMatches(*binding)) { return std::nullopt; }
    const auto account = accounts_.Read(binding->subscription);
    if (!account || !(account->subscription == binding->subscription) || account->iccid.empty() ||
        !account->current) {
        return std::nullopt;
    }
    return PrimaryTargetFacts {*binding, account->iccid, account->current->active};
}

void TopologyService::AcceptCardDiscoveryObservation(const CardDiscoveryObservation &observation)
{
    (void)DiscoverAccount(observation);
}

DiscoveryAcceptance TopologyService::DiscoverAccount(const CardDiscoveryObservation &observation)
{
    const AccountStoreWriteResult noWrite {0, false, false};
    DiscoveryAcceptance rejected {0, false, std::nullopt, noWrite};
    std::optional<DiscoveryStoreTicket> ticket;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        if (!observations_.AcceptDiscovery(observation)) { return rejected; }
        ticket = observations_.BeginStore(observation, bindings_);
        if (!ticket || !roles_) { return rejected; }
    }

    const auto matched = store_.MatchAccountByIccid(observation.iccid);
    if (!matched.callFinished) {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        (void)observations_.EndPreparationFailure(*ticket, false, bindings_);
        rejected.storage = {matched.legacyResult, false, false};
        return rejected;
    }
    const std::optional<SubscriptionId> existingSubscription = matched.existing ?
        std::make_optional(matched.existing->subscription) : std::nullopt;
    if (existingSubscription) {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        for (const auto &slot : bindings_.ReadRuntimeSlots()) {
            if (slot.acceptedBinding && slot.acceptedBinding->subscription == *existingSubscription &&
                !(slot.slot == observation.session.application.slot)) {
                (void)observations_.EndPreparationFailure(*ticket, true, bindings_);
                rejected.storage = {matched.legacyResult, false, true};
                return rejected;
            }
        }
    }
    const auto rolePlan = roles_->PrepareAccountRoleFragment(
        {existingSubscription, observation.session.application.slot, useLegacySingleSlotRoles_});
    const auto preparationValid = rolePlan && roles_->StillOwnsInitialRoleReservation(*rolePlan);
    bool stillPrepared = false;
    {
        std::lock_guard<std::recursive_mutex> guard(commitLock_);
        stillPrepared = preparationValid && observations_.StillCurrent(observation) &&
            bindings_.ReadPositionRevision(observation.session.application.slot).value_or(0) == ticket->bindingRevision;
        if (!stillPrepared) {
            (void)observations_.EndPreparationFailure(*ticket, true, bindings_);
        }
    }
    if (!stillPrepared) {
        if (rolePlan) {
            (void)roles_->FinalizeInitialRoleReservation(*rolePlan,
                {true, EffectKnowledge::ConfirmedNotApplied, true});
        }
        rejected.storage = {rolePlan ? 0 : matched.legacyResult, false, true};
        return rejected;
    }

    const AccountDiscoveryWritePlan plan {observation, ticket->bindingRevision, existingSubscription,
        {observation.session.application.slot, observation.iccid,
         observation.legacyLabel ? observation.legacyLabel :
             (matched.existing ? matched.existing->legacyLabel : std::nullopt),
         observation.active, observation.isEsim}, *rolePlan};
    const auto stored = store_.WriteDiscoveredAccount(plan);

    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const bool stillValid = observations_.StillCurrent(observation) &&
        bindings_.ReadPositionRevision(observation.session.application.slot).value_or(0) == ticket->bindingRevision;
    const bool storageKnown = stored.callFinished && stored.writeConfirmed &&
        stored.effect == EffectKnowledge::ConfirmedApplied && stored.assignedSubscription;
    const bool roleOwned = roles_->StillOwnsInitialRoleReservation(*rolePlan);
    const bool identityMatches = !existingSubscription ||
        (stored.assignedSubscription && *stored.assignedSubscription == *existingSubscription);
    if (storageKnown && identityMatches && stillValid && roleOwned && stored.assignedSubscription) {
        const auto account = accounts_.Read(*stored.assignedSubscription);
        if (account && (account->iccid != observation.iccid)) {
            (void)observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
                stored.callFinished, bindings_, false);
            return {stored.legacyResult, false, stored.assignedSubscription,
                {stored.legacyResult, stored.writeConfirmed, stored.callFinished}};
        }
        if (!roles_->AcceptDiscoveredRoleProjection(*rolePlan, *stored.assignedSubscription)) {
            (void)observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
                stored.callFinished, bindings_, false);
            return {stored.legacyResult, false, stored.assignedSubscription,
                {stored.legacyResult, stored.writeConfirmed, stored.callFinished}};
        }
        if (!account) {
            const AccountProjection projection {{}, {},
                observation.legacyLabel ? observation.legacyLabel :
                    (matched.existing ? matched.existing->legacyLabel : std::nullopt),
                std::nullopt, observation.active, {}, observation.session.application.slot};
            const AccountRecord discovered {*stored.assignedSubscription, observation.iccid, 1,
                projection, projection};
            if (accounts_.AcceptDiscoveredAccount(discovered) != AccountAcceptance::Accepted) {
                (void)observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
                    stored.callFinished, bindings_, false);
                return {stored.legacyResult, false, stored.assignedSubscription,
                    {stored.legacyResult, stored.writeConfirmed, stored.callFinished}};
            }
        }
        const auto accepted = observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
            stored.callFinished, bindings_);
        (void)roles_->FinalizeInitialRoleReservation(*rolePlan,
            {stored.callFinished, stored.effect, accepted.bindingAccepted});
        return {stored.legacyResult, accepted.bindingAccepted, stored.assignedSubscription,
            {stored.legacyResult, stored.writeConfirmed, stored.callFinished}};
    }

    (void)observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
        stored.callFinished, bindings_, false);
    const bool reconciledNoEffect = stored.callFinished && stored.effect != EffectKnowledge::ConfirmedApplied &&
        stored.effect != EffectKnowledge::Unknown;
    (void)roles_->FinalizeInitialRoleReservation(*rolePlan,
        {stored.callFinished, stored.effect, reconciledNoEffect});
    return {stored.legacyResult, false, stored.assignedSubscription,
        {stored.legacyResult, stored.writeConfirmed, stored.callFinished}};
}
void TopologyService::AcceptCardSessionInvalidation(const CardSessionInvalidation &invalidation)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    observations_.AcceptInvalidation(invalidation);
}
void TopologyService::AcceptCardRemoval(const BindingRef &expected)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    bindings_.RemoveBinding(expected);
}

std::optional<BindingRef> TopologyService::FindBinding(SlotId slot) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    return bindings_.Find(slot);
}
std::optional<AccountView> TopologyService::ReadAccount(SubscriptionId subscription, AccountReadView view) const
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto record = accounts_.Read(subscription);
    const auto projection = accounts_.ReadProjection(subscription, view);
    if (!record || !projection) { return std::nullopt; }
    return AccountView {subscription, view, projection->displayName, projection->displayNumber,
        projection->active, record->identityRevision, projection->revisions};
}

AccountAcceptance TopologyService::AcceptActivationProjection(SubscriptionId subscription,
    AccountReadView view, uint64_t expectedActiveRevision, bool active)
{ return accounts_.AcceptActiveProjection(subscription, view, expectedActiveRevision, active); }

std::optional<BindingRef> TopologyService::AcceptStoredDiscovery(const CardDiscoveryObservation &observation,
    const AccountRecord &account, const AccountDiscoveryStoreResult &stored)
{
    std::lock_guard<std::recursive_mutex> guard(commitLock_);
    const auto existingIdentity = accounts_.FindByIccid(observation.iccid);
    if ((existingIdentity && !(*existingIdentity == account.subscription)) ||
        !stored.callFinished || !stored.writeConfirmed || stored.effect != EffectKnowledge::ConfirmedApplied ||
        !stored.assignedSubscription || !(*stored.assignedSubscription == account.subscription) ||
        account.iccid != observation.iccid || account.identityRevision == 0) {
        return std::nullopt;
    }
    (void)observations_.AcceptDiscovery(observation);
    const auto ticket = observations_.BeginStore(observation, bindings_);
    if (!ticket) { return std::nullopt; }
    const auto existing = accounts_.Read(account.subscription);
    if (!existing) {
        if (accounts_.AcceptDiscoveredAccount(account) != AccountAcceptance::Accepted) { return std::nullopt; }
    } else if (existing->iccid != account.iccid || existing->identityRevision != account.identityRevision) {
        return std::nullopt;
    }
    const auto accepted = observations_.CompleteStored(*ticket, stored.effect, stored.assignedSubscription,
        stored.callFinished, bindings_);
    if (!accepted.bindingAccepted) { return std::nullopt; }
    return bindings_.Find(observation.session.application.slot);
}

/* DiscoverAccount 已执行写前资格、锁外I/O与最终接纳；下列其余方法仍保留协议说明：
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
 *    observations_.BeginStore固定独立admission/绑定revision；序列/epoch失效停止尚未发出的步骤。
 *    匹配读取或纯规划失败走EndPreparationFailure；已结束确认未生效时原用例按旧预算
 *    RetryAfterKnownNoEffect在同一有效观察上取得新admission，不要求新R事件、不因重复观察重发。
 * 2. 锁外MatchAccountByIccid；返回后复核观察及绑定键。向roles_请求角色片段，输入
 *    可以只有位置/既有策略，existingSubscription为空不强求插入前未知的新simId。
 *    父子关联、不嵌套等待；跨Owner冲突集合一次登记，失败不得持一个Owner资格等另一个。
 * 3. 经roles_->StillOwnsInitialRoleReservation复核A角色预留及T受影响键资格后，
 *    锁外WriteDiscoveredAccount一次旧Insert/Update。
 * 4. 返回后observations_.StillCurrent、绑定键和roles_->StillOwnsInitialRoleReservation分别复核。
 *    provider返回身份/effect/callFinished交observations_.CompleteStored最终接纳绑定或登记核对；
 *    Unknown/已写或未结束不能重试，旧失败不影响后来票据；
 *    角色片段经roles_->AcceptDiscoveredRoleProjection交A；不整行回灌或恢复旧快照。
 *    若已写而观察失效，保存assignedSubscription/部分效果进入核对，不绑定到新卡。
 *    通过FinalizeInitialRoleReservation(planned,{callFinished,storageEffect,projectionsAcceptedOrReconciled})
 *    交A核验释放；DB已写但观察失效时先核对，不以callFinished单独释放；T资格遵守相同结束/核对条件。
 * AcceptCardSessionInvalidation：按同一序列失效观察，再以原BindingRef撤销相关键；
 *    R合法观察校验不替代本T序列及锁外I/O后的复核，禁止回包时补贴当前会话。
 *
 * PrepareAccountReload / AcceptAccountReload：Current与Historical分别发起LoadAccounts，
 * 保存本次view/attempt、相关身份/各字段版本及A角色投影版本。失败只保留该view旧值并
 * 沿原预算重试，不阻止另一view成功；同次读取的storedRoles交roles_->AcceptReloadedRoleProjectionSet。
 * T在发起时accounts_.BeginReload生成独立view/attempt及成员快照；结果按accounts_.AcceptReload
 * 接纳整次读取与成员移除，再用AcceptReloadProjection按相关版本接纳字段；同值重载不增号码版本，
 * 旧读取不覆盖新写的号码等字段，也不能删除读取后新增/更新的成员；
 * 同一行较新名称可以保留，而未冲突号码/active仍可接受。不要从A当前态重造旧DB角色。
 *
 * 原执行接纳点先PrepareActivationProgress取得T admission，再BeginActivationProgress；
 * AcceptActivationProgress按完整epoch/owner/request/step/attempt/session准入，
 * 在原两次Hook及失败出口更新槽进度；false不释放R资源、不证明设备静止。
 *
 * G2仍保留SetShowNumberToDB在该步骤GetTargetSimId(slot)的解析时点及旧错误轨迹；
 * 固定目标/陈旧拒绝为G3增强。名称、IMS、标签、初始化、current/all重载、Monitor、
 * provider/用户重连、卡探测、eSIM插入和普通/force/卫星旧写者全部按WU委托，不能双写。
 */
} // namespace OHOS::Telephony::MultiSim
