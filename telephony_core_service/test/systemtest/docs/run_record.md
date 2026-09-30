# Linux受控组件切片实际执行记录

2026-09-30；仓库zjuluowu/multisim，main；HEAD `2841c6f60635853a929c1755dc08f5543a89eb83`，工作树非干净。源文件原始摘要见configs/source_manifest.json；157个原文件摘要静态复核未变化。当前测试设施未提交。实际结果带源码摘要、harness摘要、binary SHA256、配置及工具版本，不能只依据HEAD忽略本地修改。

## 执行环境与工具链

Ubuntu24.04.4 / WSL2 / Linux6.18.33.2 / x86_64；GN1000(03d10f1)，Ninja1.11.1；Clang18.1.3；GNU gold(Binutils2.42)1.16；LLVM coverage18.1.3；C++17/full-LTO；GoogleTest v1.15.2固定commit b514bdc898e2951020cbdca1304b75f5950d1f59，源码编译，未引用未知机器库。

## 实际命令及退出码

从 `/mnt/e/github/multisim/telephony_core_service` 执行，独立全新输出目录：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-component-verified
```

runner退出0。实际内部GN/Ninja命令（完整argv/退出码/耗时见result.json）：

```sh
gn gen /tmp/sim-component-verified --root=/mnt/e/github/multisim/telephony_core_service/test/systemtest --args='production_root="/mnt/e/github/multisim/telephony_core_service" gtest_root="/tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59"'
ninja -C /tmp/sim-component-verified sim_component_test
```

GN退出0，Ninja退出0，实际编译两个未修改生产源、参数边界、启动器、官方GoogleTest及用例并链接；没有链接忽略undefined-symbol选项。原OHOS构建目标没有运行。

正常运行的实际参数/环境：

```sh
LLVM_PROFILE_FILE=/tmp/sim-component-verified/normal.profraw /tmp/sim-component-verified/sim_component_test --gtest_filter=SimComponent.CapacityReadOnly --gtest_output=xml:/tmp/sim-component-verified/normal.xml
```

退出0；GoogleTest实际1条，PASS1、FAIL0、SKIP0。场景SIM-COMP-001，Level B；O-C11槽数量非负通过，公开稳定观察为1（O-L06 Legacy仅人工差分），converged=true，parameter_boundary_observed=true，synthetic_only=true，corrupted_evidence=false。

独立负向控制：

```sh
SIM_HOST_NEGATIVE_CONTROL=1 LLVM_PROFILE_FILE=/tmp/sim-component-verified/negative.profraw /tmp/sim-component-verified/sim_component_test --gtest_filter=SimComponent.CapacityReadOnly --gtest_output=xml:/tmp/sim-component-verified/negative.xml
```

退出1；GoogleTest实际1条，FAIL1，O-C11被明确检出；这是预期失败检查器验证，不是生产回归，也不改写为正常测试PASS。先运行相同生产查询，再只将观察验证输入改成-1；未修改生产代码或桩提供的业务结果，不代表RIL/参数故障恢复已验证。

## 生产执行与外部边界审计

实际链路：GoogleTest → capacity_probe启动器 → 真实SimManager公开构造 → ISimManager::GetMaxSimCount → 原生产sim_manager.cpp查询/telephony_types.h解析 → 外部GetParameter兼容边界 → 公开数量结果。

LLVM normal.profraw经llvm-profdata-18 merge和llvm-cov-18 export（均退出0）给出原生产函数 `_ZN4OHOS9Telephony10SimManager14GetMaxSimCountEv` 的执行计数2，定位真实sim_manager.cpp。该数字仅证明实际执行参与，不是业务断言或固定次数要求。链接map及生产src/hash证明方法不是fixture同名替身。原OnInit、状态/账户业务没有运行，不能由target编译推导覆盖这些功能。

参数桩仅返回const.telephony.slotCount的字符串1、virtual_modem_switch字符串false及明确缺key默认值，记录实际读取；不返回SIM数量、不实现SimManager/控制器。日志sink丢弃格式及参数，XML/JSON只有合成配置数量/布尔证据，没有卡身份。桩的compile-only平台声明不能证明真实SDK/IPC ABI兼容。

## 产物、清理及状态

- `/tmp/sim-component-verified/result.json`：完整机器可读状态、工具链、SHA/摘要、实际命令/退出码、正常及负控计数、公开观察、生产覆盖审计、清理。
- normal.xml：真实GoogleTest正常XML；negative.xml：真实GoogleTest预期失败XML。
- sim_component_test.map、normal.profraw/profdata、coverage-export.log：链接/源级运行审计；普通日志不是业务Oracle。
- tempnamespace已删除、所有已启动进程已回收；无真实卡/系统参数/账户写入，因此restore为不需要外部状态恢复。产物保留在输出目录，不写源目录。

额外准备失败检查：`python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-missing-dependency --output /tmp/sim-component-preparation-blocked` 退出1，result.json为BLOCKED/ executed=false，未生成normal.xml；没有将准备失败伪装GTest已运行。

容量差分工具自比较：`python3 test/systemtest/scripts/compare_results.py --legacy /tmp/sim-component-verified/result.json --split /tmp/sim-component-verified/result.json --output /tmp/sim-component-verified/self-comparison.json` 退出0、COMPARABLE。这仅是工具自比较，不是真实Legacy/Split两版本实验。O-L06数值差异进入人工评审，上下文不同标INCOMPARABLE。

| 交付状态 | 实际结果 |
|---|---|
| 静态设计 | 原五条+新容量15项设计、入口/依赖/capability记录完成；未确认Oracle保持待审 |
| 独立GN/Ninja构建 | 已成功，仅指定Clang容量target；GCC虚表链接失败历史保留 |
| GoogleTest实际运行 | 正常1 PASS，独立负控1预期FAIL；SKIP0 |
| SIM生产逻辑 | 真实容量查询已执行；状态/文件/账户未运行 |
| 组件边界 | 容量切片AUTOMATED / PASS，Level B |
| 服务入口边界 | 原五条全部BLOCKED，尚无实测 |
| 真实设备/完整原第一阶段标准 | 未完成，不作通过声明 |

LSP/clang-format未在当前环境核实可用；新增代码按现有C++风格人工整理，并通过实际编译。没有声称LSP诊断或原OHOS hb build通过。本次未修改解码生产源码，因此没有声称执行现有fuzzer。后续若需修改生产源须按约束另行验证。


## Linux GoogleMock 更新执行记录

SHA：2841c6f60635853a929c1755dc08f5543a89eb83（工作区新增工程未提交）。Linux 原生，无实体设备。Clang18.1.3、GN1000、Ninja1.11.1、gold/binutils2.42、GoogleTest/GoogleMock v1.15.2 固定提交；完整环境、工具版本、源码摘要和命令见 /tmp/sim-component-gmock/result.json。

实际命令：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-component-gmock
```

