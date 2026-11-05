# SIM Linux TDD 防护网

历史容量切片：**Linux 受控组件最小切片已运行**，该套件仅覆盖 `ISimManager::GetMaxSimCount()` 的只读容量查询。正常 GoogleTest 1 PASS；独立检查器负控1预期FAIL。编译、链接和源级覆盖证据确认执行了未修改的SIM生产方法。

本目录原五条完整服务入口候选仍全部BLOCKED；这些候选没有验证无卡/Ready/账户/PIN/双卡隔离、SA注册/发现、真实Client/Proxy/Stub IPC、服务入口检查、真实权限/capability、Modem或设备。另一个[SIM Guard组件工程](../sim_guard/README.md)已运行卡状态/PIN/PUK/双槽场景，不能将候选入口阻塞误读为全仓没有组件测试，也不能用组件PASS关闭完整服务/设备门禁。

上述容量历史记录来自 `main@df8f2f017f83c70145b20255b4e9042f7a5fd923` + 当时工作树，不代表当前 HEAD。G2a B+ 本轮实测独立记录如下，容量套件和原五条 BLOCKED 不计入 G2a。

2026-11-05状态整理：首次B+ G2a代码已在main；最新强化与复验证据保存于独立 `work/g2a-bplus-revalidation-20261104`，尚未合入main。实际测试代码722d0441、交付097e536a及远端关系见[执行状态 §0](../../docs/SIM_Refactor_Execution_State.md#current-status)。本次只整理文档，没有重跑下表测试或改写历史JSON。

| 测试资产 | 已保存的实际结果 | 验证边界 |
| --- | --- | --- |
| 本目录G2a查询 | 干净最终代码7 PASS；恢复历史7 PASS，返回差分一致；14项审计测试PASS | 五个生产cpp、六函数正执行；完整Core/Manager初始化未运行，产品未准入 |
| SIM Guard兼容/设施 | 17场景×5轮85 PASS；设施19 PASS；突变检出后恢复17 PASS | Legacy卡状态/PIN/PUK组件，非账户或数据库重启恢复 |
| SIM Guard缺陷/待裁决 | 5条×5轮25 FAIL，独立保留 | 兼容PASS不覆盖这些红灯；Refresh返回契约等仍需裁决 |
| 完整初始化/服务/产品 | BLOCKED/NOT_RUN | 14个SimData字段及匹配产品/Provider环境仍未闭合 |

现行目标设计及对Oracle、Phase0和差分的影响见[设计对齐](docs/design_alignment.md)与[架构入口](../../docs/README.md)。

- [组件入口核实](docs/component_entry.md)
- [服务入口capability调查](docs/capability_investigation.md)
- [依赖、源码target及平台兼容偏差](docs/dependencies.md)
- [实际运行记录与命令](docs/run_record.md)
- [当前容量切片15项设计](docs/cases/SIM-COMP-001.md)
- [首批调查](docs/investigation.md)、[边界图](docs/boundaries.md)、[工程方案](docs/architecture.md)
- [Oracle来源表](docs/oracle_catalog.md)、[原五条完整设计](docs/cases/README.md)、[Suite Matrix](docs/suite_matrix.md)
- [结果与差分计划](docs/execution_plan.md)

## Linux 场景构造

执行目标为 Linux 原生、无需实体设备。GoogleMock 只替换外部依赖；真实 SIM 生产逻辑保留。当前已将容量场景迁移至 GoogleMock，详见 [场景构造与限制](docs/linux_gmock_scenarios.md)。原五条 BLOCKED 表示生产依赖适配尚未完成，不表示 B 层级必须使用设备。

## 构建与运行

显式准备固定GoogleTest archive，来源/commit/checksum/license见 [dependency_lock.json](configs/dependency_lock.json)。构建不下载、不使用未知系统gtest库。离线准备：

```sh
python3 test/systemtest/scripts/prepare_dependencies.py --archive /tmp/sim-googletest-b514bdc.tar.gz --destination /tmp/sim-systemtest-deps
```

从Core目录运行：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-component-out
```

依赖Clang18、GNU gold、GN/Ninja、Python3.12+、llvm-profdata-18/llvm-cov-18、Linux pthread以及libxml2头；完整版本与限制见运行记录。若准备/编译失败，runner输出JSON BLOCKED，executed=false，不制造GTest XML；支持的正常路径输出normal.xml、result.json与negative.xml（预期失败，独立记录）。产物全部在输出目录，不写源目录。

## 独立target和边界

BUILD.gn把未修改生产源、平台边界、启动器、GoogleTest和测试分开。没有引用/复制现有test目标/Mock/Fixture，没有include生产.cpp或私有访问宏。容量方法无需OnInit是当前版本实测候选行为，不推广至其他方法；实际支持范围只读容量。未支持的平台API无实现，不能任意success。LTO移除未调用虚函数，不用于宣称其他SIM功能已经可运行。

正常运行只在新子进程查询生产接口，GoogleMock 参数边界提供单槽字符串配置；没有卡身份。runner有单调截止与进程组watchdog，清理临时命名空间；保留脱敏XML/JSON/覆盖和link map作为证据。观察负控只改测试证据为-1，不修改生产代码，不证明真实故障恢复。

容量差分工具 `scripts/compare_results.py --legacy <result.json> --split <result.json> --output <outside-source>/diff.json` 仅比较已实施场景：上下文变更标INCOMPARABLE，O-L06精确值差异标MANUAL_REVIEW，不按未经确认规则自动判错。当前没有真正Split版本运行结果。

## 后续场景推进与实测阻塞

已补齐InnerEvent信封的类型/所有权适配，以及真实ITelRilManager接口的GoogleMock设施。运行器额外输出adapter.xml，16项INFRA自检单独计数，不能增加SIM业务用例完成数。原五条仍BLOCKED；新增未初始化C基线候选也因真实生产方法未能链接而BLOCKED，未采集Legacy行为。

- [真实初始化依赖审计](docs/initialization_closure.md)
- [事件适配范围](docs/event_adapter.md)、[RIL Mock设计](docs/ril_mock.md)
- [匹配接口缺失与上游版本差异](configs/platform_resolution.json)

可复现的编译审计（不会执行探针、不产生GoogleTest PASS/XML）：

```sh
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-initialization-final-record
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --target sim_bootstrap_characterization --output /tmp/sim-bootstrap-final-record
```

平台准备失败由initialization_audit.json记录BLOCKED/executed=false与真实编译诊断，保持主容量套件独立可运行。运行器清除已知旧XML，避免准备失败遗留过期PASS。Windows挂载源的未来文件时间通过有界时间条件等待处理，属于构建准备，不是SIM行为Oracle或固定sleep判通过。

上游外部接口分析见 [upstream_external_contracts.md](docs/upstream_external_contracts.md)，参考仓库的分支、固定SHA、源码路径及摘要见 [upstream_contracts.json](configs/upstream_contracts.json)。新增DataShare游标/Query与公共事件发布边界通过GoogleMock显式配置，未提供服务时失败。运行器验证固定原始平台头摘要，差分要求相同外部契约清单。FFRT队列子集及真实生产TelEventHandler/TelEventQueue已运行，详见docs/ffrt_runtime.md；不等同已运行SIM初始化。

最新依赖处理见 [BLOCKED处理记录](docs/blocked_resolution_oct08.md)：21份原始平台头已固定摘要；初始化目标45个对象编译成功，当前编译错误仅剩14个未匹配存储协议字段。整体初始化编译/链接仍BLOCKED，未运行状态/账户场景。

<a id="g2a-bplus"></a>

## G2a B+ 真实只读查询（M1）

源自 `work/g2a-bplus-integration`、现已按授权合入 main，使用当前 B+ 的 `services/multisim/integration/api/real_sim_count_query.*`，不恢复 application/query 目录。原 Core 入口和 SimManager/EXT 是实际生产 cpp，RIL 仅在平台接口以 StrictMock 替代，厂商覆盖值从明确 ABI 注入。冻结七场景 Oracle 不变：1/2/0/-2/invalid → 1/2/0/-2/0；参数2、厂商5/-31 → 5/-31。

从 Git 根准备锁定依赖并运行（变量指向当前机器仓外目录）：

```sh
python3 -B telephony_core_service/test/systemtest/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
python3 -B telephony_core_service/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query"
```

`SIM_GTEST_ROOT` 为解包后的 googletest-b514bdc898e2951020cbdca1304b75f5950d1f59；URL、archive/tree SHA 以 [dependency_lock.json](configs/dependency_lock.json) 为准。runner 校验锁定树/平台头、RIL Mock 接口、五份显式源表，编译/链接五个真实生产 cpp，运行七个隔离进程后导出覆盖；六条指定函数必须在对应源文件正执行，不能关闭审计来获得 PASS。原控制块反例、零值重读和清理断言保留，新增失败/重复附着、非空 Manager 替换及 RIL 释放反例。它们证明顺序生命周期，不证明并发热替换。

2026-11-02 在干净代码提交 `ff1c6ef2ea73d36ab83a7ac2c276036c6f9a6a58` 重新构建执行：7 PASS/0 FAIL/0 BLOCKED，六条审计函数正执行。`manager_on_init_executed=false`、`core_init_executed=false`；完整初始化、SO/SA/IPC、Feature 和设备不在该 PASS 范围。新精简证据见 [g2a_bplus_evidence.json](configs/g2a_bplus_evidence.json)，原 [g2a_checkpoint_evidence.json](configs/g2a_checkpoint_evidence.json) 保留原字节、旧代码/路径及历史结果，不能当成本轮证据。

完整初始化审计仍用 `audit_initialization.py` 和锁定 OpenSSL 头包，Query 本身不依赖该包。本轮实际重跑仍 BLOCKED：45对象、43诊断、14个未匹配 SimData 字段、无缺头，未链接/运行；不得猜测字段或 Provider 成功值。未获得匹配 OHOS 产品树/hb/Feature 配置，产品构建、原 SO 初始化发布及只读冒烟均未执行；具体门禁见[唯一执行状态](../../docs/SIM_Refactor_Execution_State.md#g2a-bplus-m1)。

2026-11-05 续跑复验：实际 main 输入为 `5e209a76152a7e97411c41420ebd8d0e5ec67966`，已含上述 B+ 代码；新隔离分支 `work/g2a-bplus-revalidation-20261104` 复用原闭包，仅收窄五份显式源表的 G2a include 到 `services/multisim/integration/api`，移除 Query testonly 叶子对全骨架头路径的导出。Core/Binding/Manager/EXT 的 cpp、宏和 Feature 定义未变。

新增源表审计按实际 target/字面量列表检查，拒绝注释掉、放错 target、条件化、重复、覆盖/移除 Query、缺少窄 include 或混入其他 multisim 业务 cpp，14 项 Python 测试通过。该检查是有限的文本边界审计，不解释完整 GN/Feature，也不代替产品编译。六条生产执行审计按精确方法名编码和源码路径匹配，同时核对 `SimManager::OnInit` 零执行及 `CoreService::Init` 未参与覆盖。厂商两场景额外检查**一次附着查询只调用一次厂商边界**，保留全部已有返回值、对象控制块及生命周期断言，不固定整个 Fixture 的调用总数。

干净代码 `20f92b4821dcaf8c24e78acf9db60b681b55c57c` 新建输出目录重新编译/链接后 7 PASS；恢复的历史 tip `728ad77ed4c723efbb5cb1baf7b3f10bb9830240`（含 cc787945 检查点）也重新构建 7 PASS，七组返回差分一致。当前证据见 [g2a_bplus_revalidation_20261105.json](configs/g2a_bplus_revalidation_20261105.json) 及[执行状态 §10](../../docs/SIM_Refactor_Execution_State.md#g2a-revalidation-20261105)。两个此前证据 JSON 均保留原字节。本轮状态仅为 `G2a_BPLUS_HOST_VERIFIED`；完整初始化及产品门禁仍未闭合，没有切换生产执行路径。

最终审查修正审计测试中的方法编码样例，并补全 `ReadLegacyRealSimCount/Read`、`OnInit/Init` 正反检查；业务代码和冻结Oracle未变。最终干净代码 `722d044164c534f5898337c5b8eb1ccacb70c286` 在新输出目录 `query-final` 再次完整构建/链接和执行，仍7 PASS；JSON当前查询基线及摘要对应此提交，另保留首次20f92b48运行的关联。架构与完整初始化记录仍使用实际20f92b48基线，其相关生产源/GN没有变化。
