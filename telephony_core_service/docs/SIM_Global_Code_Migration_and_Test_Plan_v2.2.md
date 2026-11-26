# SIM 多卡代码迁移与开发指导 v2.2

**交接候选版。** 实际代码和验收状态见[执行状态](SIM_Refactor_Execution_State.md#af2-m9-status)。
本版更新 [v2.1](SIM_Global_Code_Migration_and_Test_Plan_v2.1.md) 的 M9 工程对账。P1～P7 的
子任务、目录预算、验收条件及共享文件合入规则沿用 v2.1 第 3、4 章。
AF-1 的 Owner 和依赖规则继续由已签收设计维护。开发与回归直接继承
[CD-01～03](AF2_Compatibility_Decisions_Approved_20261126.md)。

## 候选工程与样板

| 类型 | 从这里进入 | 当前证据边界 |
|---|---|---|
| 发现、绑定 | `AF2Discovery.LiveCardDiscoveryCreatesAccountAndBinding` | 正式模块、会话、真实账户及 Binding |
| 同步查询、同步修改 | `AF2AccountMetadata.LiveDualSimMetadataAndFourRoles` | M6；共用名称、标签、重复和失败序列用 SIM023-LEGACY-ACCOUNT-METADATA-DUAL |
| 持久偏好 | `AF2ImsPreference.LiveDualSimSavesAndQueriesProviderPreference` | M7A，读源及空 Binding 各自回归 |
| 四角色 | `AF2VoiceSms.LiveDualSimSelectsFourRolesThroughLegacyEntries` | M3～M5 正式入口，复用角色写集 |
| B/C1 异步 | `TargetPrimaryB` 的 B、C1 和成员交接专项 | 组件路径。正式组合根仍为 ObserveOnly／C0Only |
| 号码组合 | `AF2ShowNumber.LiveCardNumberWriteUpdatesAccountAndCompletes` | M7B，原任务固定关系、原设备结果、Provider 写和回调 |
| 卡安全短请求 | Legacy `SimStateManager::UnlockPin` | Target 需接卡应用会话、原返回、错误剩余次数、LOCKED→NOT_READY、厂商回调 |
| ICCID EF | Legacy `SimFile::ProcessGetIccIdDone`，Target `LoadSession` | Target 仍需用真实 EF 回包驱动解码、Records、发现和绑定 |

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
   Run 和 Debug、真实生产断点。GUI 结果单独交测试负责人确认。

## 构建与代码统计

Host 两后端分别编译、链接和执行。Legacy 的 SIM 基础及 eSIM 条件源由
`sim_legacy_sources.gni` 统一定义，原产品仍链接原 SIM 实现。Target Host 使用
`sim_target_sources.gni` 选择可运行闭包，逻辑模块 GN 的可见性规则继续生效。
在 OHOS 产品树完成原生静态库及链接验证后，再按原批准的 WU 门禁处理切换。

可复跑的源码统计脚本在 `test/scripts/handoff_inventory.py`。逐文件区分 Target Host
对象、逻辑 GN 声明及其他候选。头文件计为声明资产，物理行和有效行分开统计。
测试执行 ID 按后端记录，已映射共同 Scenario 只算一个语义项目。缺少审核映射时，
后端注册数用于运行覆盖，不能推出全局业务语义数。
[逐文件统计及原始回归](../services/multisim/test/docs/evidence/af2_m9_20261126/README.md)

## 候选实测范围

目录统计冻结于 `b6f6c25eed8b690da53f5b86ef3e7883cbe46916`。目录行包含其子目录。
随后 Radio 结果修订净减 25 个生产物理行，无新增生产文件。该切片新增一个 Target 专属
语义用例，Target 全量登记增至 172 个。

| 目录 | 文件 | 物理行 | 有效行 |
|---|---:|---:|---:|
| 生产合计 | 101 | 11,184 | 8,554 |
| `account` | 33 | 3,867 | 2,935 |
| `account/activation` | 7 | 365 | 179 |
| `account/discovery` | 3 | 287 | 218 |
| `account/number` | 9 | 464 | 261 |
| `card` | 17 | 1,251 | 842 |
| `card/esim` | 4 | 185 | 84 |
| `card/records` | 3 | 152 | 83 |
| `card/runtime` | 7 | 609 | 447 |
| `card/security` | 1 | 29 | 8 |
| `card/state` | 2 | 276 | 220 |
| `device` | 4 | 930 | 834 |
| `device/resource` | 4 | 930 | 834 |
| `infrastructure` | 28 | 2,351 | 1,674 |
| `infrastructure/assembly` | 11 | 1,142 | 867 |
| `infrastructure/assembly/exports` | 2 | 42 | 8 |
| `infrastructure/compatibility` | 12 | 757 | 476 |
| `infrastructure/compatibility/api` | 7 | 506 | 341 |
| `infrastructure/compatibility/events` | 5 | 251 | 135 |
| `infrastructure/request` | 4 | 371 | 276 |
| `role_assignment` | 19 | 2,785 | 2,269 |
| `role_assignment/primary` | 5 | 288 | 186 |

Target Host 归档有 25 个 CPP 对象。另有 8 个 CPP 只参与逻辑 GN，68 个头文件计声明资产。
测试 cases／framework 的 C/C++ 为 167 个文件、23,722 物理行、19,788 有效行。
相对中心目标 33,500 物理行，候选差额为 22,316 行。该差额是实测对账，P 的净增分配和
数量考核继续按原窗口及实际功能证据调整。

## 剩余任务和预算

[逐项台账](../services/multisim/test/docs/evidence/af2_m9_20261126/task-inventory.json) 摘录
SIM-001～062、WU-01～09 和 32 个 P 子任务的原入口、Owner、依赖及证据要求。尚未复核的
SIM 条目由各 Owner 对照[迁移设计](migration/SIM_Architecture_Refactor_Design.md)与真实入口认领。
所有 WU 保持原产品 writer 责任。

| 包 | 责任田及并行条件 | v2.1 原预算 / 交接前预留（净增 LOC，新增测试） | 候选边界 |
|---|---|---|---|
| P1 账户与绑定 | A。Provider 谓词、账户观察可分开，T 接线单点合入 | 1,460，34 / 300，8 | M2、M6～M8 有 Host 基线，产品 Provider、探测及号码桥需证 |
| P2 卡状态、安全、EF、资源 | B、C、G。先固定应用来源、资源及请求契约，B 写安全，C 写 codec/Records | 6,430，86 / 1,700，26 | 会话前置、资源已有验证，PIN/ICCID 待完整链路 |
| P3 激活、角色、Radio | D、G。成员规划由 G 提供，A Owner 单点接线 | 2,850，63 / 500，9 | C0 和四角色已验证，P3.4 新增成员交接负控，正式 B/C1 未结 |
| P4 电话簿、短信、STK、运营商 | E。先取证及纯规则，写业务依赖 P2 对应 EF/会话 | 5,350，61 / 0，0 | 原设备及写权维持 |
| P5 eSIM 卡侧 | F。通道、Profile、66250 和厂商口分开 | 4,050，53 / 0，0 | 原共享资源、协议、外部 Owner 逐项取证 |
| P6 入口、构建、通知、生命周期 | H。公共接口先合入，各 Owner 按原票处理 | 1,990，54 / 580，14 | P6.5 新增投递负控，产品构建和全图关闭未结 |
| P7 完整替换 | 联合验收，按 WU 协作 | 无独立预算 | 原生产仍为 Legacy |

19,050 行与 294 例是 v2.1 预留全部兑现后的条件余额。实测资产与已运行负控计入候选，
未验收的样板和演练保留原预留。独立语义用例预算在 H05/H08 对账后冻结。
八个责任田的目录、共享文件维护人和集成顺序继续按
[v2.1 §4](SIM_Global_Code_Migration_and_Test_Plan_v2.1.md#4-全局实施顺序与批次验收开发人员执行指导)。

正式样板具备后，由开发组长把 UnlockPuk、IMSI 分给 B、C 的非框架作者。在独立分支记录
修改范围、共用业务断言、负控及合入回归。进入交接基线后分别核销 P2.3、P2.6。
