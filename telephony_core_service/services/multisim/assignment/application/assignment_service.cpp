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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.4 场景2、§6.3、§9.3、§11.2、§14.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "assignment/application/assignment_service.h"
#include "topology/application/topology_service.h"
#include "assignment/application/primary_switch_plan.h"

namespace OHOS::Telephony::MultiSim {
AssignmentService::AssignmentService(const SlotCatalog &catalog, AssignmentStore &store,
    RadioExecutionPort &radio, AssignmentOutputPort &output, TopologyService &topology, CompletionPort &completion)
    : catalog_(catalog), store_(store), radio_(radio), output_(output), completion_(completion), topology_(topology) {}

void AssignmentService::AcceptLocalProfileReturnPreference()
{
    store_.WriteUserPrimaryPreference(true); // 与旧SetParameter一致：失败不覆盖原双结果。
}
std::optional<AccountRoleFragment> AssignmentService::PrepareAccountRoleFragment(
    const DiscoveryRoleContext &context) const
{
    const auto count = catalog_.Capacities().configured;
    if (!count) { return std::nullopt; } // 缺真实SIM_SLOT_COUNT输入，仅阻塞初始建行计划。
    return accountRoles_.Prepare(context, *count == 1);
}
bool AssignmentService::IsAccountRoleFragmentCurrent(const AccountRoleFragment &fragment) const
{ return accountRoles_.IsCurrent(fragment); }
RoleFragmentAcceptance AssignmentService::AcceptAccountRoleStorage(const AccountRoleFragment &fragment,
    std::optional<SubscriptionId> identity, EffectKnowledge effect)
{ return accountRoles_.AcceptStorage(fragment, identity, effect); }
RoleReloadTicket AssignmentService::BeginRoleReload(AccountReadView view, uint64_t attempt)
{ return accountRoles_.BeginReload(view, attempt); }
bool AssignmentService::AcceptRoleReload(const RoleReloadTicket &ticket,
    const std::vector<AccountRoleFragment> &roles, bool readSucceeded)
{ return accountRoles_.AcceptReload(ticket, roles, readSucceeded); }
/* 伪代码，StartPrimarySwitch / AcceptRadioStep 等仅声明：
 * SetDefaultRole等所有角色实际可见提交须同时accountRoles_.NoteLocalRoleMutation；
 * 相应角色current/all查询消费accountRoles_投影，不由T回灌整个行。
 * 在原执行解析点 radio_.ObservePrimaryExecution(target) -> PlanPrimarySwitch(userSet, facts)。
 * ThirdCardRoute优先：T映射/厂商执行及原NeedSwitchSimCard早返回，后续Primary独立完成；
 * WithoutModemReboot：设备专用RIL -> 成功后参数/主卡广播/Data -> Done(true)；
 * AlreadyM0：普通校验 -> 参数/主卡广播/Data -> 重试复位，无设备协议和Done；
 * ThreePhase：普通校验 -> 进度/SenseSwitch -> 参数预写 -> 三阶段设备 -> 广播/Data/Done。
 * 具名计划列出每个分支成功/失败偏序；A执行参数，R不代写参数。
 * 每个原返回节点 completion_.CompleteInt(command.completion, legacyResult)，
 * RequestId不作为旧int，设备后续观察不能重复完成；第三卡不等待延后切换。
 * 成功和错误均按发送时step/session/phase/member一次准入，再推进业务步骤；
 * 无效结果只核对设备效果。参数恢复不等于硬件回滚；Unknown资源由R核对。
 * 原失败重试轨迹与新准入/安全重试分开：新准入收紧须G3c按方法验收。
 *
 * 来源 MultiSimController::SetPrimarySlotId/SavePrimaryCardInfo/ResumePrimaryCardInfo/
 * RetrySetPrimarySlotId/SetDefaultCellularDataSlotId/CheckIfNeedSwitchMainSlotId；
 * RadioProtocolController::SetRadioProtocol/ProcessResponseInfoOfEvent 继续在 R。
 * CoreServiceSim::SetActiveSim/SetPrimarySlotId、EsimManager::SwitchToProfile 的偏好写归 A。
 * 迁移需同时处理 InitMainCardSlotId/建行角色/重载、5处 detached 自调用、超时/重试，
 * 不留旧直接 writer。G2 仍保留旧有界等待；新切换准入为 G3c 行为增强。
 */
} // namespace OHOS::Telephony::MultiSim
