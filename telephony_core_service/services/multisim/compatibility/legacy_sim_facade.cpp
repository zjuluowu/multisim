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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §5.4、§9.3、§16.1～16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "compatibility/legacy_sim_facade.h"

namespace OHOS::Telephony::MultiSim {
LegacySimFacade::LegacySimFacade(const SlotCatalog &catalog, LegacyIngressPort &ingress,
    CallbackHostPort &callbacks, SetShowNumberUseCase &numbers, ShowNumberQuery &query,
    ActivateSimUseCase &activation, AssignmentService &assignment, LocalCardRequests &localCards,
    CompletionHostPort &completions)
    : catalog_(catalog), ingress_(ingress), callbacks_(callbacks), numbers_(numbers), query_(query),
      activation_(activation), assignment_(assignment), localCards_(localCards), completions_(completions) {}

int32_t LegacySimFacade::RunPrimarySwitchAtExecutionPoint(const SwitchPrimaryCommand &command)
{
    auto issued = command;
    issued.completion = completions_.OpenSynchronousResult(LegacySynchronousMethod::PrimarySwitch);
    issued.completion.accountNotificationOnWholeSuccess = command.completion.accountNotificationOnWholeSuccess;
    assignment_.StartPrimarySwitch(issued);
    return completions_.AwaitOriginalSynchronousResult(issued.completion);
}

int32_t LegacySimFacade::RunOrdinaryActivationAtExecutionPoint(const ActivateSimCommand &command)
{
    auto issued = command;
    issued.completion = completions_.OpenSynchronousResult(LegacySynchronousMethod::OrdinaryActivation);
    issued.completion.accountNotificationOnWholeSuccess = command.completion.accountNotificationOnWholeSuccess;
    activation_.StartOrdinaryActivation(issued);
    return completions_.AwaitOriginalSynchronousResult(issued.completion);
}
EuiccPrimitiveResult LegacySimFacade::SwitchLocalProfileAtExecutionPoint(const SwitchProfileRequest &request)
{ return localCards_.SwitchLocalProfile(request); }

int32_t LegacySimFacade::SwitchLocalProfileAndAssignAtExecutionPoint(
    const SwitchProfileRequest &request, int32_t &legacyEnumOut)
{
    const auto result = SwitchLocalProfileAtExecutionPoint(request);
    if (result.cardEnumResult) { legacyEnumOut = *result.cardEnumResult; }
    return result.outerLegacyResult;
}
/* 伪代码，其他命令/查询有意仅声明：
 * SetShowNumber(slot, number, caller, callback)：
 *   ingress_.CheckSetShowNumberEntry，保留 SystemApp -> SET_TELEPHONY_STATE
 *   -> simManager/callback空值 -> 调用者槽能力 的原检查优先级；
 *   callbacks_.RetainCallback，构造带 slot/number/CompletionPlan 的命令；
 *   ingress_.QueueSetShowNumberTask(command)，按原受理出口返回，不等待业务完成。
 *   Core 原队列将任务回调 RunQueuedSetShowNumber -> numbers_.StartAtExecutionPoint。
 *   工作线程可能立即执行；不能保证IPC受理reply必早于业务callback。
 * GetShowNumberAtExecutionPoint -> query_.ReadAtLegacyExecutionPoint；
 * 上述同步桥在原调用线程等真实旧结果；用例Start返回RequestId只作关联。
 * 应用在每个原返回节点CompleteInt；不把受理当成功，Core/CMI保持各自入口检查。
 * VerifyPin/SwitchLocalProfile -> localCards_ 的对应具名方法，不通过万能Execute。
 *
 * 不在C预写userPreferPrimarySlot：交 A AcceptEntryPrimaryPreference，在原适用入口节点调用。
 * 槽种类/可见性读 catalog_；第三卡Voice翻译用 A ReadLegacyAssignment 的资源视图，
 * 不在此另存Radio状态。该视图尚缺完整旧第三卡Radio投影片段，见主设计源码落点缺口。
 * 所有方法均为私有骨架，未追加 ISimManager/IEsimManager/IDL/ABI。
 */
} // namespace OHOS::Telephony::MultiSim
