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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3 WU-02/07、§9.3、§11.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/activation/activate_sim_use_case.h"

#include "topology/api/legacy_entry.h"

namespace OHOS::Telephony::MultiSim {
RequestId StartLegacyOrdinaryActivation(ActivateSimUseCase &entry, const ActivateSimCommand &command)
{ return entry.StartOrdinaryActivation(command); }

ActivateSimUseCase::ActivateSimUseCase(TopologyService &topology, ActivationAssignmentPort &assignment,
    ActivationRuntimePort &runtime, ActivationHookPort &hooks, LegacyIntCompletionPort &completion, RuntimeEpoch epoch)
    : topology_(topology), assignment_(assignment), runtime_(runtime), hooks_(hooks), completion_(completion),
      operations_(epoch, RequestOwner::Activation) {}
/* 伪代码 / 普通 non-force 路径：
 * C -> StartOrdinaryActivation；原公开入口的偏好预写 -> assignment_.AcceptEntryPrimaryPreference。
 * topology_.BeginActivationProgress({step,slot,sentSession,subscription,true})；
 * G2 assignment_.WaitForLegacyPrimaryBeforeActivation(slot)保留旧4s有界等待到期仍继续，不返回新租约。
 * G3经方法级验收后，assignment_.AdmitActivationBeforeDevice({step,sentSession})登记请求并取得
 * ActivationConflictLease，保存到该请求原关联；不能先读bool再执行，不持跨Owner资格嵌套等。
 * 同一A准入处对Primary/Activation完整冲突集一次登记；不得将未获新资格当公开新错误直接启用。
 * runtime_.SetOrdinarySimActive -> 保留原activeSimMutex_跨等待及PrimaryDone同锁协作、原请求事件；
 * R 带发送时关联返回 AcceptDeviceResult，T 先准入成功/错误，再接纳 active。
 * 旧可见步骤：当前缓存 -> 全量索引/缓存 -> T以原step+sentSession接纳进度清零并经hooks_第一次UpdateSubState
 * -> DB -> 再次缓存检查/写入 -> 第二次hooks_.ApplyLegacySubStateHook -> A主卡复核
 * -> hooks_.ApplyLegacyActiveCompleteHook(SetActiveSimFunc) -> Manager账户通知 -> active Registry。
 * 在原完成节点经completion_.CompleteLegacyInt(command.completion, legacyResult)交付，
 * 区分同步结果/旧callback/内部回执；不在T短提交域中等待自己的完成。
 * G3有效设备步骤结束后，ReleaseActivationConflict(原lease,{effect,deviceStepEnded})核验原owner/domain/token；
 * 结束或效果未知转TransferActivationConflictToReconciliation，保留原资格供R证据核对，不因T进度false释放。
 * 各current/historical投影按原节点独立接纳；当前已改但全量索引失败、DB失败都保留前段效果，不能改成持久后统一发布。
 * force仍先普通RIL但不新增账户写；卫星非force是DB先于缓存，force只Hook。
 *
 * 本批仍保留声明：完成路由、卫星技术端口与完整输出计划
 * 待对应 WU-02/07 切片完善；不返回占位成功，也不接管生产流程。
 * 来源 MultiSimController::SetActiveCommonSim/UpdataCacheSetActiveState/
 * UpdateDBSetActiveResult/SetActiveSimSatellite；初始化force与失败恢复必须一起收口。
 */
} // namespace OHOS::Telephony::MultiSim
