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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.3、§9.3 SIM-023.b。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_SHOW_NUMBER_PLAN_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_SHOW_NUMBER_PLAN_H

#include <array>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
enum class NumberUpdateNode { CardMsisdn, AccountStore, CurrentAccountCache, HistoricalAccountCache };
struct ShowNumberPlan {
    std::array<NumberUpdateNode, 4> orderedNodes;
    bool rollbackCardOnAccountFailure;
};
// 使用方：T SetShowNumberUseCase。只表达旧路径必要偏序，不验证平台号码格式，
// 不把卡号码与账户号码合并为事实，也不创造跨设备/DB 原子事务。
ShowNumberPlan PlanShowNumberUpdate();
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_APPLICATION_SHOW_NUMBER_PLAN_H
