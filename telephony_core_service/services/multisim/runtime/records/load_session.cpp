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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §10.2、§11.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "runtime/records/load_session.h"
#include <utility>

namespace OHOS::Telephony::MultiSim {
LoadSession::LoadSession(SessionRef session, std::string scope, uint64_t generation)
    : session_(std::move(session)), scope_(std::move(scope)), generation_(generation) {}

bool LoadSession::AddMember(const LoadMember &member)
{
    if (member.state != LoadMemberState::Pending ||
        (member.parent && members_.find(*member.parent) == members_.end())) {
        return false;
    }
    return members_.emplace(member.request, member).second;
}

bool LoadSession::TryConsumeRead(const LoadToken &token, bool decodedSuccessfully)
{
    // 纯资格示例；未来调用方须在 R 同一 FFRT 短提交域内消费并接纳对应 Records。
    if (token.session.epoch != session_.epoch ||
        !(token.session.application.slot == session_.application.slot) ||
        token.session.application.applicationKey != session_.application.applicationKey ||
        token.session.bindingGeneration != session_.bindingGeneration ||
        token.scope != scope_ || token.recordsGeneration != generation_) {
        return false;
    }
    auto member = members_.find(token.member);
    if (member == members_.end() || member->second.access != LoadAccess::Read ||
        member->second.state != LoadMemberState::Pending) {
        return false;
    }
    member->second.state = decodedSuccessfully ? LoadMemberState::Completed : LoadMemberState::Failed;
    return true;
}

void LoadSession::InvalidateAffectedReadMembers()
{
    for (auto &[id, member] : members_) {
        if (member.access == LoadAccess::Read && member.state == LoadMemberState::Pending) {
            member.state = LoadMemberState::Invalidated;
        }
    }
}
} // namespace OHOS::Telephony::MultiSim
