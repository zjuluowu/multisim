# SIM Linux TDD 防护网

当前状态：**Linux 受控组件最小切片已运行**，仅覆盖 `ISimManager::GetMaxSimCount()` 的只读容量查询。正常 GoogleTest 1 PASS；独立检查器负控1预期FAIL。编译、链接和源级覆盖证据确认执行了未修改的SIM生产方法。

原五条服务入口候选仍全部BLOCKED；没有验证无卡/Ready/账户/PIN/双卡隔离、SA注册/发现、真实Client/Proxy/Stub IPC、服务入口检查、真实权限/capability、Modem或设备。当前不满足原始全部第一阶段标准，也不宣称真实设备系统测试完成。

基线：`zjuluowu/multisim`，main，HEAD `2841c6f60635853a929c1755dc08f5543a89eb83` + 工作树摘要。所有新增/修改只在本目录；没有提交/推送。

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

已补齐InnerEvent信封的类型/所有权适配，以及真实ITelRilManager接口的GoogleMock设施。运行器额外输出adapter.xml，9项INFRA自检单独计数，不能增加SIM业务用例完成数。原五条仍BLOCKED；新增未初始化C基线候选也因真实生产方法未能链接而BLOCKED，未采集Legacy行为。

- [真实初始化依赖审计](docs/initialization_closure.md)
- [事件适配范围](docs/event_adapter.md)、[RIL Mock设计](docs/ril_mock.md)
- [匹配接口缺失与上游版本差异](configs/platform_resolution.json)

可复现的编译审计（不会执行探针、不产生GoogleTest PASS/XML）：

```sh
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-initialization-final-record
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --target sim_bootstrap_characterization --output /tmp/sim-bootstrap-final-record
```

平台准备失败由initialization_audit.json记录BLOCKED/executed=false与真实编译诊断，保持主容量套件独立可运行。运行器清除已知旧XML，避免准备失败遗留过期PASS。Windows挂载源的未来文件时间通过有界时间条件等待处理，属于构建准备，不是SIM行为Oracle或固定sleep判通过。

上游外部接口分析见 [upstream_external_contracts.md](docs/upstream_external_contracts.md)，12个仓库的分支、固定SHA、源码路径及摘要见 [upstream_contracts.json](configs/upstream_contracts.json)。新增DataShare游标/Query与公共事件发布边界通过GoogleMock显式配置，未提供服务时失败。运行器验证固定原始平台头摘要，差分要求相同外部契约清单。FFRT运行时仍是设计方案，尚未实现；不等同已运行SIM初始化。
