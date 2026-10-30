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
    bool mayAdvanceRequest {true}; // 等待结束后只保留有界因果核对资格。
};
struct TransitionEvidence {
    StepRef origin;
    TransitionCause cause;
    uint64_t observedAt;
    std::optional<std::string> verifiedIdentity;
    bool protocolOrCorrelatedRequeryProvesCause;
    std::optional<SessionRef> observedSession; // R提供真实已验证新观察；不合成generation/默认路由。
};
struct ExpectedContinuation { SessionRef session; bool mayAdvanceRequest; };
// R 独占 bindingGeneration；换卡/应用替换使旧卡写资格失效，资料修改不递增。
// A/T 在发设备前登记期望；同槽或任意 READY 不能独自证明合法新会话。
class SessionRegistry final {
public:
    explicit SessionRegistry(RuntimeEpoch epoch) : epoch_(epoch) {}
    std::optional<SessionRef> Current(CardApplicationTarget target) const;
    bool StillCurrent(const SessionRef &session) const;
    bool PublishVerifiedSession(const SessionRef &session); // R完成T fence后才调用；不生成设备证据。
    void InvalidateForVerifiedReplacement(const SessionRef &oldSession);
    bool RegisterExpectedTransition(const ExpectedTransition &expected);
    bool EndExpectedTransitionWait(const StepRef &origin, const SessionRef &oldSession, EffectKnowledge effect);
    size_t ExpireExpectedTransitions(uint64_t monotonicNow);
    // 观察须已由R完成T fence并正式发布为current；期望匹配本身不创建/恢复会话。
    std::optional<ExpectedContinuation> AcceptExpectedContinuation(const TransitionEvidence &evidence);
private:
    RuntimeEpoch epoch_;
    struct ApplicationKey {
        SlotId slot;
        std::string application;
        bool operator<(const ApplicationKey &other) const
        {
            return slot < other.slot || (!(other.slot < slot) && application < other.application);
        }
    };
    std::map<ApplicationKey, SessionRef> sessions_;
    std::map<ApplicationKey, uint64_t> generations_; // 撤销不归零，旧会话/EF不重新获权。
    std::vector<ExpectedTransition> expected_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_SESSION_SESSION_REGISTRY_H
