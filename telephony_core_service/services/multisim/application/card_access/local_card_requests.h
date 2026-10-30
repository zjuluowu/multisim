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

namespace OHOS::Telephony::MultiSim {
struct VerifyPinRequest { RequestContext context; CardApplicationTarget target; std::string pin; };
struct ReadEfRequest { RequestContext context; CardApplicationTarget target; int32_t fileId; std::string path; };
struct VerifiedProfileRoute { int32_t portIndex; std::string routeEvidence; };
struct SwitchProfileRequest {
    RequestContext context;
    CardApplicationTarget target;
    std::string iccid;
    int32_t legacyPortIndex; // 原实参透传，不能被验证结果或默认0替代。
    bool forceDisableProfile; // 原File保存该参数；不推断当前APDU已消费force。
    std::optional<VerifiedProfileRoute> verifiedRoute;
    uint64_t expectedTransitionDeadline; // 发起者的单调时钟预算，不是设备结束证据。
};
struct EuiccPrimitiveResult {
    int32_t outerLegacyResult;
    std::optional<int32_t> cardEnumResult; // 非法slot/File未调用时不赋值，C保留原out引用。
};
struct ExpectedProfileToken { StepRef origin; SessionRef oldSession; };
struct LocalProfileRuntimeResult {
    EuiccPrimitiveResult legacy;
    bool fileReturned; // 原Manager有效槽/File调用已返回；包括card enum失败。
    EffectKnowledge profileEffect;
    bool channelEndConfirmed; // 独立证据，卡成功或本地channel清零不能推出true。
};
class LocalProfilePreferencePort {
public:
    virtual ~LocalProfilePreferencePort() = default;
    virtual void AcceptLocalProfileReturnPreference() = 0;
};
// C 的短应用请求所需，R CardRuntime 实现；不强制 PIN/APDU 创建账户或多步操作聚合。
class LocalCardPort {
public:
    virtual ~LocalCardPort() = default;
    virtual void VerifyPin(const VerifyPinRequest &request) = 0;
    virtual void ReadElementaryFile(const ReadEfRequest &request) = 0;
    // R先按原EsimManager同一slot/File条件校验，非法槽/File缺失不登记期望；
    // 取得可信sentSession后在设备发送前登记，原语仍返回原错误。
    virtual std::optional<ExpectedProfileToken> BeginExpectedProfileTransition(
        const SwitchProfileRequest &request) = 0;
    virtual LocalProfileRuntimeResult SwitchLocalProfile(const SwitchProfileRequest &request) = 0;
    // 结束短请求等待；NotSent/ConfirmedNotApplied撤销期望，其余保留至因果观察消费/期限。
    // 本方法不恢复旧EF资格、不释放未知通道、不重发高层任务。
    virtual void EndProfileRequestWait(const ExpectedProfileToken &token, EffectKnowledge effect) = 0;
};
class LocalCardRequests final {
public:
    LocalCardRequests(LocalCardPort &runtime, LocalProfilePreferencePort &preference)
        : runtime_(runtime), preference_(preference) {}
    void VerifyPin(const VerifyPinRequest &request);
    void ReadElementaryFile(const ReadEfRequest &request);
    EuiccPrimitiveResult SwitchLocalProfile(const SwitchProfileRequest &request);
private:
    LocalCardPort &runtime_;
    LocalProfilePreferencePort &preference_;
    // 仅短请求关联/完成门控；不持久 PIN、认证数据或完整BPP。
};
// SwitchLocalProfile 外层成功 != 卡成功；后续偏好交 A、绑定观察交 T。
// 下载/OSU仍走既有 EsimServiceClient/66250；不在此复制远端任务状态机。
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_APPLICATION_CARD_ACCESS_LOCAL_CARD_REQUESTS_H
