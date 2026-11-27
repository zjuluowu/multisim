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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.2 SIM-005～019/046～054、§7.2、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_STATE_PIN_REQUESTS_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_STATE_PIN_REQUESTS_H

#include <string>
#include "infrastructure/request/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct VerifyPinRequest { RequestContext context; CardApplicationTarget target; std::string pin; };
enum class PinReplyStatus { Unlocked, Incorrect, Failed, TimedOut, Unavailable };
struct PinReply {
    PinReplyStatus status;
    int32_t remaining;
    bool operator==(const PinReply &other) const { return status == other.status && remaining == other.remaining; }
};
class PinCompletionPort {
public:
    virtual ~PinCompletionPort() = default;
    virtual void CompletePin(CompletionId callback, const PinReply &result) = 0;
    virtual void PublishPinStateChange(const SessionRef &session) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_STATE_PIN_REQUESTS_H
