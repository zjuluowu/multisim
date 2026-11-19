# Legacy / Target 双后端测试架构

## 阶段二现状（基于 bcb7ff742）

| 项 | 实际状态 |
|---|---|
| Legacy Host | 现有七个独立 Legacy 程序和 124 项防护网保留；默认 `backend=legacy` 行为不变。 |
| Target Host | `sim_target_production_host` 独立链接 `AssignmentService`、`AssignmentAggregate`、`PlanPrimarySwitch` 和 `SlotCatalog`，不链接 `services/sim`。 |
| C0 | `SIM028-LEGACY-C0-DUAL-PRIMARY0` 已映射两个不同 GTest：Legacy `RealSimPrimary.C0CoreToRealManagerAndController`、Target `TargetPrimary.C0AssignmentServiceAlreadyPrimary`。 |
| Both | 要求两个后端都执行同一 Scenario ID，零匹配、单边缺失、架构-only 和未授权实现均为 BLOCKED。 |
| Device | Legacy 维持既有能力；Target SA/IPC/真实持久化/设备仍 BLOCKED。 |
| VS Code | Target C0 可由 TestMate 发现并通过 LLDB 停在真实 `AssignmentService::StartPrimarySwitch`。 |

## 可靠差分协议

`backend_capabilities.json` 的每条记录以稳定业务 `id` 为主键；每个 backend 各自声明 GTest 名、Host/Device 状态和阻塞原因。`test.py` 根据同一输入 Scenario 将过滤条件翻译为不同后端的真实测试名，再逐后端独立进程运行。

`compare_backends.py` 只接受带 Scenario ID 的结果。它将 Legacy 的 `primary_observation` 和 Target 的组件结果归一化为：

- API 结果
- Primary / Default Data 角色
- 用户偏好、主卡参数、ICCID、Data 参数
- 主卡通知与 Data 通知的偏序
- 设备切换命令数
- 业务重试数

缺失任一后端、没有共同场景、重复 Scenario、Oracle 不一致或外部业务字段不同，不能得到 PASS。内部对象地址、线程、内部请求序号和调度耗时不参与比较。

## Target C0 真实调用链

```text
AssignmentService::StartPrimarySwitch
  -> RuntimeOperationStore::Begin / AdvanceStep
  -> SlotCatalog::Find
  -> PrimaryTopologyPort::ReadPrimaryTarget  (T 外部事实：绑定、激活、ICCID)
  -> RadioExecutionPort::ReadPrimaryRadioObservation  (R 外部事实：实际 Modem)
  -> PlanPrimarySwitch(AlreadyPrimary)
  -> AssignmentAggregate::AdmitTransition / AcceptTransitionPhase
  -> AssignmentStore::WriteUserPrimaryPreference
  -> AssignmentStore::WritePrimaryParameters
  -> AssignmentAggregate::AcceptPrimaryParameterProjection
  -> AssignmentOutputPort::PublishMainCardChanged
  -> AssignmentStore::WriteDefaultData
  -> AssignmentAggregate::AcceptDataParameterProjection
  -> AssignmentOutputPort::PublishDefaultDataChanged
  -> RuntimeOperationStore::MarkTerminal
  -> LegacyIntCompletionPort::CompleteLegacyInt
```

C0 只在 Radio 观察明确给出 Modem 0、目标槽映射也明确指向 Modem 0 时选择 `AlreadyPrimary`；缺失/未知 Radio 观察不被默认成 M0，也不发送 `SwitchPrimaryRadio`。非 C0 的三阶段、专用 RIL、厂商换路、重试和激活方法仍显式拒绝，未实现假成功。

## 真实逻辑与 Host 替身边界

真实 Target 逻辑包含请求创建/epoch、决策、准入、Aggregate 提交、参数/角色更新次序、通知因果次序、完成状态和返回语义。

Host 的 Recording Store、Output、Topology、Radio、Completion 只做两件事：提供 T/R/持久化的受控外部事实，或记录已经由 `AssignmentService` 决定的外部效果。替身不创建 Assignment 计划、不修改 Aggregate、不选择角色，也不制造 SUCCESS。

组件测试使用无线程、无外部设备租约的同步端口记录器；作用域结束即销毁，没有遗留 worker、订阅或 RIL 资源。它只证明组件闭环和资源局部销毁，不证明正式产品对象图生命周期。

## 阶段六：Primary B 异步闭环

WithoutModemRestart 不复用 C0 同步提交路径。`AssignmentService` 读取真实 T 目标和 R 能力后登记 DevicePending，创建业务 RequestId/Step/attempt，并向 `RadioExecutionPort` 发送一次专用请求。请求携带 `expectedBinding` 和独立 `protocolSession`；`RuntimeOperationStore` 保存已发送 SessionRef。

