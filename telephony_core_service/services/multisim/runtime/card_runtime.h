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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.1～4.2、§6.2、§10～12、§17.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_RUNTIME_CARD_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_RUNTIME_CARD_RUNTIME_H

#include <map>
#include <utility>
#include <vector>
#include "application/card_access/local_card_requests.h"
#include "assignment/ports/radio_execution_port.h"
#include "topology/ports/activation_runtime_port.h"
#include "topology/ports/card_observation_port.h"
#include "topology/ports/show_number_runtime_port.h"
#include "runtime/session/session_registry.h"
#include "runtime/records/load_session.h"
#include "runtime/resource/resource_barrier.h"
#include "model/slot_catalog.h"

namespace OHOS::Telephony {
class SimStateManager;
class SimFileManager;
class SimSmsManager;
class IccDiallingNumbersManager;
class StkManager;
class SimAccountManager;
class RadioProtocolController;
class EsimManager;
class ITelRilManager;
}
namespace OHOS::Telephony::MultiSim {
struct ProductionSlotBindings {
    SlotId slot;
    SimStateManager &state;
    SimFileManager &files;
    SimSmsManager *smsStorage;
    IccDiallingNumbersManager *phonebook;
    StkManager *stk;
    SimAccountManager *carrierConfig; // 原类是运营商配置/APN/能力，不整类迁 T账户。
};
struct PhonebookReadRequest { RequestContext context; CardApplicationTarget target; int32_t fileType; };
struct StkTerminalResponse { RequestContext context; SlotTarget target; std::string response; };

// R 是完整生产卡运行支撑，不是几层透传的薄 Adapter：
// - StateManager/Handle/Tracker 维护卡/锁态；FileManager + IccFile/各卡型File/Controller
//   保留 EF、安全、编码、MSISDN、语音信箱、CFIS、Refresh/LOADED协议及输出偏序；
// - IccDiallingNumbers*/UsimDiallingNumbersService 保留PBR/ANR/IAP；SimSms*/Stk*保留卡侧协议；
// - SimAccountManager、OperatorConfig*/MatchingRule/Privilege 保留配置、APN和权限匹配；
// - EsimManager/File/Controller 保留通道/APDU/本地Profile/BPP/通知及原双结果；
// - RadioProtocolController保留三阶段协议/session；专用Primary RIL沿共享ITelRilManager；
//   激活旧串行由activeSimMutex_形成，ctx_在cv.wait期间释放，不能误称全程持有ctx_；
// 新 SessionRegistry/LoadSession/ResourceBarrier 是其作用域治理位置，不另造成熟类空壳。
// 本批构造仅表达借用关系，工厂未实现，绝不在旧Owner旁创建第二套活动对象/缓存/发令路径。
class CardRuntime final : public ShowNumberRuntimePort, public ActivationRuntimePort,
    public RadioExecutionPort, public LocalCardPort {
public:
    CardRuntime(const SlotCatalog &catalog, RuntimeEpoch epoch, std::vector<ProductionSlotBindings> slots,
        RadioProtocolController &radio, ITelRilManager &sharedRil, EsimManager *euicc);
    std::optional<CardNumberView> ReadMsisdnCache(SlotTarget target) const override;
    void WriteMsisdn(const MsisdnWriteRequest &request) override;
    void AcceptMsisdnWriteCompletion(const MsisdnWriteResult &result); // 原事件桥结果接纳点。
    void SetOrdinarySimActive(const ActivationDeviceRequest &request) override;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId target) const override;
    void SwitchPrimaryRadio(const RadioSwitchRequest &request) override;
    void EndLegacySenseSwitch(const StepRef &origin, const RadioDomainId &domain) override;
    void VerifyPin(const VerifyPinRequest &request) override;
    void ReadElementaryFile(const ReadEfRequest &request) override;
    std::optional<LocalProfileTransitionRef> RegisterExpectedProfileTransition(
        const SwitchProfileRequest &request) override;
    LocalProfileExecutionResult SwitchLocalProfile(const SwitchProfileRequest &request) override;
    void EndLocalProfileWait(const LocalProfileTransitionRef &transition, EffectKnowledge effect) override;
    void ReadPhonebook(const PhonebookReadRequest &request);
    void SendStkTerminalResponse(const StkTerminalResponse &response);
    void BeginRecordsLoad(CardApplicationTarget target, const std::vector<int32_t> &fileDependencyClosure);
    void OnScopedRefresh(const SessionRef &session, const std::string &scope, const std::vector<int32_t> &files);
    void PublishAcceptedCardDiscovery(const SessionRef &session, const std::string &iccid);
    void InvalidateVerifiedCardSession(const SessionRef &oldSession);
    // 由原生产事件桥在发送时关联，不重新发请求；有意义的结果回到对应应用Owner。
    void BindResultSinks(MsisdnResultSink &numbers, ActivationResultSink &activation, RadioResultSink &assignment,
        CardObservationSink &observations);
private:
    const SlotCatalog &catalog_;
    std::vector<ProductionSlotBindings> slots_;
    RadioProtocolController &radio_;
    ITelRilManager &sharedRil_; // 同一Core宿主实例；SIM不Init/Stop该共享资源。
    EsimManager *euicc_; // 原单一Manager内部已拥有普通/低优先级两套EsimFile，不重复创建Manager。
    SessionRegistry sessions_;
    ResourceBarrier resources_;
    std::map<std::string, LoadSession> loads_; // 仅在完整R写权切换后接管，不与旧加载计数并行写。
    std::map<uint64_t, MsisdnWriteRequest> msisdnAssociations_; // 原请求输入元数据，非另一份RIL requestMap。
    std::map<std::pair<int32_t, std::string>, uint64_t> observationSequences_; // 每应用发现/失效共序。
    struct ResultSinks {
        MsisdnResultSink *numbers;
        ActivationResultSink *activation;
        RadioResultSink *assignment;
        CardObservationSink *observations;
    };
    std::optional<ResultSinks> resultSinks_; // H内借用抽象出口；真实回调仍需weak owner/epoch/排空。
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_RUNTIME_CARD_RUNTIME_H
