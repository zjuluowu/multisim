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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.1、§10.2、§11.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_RECORDS_LOAD_SESSION_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_RECORDS_LOAD_SESSION_H

#include <map>
#include <optional>
#include <string>
#include "foundation/sim_identity.h"

namespace OHOS::Telephony::MultiSim {
struct LoadToken {
    SessionRef session;
    std::string scope;
    uint64_t recordsGeneration;
    uint64_t member;
};
enum class LoadMemberState { Pending, Completed, Failed, Invalidated };
enum class LoadAccess { Read, Write };
struct LoadMember {
    uint64_t request;
    std::optional<uint64_t> parent;
    bool required;
    LoadAccess access;
    LoadMemberState state;
};
// R 内部加载协议：保留卡型算法、父子请求/必需可选成员与PBR/ANR/IAP依赖闭包。
// 不是 T 聚合，不走统一业务编排。Refresh只推进受影响scope的代次。
class LoadSession final {
public:
    LoadSession(SessionRef session, std::string scope, uint64_t generation);
    bool AddMember(const LoadMember &member);
    bool TryConsumeRead(const LoadToken &token, bool decodedSuccessfully);
    void InvalidateAffectedReadMembers();
    // 写入失效不代表设备取消；在途写继续保留原上下文交 R/用例Owner 核对。
    // 原 SIM/USIM/RUIM/ISIM LOADED 条件与事件节点由原 File 类继续裁决，不在此统一计数归零。
private:
    SessionRef session_;
    std::string scope_;
    uint64_t generation_;
    std::map<uint64_t, LoadMember> members_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_RECORDS_LOAD_SESSION_H