`AcceptRadioStep` 仅在 epoch/owner/request/step/attempt、目标槽、BindingRef 和 protocolSession 全部匹配时推进。ConfirmedApplied 才写 Primary/ICCID、发布主卡、联动 Data 并交付 SUCCESS；ConfirmedNotApplied 或设备错误只发送结束状态并返回 Legacy 的 LOCAL_PTR_NULL，保留旧角色。Unknown、未完成参数写入进入 `NeedsReconciliation`，不制造完成结果。

`AcceptPrimarySwitchTimeout(RequestId, elapsedMilliseconds)` 使用 Legacy 的 45000ms 截止，Host 通过受控时钟输入模拟时间。超时在聚合中转为 Reconciling（不暴露 Legacy 的进行中标志），业务 Completion 返回原错误；新请求仍受该 Radio 域冲突阻塞，迟到响应不能清除新状态或提交角色。该模型保留真实生产超时语义，但不宣称物理 Modem/HDI 已取消。

B 场景结果：`SIM028-LEGACY-B-SUCCESS-DUAL-PRIMARY0`、`SIM028-LEGACY-B-ERROR-DUAL-PRIMARY0`、`SIM028-LEGACY-B-TIMEOUT-DUAL-PRIMARY0` 均使用稳定 Scenario ID，分别与 Legacy GTest 差分。Compatibility 不比较内部 RequestId、Session、线程或调度耗时；专项关联测试验证这些字段的拒绝规则。

`AcceptStoredDiscovery` 仍只是“已确认 Provider 写入结果进入 T”的 Host/未来调用入口：它要求 callFinished、writeConfirmed、ConfirmedApplied、assigned identity、账户 ICCID 和观察一致，并通过 DiscoveryObservationLedger 建立票据后才调用 AccountRegistry/BindingIndex。它不代表完整“写前票据→锁外 I/O→写后复核”生产流程，不能被生产 Host 直接用作绕过资格的建行接口。

正执行审计要求 `AssignmentService::AcceptRadioStep/AcceptPrimarySwitchTimeout` 和 `RuntimeOperationStore::RecordSent/AcceptResult` 在真实 Target 二进制中非零计数。真实 Radio/HDI、产品 SA/IPC、Device 仍未验收。

M7.1 增加换卡、Unknown、ConfirmedApplied+非零结果及错误/迟到关联负控。已发命令后绑定变化或 Unknown/效果冲突不提交角色；外层 Completion 可按 Legacy 语义结束，但 transition 与 RuntimeOperationStore 留在 NeedsReconciliation，复用同域 `AdmitTransition` 仍会拒绝新请求。只有显式晚证据可记录到原请求，不能重播完成或通知。

M9.1/M9.2 在 A 中按完整角色写集原子登记跨 RadioDomain 资格：Primary 参数/通知、Data 参数/通知与用户偏好共同排序；Voice/SMS 未包含在 Primary 操作写集中，可独立推进；独立 Data setter 使用同一 Data 写集。A/R 设备租约仍分离。`RuntimeOperationStore` 将 Step 消费、终态、Completion 一次领取与晚设备效果分开；C1 首次 API 失败使用受控 early claim，但终态保留 Open 供预算内后台重试。`AcceptResult` 已归位 `runtime_operation_store.cpp`，不再定义在 AssignmentService。晚证据只允许首个与当前 Step/NeedsReconciliation 匹配的结果；冲突或重复结果不覆盖，A 接纳后可回收原 Store 记录，但不能释放 R 设备或重播业务效果。

M9.3 Host 调度由 `RetrySchedulerPort` 提供 Schedule/Cancel/CancelAll/Audit/Stop 能力。C1 CHECK 完整失败并结束 COMPLETE 后，A 先恢复旧参数并领取首轮 Completion，再登记 `{RequestId, epoch, owner, scheduleGeneration, attempt}`；到期事件只由 weak 原 Owner 回投，A 再核对绑定、角色资格、当前 Step 和 attempt 后发送新 Session。受控 Host 使用一个 joinable worker 串行执行业务 Owner 任务和到期任务，参数/Radio/通知仍在 scheduler 短提交域外。测试自动推进五轮，保持 6 Session/24 命令/12 状态事件；原手工 `AcceptC1RetryDue` 测试继续作为组件闭包。Stop 拒绝新请求和任务、取消未到期任务，已运行 Owner 任务结束或保留为 DeviceEffectsUnresolved；scheduler 任务和回调排空后才允许销毁 shared AssignmentService。TSan 对自动调度、Stop、绑定变化、Unknown、重复事件及销毁后事件执行。

