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

namespace OHOS::Telephony::MultiSim {
AssignmentService::AssignmentService(const SlotCatalog &catalog, AssignmentStore &store,
    RadioExecutionPort &radio, AssignmentOutputPort &output, TopologyService &topology)
    : catalog_(catalog), store_(store), radio_(radio), output_(output), topology_(topology) {}
/* 伪代码，StartPrimarySwitch / AcceptRadioStep 等仅声明：
 * C -> StartPrimarySwitch -> assignment_.AdmitTransition + PlanPrimarySwitch。
 * A 持有 AssignmentTransition，store_.PrewritePrimary 保留 last/参数的原预写节点；
 * radio_.SwitchPrimaryRadio -> R 原三阶段协议 -> AcceptRadioStep。
 * 成功和错误都先按发送时 step/session/phase/member 一次准入，再推进状态，不能
 * 先改 communicationFailed/计数再核对 session。R 负责协议 session，A 负责业务 attempt。
 * 成功：原主卡广播 -> A SetDefaultRole(Data) 的原保存/广播节点 -> EndLegacySenseSwitch。
 * 失败：store_.RestorePrimaryIfLegacyValid -> 原 Done -> 效果明确后 A 唯一重试。
 * 参数恢复不等于硬件回滚；本地超时不证明可发下一轮，未知效果交 R 隔离核对。
 * 受限第三卡换路：经 T 的映射应用入口和厂商执行端口（本批局部缺口，未宣称已实现）。
 *
 * 来源 MultiSimController::SetPrimarySlotId/SavePrimaryCardInfo/ResumePrimaryCardInfo/
 * RetrySetPrimarySlotId/SetDefaultCellularDataSlotId/CheckIfNeedSwitchMainSlotId；
 * RadioProtocolController::SetRadioProtocol/ProcessResponseInfoOfEvent 继续在 R。
 * CoreServiceSim::SetActiveSim/SetPrimarySlotId、EsimManager::SwitchToProfile 的偏好写归 A。
 * 迁移需同时处理 InitMainCardSlotId/建行角色/重载、5处 detached 自调用、超时/重试，
 * 不留旧直接 writer。G2 仍保留旧有界等待；新切换准入为 G3c 行为增强。
 */
} // namespace OHOS::Telephony::MultiSim
