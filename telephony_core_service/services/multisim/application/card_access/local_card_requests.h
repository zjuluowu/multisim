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

#ifndef OHOS_TELEPHONY_MULTISIM_APPLICATION_CARD_ACCESS_LOCAL_CARD_REQUESTS_H
#define OHOS_TELEPHONY_MULTISIM_APPLICATION_CARD_ACCESS_LOCAL_CARD_REQUESTS_H

#include <string>
#include <vector>
#include "application/orchestration/request_context.h"
#include "assignment/ports/profile_preference_port.h"

namespace OHOS::Telephony::MultiSim {
struct VerifyPinRequest { RequestContext context; CardApplicationTarget target; std::string pin; };
struct ReadEfRequest { RequestContext context; CardApplicationTarget target; int32_t fileId; std::string path; };
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
};
struct EuiccPrimitiveResult { int32_t outerLegacyResult; int32_t cardEnumResult; };
struct LocalProfileExecutionResult {
    EuiccPrimitiveResult legacy;
    bool reachedLegacyPreferenceNode; // 有效槽File调用返回；卡enum失败仍到该节点。
    EffectKnowledge effect;
    bool channelUseEnded; // 只是内部观察，完整资源恢复仍由R按命令证据负责。
};
struct LocalProfileTransitionRef { StepRef step; SessionRef originalSession; };
// C 的短应用请求所需，R CardRuntime 实现；不强制 PIN/APDU 创建账户或多步操作聚合。
class LocalCardPort {
public:
    virtual ~LocalCardPort() = default;
    virtual void VerifyPin(const VerifyPinRequest &request) = 0;
    virtual void ReadElementaryFile(const ReadEfRequest &request) = 0;
    virtual std::optional<LocalProfileTransitionRef> RegisterExpectedProfileTransition(
        const SwitchProfileRequest &request) = 0;
    virtual LocalProfileExecutionResult SwitchLocalProfile(const SwitchProfileRequest &request) = 0;
    virtual void EndLocalProfileWait(const LocalProfileTransitionRef &transition, EffectKnowledge effect) = 0;
};
class LocalCardRequests final {
public:
    LocalCardRequests(LocalCardPort &runtime, ProfilePreferencePort &preferences);
    void VerifyPin(const VerifyPinRequest &request);
    void ReadElementaryFile(const ReadEfRequest &request);
    EuiccPrimitiveResult SwitchLocalProfile(const SwitchProfileRequest &request);
private:
    LocalCardPort &runtime_;
    ProfilePreferencePort &preferences_;
    // 仅短请求关联/完成门控；不持久 PIN、认证数据或完整BPP。
};
// LocalCardRequests拥有本地短请求的期望登记/等待结束，R负责签发新Session与未知效果保留。
// G3b桥未闭合的用例保持旧构建；不因缺期望证据新增旧错误或以任意READY作接续。
// SwitchLocalProfile 外层成功 != 卡成功；偏好在原节点交A，绑定观察交T而非从返回码推断。
// 下载/OSU仍走既有 EsimServiceClient/66250；不在此复制远端任务状态机。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_APPLICATION_CARD_ACCESS_LOCAL_CARD_REQUESTS_H
