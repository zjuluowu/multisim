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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.2、§5.3、§9.3 SIM-023.b、§11～12、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SET_SHOW_NUMBER_USE_CASE_H
#define OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SET_SHOW_NUMBER_USE_CASE_H

#include "foundation/orchestration/runtime_operation_store.h"
#include "topology/topology_service.h"
#include "topology/account_profile/show_number_plan.h"
#include "topology/api/show_number_runtime_port.h"
#include "topology/api/show_number_completion_port.h"

namespace OHOS::Telephony::MultiSim {
// T 为唯一主编排 Owner；O 只提供 request/effect/wait 容器。
// 来源：CoreServiceSim/SimManager/MultiSimController::SetShowNumber；
// SimFileManager::SetSimTelephoneNumber、SimFile::UpdateMsisdnNumber 的协议留 R。
class SetShowNumberUseCase final : public MsisdnResultSink {
public:
    SetShowNumberUseCase(TopologyService &topology, ShowNumberRuntimePort &runtime,
        ShowNumberCompletionPort &completion, RuntimeEpoch epoch);
    // C 的原异步任务执行点调用；不在 IPC 接收时提前解析账户。
    // 本入口表达 G3b 固定执行目标与准入增强，尚未授权生产启用。
    RequestId StartAtExecutionPoint(const SetShowNumberCommand &command);
    void AcceptMsisdnResult(const MsisdnWriteResult &result) override;
    void OnLegacyWaitTimeout(StepRef step); // 原错误出口，设备效果可为 Unknown。
    void AcceptLateEvidence(const MsisdnWriteResult &result) override; // 不复活账户写入/原成功完成。
private:
    struct PendingNumber {
        NumberExecutionTarget target;
        std::u16string number;
        ShowNumberPlan plan;
    };
    void ContinueAccountUpdate(RequestId request, const PendingNumber &pending);
    void FinishWithLegacyResult(RequestId request, int32_t legacyResult,
        EffectKnowledge cardEffect, std::optional<NumberAcceptance> account);
    TopologyService &topology_;
    ShowNumberRuntimePort &runtime_;
    ShowNumberCompletionPort &completion_;
    RuntimeOperationStore operations_; // 无全局 OperationCoordinator。
    std::map<uint64_t, PendingNumber> pending_; // 请求输入，不是业务可写缓存。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_TOPOLOGY_PROFILE_SET_SHOW_NUMBER_USE_CASE_H
