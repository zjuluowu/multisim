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
#include "application/orchestration/request_context.h"
#include "topology/domain/account_registry.h"

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
    int32_t legacyResult;
};
enum class ActivationKind { Ordinary, Force, Satellite, SatelliteForce };
struct ActivateSimCommand {
    SlotTarget origin;
    bool enable;
    ActivationKind kind;
    bool fromPublicEntry;
    CompletionPlan completion;
};
struct AccountView {
    SubscriptionId subscription;
    AccountReadView view;
    std::u16string displayName;
    std::u16string displayNumber;
    bool activeProjection;
    uint64_t identityRevision;
    AccountFieldRevisions fields;
};
struct AccountStoreWriteResult {
    int32_t legacyResult;
    bool writeConfirmed;
    bool callFinished; // 不确定调用已结束时保留原冲突资格，不盲重写。
};
struct NumberFieldRevisions { std::optional<uint64_t> current; std::optional<uint64_t> historical; };
struct NumberExecutionTarget {
    BindingRef binding;
    SessionRef session;
    uint64_t identityRevision;
    NumberFieldRevisions numberRevisions; // 改名/IMS/active不应使合法号码结果失效。
};
struct NumberAcceptance {
    int32_t legacyResult;
    AccountStoreWriteResult storage;
    bool currentCacheAccepted;
    bool historicalCacheAccepted;
};
// 同一R应用观察流按序进入T；R已校验不替代T跨Owner/锁外I/O后的最终接纳。
struct CardDiscoveryObservation {
    SessionRef session;
    uint64_t observationSequence;
    std::string iccid;
};
struct CardSessionInvalidation {
    SessionRef invalidatedSession;
    uint64_t observationSequence;
};
struct DiscoveryAcceptance {
    int32_t legacyResult;
    bool bindingAccepted;
    std::optional<SubscriptionId> storedSubscription;
    AccountStoreWriteResult storage;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_TOPOLOGY_REQUESTS_H
