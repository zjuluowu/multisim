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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_DISCOVERY_ADMISSION_H
#define OHOS_TELEPHONY_MULTISIM_DISCOVERY_ADMISSION_H

#include <map>
#include <optional>
#include "topology/api/account_lifecycle.h"
#include "application/orchestration/request_context.h"
#include "topology/domain/binding_index.h"

namespace OHOS::Telephony::MultiSim {
// T应用短提交状态，所有方法与BindingIndex提交在同一执行域；锁外做存储。
// R在暴露新session前先Fence；发现/撤销/替换走此同一序列。只校验一次R当前值不够。
struct DiscoveryStoreAcceptance {
    bool bindingAccepted;
    bool needsFieldReconciliation;
    bool duplicate;
};
class DiscoveryAdmission final {
public:
    explicit DiscoveryAdmission(RuntimeEpoch epoch) : epoch_(epoch) {}
    bool Fence(const SessionRef &session, uint64_t sequence);
    std::optional<DiscoveryTicket> Begin(const CardDiscoveryObservation &observation, const BindingIndex &bindings);
    bool IsCurrent(const DiscoveryTicket &ticket, const BindingIndex &bindings) const;
    bool Finish(const DiscoveryTicket &ticket, const BindingIndex &bindings);
    bool EndPreparationFailure(const DiscoveryTicket &ticket, bool callFinished, const BindingIndex &bindings);
    // 只有原用例的显式预算重试调用；重复观察本身不重发。同一fence下发新admission。
    std::optional<DiscoveryTicket> RetryAfterKnownNoEffect(const DiscoveryTicket &ticket, const BindingIndex &bindings);
    DiscoveryStoreAcceptance CompleteStored(const DiscoveryTicket &ticket, EffectKnowledge effect,
        std::optional<SubscriptionId> identity, bool callFinished, BindingIndex &bindings);
private:
    bool MatchesFence(const DiscoveryTicket &ticket, const BindingIndex &bindings) const;
    struct Entry {
        SessionRef session;
        uint64_t sequence;
        uint64_t admission;
        bool pending;
        EffectKnowledge lastEffect;
        bool callFinished;
        std::string iccid;
    };
    RuntimeEpoch epoch_;
    std::map<SlotId, Entry> entries_;
    uint64_t admissions_ = 0;
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_DISCOVERY_ADMISSION_H
