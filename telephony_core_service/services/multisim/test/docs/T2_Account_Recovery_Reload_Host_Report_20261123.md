# T2 账户发现恢复、T/A 并发与 Current/Historical Reload 报告

输入基线：`5796fce68dfcc2ea152106b7ab65c217c63557ad`。本批不迁移生产写权，不接 PIN/PUK、
Records、eSIM、Device 或 SA 生命周期。

## 1. T/A 锁顺序与执行域

T1 交错审查确认潜在反序：

- A `StartPrimarySwitch` 已在 `primaryCommitLock_` 内读取 T 的 PrimaryTarget。
- T1 的 `DiscoverAccount` 在部分返回阶段持 `commitLock_` 调用 A role accept/finalize。

这两条组合形成 A→T / T→A 反序。T2 修复原则：

1. T 在短 commit 域中只接纳 Observation/Ledger/Binding/AccountRegistry。
2. T 的外部 Provider Match/Write 不持 T lock。
3. T 调 A 时始终不持 T lock；A 的 `AccountRolePlanningPort` 全部方法由 A 的
   `primaryCommitLock_` 保护。
4. Provider 回包后的 A projection accept 在 A lock 内完成；随后重新进入 T commit 域接纳账户/绑定。
5. A `StartPrimarySwitch` 的 A→T 只读路径没有 T→A 嵌套，因此不存在反序环。
6. 不持 T/A 锁同步等待 Provider；不使用全局业务锁。

`AccountRoleProtocol::Prepare` 不再以“任何 reservation 非空”阻塞全 A。预留按
`SubscriptionId` 或待分配 `SlotId` 分域；同一 identity/slot 冲突仍拒绝，不相关 slot/account 可并行。
确定性测试包含 C1/Primary in-flight × discovery、Provider I/O 中触发 A primary read/write、同 ICCID
不同 slot write-before-qualification 负控及并发失效。

## 2. Provider 结果分类

原 `SimRdbHelper` 返回和 Target 证据分开保存：

| 事实 | T2 分类 |
| --- | --- |
| 原 Insert/Update 返回不是 `INVALID_VALUE`，且 read-back 身份/slot/active/esim/label/roles 全匹配 | `ConfirmedApplied` |
| 原 Insert/Update 失败，即使某行当前匹配 | `Unknown`；匹配只可作恢复证据，不能证明本次调用生效 |
| 原调用已返回但 read-back 失败/空行/字段不匹配 | `Unknown` |
| Provider 确认不写且无匹配责任 | `Rejected`/`ConfirmedNotApplied` |
| Observation 或 binding 已失效 | 只记录已写/Unknown，不允许 Binding，并进入 reconciliation |

因此 T2 不把“当前行匹配预期”误推为本次写入因果。Provider 缺少 CAS/transaction 的事实保持不变。

## 3. Unknown / stale-applied 恢复

T2 复用 `DiscoveryObservationLedger` 的已登记 ticket，并在 `TopologyService` 保存最小
`PendingDiscovery { ticket, A rolePlan, original store result }`。这不是第二个账户 cache；
AccountRecord 仍只在 `AccountRegistry`。

恢复入口 `ReconcileDiscovery(observation)`：

1. 按 slot/application key 取得原 ticket，复核 epoch/session/ICCID。
2. 锁外再次 `MatchAccountByIccid`。
3. 无行、查询失败、身份冲突或 A reservation 已失效时保持 pending。
4. A 接纳原 role plan；T 接纳或核对 AccountRecord。
5. `ReconcileStored` 结束 Ledger 的未决状态，但不调用 Binding。
6. A `FinalizeInitialRoleReservation` 在 evidence 完整后释放。
7. 从 pending map 清除；旧重复 reconciliation 拒绝。

写已发生但卡已拔出/换卡：旧 Session 不能恢复 Binding。它只核对账户历史身份及 A/T 资格。
新卡必须用新 observation/session 才能建当前 Binding。Unknown 不自动重发 Insert。

`DiscoveryObservationLedger` 额外保留 per-ICCID in-flight owner，阻止两个 slot 对同一 ICCID
在写前同时 Provider 建行。Confirmed/known-not-applied/reconciled 释放该 reservation；
Unknown/applied-stale 保留直至恢复。

## 4. Current/Historical Reload

`AccountStoreAdapter::LoadAccounts` 由现有 Host bridge 调用原 `SimRdbHelper`：

- `AccountReadView::Current` → `QueryAllValidData`
- `Historical` → `QueryAllData`

