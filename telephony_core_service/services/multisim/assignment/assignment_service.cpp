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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.3、§8.2、§9.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "assignment/assignment_service.h"
#include "assignment/primary/primary_switch_plan.h"

#include "assignment/api/legacy_entry.h"

namespace OHOS::Telephony::MultiSim {
RequestId StartLegacyPrimarySwitch(AssignmentService &entry, const SwitchPrimaryCommand &command)
{ return entry.StartPrimarySwitch(command); }

AssignmentService::AssignmentService(const SlotCatalog &catalog, AssignmentStore &store,
    RadioExecutionPort &radio, AssignmentOutputPort &output, PrimaryTopologyPort &topology,
    LegacyIntCompletionPort &completion, RuntimeEpoch epoch)
    : catalog_(catalog), operations_(epoch, RequestOwner::Assignment), store_(store), radio_(radio),
      output_(output), topology_(topology), completion_(completion) {}
void AssignmentService::AcceptLocalProfilePreference(const StepRef &, SlotId)
{
    store_.WriteUserPrimaryPreference(true); // 原File返回后写入；参数失败不覆盖旧双结果。
}
std::optional<AccountRoleFragment> AssignmentService::PrepareAccountRoleFragment(const AccountRolePlanningInput &input)
{ return accountRoles_.Prepare(input); }
bool AssignmentService::AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned)
{ return accountRoles_.AcceptDiscovered(planned, assigned); }
bool AssignmentService::StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const
{ return accountRoles_.StillOwns(planned); }
bool AssignmentService::FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
    const InitialRoleFinalizationEvidence &evidence)
{ return accountRoles_.Finalize(planned, evidence); }
uint64_t AssignmentService::ReadRoleProjectionRevision(bool historicalView) const
{ return accountRoles_.Revision(historicalView); }
bool AssignmentService::AcceptReloadedRoleProjectionSet(const std::vector<StoredAccountRoleFragment> &loaded,
    uint64_t loadAttempt, uint64_t expectedRevision, bool historicalView)
{ return accountRoles_.AcceptReloadSet(loaded, loadAttempt, expectedRevision, historicalView); }
bool AssignmentService::AcceptReloadedRoleProjection(const StoredAccountRoleFragment &loaded,
    uint64_t, uint64_t expectedRevision, bool historicalView)
{ return accountRoles_.AcceptSingleProjection(loaded, expectedRevision, historicalView); }
/* 仅声明方法的流程契约：
 * StartPrimarySwitch在原执行点读取T窄事实与R动态PrimaryRadioObservation，不从目录推断当前分配。
 * 同一请求的全局角色/Primary参数域与真实Radio冲突集在唯一准入处一次登记；跨Owner子请求带parent。
 * 不在A持资格同步等T；第三卡映射返回子请求/原早返回轨迹，后续事件再推进，不经CMI自调用。
 * PlanPrimarySwitch按原分支优先级选择：第三卡映射 -> 用户专用RIL -> 已M0 -> 普通三阶段。
 * ThreePhaseProtocol: 进度/SenseSwitch -> 参数/last预写 -> R三阶段 -> 原广播 -> Data -> Done。
 * WithoutModemRestart: 原开始事件 -> R共享RIL专用SetPrimarySlot -> 成功才保存/联动Data；失败仍Done(true)。
 * AlreadyPrimary: 原保存/联动Data，不发送设备协议；VendorRouteMapping经T子请求与原厂商端口。
 * 成功/错误都先按epoch/owner/request/step/attempt、原session/phase/member准入，再修改阶段/计数。
 * Done与EndLegacySenseSwitch核对原请求资格，旧完成不能清新transition；阶段结束不是设备成功。
 * completion_.CompleteLegacyInt在各旧返回点交C；同步等待在宿主适配层，不占A提交域。
 * 失败按旧参数条件恢复；恢复不等于设备回滚；Unknown交R，唯一业务重试由A授权。
 * G2保留原入口/预算/重试；新准入和隔离是G3c行为增强，不由本骨架宣称已启用。
 *
 * WaitForLegacyPrimaryBeforeActivation仅保留G2原有界等待/超时继续，不创建G3资格。
 * G3 AdmitActivationBeforeDevice以原step+sentSession在assignmentConflicts_和Primary同处核验并登记
 * 完整冲突集，返回ActivationConflictLease；等待/拒绝需方法级行为验收，不隐含新增生产策略。
 * ReleaseActivationConflict只对匹配lease且MayReleaseActivationConflict证据成立的登记释放；
 * Unknown或结束未证时TransferActivationConflictToReconciliation保留同一token/原归属，T进度清零不释放。
 *
 * PrepareAccountRoleFragment按发现上下文预留角色域，未知simId保持optional；原Insert返回id后
 * AcceptDiscoveredRoleProjection将计划绑定该id。T发I/O前及返回后经StillOwnsInitialRoleReservation复核。
 * FinalizeInitialRoleReservation只在MayFinalizeInitialRoles成立且token/角色域资格匹配时释放；
 * DB已写但观察失效、Unknown或投影尚未接纳/核对均保留原资格，不能仅凭callFinished解锁。
 * current/all角色载荷来自原两读源，成功整次读取用AcceptReloadedRoleProjectionSet独立替换；
 * 失败不调用/不清空，原setter只改current并同步accountRoles_.NoteLocalRoleMutation。
 * SetDefaultRole等实际提交均更新这个唯一角色资格；不以当前A状态反填DB结果。
 * 入口/eSIM偏好均在原指定节点交A；迁移关闭旧参数直写、恢复、重试事件及detached CMI自调用。
 */
} // namespace OHOS::Telephony::MultiSim
