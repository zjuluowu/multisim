# 当前仓库与 Linux 环境调查

调查日期：2026-09-30。源码引用均相对 `telephony_core_service/`，以 HEAD `2841c6f60635853a929c1755dc08f5543a89eb83` 及工作树摘要为定位基准。没有联网引用上游源码；origin 是 `https://github.com/zjuluowu/multisim.git`，upstream 配置是 `https://github.com/openharmony/telephony_core_service.git`，未 fetch，也未假定 upstream 当前分支/SHA。

## 1. 仓库与工作树

- FACT：Git 根是 `/mnt/e/github/multisim`，Core 位于普通子目录，分支 main，HEAD 如上。
- SOURCE_EVIDENCE：实际执行 `git rev-parse --show-toplevel`、`git branch --show-current`、`git rev-parse HEAD` 均退出 0；最近提交新增 9 份 docs 文件。
- FACT：新增本工程前 `git status --porcelain --untracked-files=all` 共 1042 项：1041 个 ` M`、1 个 `??`（父目录 `.codegraph/.gitignore`）。暂存区原为空。
- SOURCE_EVIDENCE：`git diff --ignore-space-at-eol --stat -- services/sim services/core BUILD.gn interfaces` 退出 0，无输出；选取 sim_manager.cpp、multi_sim_controller.cpp、capability manager 与 HEAD 做 CRLF→LF 比较均相同。
- INFERENCE：大量状态项可能与换行有关；只核实上述范围/抽样，不宣称整个工作树只有换行变化。HEAD 不能单独代表脏工作树全部内容。
- ENVIRONMENT_LIMITATION：当前不是完整 OHOS 根，无 build/、.gn。后续运行需重新采集 HEAD、dirty 状态和源文件摘要。
- OPEN_QUESTION：将来 Legacy/Split 分支及干净基线由负责人指定；不能把历史文档中的其他仓分支作为当前 Split。

## 2. 文档版本与一致性

| 文档 | 实际标注 | 核实结论 |
|---|---|---|
| MultiSIM/01_sim_subsystem_baseline.md | v2.0；2026-08-18；cd3bca9c… | 历史分析输入 |
| MultiSIM/02_current_architecture_and_evolution.md | v2.1；2026-08-28；初始 cd3bca9c… | 历史与演进输入 |
| MultiSIM/03_esim_slot_and_sa_boundary.md | 2026-09-06；多个缩写 SHA | 不能解析成单一精确基线 |
| MultiSIM/04_ddd_target_architecture.md | v2.0；2026-09-03；90ad4f1f… | 目标设计，非当前业务契约 |
| MultiSIM/05_detailed_class_and_data_model.md | v1.1；2026-09-09；无唯一 SHA | 模型设计输入 |
| MultiSIM/06_refactoring_proposal_slides.md | Marp；footer 2026-08-28；无唯一 SHA | 汇报材料 |
| MultiSIM/07_subscription_refactoring_status.md | 第2节：telephony 仓、两个 shadow 分支 | 与当前仓及布局冲突 |
| MultiSIM/readme.txt | 无独立版本/日期 | 索引还未列 07 |
| openharmony_telephony_core_service_SIM_software_design.md | v2.0；2026-08-18；cd3bca9c… | 历史设计描述 |

- FACT / SOURCE_EVIDENCE：这些文件均存在；版本见文件前 10 行、07 第15–22行；`git cat-file -t` 对 cd3bca9ca9b1b0c163ba05a3cef5bba5ae2ac038 和 90ad4f1f005191eae3843e441de18388703cb9a5 均返回 commit（退出 0）。
- FACT：读取两历史 SHA 的 services/sim/src/sim_manager.cpp、multi_sim_controller.cpp 并正规化换行，与当前文件比较均不相同；尚未逐行为整个文档建立一致性证明。
- FACT：`rg --files services/sim -g '*subscription*' -g '*euicc*'` 没有匹配；07 声称的 subscription.h、subscription_topology.cpp 等不在当前 SIM 目录。
- INFERENCE：07 的“已完成”属于其记录的其他仓/分支，不能在本仓声明完成。旧文档的三卡和目标 Subscription 模型不得移植为当前产品上限/公共标识规则。
- OPEN_QUESTION：文档原作者确认历史上下文与下一步迁移分支；当前只以源码确定可执行入口。

