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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §9.1～9.4、§15.1、§16.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "infrastructure/compatibility/events/legacy_completion.h"
#include <limits>

namespace OHOS::Telephony::MultiSim {
std::optional<SynchronousWaitRef> LegacyCompletion::RetainPrimaryWaiter()
{
    std::lock_guard<std::mutex> guard(waitersLock_);
    if (nextWaiter_ == std::numeric_limits<uint64_t>::max()) { return std::nullopt; }
    const auto waiter = nextWaiter_;
    primaryWaiters_.try_emplace(waiter);
    ++nextWaiter_;
    return SynchronousWaitRef {waiter, SynchronousWaitRef::Origin::ModulePrimary};
}

std::optional<int32_t> LegacyCompletion::TakePrimaryResult(SynchronousWaitRef waiter)
{
    if (waiter.origin != SynchronousWaitRef::Origin::ModulePrimary) { return std::nullopt; }
    std::lock_guard<std::mutex> guard(waitersLock_);
    const auto found = primaryWaiters_.find(waiter.value);
    if (found == primaryWaiters_.end()) { return std::nullopt; }
    const auto result = found->second.Result();
    primaryWaiters_.erase(found);
    return result;
}

void LegacyCompletion::CompleteLegacyInt(const CompletionPlan &plan, int32_t legacyResult)
{
    const auto *route = std::get_if<SynchronousCompletionRoute>(&plan.route);
    if (route && route->waiter.origin == SynchronousWaitRef::Origin::ModulePrimary) {
        {
            std::lock_guard<std::mutex> guard(waitersLock_);
            const auto found = primaryWaiters_.find(route->waiter.value);
            if (found == primaryWaiters_.end() || !found->second.Deliver(legacyResult)) { return; }
        }
        // The synchronous C0 call has not yet returned to the waiter. No lock spans notification.
        if (primary_ && legacyResult == 0 && plan.accountNotificationOnWholeSuccess) {
            primary_->NotifyOriginalAccountChanged();
        }
        return;
    }
    if (fallback_) {
        fallback_->CompleteLegacyInt(plan, legacyResult);
        return;
    }
    if (legacyResult == 0 && plan.accountNotificationOnWholeSuccess) { output_->NotifyOriginalAccountChanged(); }
    output_->DeliverOriginalInt(plan.route, legacyResult);
}

void LegacyCompletion::CompleteShowNumber(const ShowNumberCompletion &completion)
{
    if (output_) {
        if (const auto *route = std::get_if<CallbackCompletionRoute>(&completion.plan.route)) {
            if (completion.legacyResult == 0 && completion.plan.accountNotificationOnWholeSuccess) {
                output_->NotifyOriginalAccountChanged();
            }
            output_->TransferOriginalShowNumberResult(route->callback, completion.legacyResult);
            return;
        }
    }
    CompleteLegacyInt(completion.plan, completion.legacyResult);
}
} // namespace OHOS::Telephony::MultiSim
