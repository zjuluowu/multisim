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
#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_RESULT_SINKS_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_RESULT_SINKS_H

#include "topology/ports/show_number_runtime_port.h"
#include "topology/ports/activation_runtime_port.h"
#include "topology/ports/card_discovery_sink.h"
#include "assignment/ports/radio_execution_port.h"

namespace OHOS::Telephony::MultiSim {
// H同模块内绑定的窄出口集合；不是总线，不持应用聚合，不决定结果接纳资格。
// R实际事件桥先核对原serial/发起session等，再选择推进或仅证据出口。
struct RuntimeResultSinks {
    MsisdnResultSink &numbers;
    ActivationResultSink &activation;
    RadioResultSink &assignment;
    CardDiscoverySink &discovery;
    void DeliverMsisdn(const MsisdnWriteResult &result, bool mayAdvanceRequest) const
    {
        if (mayAdvanceRequest) {
            numbers.AcceptMsisdnResult(result);
        } else {
            numbers.AcceptLateEvidence(result);
        }
    }
    void DeliverActivation(const ActivationDeviceResult &result) const { activation.AcceptDeviceResult(result); }
    void DeliverRadio(const RadioSwitchResult &result) const { assignment.AcceptRadioStep(result); }
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_RESULT_SINKS_H
