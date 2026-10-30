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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.3、§9.3 SIM-023.b、§16.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_COMPLETION_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_COMPLETION_H

#include "compatibility/legacy_boundary_ports.h"
#include "application/orchestration/completion_port.h"
#include "topology/ports/show_number_completion_port.h"

namespace OHOS::Telephony::MultiSim {
// T号码完成端口的实现；只解释旧出口，不通过通知再写业务状态。
class LegacyCompletion final : public ShowNumberCompletionPort, public CompletionPort {
public:
    LegacyCompletion(LegacyOutputPort &output, CompletionHostPort &host) : output_(output), host_(host) {}
    void CompleteInt(const CompletionPlan &plan, int32_t legacyResult) override;
    void CompleteShowNumber(const ShowNumberCompletion &completion) override;
private:
    LegacyOutputPort &output_;
    CompletionHostPort &host_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_COMPLETION_H
