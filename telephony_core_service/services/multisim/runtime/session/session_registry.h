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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §10.1～10.3、§11.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_SESSION_SESSION_REGISTRY_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_SESSION_SESSION_REGISTRY_H

#include <map>
#include <optional>
#include <vector>
#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
enum class TransitionCause { OrdinaryActivation, ProfileSwitch, RadioSwitch, VerifiedRouteChange };
struct ExpectedTransition {
    StepRef step;
    SessionRef oldSession;
    std::vector<CardApplicationTarget> affected;
    std::optional<std::string> expectedIdentity;
    TransitionCause allowedCause;
    uint64_t monotonicDeadline;
    bool consumed;
};
struct TransitionEvidence {
    StepRef origin;
    TransitionCause cause;
    uint64_t observedAt;
    std::optional<std::string> verifiedIdentity;
    bool protocolOrCorrelatedRequeryProvesCause;
};
// R 独占 bindingGeneration；换卡/应用替换使旧卡写资格失效，资料修改不递增。
// A/T 在发设备前登记期望；同槽或任意 READY 不能独自证明合法新会话。
class SessionRegistry final {
public:
    explicit SessionRegistry(RuntimeEpoch epoch) : epoch_(epoch) {}
    std::optional<SessionRef> Current(CardApplicationTarget target) const;
    bool StillCurrent(const SessionRef &session) const;
    void InvalidateForVerifiedReplacement(const SessionRef &oldSession);
    void RegisterExpectedTransition(const ExpectedTransition &expected);
    std::optional<SessionRef> AcceptExpectedContinuation(const TransitionEvidence &evidence);
private:
    RuntimeEpoch epoch_;
    std::map<std::string, SessionRef> sessions_; // 实现须用完整资源/应用键，不能只用slot。
    std::vector<ExpectedTransition> expected_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_SESSION_SESSION_REGISTRY_H
