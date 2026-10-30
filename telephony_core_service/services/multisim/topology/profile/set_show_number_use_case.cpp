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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.3、§9.3～9.5、§11.1～11.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/profile/set_show_number_use_case.h"

namespace OHOS::Telephony::MultiSim {
SetShowNumberUseCase::SetShowNumberUseCase(TopologyService &topology, ShowNumberRuntimePort &runtime,
    ShowNumberCompletionPort &completion, RuntimeEpoch epoch)
    : topology_(topology), runtime_(runtime), completion_(completion), operations_(epoch, RequestOwner::ShowNumber) {}

/* 伪代码 / 尚未定义流程方法，不可调用：
 * StartAtExecutionPoint(command):
 *   保留旧 IsValidData、FileManager空值检查与错误优先级；不得增加号码格式限制。
 *   runtime_.ReadMsisdnCache(command.origin) -> 发起时 session + alphaTag；
 *   topology_.PrepareNumberTarget(command.origin, session) -> 固定 binding/账户输入；
 *   operations_.Begin(command.origin, command.completion)；保存 PendingNumber；
 *   plan = PlanShowNumberUpdate()；runtime_.WriteMsisdn({step, session, alphaTag, number})。
 *   发起时关联由 R 复用原事件/serial，R 将结果送 AcceptMsisdnResult。
 *
 * AcceptMsisdnResult(result):
 *   同一 T 执行域复核请求/step/attempt/目标/终态，operations_.AcceptResult 一次消费。
 *   legacyWaitSucceeded=false -> FinishWithLegacyResult(原FAIL, effect, 无账户结果)。
 *   超时 OnLegacyWaitTimeout 终结旧等待并记 Unknown；不推断 ConfirmedNotApplied。
 *   有效成功 -> ContinueAccountUpdate -> topology_.AcceptNumberAfterCardWrite(target, number)。
 *   逐步保留 DB/当前/历史缓存结果，再 FinishWithLegacyResult(...)->completion_.CompleteShowNumber。
 *   卡成功/DB失败不调用反向写卡；不重放 WriteMsisdn；没有跨卡与DB统一事务。
 *
 * AcceptLateEvidence(result):
 *   R 仅在原会话证据可信时接纳卡效果；T 记录核对信息。
 *   不 ContinueAccountUpdate，不再次 callback，不补发旧请求账户通知。
 *
 * 兼容区分：旧公共入口仅捕获 slot，File 按任务执行时的 slot 取对象；账户又在
 * SetShowNumberToDB 时 GetTargetSimId。这里固定执行目标、逐结果准入是主设计推荐增强，
 * 不能把以上伪代码当成 G2 等价实现。前移到 IPC 接收时固定更须 TGT-OPEN-011 评审。
 * 独立 ShowNumberQuery 的回写是新请求，不通过 AcceptLateEvidence 复活本请求。
 */
} // namespace OHOS::Telephony::MultiSim
