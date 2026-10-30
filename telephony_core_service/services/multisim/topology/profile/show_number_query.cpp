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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/profile/show_number_query.h"

namespace OHOS::Telephony::MultiSim {
ShowNumberQuery::ShowNumberQuery(TopologyService &topology, ShowNumberRuntimePort &runtime,
    NumberQueryHostPort &host) : topology_(topology), runtime_(runtime), host_(host) {}
/* 伪代码，两个查询方法仅声明：
 * ReadAtLegacyExecutionPoint：先保留 IsValidData/FileManager 检查；
 * runtime_.ReadMsisdnCache 有号码时与 topology_.ReadAccount 比较，不同则独立回写并返回；
 * 卡缓存空 -> FindBinding/GetTargetSimId 原错误 -> 账户号码非空则直接返回；
 * 两者空 -> host_.ReadImpuNumber，非空且不同则 PostIndependentNumberBackwrite；
 * 保留 IMPU 原失败/空值语义与原返回节点，不等回写，不统一读成账户快照。
 * RunIndependentBackwrite：发起时保存 binding/session/身份与号码字段版本，执行时复核再调用
 * topology_.AcceptIndependentNumberBackwrite；拒绝后不写当前槽的新账户。
 * 旧闭包仅捕获 slot/number，执行时才 GetTargetSimId；固定回写目标属于 G3b 行为增强。
 * 来源 MultiSimController::GetShowNumber/GetSimTelephoneNumber 的 TelFFRTUtils::Submit。
 */
} // namespace OHOS::Telephony::MultiSim
