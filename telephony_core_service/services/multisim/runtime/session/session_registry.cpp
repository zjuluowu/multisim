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
// PROPOSED: R原短执行域串行保护的会话/期望纯逻辑，不发令、不恢复旧EF、不释放资源。
#include "runtime/session/session_registry.h"
#include <algorithm>

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameStep(const StepRef &left, const StepRef &right)
{
    return left == right;
}
bool SameApplication(const CardApplicationTarget &left, const CardApplicationTarget &right)
{
    return left.slot == right.slot && left.applicationKey == right.applicationKey;
}
bool SameSession(const SessionRef &left, const SessionRef &right)
{
    return left.epoch == right.epoch && SameApplication(left.application, right.application) &&
        left.bindingGeneration == right.bindingGeneration;
}
}

std::optional<SessionRef> SessionRegistry::Current(CardApplicationTarget target) const
{
    const auto found = sessions_.find({target.slot, target.applicationKey});
    return found == sessions_.end() ? std::nullopt : std::optional<SessionRef>(found->second);
}

bool SessionRegistry::StillCurrent(const SessionRef &session) const
{
    const auto current = Current(session.application);
    return session.epoch == epoch_ && current && SameSession(*current, session);
}

bool SessionRegistry::MayPublishVerifiedSession(const SessionRef &session) const
{
    if (session.epoch != epoch_ || session.bindingGeneration == 0) {
        return false;
    }
    const ApplicationKey key {session.application.slot, session.application.applicationKey};
    const auto generation = generations_.find(key);
    if (generation != generations_.end() && session.bindingGeneration <= generation->second) {
        return StillCurrent(session); // 只允许当前完全相同会话幂等，不复活撤销代次。
    }
    return true;
}

bool SessionRegistry::PublishVerifiedSession(const SessionRef &session)
{
    if (!MayPublishVerifiedSession(session)) { return false; }
    const ApplicationKey key {session.application.slot, session.application.applicationKey};
    generations_.insert_or_assign(key, session.bindingGeneration);
    sessions_.insert_or_assign(key, session);
    return true;
}

void SessionRegistry::InvalidateForVerifiedReplacement(const SessionRef &oldSession)
{
    if (StillCurrent(oldSession)) {
        sessions_.erase({oldSession.application.slot, oldSession.application.applicationKey});
    }
}

bool SessionRegistry::RegisterExpectedTransition(const ExpectedTransition &expected)
{
    if (!StillCurrent(expected.oldSession) || expected.consumed || expected.monotonicDeadline == 0 ||
        expected.waitEnded || expected.step.request.epoch != epoch_ ||
        expected.step.request.owner == RequestOwner::Unspecified || expected.step.request.value == 0 ||
        expected.affected.empty()) {
        return false;
    }
    const auto duplicate = std::find_if(expected_.begin(), expected_.end(), [&](const auto &item) {
        return SameStep(item.step, expected.step);
    });
    if (duplicate != expected_.end()) {
        return false; // 重复登记不得延长预算或重新开启旧用例推进资格。
    }
    auto registered = expected;
    registered.waitEnded = false;
    expected_.push_back(registered);
    return true;
}

bool SessionRegistry::CancelExpectedConfirmedNotSent(const StepRef &origin)
{
    const auto found = std::find_if(expected_.begin(), expected_.end(), [&](const auto &item) {
        return SameStep(item.step, origin);
    });
    if (found == expected_.end() || found->effect != EffectKnowledge::NotSent) { return false; }
    expected_.erase(found);
    return true;
}

void SessionRegistry::EndExpectedWait(const StepRef &origin, EffectKnowledge effect)
{
    const auto found = std::find_if(expected_.begin(), expected_.end(), [&](const auto &item) {
        return SameStep(item.step, origin);
    });
    if (found == expected_.end()) { return; }
    if (effect == EffectKnowledge::NotSent || effect == EffectKnowledge::Rejected ||
        effect == EffectKnowledge::ConfirmedNotApplied) {
        expected_.erase(found);
    } else {
        found->waitEnded = true;
        found->effect = effect; // 有界核对保留，不恢复旧请求推进/完成资格。
    }
}

size_t SessionRegistry::ExpireExpectedTransitions(uint64_t monotonicNow)
{
    const auto previous = expected_.size();
    expected_.erase(std::remove_if(expected_.begin(), expected_.end(), [&](const auto &item) {
        return item.monotonicDeadline <= monotonicNow;
    }), expected_.end());
    return previous - expected_.size();
}

std::optional<ExpectedContinuation> SessionRegistry::AcceptExpectedContinuation(const TransitionEvidence &evidence)
{
    const auto found = std::find_if(expected_.begin(), expected_.end(), [&](const auto &item) {
        return SameStep(item.step, evidence.origin);
    });
    if (found == expected_.end()) {
        return std::nullopt;
    }
    if (evidence.observedAt >= found->monotonicDeadline) {
        expected_.erase(found);
        return std::nullopt;
    }
    if (!evidence.protocolOrCorrelatedRequeryProvesCause || evidence.cause != found->allowedCause ||
        !evidence.observedSession || evidence.observedSession->epoch != epoch_ ||
        (found->expectedIdentity && found->expectedIdentity != evidence.verifiedIdentity) ||
        (SameApplication(evidence.observedSession->application, found->oldSession.application) &&
         evidence.observedSession->bindingGeneration <= found->oldSession.bindingGeneration)) {
        return std::nullopt;
    }
    const auto affected = std::find_if(found->affected.begin(), found->affected.end(), [&](const auto &application) {
        return SameApplication(application, evidence.observedSession->application);
    });
    if (affected == found->affected.end() || !StillCurrent(*evidence.observedSession)) {
        return std::nullopt;
    }
    ExpectedContinuation accepted {*evidence.observedSession, !found->waitEnded};
    expected_.erase(found); // 一次消费；不会重新接纳旧EF或重复推进请求。
    return accepted;
}
} // namespace OHOS::Telephony::MultiSim
