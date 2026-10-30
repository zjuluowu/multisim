# SIM Linux TDD 防护网

当前状态：**Linux 受控组件最小切片已运行**，仅覆盖 `ISimManager::GetMaxSimCount()` 的只读容量查询。正常 GoogleTest 1 PASS；独立检查器负控1预期FAIL。编译、链接和源级覆盖证据确认执行了未修改的SIM生产方法。

原五条服务入口候选仍全部BLOCKED；没有验证无卡/Ready/账户/PIN/双卡隔离、SA注册/发现、真实Client/Proxy/Stub IPC、服务入口检查、真实权限/capability、Modem或设备。当前不满足原始全部第一阶段标准，也不宣称真实设备系统测试完成。

当前工作仓：`zjuluowu/multisim/main@df8f2f017f83c70145b20255b4e9042f7a5fd923` + 未提交工作树；历史执行各有自己的SHA/摘要，不能回写。本测试实现限定本目录；本轮另刷新docs设计，未重跑SIM测试。

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

## G2a 原 Manager 只读附着切片

独立入口（不计入上面的容量或 INFRA PASS 数字）。从仓库根目录执行，先将下列变量设为本机实际路径；依赖和输出均放在源码目录之外：

```sh
: "${MULTISIM_GTEST_ARCHIVE:?指定dependency_lock.json锁定的GoogleTest归档}"
: "${MULTISIM_DEPS_DIR:?指定仓库外的依赖目录}"
: "${MULTISIM_OUTPUT_DIR:?指定仓库外的输出目录}"
python3 telephony_core_service/test/systemtest/scripts/prepare_dependencies.py \
  --archive "$MULTISIM_GTEST_ARCHIVE" --destination "$MULTISIM_DEPS_DIR"
python3 telephony_core_service/test/systemtest/scripts/run_g2a_query.py \
  --gtest-root "$MULTISIM_DEPS_DIR/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59" \
  --output "$MULTISIM_OUTPUT_DIR/g2a-query"
```

归档下载地址、SHA256和源码树摘要均由[依赖锁](configs/dependency_lock.json)给出，准备脚本只离线校验/解包，不隐式下载。已准备依赖时可直接将 `--gtest-root` 指向固定源码树。主机环境限定 Linux amd64、Python 3.12+、Clang/LLVM 18、GN、Ninja、GNU gold、pthread 和 libxml2 开发头；这不承诺其他平台/SDK布局。完整初始化审计另需锁定的 OpenSSL 头，以 `--openssl-root` 传入，不依赖旧电脑目录。libxml2 默认取标准目录；Query 与初始化审计均可用 `--libxml2-include "$SIM_LIBXML2_INCLUDE"` 显式传入，或设置 `SIM_LIBXML2_INCLUDE` 环境变量。该参数只选择头文件根，不改变默认行为或验证范围。

[精简历史证据](configs/g2a_checkpoint_evidence.json)记录源摘要、七个场景、正执行函数及完整初始化局部阻塞，旧 PASS 不能自动归给当前 HEAD。不同克隆路径会改变二进制/覆盖中的路径，复验检查源码及实际执行，不要求历史二进制 SHA 相同。原临时日志、XML和二进制不属于恢复前提。

仍使用 `configs/dependency_lock.json` 中的固定 GoogleTest 归档与 `prepare_dependencies.py`。该 target 编译真实 `CoreServiceSim::GetRealSimCount`、原 `SimManager` 构造与 `GetRealSimCount`、原 `TelephonyExtWrapper`，以及新的 Application 查询和 Core HostPort。原 Manager/共享 RIL 控制块、附着前后返回、空 Manager 原宏回退和 SetSimManager 撤销附着均由实际生产代码验证。RIL 仅为严格平台边界，场景不允许设备调用；可选厂商场景通过真实 `dlopen/dlsym` 加载输出目录中只提供 `GetRealSimCountExt` 的显式测试 ABI 边界，验证原返回值未被查询层改写。

runner 在独立进程验证配置 `1/2/0/负值/畸形字符串` 的原 `atoi` 行为，以及厂商返回覆盖和负返回透传；输出 `g2a-query-result.json`、七份 XML、原源码摘要、链接图和按精确生产文件路径核对的执行覆盖。产物只放在输出目录。失败准备保持 BLOCKED/executed=false，并清除此入口已知的旧 PASS XML，不复用历史成功证据。

本切片只初始化 HostPort/查询附着。此方法原本不依赖 `SimManager::OnInit`，测试未执行 `Manager::OnInit`、`CoreService::Init`、卡状态初始化、IPC/SA/权限或设备业务；LTO 仅闭合所选查询。完整初始化审计仍使用原独立入口，不能由本切片 PASS 推导其完成。
