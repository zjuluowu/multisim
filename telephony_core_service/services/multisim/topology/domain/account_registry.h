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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2～4.3、§6.3、§8～9。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H

#include <map>
#include <optional>
#include <string>
#include "model/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
// WU-01/02/05 的纯状态；Application TopologyService 是唯一可写入口。
// 当前/历史投影片段属于同一 Owner，分节点可见，不是第二套独立 writer。
struct AccountRecord {
    SubscriptionId subscription;
    std::string iccid;
    std::u16string displayName;
    std::optional<int32_t> legacyLabel;
    std::optional<int32_t> imsPreference;
    std::optional<std::u16string> currentDisplayNumber;
    std::optional<std::u16string> historicalDisplayNumber;
    bool activeProjection;
    bool activationInProgress;
    uint64_t version;
};
enum class AccountAcceptance { Accepted, MissingAccount, ChangedInputs };
enum class NumberVisibilityNode { CurrentCache, HistoricalCache };
class AccountRegistry final {
public:
    std::optional<AccountRecord> Read(SubscriptionId subscription) const;
    AccountAcceptance AcceptNumberVisibility(SubscriptionId subscription, uint64_t expectedVersion,
        NumberVisibilityNode node, const std::u16string &number);
    AccountAcceptance AcceptActiveProjection(SubscriptionId subscription, uint64_t expectedVersion, bool active);
    AccountAcceptance SetActivationInProgress(SubscriptionId subscription, bool inProgress);
    AccountAcceptance AcceptDisplayName(SubscriptionId subscription, uint64_t version, const std::u16string &name);
    AccountAcceptance AcceptLabel(SubscriptionId subscription, uint64_t version, int32_t legacyLabel);
    AccountAcceptance AcceptImsPreference(SubscriptionId subscription, uint64_t version, int32_t preference);
    // InitIccId/InsertEsimData：沿旧匹配政策接纳已分配 simId，不在此产生新公共身份。
    AccountAcceptance AcceptDiscoveredAccount(const AccountRecord &account);
private:
    friend class TopologyService; // 真正应用服务在同 namespace；未来写权封装不向 C 暴露容器。
    std::map<SubscriptionId, AccountRecord> accounts_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_DOMAIN_ACCOUNT_REGISTRY_H
