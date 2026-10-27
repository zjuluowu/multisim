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

## Linux调度与生产事件传输推进（2026-09-30）

工作仓HEAD `df8f2f017f83c70145b20255b4e9042f7a5fd923`，本轮工作树未提交；生产原文件保持未修改。Ubuntu24.04/WSL2 x86_64，Clang18.1.3、gold1.16/binutils2.42、GN1000、Ninja1.11.1、C++17/full-LTO、GoogleTest/GoogleMock固定commit b514bdc898e2951020cbdca1304b75f5950d1f59。真实版本、源码/二进制摘要、实际命令及耗时均记录在result.json。

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-scheduler-stage
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-scheduler-initialization
```

正常runner/GN/Ninja退出0（Ninja -j4）；SIM-COMP-001：1 PASS、0 FAIL/SKIP。负控退出1、1预期FAIL，已确认检测有效。设施独立13 PASS、0 FAIL/SKIP；新增INFRA-EVENT-004/005与INFRA-FFRT-001/002各有15项设计。原五个SIM场景及C候选仍BLOCKED、未执行。

真实event_transport_production_host编译未修改utils/common/src/tel_event_handler.cpp和tel_event_queue.cpp，Linux调度/平台base独立target；不重写SIM逻辑。正常LLVM证据：SimManager::GetMaxSimCount=2；事件证据：生产SendEvent=2、SubmitToFFRT=2、PopEvent=1。均只是此次运行参与审计，不是固定业务次数Oracle。GoogleMock是外部事件消费端；未提供SIM状态/账户结果。General同步任务也执行真实TelFFRTUtils包装。

normal.xml、negative.xml、adapter.xml、result.json、两个map及profile均在输出目录。设施在2s内以完成/停止条件收敛，明确禁止清理失败后继续销毁观察者或运行下一例；进程整体看门狗10s。没有固定长sleep判PASS，没有卡身份/平台持久化修改；运行器cleanup成功。

初始化审计：GN退出0，Ninja/runner退出1；36个初始化生产object编译，整体BLOCKED/ executed=false / GoogleTest数量0，没有执行XML。新增缺失头为bundle_mgr_proxy.h和hks_api.h；仍有14个fork数据协议符号及原生指针、参数/配置/JSON/账户接口子集的编译诊断。数量增加不表示SIM初始化执行或账户覆盖；完整源诊断见initialization_audit.json与ninja-initialization.log。

## 2026-10-08：平台编译契约和本地指针载体

实际基线HEAD=df8f2f017f83c70145b20255b4e9042f7a5fd923，main，工作树未提交。生产源码未修改；157份初始生产/文档清单摘要复核一致。环境为Ubuntu24.04 / WSL2 / Linux6.18.33.2 / x86_64，GN1000(03d10f1)、Ninja1.11.1、Clang18.1.3、gold1.16/binutils2.42、LLVM18.1.3、C++17/full-LTO；GoogleTest/GoogleMock固定b514bdc898e2951020cbdca1304b75f5950d1f59。

最终实际命令（依赖位于明确的/tmp离线目录，不在构建时下载）：

```sh
python3 test/systemtest/scripts/run.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /tmp/sim-closure-oct08/run
python3 test/systemtest/scripts/audit_initialization.py --gtest-root /tmp/sim-systemtest-deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --openssl-root /tmp/sim-systemtest-openssl --output /tmp/sim-closure-oct08/audit
```

正常runner退出0，GN/Ninja与正常测试进程均退出0。B组件SIM-COMP-001：1 PASS、0 FAIL、0 SKIP；通过ISimManager调用真实sim_production_host中的SimManager::GetMaxSimCount，LLVM记录其执行2次，链接图sim_component_test.map。执行次数仅用于证明生产参与，不是业务Oracle。该场景没有调用OnInit或建立无卡状态。

负控：测试进程退出1、1项预期FAIL，expected_failure_verified=true。检查器观察值受控改为-1违反O-C11；没有修改生产代码，不表示生产故障，也不增加业务用例数。INFRA独立进程退出0，16 PASS、0 FAIL/SKIP，其中新增本地指针3项；这些设施不算SIM状态/账户覆盖。真实event_transport_production_host的SendEvent/Submit/Pop任务路径有LLVM执行证据。

机器结果：/tmp/sim-closure-oct08/run/result.json；GoogleTest normal.xml、negative.xml、adapter.xml分别记录正常、负控与设施结果。converged=true、timed_out=false；cleanup.completed=true、temporary_namespace_removed=true、processes_reaped=true。只包含合成测试值、别名和布尔证据，identity_plaintext_emitted=false。未修改平台持久化状态。结果中保留执行当时的源码/工具链/二进制摘要，随后仅同步说明文档与静态状态清单。

初始化审计runner退出1；GN退出0，Ninja -k0 -j4 sim_initialization_link_probe退出1，45个初始化生产object编译成功。missing_headers=[]；43条编译诊断全部是SimData的14个成员缺失，涉及5个生产文件，详见configs/storage_protocol_requirements.json。链接阶段尚未到达。status=BLOCKED、executed=false、gtest_tests=0；没有场景执行XML，不能将已编译object数算作测试数。

补齐头文件与运行验证分开：HUKS、ConfigPolicy、cJSON、参数watcher、string_ex、OS账户使用固定上游原始头，总计21份原始平台头摘要已验证；Bundle/Parcel/死亡通知是来源明确的最小声明。除本地指针载体外，本轮没有实现这些模块的运行提供者。GitHub与Gitee已核对的存储头字节相同，均缺14字段，不猜测协议值。平台声明静态可编译不表示服务语义已验证。

原五条SIM场景和SIM-CHAR-001仍BLOCKED。真实服务入口、SA注册发现、Client/Proxy/Stub IPC、真实权限/capability、Modem/设备行为均未覆盖。后续先取得匹配fork存储协议，再审计真实生产链接及受控运行装配，最终经正式RIL响应建立Absent并查询状态/账户。完整OHOS/hb与设备测试未执行；当前环境没有LSP新增诊断报告。静态Python/JSON解析、GN格式及来源摘要检查不替代这些验证。

首次生产事件完整LTO构建曾超出120s（退出124、未执行）；首次新初始化闭包审计超出180s（退出124、未执行）。本轮将两者构建预算设为300s并限定-j4，最终记录是上面真实结果；业务收敛/进程看门狗未放宽。不宣称hb/LSP/真实设备/IPC验证，未修改生产源码故无需以本轮声称运行生产解码fuzzer。
