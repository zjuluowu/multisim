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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.1、§11～12。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_COMPLETION_CELL_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_COMPLETION_CELL_H
#include <optional>
#include "application/orchestration/request_context.h"
namespace OHOS::Telephony::MultiSim {
// C宿主按SynchronousWaitRef/原完成路由持有一个cell；所有调用由宿主的同一受控域保护。
// 此类仅是一次完成状态机。真实等待/唤醒由CallbackHostPort使用FFRT实现；
// 应用提交域不得等待，宿主在投递前建立cell，完成先于等待时也不能丢失结果。
class CompletionCell final {
public:
    bool Deliver(int32_t result)
    {
        if (state_ != WaitState::Waiting) { return false; }
        result_ = result;
        state_ = WaitState::Delivered;
        return true;
    }
    std::optional<int32_t> Result() const { return result_; }
    WaitState State() const { return state_; }
    bool EndWait(WaitState reason)
    {
        if (reason != WaitState::CancelledWait && reason != WaitState::CallerDied &&
            reason != WaitState::TimedOut) { return false; }
        if (state_ != WaitState::Waiting) { return false; }
        state_ = reason;
        return true;
    }
private:
    std::optional<int32_t> result_;
    WaitState state_ {WaitState::Waiting};
};
} // namespace OHOS::Telephony::MultiSim
#endif
