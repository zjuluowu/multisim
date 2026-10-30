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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2～4.3、§6.3、§9.2～9.3、§13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_TOPOLOGY_SERVICE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_TOPOLOGY_SERVICE_H

#include <map>
#include <optional>
#include "topology/api/topology_requests.h"
#include "topology/domain/account_registry.h"
#include "topology/domain/binding_index.h"
#include "topology/discovery/discovery_observation_ledger.h"
#include "topology/activation/activation_progress.h"
#include "topology/api/account_store.h"
#include "topology/api/account_role_planning_port.h"
#include "topology/api/card_observation_port.h"
#include "assignment/api/primary_topology_port.h"

namespace OHOS::Telephony::MultiSim {
struct ReloadAccountInput {
    SubscriptionId subscription;
    uint64_t expectedIdentityRevision;
    AccountFieldRevisions expectedFields;
};
struct ReloadRequest {
    AccountLoadRequest storeRequest;
    AccountReloadTicket acceptance; // T生成独立view/attempt及成员快照，实际读源仍由storeRequest指定。
    uint64_t expectedRoleProjectionRevision;
    std::vector<ReloadAccountInput> affected;
};
struct AccountFieldReservation { uint64_t token; SubscriptionId subscription; }; // 非DB锁。
// T唯一事实接纳入口；各方法在所属短提交域工作，不持锁跨DB/RIL/其他Owner调用。
class TopologyService final : public CardObservationSink, public PrimaryTopologyPort {
public:
    TopologyService(const SlotCatalog &catalog, AccountStore &store, RuntimeEpoch epoch);
    void BindRolePlanner(AccountRolePlanningPort &roles); // H在A创建后、发布前绑定，未绑定即初始化失败。
    std::optional<BindingRef> FindBinding(SlotId slot) const;
    std::optional<AccountView> ReadAccount(SubscriptionId subscription,
        AccountReadView view = AccountReadView::Current) const;
    int32_t SetDisplayName(SlotTarget origin, const std::u16string &name);
    int32_t SetSimLabel(SubscriptionId subscription, int32_t legacyLabel);
    int32_t SaveImsPreference(SlotTarget origin, int32_t preference);
    int32_t QueryImsPreference(SlotTarget origin, int32_t &preference);
    std::optional<NumberExecutionTarget> PrepareNumberTarget(SlotTarget origin, const SessionRef &session) const;
    NumberAcceptance AcceptNumberAfterCardWrite(const NumberExecutionTarget &target, const std::u16string &number);
    NumberAcceptance AcceptIndependentNumberBackwrite(const NumberExecutionTarget &target,
        const std::u16string &number);
    ReloadRequest PrepareAccountReload(AccountReadView view, uint64_t loadAttempt);
    void AcceptAccountReload(const ReloadRequest &request, const AccountLoadResult &loaded);
    void AcceptCardDiscoveryObservation(const CardDiscoveryObservation &observation) override;
    void AcceptCardSessionInvalidation(const CardSessionInvalidation &invalidation) override;
    void AcceptCardRemoval(const BindingRef &expected);
    std::optional<ActivationTransition> PrepareActivationProgress(const ActivationTransition &transition);
    bool BeginActivationProgress(const ActivationTransition &transition);
    bool AcceptActivationProgress(const StepRef &owner, const SessionRef &session, bool inProgress);
    bool ReadLegacyActivationProgress(SlotId slot) const;
    AccountAcceptance AcceptActivationProjection(SubscriptionId subscription, AccountReadView view,
        uint64_t expectedActiveRevision, bool active);
    std::optional<PrimaryTargetFacts> ReadPrimaryTarget(SlotId slot) const override;
    RequestId BeginRouteMapping(const StepRef &parent, SlotTarget target) override;
private:
    AccountFieldReservation ReserveNumberField(const NumberExecutionTarget &target);
    void ReleaseAfterStoreCallFinished(const AccountFieldReservation &reservation,
        const AccountStoreWriteResult &result);
    AccountRegistry accounts_;
    BindingIndex bindings_;
    DiscoveryObservationLedger observations_;
    AccountStore &store_;
    AccountRolePlanningPort *roles_ {nullptr}; // H借用，无强环；只在发布前绑定。
    ActivationProgress activationProgress_;
    // 发现：R顺序观察进入observations_；预留旧/新键；Match→A角色预留→一次原Insert/Update。
    // 返回后复核observations_/绑定键/角色资格，分别接纳；已写但失效进入核对。
    // 加载：Current/Historical独立尝试/失败/重试，按identity和相关字段版本接纳。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_TOPOLOGY_SERVICE_H
