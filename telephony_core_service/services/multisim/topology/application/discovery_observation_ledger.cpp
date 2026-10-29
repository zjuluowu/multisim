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

#include "topology/application/discovery_observation_ledger.h"

namespace OHOS::Telephony::MultiSim {
namespace {
bool SameSession(const SessionRef &left, const SessionRef &right)
{
    return left.epoch == right.epoch && left.application.slot == right.application.slot &&
        left.application.applicationKey == right.application.applicationKey &&
        left.bindingGeneration == right.bindingGeneration;
}
} // namespace

bool DiscoveryObservationLedger::AcceptDiscovery(const CardDiscoveryObservation &observation)
{
    if (observation.session.epoch != epoch_ || observation.observationSequence == 0) {
        return false;
    }
    Key key {observation.session.application.slot, observation.session.application.applicationKey};
    auto current = accepted_.find(key);
    if (current != accepted_.end() && observation.observationSequence <= current->second.sequence) {
        return false;
    }
    accepted_.insert_or_assign(key, Entry {observation.session, observation.observationSequence, true});
    return true;
}

bool DiscoveryObservationLedger::AcceptInvalidation(const CardSessionInvalidation &invalidation)
{
    if (invalidation.invalidatedSession.epoch != epoch_ || invalidation.observationSequence == 0) {
        return false;
    }
    Key key {invalidation.invalidatedSession.application.slot,
        invalidation.invalidatedSession.application.applicationKey};
    auto current = accepted_.find(key);
    if (current != accepted_.end() && (invalidation.observationSequence <= current->second.sequence ||
        !SameSession(current->second.session, invalidation.invalidatedSession))) {
        return false; // 旧会话撤销不清除已接受的新会话。
    }
    accepted_.insert_or_assign(key,
        Entry {invalidation.invalidatedSession, invalidation.observationSequence, false});
    return true;
}

bool DiscoveryObservationLedger::StillCurrent(const CardDiscoveryObservation &observation) const
{
    Key key {observation.session.application.slot, observation.session.application.applicationKey};
    auto current = accepted_.find(key);
    return current != accepted_.end() && current->second.valid &&
        current->second.sequence == observation.observationSequence &&
        SameSession(current->second.session, observation.session);
}
} // namespace OHOS::Telephony::MultiSim
