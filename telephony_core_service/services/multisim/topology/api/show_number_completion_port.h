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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.3、§9.1～9.3、§16.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_SHOW_NUMBER_COMPLETION_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_SHOW_NUMBER_COMPLETION_PORT_H

#include <optional>
#include "topology/api/topology_requests.h"

namespace OHOS::Telephony::MultiSim {
struct ShowNumberCompletion {
    CompletionPlan plan;
    int32_t legacyResult;
    EffectKnowledge cardEffect;
    std::optional<NumberAcceptance> account;
};
// T SetShowNumberUseCase 的输出契约；C LegacyCompletion解释，C输出Adapter/宿主交付原出口。
class ShowNumberCompletionPort {
public:
    virtual ~ShowNumberCompletionPort() = default;
    virtual void CompleteShowNumber(const ShowNumberCompletion &completion) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_API_SHOW_NUMBER_COMPLETION_PORT_H
