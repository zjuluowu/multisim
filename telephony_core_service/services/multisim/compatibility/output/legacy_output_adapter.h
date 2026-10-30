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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §9.3、§14.4、§15。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_OUTPUT_LEGACY_OUTPUT_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_OUTPUT_LEGACY_OUTPUT_ADAPTER_H

#include "compatibility/api/legacy_boundary_ports.h"

namespace OHOS::Telephony { class MultiSimMonitor; }
namespace OHOS::Telephony::MultiSim {
class LegacyOutputAdapter final : public LegacyOutputPort {
public:
    LegacyOutputAdapter(MultiSimMonitor &monitor, CallbackHostPort &callbacks)
        : monitor_(monitor), callbacks_(callbacks) {}
    void NotifyOriginalAccountChanged() override;
    void TransferOriginalShowNumberResult(CompletionId completion, int32_t legacyResult) override;
    void DeliverOriginalInt(const CompletionRoute &route, int32_t legacyResult) override;
private:
    MultiSimMonitor &monitor_;
    CallbackHostPort &callbacks_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_OUTPUT_LEGACY_OUTPUT_ADAPTER_H
