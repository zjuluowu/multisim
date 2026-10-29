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

#include "topology/application/activate_sim_use_case.h"

namespace OHOS::Telephony::MultiSim {
ActivateSimUseCase::ActivateSimUseCase(TopologyService &topology, AssignmentService &assignment,
    ActivationRuntimePort &runtime, ActivationHookPort &hooks)
    : topology_(topology), assignment_(assignment), runtime_(runtime), hooks_(hooks) {}
/* 伪代码 / 普通 non-force 路径：
 * C -> StartOrdinaryActivation；原公开入口的偏好预写 -> assignment_.AcceptEntryPrimaryPreference。
 * topology_.AcceptActivationProgress(true)；assignment_.AdmitActivationBeforeDevice；
 * G2 保留旧4s有界等待到期仍继续，不能以新 DomainBusy 改旧返回。
 * runtime_.SetOrdinarySimActive -> 复用 SetActiveSimToRil 的进程级静态串行与请求事件；
 * R 带发送时关联返回 AcceptDeviceResult，T 先准入成功/错误，再接纳 active。
 * 旧可见步骤：当前缓存 -> 全量索引/缓存 -> T接纳进度清零并经hooks_第一次UpdateSubState
 * -> DB -> 再次缓存检查/写入 -> 第二次hooks_.ApplyLegacySubStateHook -> A主卡复核
 * -> hooks_.ApplyLegacyActiveCompleteHook(SetActiveSimFunc) -> Manager账户通知 -> active Registry。
 * 当前缓存已改但全量索引失败、DB失败，都保留前段效果，不能改成持久后统一发布。
 * force仍先普通RIL但不新增账户写；卫星非force是DB先于缓存，force只Hook。
 *
 * 本批只补关键声明：激活完成端口、卫星技术端口、两次缓存兼容片段与完整输出计划
 * 待对应 WU-02/07 切片完善；不返回占位成功，也不接管生产流程。
 * 来源 MultiSimController::SetActiveCommonSim/UpdataCacheSetActiveState/
 * UpdateDBSetActiveResult/SetActiveSimSatellite；初始化force与失败恢复必须一起收口。
 */
} // namespace OHOS::Telephony::MultiSim