最终 runner、GN、Ninja、正常 GoogleTest 退出码均0；正常1用例 PASS，FAIL/SKIP均0。观察负控1用例预期FAIL（退出1），独立记录，不计新增业务用例。覆盖导出确认真实 SimManager::GetMaxSimCount 执行。normal.xml、negative.xml、result.json、link map 和覆盖产物均在上述输出目录。清理成功，无卡身份或持久化修改。

首次迁移运行曾 FAIL：StrictMock 拒绝未配置 const.product.devicetype。根据 interfaces/innerkits/include/telephony_types.h:113 的外部参数读取显式配置 phone 后重跑通过；没有修改生产代码。最终同路径产物属于通过的重跑记录。

静态设计更新完成；构建成功；测试实际运行；SIM 生产逻辑实际执行；仅组件容量边界通过。原五条仍 BLOCKED，服务入口未执行。未执行 hb、设备测试、独立 LSP 诊断或真实 Legacy/Split 对比。

## 本轮剩余场景推进记录

基线SHA仍为2841c6f60635853a929c1755dc08f5543a89eb83，工作区新增工程未提交。Linux/WSL2、Clang18.1.3、gold1.16/binutils2.42、GN1000、Ninja1.11.1、固定GoogleTest/GoogleMock提交b514bdc898e2951020cbdca1304b75f5950d1f59。

