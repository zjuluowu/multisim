# SIM 多卡代码迁移与开发指导 v2.2

**架构基础交付候选。** Architecture Ready、业务特性补充、完整生产替换按各自证据推进。 实际代码和验收状态见[执行状态](SIM_Refactor_Execution_State.md#af2-m9-status)。
本版更新 [v2.1](SIM_Global_Code_Migration_and_Test_Plan_v2.1.md) 的 M9 工程对账。P1～P7 的
子任务、目录预算、验收条件及共享文件合入规则沿用 v2.1 第 3、4 章。
业务 Owner 与依赖规则由主设计维护。开发与回归继承 [AD-01～04](SIM_Target_Architecture_Design_v1.2.md#architecture-ready-decisions)
及 [CD-01～04](AF2_Compatibility_Decisions_Approved_20261126.md)。

## 候选工程与样板

| 类型 | 从这里进入 | 当前证据边界 |
|---|---|---|
| 发现、绑定 | `AF2Discovery.LiveCardDiscoveryCreatesAccountAndBinding` | 正式模块、会话、真实账户及 Binding |
| 同步查询、同步修改 | `AF2AccountMetadata.LiveDualSimMetadataAndFourRoles` | M6；共用名称、标签、重复和失败序列用 SIM023-LEGACY-ACCOUNT-METADATA-DUAL |
| 持久偏好 | `AF2ImsPreference.LiveDualSimSavesAndQueriesProviderPreference` | M7A，读源及空 Binding 各自回归 |
| 四角色 | `AF2VoiceSms.LiveDualSimSelectsFourRolesThroughLegacyEntries` | M3～M5 正式入口，复用角色写集 |
| B/C1 异步 | `AF2Primary.DedicatedSwitchAwaitsOriginalCompletionThroughFormalModule` | 正式 Host B 已贯通兼容等待、原成员许可、回包、逐键 Store 与关闭。C1 仍在组件图验证 |
| 号码组合 | `AF2ShowNumber.LiveCardNumberWriteUpdatesAccountAndCompletes` | M7B，原任务固定关系、原设备结果、Provider 写和回调 |
| 卡安全短请求 | Legacy `SimStateManager::UnlockPin`，Target `SimModule::UnlockPin` | Host 正式卡应用、原资源和回包、剩余次数、LOCKED→NOT_READY 已验证。等待由受控设备适配器报告。真实 RIL 及厂商回调独立验收 |
| ICCID EF | Legacy `SimFile::ProcessGetIccIdDone`，Target `SimModule::LoadIccid` | Host 透明 EF 已执行原响应解码、Records、账户发现及绑定。其他卡型和产品路由独立验证 |

账户加载见 [M8 Reload 调试](../services/multisim/test/docs/M8_Account_Reload_Debug.md)。
M9 门禁、Radio 反例及证据范围见[验收报告](AF2_M9_Developer_Handoff_Acceptance_Report.md)。

## 从干净工程运行

1. 按 [Host 使用说明](../services/multisim/test/docs/Host_Usage.md) 准备固定 GoogleTest、
   OpenSSL 与 runtime。每个 worktree 使用独立的 `--out`。
2. 使用 `test.py list` 查精确用例。用 `run --backend target --debug-build` 单例进入
   生产实现，用 `run --backend both --filter <Scenario>` 比较共同业务。
3. 普通 Target 专属测试放在 `test/cases/target/`，下一次增量构建自动刷新 GN 源表。
   共同业务优先复用 `cases/shared/` 的场景体，再接两端真实装配。产品独立行为、内部
   架构行为和已批准 CD 继续用原分类。
4. 在 VS Code Remote WSL 中运行 Prepare Testing Panel，检查枚举、刷新、新用例
   Run、Debug 和生产断点。实际 GUI 未执行时记录 `GUI_NOT_RUN`，CLI 与 LLDB 保留实测证据。

## 构建与代码统计

Host 两后端分别编译、链接和执行。Legacy 的 SIM 基础及 eSIM 条件源由
`sim_legacy_sources.gni` 统一定义。Host 全量业务链接 `sim_legacy_production_host`，
原产品根仍链接 Legacy。Target Host 使用
`sim_target_sources.gni` 选择可运行闭包，逻辑模块 GN 的可见性规则继续生效。
在 OHOS 产品树完成原生静态库及链接验证后，再按原批准的 WU 门禁处理切换。

可复跑的源码统计脚本在 `test/scripts/handoff_inventory.py`。逐文件区分 Target Host
对象、逻辑 GN 声明及其他候选。头文件计为声明资产，物理行和有效行分开统计。
测试执行 ID 按后端记录，已映射共同 Scenario 只算一个语义项目。缺少审核映射时，
后端注册数用于运行覆盖，不能推出全局业务语义数。
[逐文件统计及原始回归](../services/multisim/test/docs/evidence/af2_m9_20261126/README.md)

## 候选实测范围

以下为本轮 Radio 发令许可检查点的实测。目录行包含其子目录。

| 目录 | 文件 | 物理行 | 有效行 |
|---|---:|---:|---:|
| 生产合计 | 107 | 12,212 | 9,441 |
| `account` | 33 | 3,908 | 2,970 |
| `account/activation` | 7 | 365 | 179 |
| `account/discovery` | 3 | 309 | 236 |
| `account/number` | 9 | 464 | 261 |
| `card` | 23 | 1,930 | 1,424 |
| `card/esim` | 4 | 185 | 84 |
| `card/records` | 5 | 426 | 321 |
| `card/runtime` | 9 | 867 | 667 |
| `card/security` | 3 | 137 | 98 |
| `card/state` | 2 | 315 | 254 |
| `device` | 4 | 991 | 888 |
| `device/resource` | 4 | 991 | 888 |
| `infrastructure` | 28 | 2,507 | 1,818 |
| `infrastructure/assembly` | 11 | 1,249 | 966 |
| `infrastructure/assembly/exports` | 2 | 42 | 8 |
| `infrastructure/compatibility` | 12 | 799 | 514 |
| `infrastructure/compatibility/api` | 7 | 518 | 353 |
| `infrastructure/compatibility/events` | 5 | 281 | 161 |
| `infrastructure/request` | 4 | 378 | 283 |
| `role_assignment` | 19 | 2,876 | 2,341 |
| `role_assignment/primary` | 5 | 316 | 204 |

Target Host 归档编入 29 个 CPP，另有 7 个 CPP 只参与逻辑 GN，71 个头文件计声明资产。
Target 登记 212 个，最近 Legacy 全量登记 149 个。已证实 14 个共同语义，内含 12 个冻结 Oracle。
测试 cases／framework 的 C/C++ 为 173 个文件、25,468 物理行、21,361 有效行。
相对本轮输入，生产 C/C++ 净增 1,059 物理行。距原中心预算 33,500 行的差额为 21,288 行，
实际业务和测试预算在按完成凭证核销后更新。
[当前源码与测试统计](../services/multisim/test/docs/evidence/af2_m9_d01_permission_20261127/inventory.json.gz)

## 剩余任务和预算

[逐项台账](../services/multisim/test/docs/evidence/af2_m9_20261126/task-inventory.json) 摘录
SIM-001～062、WU-01～09 和 32 个 P 子任务的原入口、Owner、依赖及证据要求。尚未复核的
SIM 条目按[迁移设计](migration/SIM_Architecture_Refactor_Design.md)和实际生产入口及用例逐项核实。
所有 WU 保持原产品 writer 责任。

| 包 | 责任田及并行条件 | v2.1 原预算 / 架构基础预留（净增 LOC，新增测试） | 候选边界 |
|---|---|---|---|
| P1 账户与绑定 | A。Provider 谓词、账户观察可分开，T 接线单点合入 | 1,460，34 / 300，8 | M2、M6～M8 有 Host 基线，产品 Provider、探测及号码桥需证 |
| P2 卡状态、安全、EF、资源 | B、C、G。先固定应用来源、资源及请求契约，B 写安全，C 写 codec/Records | 6,430，86 / 1,700，26 | 正式 UnlockPin、透明 ICCID、GID1 和共用传输已验证。已实测同卡字段及邻卡 Refresh 隔离。其他卡型及扩展演练仍待 |
| P3 激活、角色、Radio | D、G。成员规划由 G 提供，A Owner 单点接线 | 2,850，63 / 500，9 | C0 和四角色已验证，P3.4 新增成员交接负控，正式 B/C1 未结 |
| P4 电话簿、短信、STK、运营商 | E。先取证及纯规则，写业务依赖 P2 对应 EF/会话 | 5,350，61 / 0，0 | 原设备及写权维持 |
| P5 eSIM 卡侧 | F。通道、Profile、66250 和厂商口分开 | 4,050，53 / 0，0 | 原共享资源、协议、外部 Owner 逐项取证 |
| P6 入口、构建、通知、生命周期 | H。公共接口先合入，各 Owner 按原票处理 | 1,990，54 / 580，14 | P6.5 新增投递负控，产品构建和全图关闭未结 |
| P7 完整替换 | 联合验收，按 WU 协作 | 无独立预算 | 原生产仍为 Legacy |

19,050 行与 294 例是 v2.1 预留全部兑现后的条件余额。实测资产与已运行负控计入候选，
未验收的样板和演练保留原预留。独立语义用例预算在 H05/H08 对账后冻结。
八个责任田的目录、共享文件维护人和集成顺序继续按
[v2.1 §4](SIM_Global_Code_Migration_and_Test_Plan_v2.1.md#4-全局实施顺序与批次验收开发人员执行指导)。

UnlockPin 与 ICCID EF 具备稳定样板后，独立 Agent 从同一基线分别用 UnlockPuk、IMSI 在独立 worktree
验证扩展的改动范围、共用业务断言、负控及合入回归。实际完成的能力与测试分别核销 P2.3、P2.6。

## 架构基础交付续批

Legacy 与 Target 的 Host 归档分别编入 51 和 29 个生产 CPP。完整 Legacy 归档在
Primary 和 Bootstrap 两个消费者中共用，容量探针保留窄闭包。源码增删后检查 GN
归属及归档内容。条件 Feature 与产品原生构建待实际产品树验证。

八个已经验证的共用业务路径有源码共用体，其中 M6 继承已有实现。五个 B/C1 共同契约
继续使用组件测试与原 Oracle。Reload 连续 ID 的正向加载形成独立共用场景，Target 保留内部因果规则专项。
稀疏 simId 按 CD-04 保留原值，独立验证 Target 历史账户保留及 Legacy 已知遗漏，继续核对原产品调用者。工作包台账保留未具备证据的业务标记，未将场景体抽取计作新增业务。新增 Reload 双端登记只计一个共用测试。
[基础证据与问题清单](../services/multisim/test/docs/evidence/af2_m9_foundation_20261126/README.md) ·
[SIM/WU/P 台账](../services/multisim/test/docs/evidence/af2_m9_foundation_20261126/task-inventory.json)