## 3. 构建与测试约定

- FACT / SOURCE_EVIDENCE：根 BUILD.gn:14 导入 //build/ohos.gni，:30 为 ohos_shared_library("tel_core_service")；:203 起依赖 innerkits API、libtel_common，:212 起列平台 external_deps。
- FACT / SOURCE_EVIDENCE：test/unittest/sim_gtest/BUILD.gn:14 导入 //build/test.gni；:18–22 依赖 test:core_service_test_base、现有 ffrt_mocked；:40–41 链接 googletest:gmock_main/gtest_main；:90 起模板使用 ohos_unittest、module_out_path、exceptions、LTO。
- FACT：当前 checkout 没有 //build/test.gni、//build/ohos.gni 或本仓 Linux host 测试模板；搜索本仓 GN/GNI 没有 host_toolchain/is_host 约定。现有 build 模板不在仓内。
- ENVIRONMENT_LIMITATION：缺模板定义，无法证明 ohos_unittest 不支持 host，也无法证明其支持；已证实的是“当前环境无法求值该模板”。host/target 选择及 C++ 默认标准需完整 OHOS build 仓版本核实。
- FACT：本仓 GN/GNI 未找到显式 -std=c++ / cpp_std / cxx_std，不能从编译器默认值认定项目标准。
- INFERENCE：新增 systemtest 自包含 .gn/build config/host toolchain 可保持 GN→Ninja→GoogleTest，但属于新 host 适配方案，不能将其称为现有 ohos_unittest 的 Linux 变体。
- OPEN_QUESTION：优先取得匹配平台 build 仓核实 Linux host 机制；否则批准本目录最小 GN host 适配层。

## 4. 环境与实际命令记录

所有命令在当前 Core 目录执行；工具查询没有执行测试。configs/environment_probe.json 保存重新执行的环境命令、时间、完整输出与退出码；没有测试执行结果。

| 实际命令 | 退出码 | 结果 |
|---|---|---|
| uname -a | 0 | WSL2 Linux 6.18.33.2，x86_64 |
| cat /etc/os-release | 0 | Ubuntu 24.04.4 LTS |
| gn --version | 0 | 1000 (03d10f1) |
| ninja --version | 0 | 1.11.1 |
| g++ --version | 0 | Ubuntu 13.3.0 |
| clang++ --version | 0 | Ubuntu clang 18.1.3；x86_64-pc-linux-gnu |
| command -v gn ninja hb g++ clang++ pkg-config | 0 | gn/ninja/g++/clang++ 存在；hb、pkg-config 未找到 |
| ls /usr/include/gtest /usr/src/googletest /mnt/e/github/multisim/build/test.gni /mnt/e/github/multisim/.gn | 2 | 四路径均不存在 |
| gn gen /tmp/sim-systemtest-investigation-no-root | 1 | Can't find source root；没有 .gn |
| git diff --ignore-space-at-eol --stat -- services/sim services/core BUILD.gn interfaces | 0 | 无输出 |
| g++ -fsyntax-only -Iservices/sim/include -Iinterfaces/innerkits/include services/sim/src/sim_state_manager.cpp | 1 | 缺少 ffrt.h；仅生产源依赖探针，非完整构建 |

ENVIRONMENT_LIMITATION：未安装路径不意味着系统任何位置都没有 GoogleTest；本仓也没有 vendored GoogleTest。未发现可用安装/构建目标，尚未选定其版本。没有核实可运行 SA、Binder、平台系统服务、Modem 或实际 SIM，不能覆盖 A。没有 hb，因此未尝试声称 hb build 成功。测试数量 0，PASS/FAIL/SKIP 均 0，五项计划场景 BLOCKED。gn gen 失败属于准备阶段阻塞，不是失败的 GoogleTest。