M10 接入 `ResourceRadioRuntime`：每个 B/C1 成员命令先经 `ResourceBarrier` 的完整资源 Acquire；同一 protocolSession+phase 的多成员共用一个 lease/fence，只 MarkSent 一次；真实 Host 平台返回的 opaque transport token 将结果关联回原 Step/member。只有全成员均带独立 `commandEnded/resourceReusable` 的匹配证据才 Complete 并上送正常业务结果；缺证据仅向 A 上送 Unknown/NeedsReconciliation，不把资源标为 Available；超时或 Unknown 隔离，晚回包需额外 `currentStateVerified` 才能 Recovery。C0 不取得 R lease。

M10.2 发送边界契约将平台返回分为 `ConfirmedNotSent`、`HandoffAccepted`、`HandoffUnknown`。前者才能由 R 撤销未发送 lease；后者仍须 MarkSent/隔离，禁止伪装为本地未发。回包可在 Submit 返回前到达：R 将 token 先登记为 submitting，先缓冲证据，返回后分类 handoff，再接纳；回调线程只改 R 短提交域，合法业务结果经 `RadioOwnerDispatchPort` 投递到单一 Assignment Owner worker。OwnerCallbacksDrained 同时检查 R groups/transmissions、Submit-in-progress、缓冲结果以及 dispatcher running/pending；DeviceResourcesDrained 另外要求全部 ResourceBarrier 为 Available。两者不可互推。

M10 设备证据等级与 Legacy 对照：

| 业务 | 请求/关联 | 业务完成观察 | 命令结束/可复用 |
| --- | --- | --- | --- |
| B SetPrimarySlot | VERIFIED_SOURCE：`MultiSimController::SendSetPrimarySlotEvent` → `TelRilManager::SetPrimarySlot`，RIL serial/requestMap 关联 InnerEvent owner | VERIFIED_SOURCE：`SetPrimarySlotResponse` 的 `RadioResponseInfo.error` 唤醒等待；VERIFIED_HOST：ResourceRadioRuntime 仅在显式 end/reuse 后转发 | OPEN：HDI response 未包含设备静止或 channel 复用证明 |
| C1 CHECK | VERIFIED_SOURCE：每个真实 slot 的 `SetRadioProtocol(CHECK)`，共享 controller sessionId | VERIFIED_SOURCE：逐 slot response 按 sessionId；全部成员后决定 UPDATE 或 COMPLETE | OPEN：response 不证明旧命令结束或全资源可复用 |
| C1 UPDATE | VERIFIED_SOURCE：逐 slot `SetRadioProtocol(UPDATE)`；response handler 不推进 | VERIFIED_SOURCE：只由 `RADIO_SIM_RADIO_PROTOCOL_NOTIFY` 的匹配 sessionId 完成成员计数 | OPEN：NOTIFY 不是独立 ACK，也不含 resource reusable |
| C1 COMPLETE | VERIFIED_SOURCE：逐 slot COMPLETE，success/fail 载荷及共享 session | VERIFIED_SOURCE：所有响应后结束 controller 通信 | OPEN：业务协议结束不证明 Modem/channel 已静止 |

VERIFIED_HOST：当前 Target 使用一个保守共享 Host 资源键，禁止按 SlotId 推断并发。INFERENCE：Legacy 单一 RadioProtocolController/session 令 C1 成员在 Target Host 逻辑上共享协议资格，但物理 Modem/RIL instance 是否独立仍 OPEN。VERIFIED_DEVICE：无；当前仓/环境未提供 HDI 完成/复用字段、匹配设备；`hb` 亦不在当前 Host 工具链 PATH；实际执行 `hb build -T //base/telephony/core_service:tel_core_service` 返回 127 `command not found`，故 native 产品编译/链接未运行（PRODUCT_BUILD_BLOCKED），不是已验收。产品平台适配必须在 `RadioPlatformPort` 后方把 HandoffAccepted 时生成的 RIL serial 记录为 `transportSerial`，并与 R 的 transport token 关联；token/serial 只用于传输关联，不替代 Step/session/member/binding/phase 核验。当前 Host 用 token 与 serial 等值 fixture，仅证明关联边界。平台还须补充 `commandEnded/currentStateVerified/resourceReusable` 的真实来源；缺任一来源仍 Quarantine，不能自动 Available。

