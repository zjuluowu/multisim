# AF-2 M7A IMS Preference 开发与调试

## Legacy 入口与契约

`CoreManagerInner::{Save,Query}ImsSwitch` 是进程内 CMI 入口，直接转到 `SimManager` 和
`MultiSimController`。本链路没有独立的 IPC 权限检查。CMI 缺少 Manager 时返回 `-1`。
Manager 先取生命周期 Lease，再检查槽范围及 Controller。无效槽返回
`TELEPHONY_ERR_ARGUMENT_INVALID`。这几项早期拒绝会保留查询调用者的原输出变量。
[CoreManagerInner](../../../../frameworks/native/src/core_manager_inner.cpp#L2589) · [SimManager](../../../../services/sim/src/sim_manager.cpp#L1408)

| 分支 | 原行为 |
|---|---|
| 取得 ICCID | `GetTargetIccId` 在锁内复制当前槽缓存的 ICCID。它检查缓存边界，不要求账户激活，也不判空字符串。Controller 找不到缓存位置或 DB Helper 对象为空时返回 `-1`，查询还把输出设为 `-1`。 |
| 保存 | `UpdateDataByIccId` 在普通 SIM URI 上提交 `icc_id = 原 ICCID` 与单列 `ims_switch`。任何输入整数都直传，重复请求也写。原返回直接透传，未单独定义「只有 0 才成功」的业务判据。 |
| 查询 | `QueryDataByIccId` 现场查询 Provider，使用同一 URI 和谓词。原 Helper 请求所有列并读取结果中的 `ims_switch`。Controller 以 `-1` 初始化临时值，忽略 Helper 的返回，原出口返回 0 并带回该值。没有行、Helper 获取失败、查询失败和行中明确存放 `-1` 的外部结果均可为 `(0, -1)`。 |
| 账户状态与通知 | 保存和查询均不更新 Current/Historical 的偏好值，不通知观察者，也不操作 IMS 设备。 |

URI 为 `datashare:///com.ohos.simability/sim/sim_info`。
[MultiSimController](../../../../services/sim/src/multi_sim_controller.cpp#L1956) · [GetTargetIccId](../../../../services/sim/src/multi_sim_controller.cpp#L1902) · [SimRdbHelper 查询](../../../../services/sim/src/sim_rdb_helper.cpp#L430) · [SimRdbHelper 写入](../../../../services/sim/src/sim_rdb_helper.cpp#L541)

## Target 调用与资格

`SimModule::SaveImsSwitch(slot, preference)` 经 H 准入和 C 错误映射进入
`TopologyService::SaveImsPreference`。T 复用 M6 的 `UpdateMetadata`，在提交锁内由
`BindingRef` 得到持久 `SubscriptionId` 和 `AccountRecord.iccid`，按账户和字段登记一次
`AccountMetadataOperation`。I 在锁外提交原字段，C 唯一的同步完成透传已结束调用的 Provider
返回码，无账户通知。T 单独判定原效果。已证写入且原绑定、身份和字段版本仍有效时，
Current、Historical 的 `imsPreference` 版本分别推进，保护较旧的读镜像，缓存的偏好值保持原读源。
无法确认的效果仍持有相应写权，普通同值查询不能结清。Stop 封新请求并保留 H 调用寿命，
原效果通过 `ReconcileAccountMetadata(operation)` 精确核对，不补发旧结果或更新后继账户。

查询经 `TopologyService::QueryImsPreference` 固定原 ICCID，释放提交锁后调用
`AccountStoreAdapter::QueryImsPreferenceByIccid`。Host Provider 可仅投影所需列，内部区分
`Failed`、`NoRow` 和 `Value`（包含明确的 `-1`），槽有效且取得原绑定后保留 Legacy 的
`(0, value/UNKNOWN)` 出口。查询进行中换卡时，原调用可向自己的输出变量返回旧 ICCID 的结果，
后续调用使用新绑定。查询本身无缓存接纳和写入证据。

同账户同字段互斥。Host Provider 证明单列隔离后，IMS Preference 与名称、标签并行。
隔离未证时，同账户保守互斥，不同账户仍可推进。全部外部读写在 T 提交锁外。

## 独立兼容差异

- [CD-02](../../../../docs/AF2_Compatibility_Decisions_Approved_20261126.md#cd-02) 已批准。Legacy 单激活空槽以空 ICCID 调用 Provider，保存返回 `0`，查询返回 `(0, -1)`。Target 无当前 Binding 时保存返回 `-1`，查询返回 `(-1, -1)`，Provider 调用为零。已绑定 IMS 的原读源和返回继续独立验证。产品切换前仍需核验调用者适配。[2026-11-25 原始证据](evidence/af2_m7a_20261125/README.md)
- [CD-01](../../../../docs/AF2_Compatibility_Decisions_Approved_20261126.md#cd-01) 对未知 ICCID 标签的批准独立生效，已知历史账户仍允许改标签。[M6 契约](M6_Account_Metadata_Debug.md)

## 单步示例

从仓库根执行。Developer 和 Verify 使用独立构建目录。

```bash
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" run --backend target --debug-build \
  --out /tmp/multisim-m7a-dev --output /tmp/multisim-m7a-dev-run \
  --filter AF2ImsPreference.LiveDualSimSavesAndQueriesProviderPreference

lldb /tmp/multisim-m7a-dev/debug/build/sim_test_target_host
```

在 LLDB 中用 `breakpoint set -n <完全限定函数名>`，或者按下表行号设置断点。
输入 `run --gtest_filter=AF2ImsPreference.LiveDualSimSavesAndQueriesProviderPreference`，
使用 `thread step-over` 对比变量。用例定义在 [业务用例](../cases/target/primary/c0_topology_integration_test.cpp#L2483)。

| 断点 | 实际观察 |
|---|---|
| [SimModule::SaveImsSwitch](../../infrastructure/assembly/sim_module_factory.cpp#L99) → [LegacySimFacade](../../infrastructure/compatibility/api/legacy_account_entry.cpp#L51) | 槽 0，偏好 21，无调用者权限参数 |
| [TopologyService::UpdateMetadata](../../account/topology_metadata.cpp#L92) 和 [字段登记](../../account/topology_metadata.cpp#L137) | Binding 属于持久订阅 40。短提交域登记 `ImsPreference`，原步骤号 1 |
| [AccountStoreAdapter::WriteImsPreference](../../account/account_store_adapter.cpp#L54) | 普通 SIM URI、原卡 ICCID、单列 `ims_switch`、整数 21 |
| [AccountRegistry](../../account/account_registry.cpp#L257) | Current 的 `imsPreference` 版本从 0 变 1，缓存偏好值仍未设置 |
| [LegacyCompletion](../../infrastructure/compatibility/events/legacy_completion.cpp#L42) | 原同步整数 0，成功也不通知账户观察者 |
| [QueryImsSwitch](../../infrastructure/assembly/sim_module_factory.cpp#L108) → [真实 Store 读](../../account/account_store_adapter.cpp#L63) → [原输出](../../account/topology_metadata.cpp#L62) | 独立 Provider 查询同一 ICCID，`Value` 为 21，输出从 UNKNOWN 变为 21，返回 0 |
| [Stop](../../infrastructure/assembly/sim_module_factory.cpp#L267) → [销毁](../../infrastructure/assembly/sim_module_factory.cpp#L373) | 在途数为 0，断点进入实际 `delete module`，进程退出 0 |

用例还验证订阅 41 的独立偏好 `-7`、重复保存与零设备命令。VS Code GUI 人工未验收。

## 修改局部性与 M7B

| 目标源码 | M6 | M7A |
|---|---:|---:|
| 新增文件 | 2 | 0 |
| 修改文件 | 13 | 12 |
| 新增行 | 459 | 186 |
| 删除行 | 17 | 51 |

M7A 修改账户域的 8 个文件及 H/C 的 4 个文件。复用 M6 的短锁准入、请求票据、效果核对、
保活及同步完成。查询增加一个 Provider 读入口，使用原 AccountRegistry 身份，不增加状态缓存。
R、A、公共请求实现和 GN 依赖图保持原职责和边界。

M7B 先分开核对号码的卡端 MSISDN、账户展示号码和 IMS IMPU 的读源，再沿既有
`PrepareNumberTarget` 和号码字段写权接线。记录卡写与数据库写的分别结果、原会话及迟到证明。
只有对应阶段取得原效果后才接纳该阶段，号码业务不可直接套用 IMS 的单列 Provider 路径。

[验证及原始证据](evidence/af2_m7a_20261125/README.md)
