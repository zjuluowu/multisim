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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.1、§11～12。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "application/card_access/local_card_requests.h"
namespace OHOS::Telephony::MultiSim {
EuiccPrimitiveResult LocalCardRequests::SwitchLocalProfile(const SwitchProfileRequest &request)
{
    // R以sentSession/step登记。无可信会话可登记时仍保留旧原语参数与错误，
    // 不虚构默认Port或因果关系；相关增强切片维持关闭。
    const auto expected = runtime_.BeginExpectedProfileTransition(request);
    const auto result = runtime_.SwitchLocalProfile(request);
    if (result.fileReturned) {
        preference_.AcceptLocalProfileReturnPreference(); // 原File返回后的节点，与card enum无关。
    }
    if (expected) { runtime_.EndProfileRequestWait(*expected, result.profileEffect); }
    return result.legacy; // 外层int与卡enum不合并，关闭未知也不篡改旧返回。
}
} // namespace OHOS::Telephony::MultiSim
