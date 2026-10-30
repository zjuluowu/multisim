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
#include "foundation/request_ref.h"

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
    bool waitEnded {false};
    EffectKnowledge effect {EffectKnowledge::NotSent};
};
struct TransitionEvidence {
    StepRef origin;
    TransitionCause cause;
    uint64_t observedAt;
    std::optional<std::string> verifiedIdentity;
    bool protocolOrCorrelatedRequeryProvesCause;
    std::optional<SessionRef> observedSession; // R已验证且发布的真实新观察；不按期望合成代次。
};
struct ExpectedContinuation { SessionRef session; bool mayAdvanceRequest; };
// R 独占 bindingGeneration；换卡/应用替换使旧卡写资格失效，资料修改不递增。
// T/A或具名本地Profile短请求在发设备前登记期望；G3b也须桥接尚未迁入G3c的旧用例。
// 同槽或任意READY不能独自证明合法新会话；期望不是设备效果事实。
class SessionRegistry final {
public:
    explicit SessionRegistry(RuntimeEpoch epoch) : epoch_(epoch) {}
    std::optional<SessionRef> Current(CardApplicationTarget target) const;
    bool StillCurrent(const SessionRef &session) const;
    bool MayPublishVerifiedSession(const SessionRef &session) const; // 替换前预检，旧观察不得先撤销当前会话。
    bool PublishVerifiedSession(const SessionRef &session); // 先交T旧会话失效，再正式暴露新会话。
    void InvalidateForVerifiedReplacement(const SessionRef &oldSession);
    bool RegisterExpectedTransition(const ExpectedTransition &expected);
    std::optional<ExpectedContinuation> AcceptExpectedContinuation(const TransitionEvidence &evidence);
    size_t ExpireExpectedTransitions(uint64_t monotonicNow);
    bool CancelExpectedConfirmedNotSent(const StepRef &origin);
    void EndExpectedWait(const StepRef &origin, EffectKnowledge effect);
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
    std::map<ApplicationKey, uint64_t> generations_; // 撤销也保留代次，不复活旧EF。
    std::vector<ExpectedTransition> expected_;
    // EndExpectedWait只结束本次等待；Unknown继续保留必要原关联交R核对，
    // 不签发新Session、不重放原语、不因调用者已返回释放资源或复活原完成出口。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_SESSION_SESSION_REGISTRY_H
