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

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_TOPOLOGY_SERVICE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_TOPOLOGY_SERVICE_H

#include <optional>
#include "topology/api/topology_requests.h"
#include "topology/domain/account_registry.h"
#include "topology/domain/binding_index.h"
#include "topology/ports/account_store.h"
#include "assignment/api/assignment_requests.h"

namespace OHOS::Telephony::MultiSim {
struct ReloadAccountInput { SubscriptionId subscription; uint64_t expectedAccountVersion; };
struct ReloadRequest { uint64_t loadAttempt; std::vector<ReloadAccountInput> affected; };
struct AccountFieldReservation { uint64_t token; SubscriptionId subscription; }; // 应用执行资格，不是 DB 锁。

// T 唯一事实接纳入口。持有纯模型，不持有 R/I 具体类型、共享 RIL 或 callback。
// 旧 MultiSimController 分拆：InitIccId/InitShowNumber/ForgetAllData/账户缓存归此；
// 默认角色/Primary 归 A；设备状态、EF 和协议会话留 R。
class TopologyService final {
public:
    TopologyService(const SlotCatalog &catalog, AccountStore &store);
    std::optional<BindingRef> FindBinding(SlotId slot) const;
    std::optional<AccountView> ReadAccount(SubscriptionId subscription) const;
    // 简单资料命令直接调用字段端口，不强建多步关联；名称不写卡。
    int32_t SetDisplayName(SlotTarget origin, const std::u16string &name);
    int32_t SetSimLabel(SubscriptionId subscription, int32_t legacyLabel);
    int32_t SaveImsPreference(SlotTarget origin, int32_t preference);
    int32_t QueryImsPreference(SlotTarget origin, int32_t &preference); // 仍按ICCID访问store，不改纯内存。
    std::optional<NumberExecutionTarget> PrepareNumberTarget(SlotTarget origin, const SessionRef &session) const;
    NumberAcceptance AcceptNumberAfterCardWrite(const NumberExecutionTarget &target, const std::u16string &number);
    NumberAcceptance AcceptIndependentNumberBackwrite(const NumberExecutionTarget &target,
        const std::u16string &number);
    void AcceptAccountReload(const ReloadRequest &request); // 版本在发起时保存，不能旧行覆盖新字段。
    // 发现用例先从 A PrepareAccountRoleFragment 取得只读片段；T 不自行决定角色。
    void AcceptCardDiscovery(SlotId slot, const std::string &iccid, const AccountRoleFragment &roles);
    void AcceptCardRemoval(const BindingRef &expected); // 同行解绑/标签/active 按旧条件生成计划。
    AccountAcceptance AcceptActivationProgress(SubscriptionId subscription, bool inProgress);
    AccountAcceptance AcceptActivationProjection(SubscriptionId subscription, uint64_t version, bool active);
private:
    AccountFieldReservation ReserveNumberField(const NumberExecutionTarget &target);
    void ReleaseAfterStoreCallFinished(const AccountFieldReservation &reservation,
        const AccountStoreWriteResult &result);
    AccountRegistry accounts_;
    BindingIndex bindings_;
    AccountStore &store_;
    // 预留覆盖 I/O 全过程；效果未知时转交核对，不因等待终结释放冲突资格。
    // 无关账户/字段不串行；后续实现 FFRT 短提交域，锁内不得调用 store_。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_TOPOLOGY_SERVICE_H
