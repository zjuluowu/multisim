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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §8.2、§9.2、§13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "infrastructure/account_store_adapter.h"

namespace OHOS::Telephony::MultiSim {
/* 方法仅声明；WriteDisplayNumber的计划复用位置：
 * 与 MultiSimController::SetShowNumberToDB 相同，转换number到原编码；
 * DataShareValuesBucket只Put SimData::PHONE_NUMBER，调用
 * legacyStore_.UpdateDataBySimId(patch.subscription.legacySimId, values)。
 * 保留INVALID_VALUE->原DATABASE_WRITE_FAIL，其余返回的旧解释；不宣称CAS。
 * DB返回后交T接纳缓存，Adapter不改localCacheInfo_/allLocalCacheInfo_。
 * LoadAccounts/QueryImsPreferenceByIccid沿原GetList/GetAllList/QueryImsSwitch读源与错误。
 * provider重连/初始化/用户切换不可借Adapter整行回灌A角色或覆盖较新T字段。
 */
} // namespace OHOS::Telephony::MultiSim