## 5. 依赖可运行性与生产驱动

FACT / SOURCE_EVIDENCE：SimManager 构造接收 ITelRilManager（sim_manager.cpp:34），OnInit(:41) 组装真实状态、文件、账户、STK 等管理逻辑；CoreManagerInner::SetTelRilMangerObj(:54)、OnInit(:40) 提供正式依赖装配；CoreServiceSim::SetSimManager(:51) 是正式组合接口。外部事件通过 ITelRilManager::RegisterCoreNotify/GetSimStatus 进入（sim_state_handle.cpp:140、:551）。状态、文件和账户业务本身必须保留生产实现。

INFERENCE：通过服务公开转发入口 + 真实 SimManager + RIL 契约替身可形成 B；但当前公开转发路径的 capability 检查和大量缺失平台 ABI 阻止宣称可运行。完整链接闭包未验证。仅编译 CoreServiceSim 并挂 Mock ISimManager 不满足目标。

ENVIRONMENT_LIMITATION：标准 Linux C++运行库可用；RIL/HDI、DataShare、事件、State Registry、权限、参数/Preferences、FFRT/EventHandler、SA/IPC 等没有经过本环境 host 链接运行验证，详见边界图。不能将 OHOS 外部依赖名称等同 Linux 已有共享库。

OPEN_QUESTION：GoogleTest 获取方式与固定版本、平台库 host 复用/替换清单、权限策略、允许的公开入口以及 Oracle 审核。

## 6. 重大冲突与外部协作

1. capability manager cpp:30–32 对 slotId!=3 返回 false（头文件:23 SLOT_3_INDEX=3）；CoreServiceSim cpp:70/:98/:193/:388 又将 false 作为 SLOTID_INVALID。FACT 是此控制流；“这是 bug”只是待负责人确认的解释，尚未运行。禁止测试桩强制 capability 返回 true。见 O-Q08。
2. sim_manager.cpp:176–184 经 HasSimCardInner 失败输出 NOT_PRESENT/success；不能将未初始化/无效 slot 的静态候选行为当成“确实无卡”。见 O-Q01。
3. multi_sim_controller.cpp:1965–1983 空 active list 返回 NO_SIM_CARD；是否是稳定业务需求待确认，不能写成永远 success+空数组。
4. multi_sim_controller.cpp:1522、:1689 调默认 Data Setter；不能由此声明 Primary 与 Data 永久联动。
5. 约束文档描述锁迁移，当前 core_service_sim.cpp:29/:43、sim_state_manager.cpp:172/:180 仍有既有 std::mutex。只禁止新增使用，不擅自改旧代码。
6. 参数默认 slotCount=1；telephony_types.h:49–56 有 3、分布式 8 常量；这不证明本产品支持四张物理 SIM。必须查询产品能力与配置。

需配合：匹配版本 OHOS build/test.gni 与 host toolchain（build 维护者）、GoogleTest 来源（工具链维护者）、HDI/RIL和DataShare API SDK/头文件（相关仓负责人）、账户/角色/事件/PIN 业务 Oracle（SIM 负责人）、A 设备拓扑和专门授权（设备实验室）。不需要上游当前默认分支来替代本仓依据。

可行性：GN/Ninja 可执行已核实；Linux GN+GoogleTest 产品链路未核实成功，需批准 host 产品目标/适配，再进行编译探针。当前调查设计完成不等于第一阶段整体验收完成。

## 批准后的执行增量

首批调查结论作为历史记录保留。当前已提供独立GN适配，固定GoogleTest并实际执行组件容量切片；参见component_entry.md、dependencies.md、run_record.md。没有补齐OHOS原build模板，也没有验证ohos_unittest host兼容。原五条服务入口场景仍BLOCKED。
