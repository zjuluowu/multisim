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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_TOPOLOGY_REQUESTS_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_TOPOLOGY_REQUESTS_H

#include <string>
#include "topology/api/account_lifecycle.h"
#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct SetShowNumberCommand {
    SlotTarget origin;
    std::u16string number;
    CompletionPlan completion;
};
enum class NumberSource { CardMsisdnCache, AccountDisplay, ImsImpu, Empty };
struct ShowNumberRead {
    std::u16string number;
    NumberSource source;
    int32_t legacyResult; // 保留现有错误值；内部不另造公开错误码。
};
enum class ActivationKind { Ordinary, Force, Satellite, SatelliteForce };
struct ActivateSimCommand {
    SlotTarget origin;
    bool enable;
    ActivationKind kind;
    bool fromPublicEntry; // 入口偏好提示仅原指定分支产生，交 A 接纳。
    CompletionPlan completion;
};
// 与卡 MSISDN 分离的 T 只读资料；不包含 A 可写角色或 R Records。
struct AccountView {
    SubscriptionId subscription;
    std::u16string displayName;
    std::u16string displayNumber;
    bool activeProjection;
    AccountFieldVersions fields; // 所读投影字段版本，不作为号码的统一冲突资格。
};
struct AccountStoreWriteResult {
    int32_t legacyResult;
    bool writeConfirmed;
    bool callFinished; // 未结束/效果未知不释放本地冲突资格。
};
struct NumberExecutionTarget {
    BindingRef binding;
    SessionRef session;
    std::optional<uint64_t> currentNumberVersion;
    std::optional<uint64_t> historicalNumberVersion;
};
struct NumberAcceptance {
    int32_t legacyResult;
    AccountStoreWriteResult storage;
    bool currentCacheAccepted;
    bool historicalCacheAccepted;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_TOPOLOGY_REQUESTS_H
