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
