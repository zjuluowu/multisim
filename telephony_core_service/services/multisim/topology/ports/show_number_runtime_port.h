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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.1、§9.3、§11.1、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_SHOW_NUMBER_RUNTIME_PORT_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_SHOW_NUMBER_RUNTIME_PORT_H

#include <optional>
#include <string>
#include "application/orchestration/request_context.h"

namespace OHOS::Telephony::MultiSim {
struct MsisdnWriteRequest {
    StepRef step;
    SessionRef session; // 发起时固定，R 保存号码/tag 与原传输关联。
    std::u16string alphaTag;
    std::u16string number;
};
struct MsisdnWriteResult {
    StepRef step;
    SessionRef sentSession;
    EffectKnowledge effect;
    bool legacyWaitSucceeded;
    // false 可能为错误或等待超时；无证据时 effect=Unknown，不能推断未写卡。
};
// R只绑定该窄结果出口；普通结果与晚到证据由同一用例分别接纳。
class MsisdnResultSink {
public:
    virtual ~MsisdnResultSink() = default;
    virtual void AcceptMsisdnResult(const MsisdnWriteResult &result) = 0;
    virtual void AcceptLateEvidence(const MsisdnWriteResult &result) = 0;
};
struct CardNumberView {
    SessionRef session;
    std::u16string number;
    std::u16string alphaTag;
};
// Owner/消费者：T 的号码设置与查询用例；实现：R CardRuntime。
// 不是通用 ICardChannel。此处只读不额外发 RIL；写调用沿原卡型协议。
class ShowNumberRuntimePort {
public:
    virtual ~ShowNumberRuntimePort() = default;
    virtual std::optional<CardNumberView> ReadMsisdnCache(SlotTarget target) const = 0;
    virtual void WriteMsisdn(const MsisdnWriteRequest &request) = 0;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PORTS_SHOW_NUMBER_RUNTIME_PORT_H
