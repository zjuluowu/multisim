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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 无生产调用或设备流程定义。

#include "application/card_access/local_card_requests.h"

namespace OHOS::Telephony::MultiSim {
LocalCardRequests::LocalCardRequests(LocalCardPort &runtime, ProfilePreferencePort &preferences)
    : runtime_(runtime), preferences_(preferences) {}

EuiccPrimitiveResult LocalCardRequests::SwitchLocalProfile(const SwitchProfileRequest &request)
{
    const auto transition = runtime_.RegisterExpectedProfileTransition(request);
    const auto result = runtime_.SwitchLocalProfile(request);
    if (result.reachedLegacyPreferenceNode) {
        preferences_.AcceptLocalProfilePreference(request.context.step, request.target.slot);
    }
    if (transition) { runtime_.EndLocalProfileWait(*transition, result.effect); }
    return result.legacy;
}
/* 原语义的具名续接顺序：
 * 1. 复用原入口检查与发起时RequestContext；R在原调用点保存legacyPortIndex/force/ICCID。
 * 2. 本短请求在发出设备步骤前调用RegisterExpectedProfileTransition。
 *    R以发起时会话、目标ICCID、受影响应用、原预算及因果证据登记ExpectedTransition；
 *    不能以当前slot/任意READY补造接续。未具备桥的切片不启用新会话门控。
 * 3. runtime_.SwitchLocalProfile复用原EsimManager/File的通道/APDU/双结果，
 *    reachedLegacyPreferenceNode才调用preferences_.AcceptLocalProfilePreference(step, slot)。
 *    非法槽不提示；有效槽File返回后即到该节点，即使cardEnum失败也仍提示。
 *    由A按原SetParameter节点处理；不把提示写入改成卡成功条件，不反转原双结果。
 * 4. 结束本地等待时EndLocalProfileWait；Unknown或通道关闭未证实转R保留原关联/租约，
 *    不因短请求返回而清除恢复责任，不复活旧调用者。返回result.legacy原双结果。
 * 5. 由带Session/观察序号的R观察到T接受实际绑定；原语成功不是Profile/Radio就绪。
 *
 * 高层Download/OSU及同层补偿归66250，本地请求不复制其状态机或超时重跑任务。
 * DisableProfile按槽标签恢复提示经T桥；无后缀参数按原节点作旧外部输出，R不直写。
 */
} // namespace OHOS::Telephony::MultiSim