M10.1 Host Stop 区分两级：OwnerCallbacksDrained 只说明 Scheduler 的 Owner 任务、R groups/transmissions、提交中任务和缓冲结果均已排空；DeviceResourcesDrained 还要求所有资源为 Available。Host 测试可证明模拟 end/reuse 状态机，不宣称真实 Device Drain PASS。M9.3 仍不证明真实 R/HDI 命令已结束或资源可复用。生产 Scheduler/SA/IPC 接线、共享 RIL Stop、Core OnStop 和 SO 卸载仍为 BLOCKED。`dd7ac58f` 中保存的主设计/详细设计/评审稿仅作只读审查输入，公开拒绝/等待/排队错误码和 Stop 后设备恢复策略仍为 PROPOSED。Host 对冲突继续使用现有 `-1` 负控，不表示公共错误码已批准。M11 前置条件：同版本 HDI/产品头与 ABI；可证明的 physical Modem/RIL instance key；HandoffAccepted/Unknown 的真实平台映射；每类命令 end、current-state、reuse 的真实来源；Core/SA 停止与共享 RIL 生命周期；可运行 hb 产品目标；至少一个匹配 Device 的 RIL/HDI trace。当前未满足，M11 产品集成继续 BLOCKED。

M8.1 修正 UPDATE 语义：Target 不再等待虚构 RadioResponse ACK，而仅在两个成员的 RadioProtocol NOTIFY 到齐后进入 COMPLETE；状态事件 C1 固定按 Legacy `isUserSet=false` 发布。M8.2 CHECK 失败逐轮发送失败 COMPLETE、恢复 old Primary/ICCID，并由 A 将五次后台重试显式排入原请求；每轮新 Step/attempt/protocolSession，最终 24 条请求/6 session。API 第一轮失败返回一次，后继重试不重复 Completion。

M7.2 C1 通过真实 `PlanPrimarySwitch(ThreePhaseProtocol)`，先按 Legacy 顺序预写 P、发布开始状态，再为两个成员发送 CHECK(1)、UPDATE(2)、COMPLETE(4)。每阶段都要求原 StepRef、protocolSession、成员和目标 Binding；只在该阶段全部成员完成后才推进下一步。旧 session、重复成员和错误成员不推进。当前只实现成功链；五次重试/完整 C1 失败恢复仍未实现。

## 阶段五：真实 T/A 业务对象集成

C0 的 Target 前置事实现在来自真实 `TopologyService::ReadPrimaryTarget`：`BindingIndex::Find` 取得当前已接受 `BindingRef`，`AccountRegistry::ReadProjection(Current)` 提供 raw ICCID 和 active；Historical 投影仅供历史查询，不能授权 C0。`AcceptStoredDiscovery` 是已确认 provider 写入后的合法 T 接纳入口，复用 DiscoveryObservationLedger、AccountRegistry 和 BindingIndex；测试不再通过 Fake Topology 直接返回 `PrimaryTargetFacts`。

调用链：

```text
controlled R session/account-store-confirmed result
  -> TopologyService::AcceptStoredDiscovery
  -> DiscoveryObservationLedger::BeginStore / CompleteStored
  -> AccountRegistry::AcceptDiscoveredAccount
  -> BindingIndex::AcceptBinding
  -> AssignmentService::StartPrimarySwitch
  -> TopologyService::ReadPrimaryTarget
  -> AccountRegistry::Read + ReadProjection(Current)
  -> BindingIndex::Find/StillMatches
  -> PlanPrimarySwitch / AssignmentAggregate / AssignmentStoreAdapter
```

双卡 Topology Integration 覆盖同槽换卡、解除重绑、同卡再插、旧 BindingRef、失效 Subscription、inactive/historical 不一致、错误 Subscription、旧 epoch 和绑定在 Radio 观察间更新。A 再次读取 T 事实后才提交参数；旧请求不能穿越新绑定。绑定代次由 BindingIndex 单调推进，不增加全局身份表或重复缓存。

执行审计新增 `AccountRegistry::Read`、`BindingIndex::Find`、`TopologyService::ReadPrimaryTarget/AcceptStoredDiscovery` 正执行要求。SHA-256 保持独立无外部平台依赖的 Host 实现，已通过固定 Legacy 向量；产品参数/Provider 仍未接入。

## 阶段四：真实 Store Adapter Host 集成

Target C0 增加 `AssignmentStoreAdapter` 的真实实现并独立链接 `assignment_store_adapter.cpp`。A 不再由 RecordingStore 替代持久化兼容边界；Adapter Integration 使用真实 `AssignmentService + AssignmentAggregate + PlanPrimarySwitch + AssignmentStoreAdapter`，仅在 `AssignmentParameterPort` 最底层记录系统参数调用和受控结果。

`AssignmentStoreAdapter` 实现的 C0 方法为：

