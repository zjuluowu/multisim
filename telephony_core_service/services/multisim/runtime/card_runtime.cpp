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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3 WU-06/08、§10.2、§11.1、§12.3、§14.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "runtime/card_runtime.h"
#include <utility>

namespace OHOS::Telephony::MultiSim {
CardRuntime::CardRuntime(const SlotCatalog &catalog, RuntimeEpoch epoch, std::vector<ProductionSlotBindings> slots,
    RadioProtocolController &radio, ITelRilManager &sharedRil, EsimManager *euicc)
    : catalog_(catalog), slots_(std::move(slots)), radio_(radio), sharedRil_(sharedRil), euicc_(euicc),
      sessions_(epoch), resources_(epoch) {}

void CardRuntime::BindResultSinks(MsisdnResultSink &numbers, ActivationResultSink &activation,
    RadioResultSink &assignment, CardObservationSink &observations)
{
    resultSinks_.emplace(RuntimeResultSinks {numbers, activation, assignment, observations});
}

void CardRuntime::InvalidateVerifiedCardSession(const SessionRef &oldSession)
{
    if (!resultSinks_ || !sessions_.StillCurrent(oldSession)) { return; }
    const auto key = std::make_pair(oldSession.application.slot.legacySlotId, oldSession.application.applicationKey);
    const auto sequence = ++observationSequences_[key];
    // 同步短出口先撤销T资格；不得把失效排在旧发现长I/O之后。
    resultSinks_->observations.AcceptCardSessionInvalidation({oldSession, sequence});
    sessions_.InvalidateForVerifiedReplacement(oldSession);
}

void CardRuntime::PublishAcceptedCardDiscovery(const SessionRef &session, const std::string &iccid)
{
    if (!resultSinks_ || iccid.empty() || !sessions_.MayPublishVerifiedSession(session)) { return; }
    const auto old = sessions_.Current(session.application);
    if (old && (old->epoch != session.epoch || old->bindingGeneration != session.bindingGeneration)) {
        InvalidateVerifiedCardSession(*old); // 先T失效再暴露新R会话。
    }
    if (!sessions_.PublishVerifiedSession(session)) { return; }
    const auto key = std::make_pair(session.application.slot.legacySlotId, session.application.applicationKey);
    resultSinks_->observations.AcceptCardDiscoveryObservation({session, ++observationSequences_[key], iccid});
}

void CardRuntime::EndLocalProfileWait(const LocalProfileTransitionRef &transition, EffectKnowledge effect)
{
    // 发起step包含实例和用例Owner；只结束原期望等待，不释放设备或复活旧完成。
    if (transition.step.request.epoch == transition.originalSession.epoch) {
        sessions_.EndExpectedWait(transition.step, effect);
    }
}

/* 伪代码，WriteMsisdn 等运行方法有意仅声明：
 * WriteMsisdn(request)：用发起时 session 定位原 ProductionSlotBindings.files；
 * 保存 request.step、number、alphaTag 和原 InnerEvent/RIL serial 的关联；
 * 原 SimFileManager::SetSimTelephoneNumber -> SimFile::UpdateMsisdnNumber
 * -> IccDiallingNumbersHandler/EF-MSISDN 写卡；不复制编解码或再发一次设备请求。
 * ProcessSetMsisdnDone 成功原来用共享 lastMsisdn_/lastMsisdnTag_ 改 msisdn_；
 * 未来需在该实际写点同域核对发送时上下文再接纳卡缓存，不能外包一层检查就声称安全。
 * 原 bool=false 证据不足 -> Unknown，保留原 FAIL；成功/错误/投递失败均保留关联。
 * 原事件桥 -> AcceptMsisdnWriteCompletion，按 msisdnAssociations_ 核对发起输入；
 * 可信结果通过resultSinks_->numbers的MsisdnResultSink交回发起用例；
 * 旧请求迟到交其AcceptLateEvidence。R不认识具体用例类，当前不绑定真实生产结果。
 * 原会话仍有效的迟到成功可更新对应R卡观察，但不触发旧T账户步骤或原callback。
 *
 * BeginRecordsLoad/OnScopedRefresh：沿原 File 卡型分支和依赖闭包；LoadSession 只管理
 * 成员资格。SIM 原 OnAllFilesFetched：UpdateLoaded -> Records observer -> 状态/Registry
 * -> CES -> 语音信箱/EXT。不能改成所有可选文件均成功，也不能R内部每读一文件都走T/A/O。
 * 状态既接纳物理上报，也接纳原软件ABSENT/LOADED；保留SenseSwitch/指定ICCID过滤。
 *
 * PublishAcceptedCardDiscovery/InvalidateVerifiedCardSession：
 * 在R真实状态写点同域复核原Session，再为该应用分配严格递增observationSequence；
 * 发现携Session/ICCID，失效携原Session，二者经同一有序出口交CardObservationSink。
 * T先登记序列/失效再准备锁外存储，最终接纳核对原观察；不在到达时贴当前generation。
 * 同一卡拔插也保留空位置revision，不因ICCID相同复用旧资格；人工重复ABSENT不冒充换卡。
 * G3b先以旧用例桥登记/撤销ExpectedTransition，G3c后才迁完整T/A编排；R不反向启动业务。
 *
 * SwitchPrimaryRadio按request.mode分别复用原三阶段或sharedRil_.SetPrimarySlot路径；
 * 已为M0/厂商换路不伪造为三阶段。A决定参数预写/后写和业务重试，R不代写参数。
 * ReadPrimaryRadioObservation提供带来源的能力/当前Modem观察，静态SlotCatalog只给允许连接。
 * EndLegacySenseSwitch必须核对origin仍持该阶段资格，旧完成不能清新请求。
 * 激活保留旧activeSimMutex_串行，ctx_在cv.wait期间释放；不按软件槽数细分并发资源。
 *
 * 本地Profile只借用euicc_内部既有普通/低优先级File；原port/force原样传递，
 * verifiedRoute不是legacyPortIndex的替代，也不据每槽一账户推断MEP不可用。
 * R构造LocalProfileExecutionResult，精确保留原偏好节点是否到达、双结果、效果和关闭证据；
 * LocalCardRequests负责发起期望与交A提示，R只负责Session签发、真实资源和晚到恢复。
 * DisableProfile按槽提示经T桥；无后缀键只按原节点作旧外部输出，不作为关闭成功事实。
 *
 * BindResultSinks是私有结果路由契约；未来原事件桥只保留weak Owner + epoch + 在途计数，
 * 不捕获会延长Core生命周期的强环；私有sink已有实现，尚未注册真实生产callback。
 * ResourceBarrier 启用还缺资源键/可达恢复证据：不默认超时后资源可用，不启用第二套锁域。
 */
} // namespace OHOS::Telephony::MultiSim
