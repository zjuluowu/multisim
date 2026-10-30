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

#ifndef OHOS_TELEPHONY_MULTISIM_ACTIVATION_PROGRESS_H
#define OHOS_TELEPHONY_MULTISIM_ACTIVATION_PROGRESS_H

#include <map>
#include <optional>
#include "application/orchestration/request_context.h"
namespace OHOS::Telephony::MultiSim {
struct ActivationProgressTicket {
    RequestId request;
    SlotId slot;
    SessionRef sentSession;
    uint64_t admission; // T在原置位执行节点产生，不由设备回包补造。
};
// T拥有旧slot进度投影，非账户历史字段，非R资源租约。开始/替换只在原置位节点。
class ActivationProgress final {
public:
    std::optional<ActivationProgressTicket> Prepare(RequestId request, SlotId slot, const SessionRef &session);
    bool Begin(const ActivationProgressTicket &ticket);
    bool Clear(const ActivationProgressTicket &ticket);
    bool IsInProgress(SlotId slot) const;
private:
    std::map<SlotId, ActivationProgressTicket> transitions_;
    std::map<SlotId, ActivationProgressTicket> issued_;
    std::map<SlotId, uint64_t> lastStarted_; // 清理不删除水位，旧Begin不能复活。
    uint64_t admissions_ = 0;
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_ACTIVATION_PROGRESS_H