返回同次行的账户字段和角色片段，不从 T/A 当前状态补造。

`TopologyService::PrepareAccountReload`：

- 先读取 A view revision（A 域）。
- 再在 T 域经 `AccountRegistry::BeginReload` 取得独立 attempt、membership watermark、
  每账户 identity/字段 revision。
- Provider request 使用 T 签发的 attempt，不信任外部前推编号。

`AcceptAccountReload`：

- 核对 view/attempt/rows+roles 数量。
- `AccountRegistry::AcceptReload` 分字段接纳 current/historical。
- 只有 acceptedAttempt 才调用 A `AcceptReloadedRoleProjectionSet`。
- 查询失败保留原 view；空成功与失败分开。
- 旧 attempt 不覆盖新 attempt；新账户水位保护读取期间新增成员。
- 字段版本独立；新 active 等本地写入不被旧行覆盖。

## 5. 测试

T2 Target 专项 11 项：

- stale applied recovery 不重建旧 Binding。
- Unknown 缺 Provider row 不释放；后续可信 row 恢复并释放。
- 同 ICCID 不同 slot 在 Provider 写前拒绝。
- C1 in-flight 与不相关 discovery。
- 不相关 slot 的 pending A reservation 可并行。
- Current/Historical 分别加载。
- 失败/空成功分离。
- 旧 attempt 不覆盖新字段。
- Reload 不删除 attempt 后新增账户。
- T/A lock order 确定性交错。

Provider bridge 6 项：Query failure/no-row、Current/Historical Load、Insert read-back、
failed-write+matching-row Unknown、Unknown read-back。

## 6. Provider bridge 归位审计

当前实现仍位于 test framework `legacy_account_store_provider.cpp`。正式
`AccountStoreAdapter` 只声明并通过桥函数调用；没有产品 target 链接该实现。

原因：

- 原 `SimRdbHelper`、DataShare、provider header 在此仓的 product assembly 之外；
- multisim skeleton GN 有意闭合为 platform-free C++，没有生产 T1 Provider target；
- 将 test bridge 直接移入 multisim/product target 会把测试 DataShare/SimRdb definitions
  带入真实装配，并且改变架构依赖边界。

因此当前结论为 **VERIFIED_HOST / PRODUCT_BRIDGE_BLOCKED**。真正产品归位的最小单元是由
现有 `SimModule` owning assembly 提供一个 `AccountStoreAdapter` companion source，链接原
`SimRdbHelper`/`sim_data`，同时不把 helper 包装成 T cache。该步骤只能在获得产品 GN/Provider
闭包、原 build tree 和依赖所有权后实施；本轮不把 test bridge 解释为产品化。

## 7. 回归与分级

- T2 Target 11/11；TSan 11/11；ASan 11/11。
- Provider bridge 6/6；TSan 6/6；ASan 6/6。
- 本轮 fresh 运行：Legacy exact 140/140，Target current 102/102，合计 242 项业务执行均通过；
  architecture 的 canonical digest 使聚合命令整体为 FAIL，但两个业务 backend 均各自 PASS。
  六个 frozen Both/Oracle/Legacy-Target 差异为 0。执行审计中 Discover/Reconcile/PrepareReload/
  AcceptReload 分别为 34/4/8/8，A reservation prepare/accept/still/finalize 为 25/18/52/27，
  Provider Load/Match/Write 为 4/3/3。
- T1/Legacy/Primary/Host Stop 门禁保持，不修改 frozen Golden。
- architecture 分别报告 structure、migration token、canonical digest。现有独立提交
  `06ca400a11` 已保存的受保护目录设计文档与固定 manifest digest 不一致；本批不覆盖、不改摘要、不重置。
- `hb`/`hdc` 缺失时产品 Provider/SA/Device 仍 BLOCKED。
- L3.2 的真实 Legacy graph 全图 sanitizer 仍因 Host FFRT stub zero-page/repeated-join 缺陷 BLOCKED；
  T2 新增的 Provider bridge 6/6 TSan/ASan 与 Target T2 11/11 TSan/ASan 不替代该独立缺陷。

分级：

- **T2_HOST_L3_PASS**：真实 T/A/Ledger/Registry/Binding 及受控 Legacy Provider bridge 闭环。
- **PRODUCT_PROVIDER_BLOCKED**：正式 product assembly/Provider 数据库运行未接线。
- **DEVICE_L4_BLOCKED / SO_L5_BLOCKED** 保持。
