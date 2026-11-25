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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §8.2、§9.3、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "account/number/show_number_query.h"
#include <stdexcept>

namespace OHOS::Telephony::MultiSim {
ShowNumberQuery::ShowNumberQuery(TopologyService &topology, ShowNumberRuntimePort &runtime,
    NumberQueryHostPort &host) : topology_(topology), runtime_(runtime), host_(host) {}
ShowNumberRead ShowNumberQuery::ReadAtLegacyExecutionPoint(SlotTarget origin, const NumberLegacyErrors &errors,
    const std::function<void(NumberBackwrite)> &postBackwrite)
{
    const auto session = topology_.ReadNumberSession(origin.slot);
    if (!session) { return {{}, NumberSource::Empty, errors.noCard}; }
    const auto target = topology_.PrepareNumberTarget(origin, *session);
    if (!target) { return {{}, NumberSource::Empty, errors.invalidArgument}; }
    const auto card = runtime_.ReadMsisdnCache(*session);
    if (!card) { return {{}, NumberSource::Empty, errors.unavailable}; }
    const auto account = topology_.ReadAccount(target->binding.subscription);
    if (!account || account->identityRevision != target->identityRevision) {
        return {{}, NumberSource::Empty, errors.invalidArgument};
    }
    const auto result = [&]() -> ShowNumberRead {
        if (!card->number.empty()) { return {card->number, NumberSource::CardMsisdnCache, 0}; }
        if (!account->displayNumber.empty()) { return {account->displayNumber, NumberSource::AccountDisplay, 0}; }
        try {
            const auto impu = host_.ReadImpuNumber(origin.slot);
            return {impu.number, impu.number.empty() ? NumberSource::Empty : NumberSource::ImsImpu, impu.legacyResult};
        } catch (const std::exception &) {
            return {{}, NumberSource::Empty, errors.unavailable};
        }
    }();
    if (result.legacyResult == 0 && !result.number.empty() && result.number != account->displayNumber) {
        postBackwrite({target->binding, target->session, target->identityRevision, target->numberRevisions,
            result.number, result.source});
    }
    return result;
}

void ShowNumberQuery::RunIndependentBackwrite(const NumberBackwrite &backwrite)
{
    topology_.AcceptIndependentNumberBackwrite({backwrite.binding, backwrite.sourceSession,
        backwrite.expectedIdentityRevision, backwrite.expectedNumberRevisions}, backwrite.number);
}
} // namespace OHOS::Telephony::MultiSim
