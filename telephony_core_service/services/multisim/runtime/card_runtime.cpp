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
    RadioProtocolController &radio, EsimManager *ordinaryEuicc, EsimManager *lowPriorityEuicc)
    : catalog_(catalog), slots_(std::move(slots)), radio_(radio), ordinaryEuicc_(ordinaryEuicc),
      lowPriorityEuicc_(lowPriorityEuicc), sessions_(epoch) {}

/* 伪代码，WriteMsisdn 等运行方法有意仅声明：
 * WriteMsisdn(request)：用发起时 session 定位原 ProductionSlotBindings.files；
 * 保存 request.step、number、alphaTag 和原 InnerEvent/RIL serial 的关联；
 * 原 SimFileManager::SetSimTelephoneNumber -> SimFile::UpdateMsisdnNumber
 * -> IccDiallingNumbersHandler/EF-MSISDN 写卡；不复制编解码或再发一次设备请求。
 * ProcessSetMsisdnDone 成功原来用共享 lastMsisdn_/lastMsisdnTag_ 改 msisdn_；
 * 未来需在该实际写点同域核对发送时上下文再接纳卡缓存，不能外包一层检查就声称安全。
 * 原 bool=false 证据不足 -> Unknown，保留原 FAIL；成功/错误/投递失败均保留关联。
 * 原事件桥 -> AcceptMsisdnWriteCompletion，按 msisdnAssociations_ 核对发起输入；
 * 可信结果通过 resultOwners_ 交 SetShowNumberUseCase::AcceptMsisdnResult；
 * 旧请求迟到交 AcceptLateEvidence。成员只声明，当前不绑定真实生产结果。
 * 原会话仍有效的迟到成功可更新对应R卡观察，但不触发旧T账户步骤或原callback。
 *
 * BeginRecordsLoad/OnScopedRefresh：沿原 File 卡型分支和依赖闭包；LoadSession 只管理
 * 成员资格。SIM 原 OnAllFilesFetched：UpdateLoaded -> Records observer -> 状态/Registry
 * -> CES -> 语音信箱/EXT。不能改成所有可选文件均成功，也不能R内部每读一文件都走T/A/O。
 * 状态既接纳物理上报，也接纳原软件ABSENT/LOADED；保留SenseSwitch/指定ICCID过滤。
 *
 * BindResultOwners 是私有结果路由契约；未来原事件桥只保留 weak Owner + epoch + 在途计数，
 * 不捕获会延长Core生命周期的强环；目前未定义、未注册真实 callback。
 * ResourceBarrier 启用还缺资源键/可达恢复证据：不默认超时后资源可用，不启用第二套锁域。
 */
} // namespace OHOS::Telephony::MultiSim