正常工程最终实际命令：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-component-final
```

runner/GN/Ninja退出0；SIM-COMP-001正常1 PASS，负控1预期FAIL/退出1；platform_adapter_test独立4 PASS、0 FAIL/SKIP。自检3项验证事件类型/所有权/弱生命周期，1项验证正式RIL Mock响应；纯外部设施自检不算SIM生产业务场景。result.json明确分开counts_scope、infrastructure_checks与blocked_scenarios。normal.xml/negative.xml/adapter.xml和map/coverage均在输出目录；实际SimManager::GetMaxSimCount覆盖非0，计数只用于审计。清理成功、没有真实卡身份或持久化修改。

初始化依赖审计实际命令：

```sh
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-initialization-final-record
```

GN退出0，Ninja/runner退出1，26个初始化生产源对象编译成功，但整体没有链接；status=BLOCKED/executed=false/GTest数量0，无XML。initialization_audit.json列全部实际诊断与源hash。剩余缺失头包括ability_manager_client.h、cellular_data_client.h、configuration.h、hisysevent.h、sim_data.h；已声明但尚未支持的Want/CommonEvent/DataShare等接口也有真实编译错误。26个源可编译不是26条测试，也不等于状态业务实际执行。

未初始化C基线候选实际命令：

```sh
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --target sim_bootstrap_characterization --output /tmp/sim-bootstrap-final-record
```

GN退出0，Ninja/runner退出1，缺少真实SimStateManager::HasSimCard/GetSimState与MultiSimController::GetActiveSimAccountInfoList链接符号。BLOCKED、0执行、无XML、无Legacy采集。不用内部业务Mock补齐这些符号。

准备失败核实：run.py --gtest-root /tmp/sim-dependency-not-present --output /tmp/sim-preparation-final-blocked，退出1，BLOCKED/executed=false，无normal.xml；没有将准备失败伪装GTest执行。

静态检查实际通过：157项原生产/文档摘要均未变化；Python AST/JSON语法；7条SIM设计各15项；RIL mock生成器--check；GN格式化已执行。未执行独立LSP、hb、设备测试、fuzzer或真实Legacy/Split双版本比较。平台上游来源、固定版本、许可证和不匹配差异已记录，未使用其他测试工程的Mock或辅助源码。

本轮结论：工程和外部协议设施有进展，原五条业务场景仍全部未完成；缺少匹配外部契约及完整状态/账户运行适配。实体设备不构成Linux B层级依赖。

## GitHub外部接口分析与平台适配（2026-09-30）

直接读取OpenHarmony GitHub的固定提交接口；12个仓库master观测SHA和各文件摘要已记录于configs/upstream_contracts.json。上游不是当前hmos fork的匹配版本，差异详见upstream_external_contracts.md。新增DataShare/CES设施由GoogleMock构造，没有添加SIM业务替身。

实际命令：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-upstream-platform-run
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-upstream-initialization-audit
```

HEAD 2841c6f60635853a929c1755dc08f5543a89eb83，工作区非干净；上述同一Linux/WSL2工具链。运行器/GN/Ninja退出0，SIM-COMP-001：1 PASS；设施：9 PASS、0 FAIL/SKIP；负控：1预期FAIL/退出1且检测有效。真实SimManager::GetMaxSimCount覆盖计数2仅作参与审计。normal.xml、negative.xml、adapter.xml、result.json在输出目录；清理/隔离成功。新增设施验证零行游标、Query失败与零行区分、原始ValuesBucket重复Put、事件载体/失败传播、scope清理。未安装外部Mock服务时不默认成功。

初始化审计：runner/Ninja退出1、GN退出0；29个初始化生产object编译，最终BLOCKED，executed=false、GoogleTest数量0，无执行XML。缺少ability_manager_client.h、cellular_data_client.h、hisysevent.h；另有fork SimData字段及事件/平台API子集未适配等编译诊断。完整原始诊断与实际命令在initialization_audit.json及ninja-initialization.log。29个object不是29个用例，也不是初始化已运行。原五个场景和C候选仍BLOCKED。

FFRT/TelEventQueue运行适配只有设计，尚未实施。组件测试未覆盖服务入口、SA、IPC、真实权限/capability、Modem、设备服务；没有真实Legacy/Split双版本执行。
