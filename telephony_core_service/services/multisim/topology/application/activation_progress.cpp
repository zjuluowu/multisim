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

#include "topology/application/activation_progress.h"

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameActivationSession(const SessionRef &left, const SessionRef &right)
{
    return left.epoch == right.epoch && left.application.slot == right.application.slot &&
        left.application.applicationKey == right.application.applicationKey &&
        left.bindingGeneration == right.bindingGeneration;
}
} // namespace

std::optional<ActivationTransition> ActivationProgress::Prepare(const ActivationTransition &transition)
{
    const auto owner = transition.owner.request.owner;
    if (transition.owner.request.value == 0 ||
        (owner != RequestOwner::Activation && owner != RequestOwner::LegacyBridge) ||
        transition.owner.request.epoch != epoch_ || transition.sentSession.epoch != epoch_ ||
        !(transition.slot == transition.sentSession.application.slot) || !transition.legacyProgress) {
        return std::nullopt;
    }
    auto qualified = transition;
    qualified.admission = ++nextAdmission_;
    prepared_.insert_or_assign(qualified.slot, qualified);
    return qualified;
}

bool ActivationProgress::Begin(const ActivationTransition &transition)
{
    const auto prepared = prepared_.find(transition.slot);
    if (prepared == prepared_.end() || transition.admission <= lastStarted_[transition.slot] ||
        transition.admission != prepared->second.admission || !(prepared->second.owner == transition.owner) ||
        !SameActivationSession(prepared->second.sentSession, transition.sentSession) || !transition.legacyProgress) {
        return false;
    }
    auto current = transitions_.find(transition.slot);
    if (current != transitions_.end() && (current->second.legacyProgress ||
        current->second.owner == transition.owner)) { return false; }
    lastStarted_[transition.slot] = transition.admission;
    transitions_.insert_or_assign(transition.slot, transition);
    return true;
}

bool ActivationProgress::Accept(const StepRef &owner, const SessionRef &session, bool inProgress)
{
    if (owner.request.epoch != epoch_ || session.epoch != epoch_) {
        return false;
    }
    auto current = transitions_.find(session.application.slot);
    if (current == transitions_.end() || !(current->second.owner == owner) ||
        !SameActivationSession(current->second.sentSession, session)) {
        return false;
    }
    // 只有Begin建立进行中事实；旧步骤不得在已清零后将其复活。
    if (inProgress && !current->second.legacyProgress) {
        return false;
    }
    current->second.legacyProgress = inProgress;
    return true;
}

bool ActivationProgress::ReadLegacy(SlotId slot) const
{
    auto current = transitions_.find(slot);
    return current != transitions_.end() && current->second.legacyProgress;
}
} // namespace OHOS::Telephony::MultiSim
