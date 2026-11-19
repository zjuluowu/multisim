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
