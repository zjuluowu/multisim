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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§6.2 SIM-005～019/046～054、§7.2、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_EUICC_PROFILE_REQUESTS_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_EUICC_PROFILE_REQUESTS_H

#include <string>
#include "foundation/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct VerifiedPortRoute {
    std::string physicalEuiccDomain;
    int32_t portIndex;
    uint64_t routeRevision;
}; // 仅有经设备/产品证实的映射时提供；不由旧port参数或标签反推。
struct SwitchProfileRequest {
    RequestContext context;
    CardApplicationTarget target;
    std::string iccid;
    int32_t legacyPortIndex; // 原实参原样透传；是否编码Port沿原产品MEP分支。
    bool forceDisableProfile; // 保留原契约；现生产保存该值，不据此声称已参与APDU编码。
    std::optional<VerifiedPortRoute> verifiedRoute; // 未知不填0，也不覆盖legacyPortIndex。
    uint64_t expectedTransitionDeadline; // 发起者的真实单调预算；不统一秒数或推断设备已结束。
};
struct EuiccPrimitiveResult { int32_t outerLegacyResult; std::optional<int32_t> cardEnumResult; };
// 非法槽/File未调用时cardEnumResult为空，C保留旧out参数；有效File失败仍有enum。
struct LocalProfileExecutionResult {
    EuiccPrimitiveResult legacy;
    bool reachedLegacyPreferenceNode; // 有效槽File调用返回；卡enum失败仍到该节点。
    EffectKnowledge effect;
    bool channelUseEnded; // 只是内部观察，完整资源恢复仍由R按命令证据负责。
};
struct LocalProfileTransitionRef { StepRef step; SessionRef originalSession; };
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_EUICC_PROFILE_REQUESTS_H
