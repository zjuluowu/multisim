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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§6.1、§16.1～16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H

#include "compatibility/legacy_boundary_ports.h"
#include "topology/application/set_show_number_use_case.h"
#include "topology/application/show_number_query.h"
#include "topology/application/activate_sim_use_case.h"
#include "application/card_access/local_card_requests.h"

namespace OHOS::Telephony::MultiSim {
// 目标 C 私有桥，未来原 CoreServiceSim/SimManager/CMI 委托至此；不替换它们的公开签名。
class LegacySimFacade final {
public:
    LegacySimFacade(const SlotCatalog &catalog, LegacyIngressPort &ingress, CallbackHostPort &callbacks,
        SetShowNumberUseCase &numbers, ShowNumberQuery &query, ActivateSimUseCase &activation,
        AssignmentService &assignment, LocalCardRequests &localCards, CompletionHostPort &completions);
    int32_t SetShowNumber(int32_t slotId, const std::u16string &number,
        LegacyCallerRef caller, LegacyCallbackRef callback);
    void RunQueuedSetShowNumber(const SetShowNumberCommand &command);
    ShowNumberRead GetShowNumberAtExecutionPoint(int32_t slotId);
    int32_t RunOrdinaryActivationAtExecutionPoint(const ActivateSimCommand &command);
    int32_t RunPrimarySwitchAtExecutionPoint(const SwitchPrimaryCommand &command);
    AssignmentView ReadAssignmentForLegacyProjection() const;
    void VerifyPinAtExecutionPoint(const VerifyPinRequest &request);
    EuiccPrimitiveResult SwitchLocalProfileAtExecutionPoint(const SwitchProfileRequest &request);
    int32_t SwitchLocalProfileAndAssignAtExecutionPoint(const SwitchProfileRequest &request, int32_t &legacyEnumOut);
private:
    const SlotCatalog &catalog_;
    LegacyIngressPort &ingress_;
    CallbackHostPort &callbacks_;
    SetShowNumberUseCase &numbers_;
    ShowNumberQuery &query_;
    ActivateSimUseCase &activation_;
    AssignmentService &assignment_;
    LocalCardRequests &localCards_;
    CompletionHostPort &completions_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H