- `WriteUserPrimaryPreference` → `persist.telephony.userPreferPrimarySlot`
- `WritePrimaryParameters` → `persist.telephony.MainSlotId`，非空 raw ICCID 经 SHA-256/大写 Hex 后写 `persist.telephony.MainCard.Iccid`
- `WriteDefaultData` → `persist.telephony.MainCellularDataSlotId`
- `ReadPrimaryParameters` / `RestorePrimaryIfLegacyValid` → 原快照读取与 Legacy 有效槽规则

`WriteDefaultRole` 仍明确返回未确认失败，因为 C0 不使用 SIM 账户数据库；不建立第二套角色数据库，也不绕过 A 调用 Legacy Controller。Host 平台端口只模拟系统参数 API；真实 Provider/SA/IPC 未接入。

`callFinished=false` 的语义不是成功码 0：AssignmentService 不交付 Completion，不提交对应业务投影，并将原请求置为 `NeedsReconciliation`；已发生的平台效果由 Host 端口记录，但聚合保留该请求的进行中/未决状态。`callFinished=true` 的非零参数结果在 C0 按 Legacy 的 void/忽略结果语义保留为外部参数事实，不覆盖原 API 成功；主卡/Data 的内存角色和通知仍按原顺序推进。重复完成由 RuntimeOperationStore 拒绝旧 Step/重复/终态请求。

Adapter Integration 的 `validation_level=adapter_integration` 与 Component C0 独立；C0 Scenario 配对仍只登记原 Component 用例，避免重复 Scenario ID。LLVM 执行审计要求三个 Adapter 写方法正执行。

## 阶段三真实性与异常覆盖

`Observation()` 不再写入 Primary/Data、参数、通知、设备次数或重试的固定成功值。Primary/Data 来自 `AssignmentView`，参数来自 Recording Store 的实际持久化状态，通知来自 Output 的实际记录，设备/重试次数来自 Radio 记录，返回值来自 Completion 端口。任何实际业务实现变化都会改变归一化观测并被 Oracle 或 Legacy/Target 差分检出。

C0 Host 组件覆盖还包括：无绑定、未激活、绑定代次变化、Radio 未知/失效/不同映射、用户偏好失败、Primary 参数失败、Data 参数失败及其部分效果、同域冲突请求、失败后重新请求、旧 epoch、旧 Step、重复完成和终态拒绝。失败测试验证真实聚合投影与每个外部端口的效果；未为未裁决的 Legacy 参数失败顺序新增跨后端兼容码，Legacy 原路径仍以冻结 Golden 作为条件性 Host 基线。

`compare_backends.py` 的 C0 期望从冻结 `primary_inputs.json` / `primary_golden.json` 动态构造，并根据目标账户的 raw ICCID 计算 Legacy 兼容 SHA-256 大写 Hex；历史 Golden 文件保持原字节。比较器不持有第二份手工 C0 期望。

## ICCID 与持久化边界

Legacy 路径是 `SimFileManager::GetSimIccId()` → `SavePrimaryCardInfo(slot)` → SHA-256 → `BytesConvertToHexString` 大写 Hex → `persist.telephony.MainCard.Iccid`。Target 裁决为：`PrimaryTopologyPort` 提供当前绑定和 raw ICCID；`AssignmentService` 不编码、不计算哈希，只把 raw ICCID 传给 `AssignmentStore`；`AssignmentStoreAdapter` 负责 Legacy 兼容编码及原参数写入。当前 Adapter 仅有契约声明，正式实现及 Provider 参数适配尚未接入；Recording Store 的 Host 编码验证不能冒充真实持久化 PASS。

## 验证结果

- Target C0 单例：PASS
- Legacy C0 单例：PASS
- Both C0：Legacy PASS / Target PASS / Oracle PASS / Legacy-Target PASS / Overall PASS
- C0 比较字段差异：0
- 差分负控：零匹配、单边 BLOCKED、结果不同、架构-only 均 BLOCKED/FAIL
- Target C0 LLDB：在真实 `AssignmentService::StartPrimarySwitch` 停止并继续 PASS

历史 Legacy Golden 未修改。Target 组件 PASS 仍不是原生持久化、产品 SA/IPC、真实设备或完整生命周期验收。

## 后续 B、C1

下一批 Primary B/C1 需要按真实决策分别实现异步 Radio 请求关联、`AcceptRadioStep`、参数恢复、唯一业务重试、完成超时/迟到证据和真实 Radio 端口。激活、T 角色发现、Provider 持久化、产品装配、SA/IPC、厂商设备结束证据仍需单独验收，不从本轮 C0 组件结果推定完成。
