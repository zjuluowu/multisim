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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§6.1、§16.1～16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H

#include "infrastructure/compatibility/api/legacy_boundary_ports.h"
#include "card/esim/local_card_requests.h"
#include "infrastructure/assembly/slot_catalog.h"
#include "account/legacy_entry.h"
#include "role_assignment/legacy_entry.h"
#include "role_assignment/primary/primary_topology_port.h"
#include "infrastructure/compatibility/events/legacy_completion.h"

namespace OHOS::Telephony::MultiSim {
// 目标 C 私有桥，未来原 CoreServiceSim/SimManager/CMI 委托至此；不替换它们的公开签名。
class LegacySimFacade final {
public:
    LegacySimFacade(const SlotCatalog &catalog, LegacyIngressPort &ingress, CallbackHostPort &callbacks,
        SetShowNumberUseCase &numbers, ShowNumberQuery &query, ActivateSimUseCase &activation,
        AssignmentService &assignment, LocalCardRequests &localCards);
    int32_t SetShowNumber(int32_t slotId, const std::u16string &number,
        LegacyCallerRef caller, LegacyCallbackRef callback);
    void RunQueuedSetShowNumber(const SetShowNumberCommand &command);
    ShowNumberRead GetShowNumberAtExecutionPoint(int32_t slotId);
    int32_t RunOrdinaryActivationAtExecutionPoint(const ActivateSimCommand &command);
    // The H lease covers validation, dispatch and taking the original synchronous result.
    static int32_t SetPrimarySlotId(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
        LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology, LegacyCompletion &completion,
        const std::function<bool(SlotId)> &hasVerifiedCard,
        const std::function<RequestId(const SwitchPrimaryCommand &)> &issue,
        const std::function<bool(RequestId, uint64_t)> &timeout = {});
    static int32_t SetDefaultCellularDataSlotId(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
        LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology, LegacyCompletion &completion,
        AssignmentService &assignment, const std::function<bool(SlotId)> &hasVerifiedCard);
    static int32_t SetDefaultCallMessageRole(DefaultRole role, SlotId slot, LegacyCallerRef caller,
        const SlotCatalog &catalog, LegacyPrimaryHostPort &host, const PrimaryTopologyPort &topology,
        LegacyCompletion &completion, AssignmentService &assignment,
        const std::function<bool(SlotId)> &hasVerifiedCard);
    static int32_t CheckNumberEntry(SlotId slot, LegacyCallerRef caller, LegacyCallbackRef callback,
        bool writing, bool available, LegacyPrimaryHostPort &host);
    static NumberLegacyErrors NumberErrors(const LegacyPrimaryHostPort &host);
    static int32_t SaveImsSwitch(SlotId slot, const SlotCatalog &catalog, LegacyPrimaryHostPort &host,
        LegacyCompletion &completion, const std::function<int32_t(const CompletionPlan &)> &issue);
    static int32_t QueryImsSwitch(SlotId slot, const SlotCatalog &catalog, LegacyPrimaryHostPort &host,
        const std::function<int32_t()> &read);
    static int32_t SetShowName(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
        LegacyPrimaryHostPort &host, LegacyCompletion &completion, const std::function<bool(SlotId)> &hasCard,
        const std::function<AccountMetadataResult(const CompletionPlan &)> &issue);
    static int32_t SetSimLabelIndexByIccId(LegacyPrimaryHostPort &host, LegacyCompletion &completion,
        const std::function<AccountMetadataResult(const CompletionPlan &)> &issue);
    static int32_t GetShowName(SlotId slot, LegacyCallerRef caller, const SlotCatalog &catalog,
        LegacyPrimaryHostPort &host, std::u16string &name, const std::function<bool(SlotId)> &hasCard,
        const std::function<std::optional<AccountView>()> &read);
    AssignmentView ReadAssignmentForLegacyProjection() const;
    void VerifyPinAtExecutionPoint(const VerifyPinRequest &request);
    EuiccPrimitiveResult SwitchLocalProfileAtExecutionPoint(const SwitchProfileRequest &request);
    int32_t SwitchLocalProfileAndAssignAtExecutionPoint(const SwitchProfileRequest &request, int32_t &enumResult);
private:
    const SlotCatalog &catalog_;
    LegacyIngressPort &ingress_;
    CallbackHostPort &callbacks_;
    SetShowNumberUseCase &numbers_;
    ShowNumberQuery &query_;
    ActivateSimUseCase &activation_;
    AssignmentService &assignment_;
    LocalCardRequests &localCards_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_SIM_FACADE_H
