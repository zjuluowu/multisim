# Windows VS Code + WSL SIM Host

从 Git 根目录打开 Remote-WSL 工作区。工程复用本目录的独立 `.gn`、平台边界及相邻 `sim_guard`，不使用实验 `linux_sim_guard`，不依赖完整 OHOS 产品构建。生产代码只读；目标架构骨架不计入卡业务覆盖。当前验收、失败和工具版本见 [host_validation_20261111.json](configs/host_validation_20261111.json) 与[唯一执行状态](../../../../docs/SIM_Refactor_Execution_State.md#wsl-host-20261111)。

## 命令行

需要 Linux x86_64 的 clang++、GN、Ninja、GNU gold、ar、Python3、pthread 和 libxml2 开发头；源级执行审计另需 llvm-profdata-18/llvm-cov-18。不调用 Windows 编译工具，不自动安装或升级工具。Clang 使用系统 libstdc++/glibc；并非 OHOS libc++/musl ABI 验证。

已有 GoogleTest 必须符合 [dependency_lock.json](configs/dependency_lock.json) 的源码树摘要，不能将未知系统库混入固定版本测试。依赖准备是显式操作，构建阶段不会下载：

```sh
# 路径由本机选择；不要依赖历史 /tmp 目录。
export SIM_DEPS_DIR="$HOME/sim-host-deps"
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o /tmp/sim-gtest.tar.gz
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/prepare_dependencies.py --archive /tmp/sim-gtest.tar.gz --destination "$SIM_DEPS_DIR"
export SIM_GTEST_ROOT="$SIM_DEPS_DIR/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59"
export SIM_HOST_TOOLS=telephony_core_service/services/multisim/test/systemtest/tools
./$SIM_HOST_TOOLS/build_host.sh --gtest-root "$SIM_GTEST_ROOT"
./$SIM_HOST_TOOLS/run_host_tests.sh
./$SIM_HOST_TOOLS/run_host_tests.sh selected --filter 'SessionLifecycle.INIT_001_SingleEnumeration'
./$SIM_HOST_TOOLS/debug_host.sh --filter 'SessionLifecycle.INIT_001_SingleEnumeration'
python3 -B "$SIM_HOST_TOOLS/host.py" clean
```

`host.py configure/build/all/selected/clean/debug` 为统一入口，支持 `--out`、`--gtest-root`、`--jobs`；`selected/debug` 支持 `--target` 和 `--filter`。默认输出 Git 根 `.host-out`（已忽略），首次 configure 保存本机依赖路径，之后 Tasks 可直接使用。输出目录变更时同步 IDE 配置的 `.host-out` 路径。Clean 仅运行 Ninja 清理已生成目标，保留 GN 配置、依赖和测试证据。

GN 导出 `.host-out/build/compile_commands.json`，Ninja 增量构建五个实际目标。IDE Debug profile 使用 `-g -O1 -fno-omit-frame-pointer`，保留原 LTO/虚函数消除闭包，不忽略未解析符号。未接入平台的不可达方法会被裁剪；这不证明完整 SIM 可链接。LLDB 可能显示优化变量或合并单步位置；历史 runner 默认仍为原 `-O2` profile。

每次测试写入唯一 `.host-out/runs/run-*/`，保留原生日志、GoogleTest JUnit 兼容 XML、profraw 与 `result.json`，最新完成结果另存 `.host-out/latest.json`。构建/测试失败返回 1，缺依赖等准备阻塞返回 2；零用例和 skipped 不算通过。`error.json` 记录最近一次准备或构建错误，判断本次执行必须结合退出码和本次日志，不能引用旧 latest.json 冒充新结果。完整入口运行六个分类，包括全部五条独立缺陷，红灯保持非零退出。原有生产函数正执行/负控审计仍由 [run_sim_guard.py](../sim_guard/run_sim_guard.py) 提供，IDE 快速入口不替代完整发布门禁。

## VS Code

使用 Windows VS Code 的 `WSL: Reopen Folder in WSL`，打开 Git 根，而非仅打开 `telephony_core_service`。左下角应显示 `WSL: Ubuntu-24.04`，终端执行 `uname -a` 确认 Linux。[Remote-WSL 官方说明](https://code.visualstudio.com/docs/remote/wsl)区分 Windows 本地扩展和 WSL 工作区扩展。本次自动化会话没有可确认的 VS Code Server/扩展清单，Windows code CLI 出现 vsock 错误；GUI 连接和扩展安装状态需要人工核实。

扩展建议已写入根 `.vscode/extensions.json`；Windows 本机安装 `ms-vscode-remote.remote-wsl`，在 WSL 端安装：

- `llvm-vs-code-extensions.vscode-clangd`：读取真实编译数据库。
- `vadimcn.vscode-lldb`：CodeLLDB 启动测试、筛选、断点、变量和调用栈。
- `matepek.vscode-catch2-test-adapter`：TestMate C++，发现五个明确的测试二进制，包含缺陷红灯。
- `msedge-dev.gnls`：GN/GNI 编辑、跳转支持。

在 Extensions 中选择扩展并点击 **Install in WSL**；本任务没有自动安装扩展。配置依据 [TestMate 配置](https://github.com/matepek/vscode-catch2-test-adapter/blob/master/documents/configuration/test.advancedExecutables.md)、[CodeLLDB 手册](https://github.com/vadimcn/codelldb/blob/master/MANUAL.md)及 [GN 扩展](https://marketplace.visualstudio.com/items?itemName=msedge-dev.gnls)。构建后 TestMate 应发现用例；具体版本兼容性、发现、点击运行/调试均为 MANUAL。Tasks 不依赖测试扩展，可直接运行全部或输入 GTest filter。

Tasks：Configure Host、Build Host、Run All Tests、Run Selected Test、Clean Host。F5 选择 `SIM Host: selected GTest (WSL)`，选择目标并输入筛选表达式；preLaunchTask 自动增量构建。命令行 LLDB 示例：`breakpoint set -n OHOS::Telephony::SimStateManager::GetSimState`、`run`、`frame variable`、`bt`、`next`、`continue`。PIN/PUK 场景包含合成凭证，调试时不要导出含凭证的变量或内存。

## 真实性和限制

真实闭包为 `SimStateManager/SimStateHandle/IccState`、`ObserverHandler`、`CoreManagerInner`、`TelephonyExtWrapper`、`TelEventHandler/TelEventQueue`；容量目标另编译真实 `sim_manager.cpp`。FakeRil 提供硬件事实与响应，SIM 生产代码决定公开状态和解锁结果。系统参数、RIL、Registry/CES、SA、有限 FFRT/EventHandler host 运行时是模拟边界。DataShare/Provider 和 LPA 不在闭包，不创建虚假成功结果。

17 条兼容场景覆盖单/双卡、存在性/状态、组件初始化缺 RIL、通知回调、PIN/PUK、异步交错、销毁重建；4 条 PIN 外部 Hook 契约、19 条设施及 1 条容量单列。5 条独立缺陷保留：迟到 READY、替换卡旧 READY、通用 RIL 错误映射、替换卡旧 PIN、Refresh 返回语义冲突（待裁决）。完整 Core/Manager OnInit、账户、默认角色、数据库恢复、eSIM、IPC/设备未覆盖；历史 Provider 缺字段证据保留，本轮没有重跑该闭包，不能记为新 PASS。

2026-11-11 主卡接口增量：新增 `sim_primary_slot_entry_test` 与[覆盖审计入口](primary_slot/README.md)，真实入口契约14条单列，Host全量合计60条（原有46条保留）。ISimManager内部接缝之后的整部件主卡调用链仍BLOCKED；默认审计命令返回2，只有显式 `--entry-only` 才按入口覆盖验收。当前证据见 [primary_slot_validation_20261111.json](configs/primary_slot_validation_20261111.json)。

## SIM-028 真实 Manager 场景

真实业务目标、固定依赖及剩余生命周期红灯见 [primary_slot/README.md](primary_slot/README.md)。准备并配置依赖后，使用 `tools/run_host_tests.sh selected --target sim_primary_slot_real_test --filter 'RealSimPrimary.*'`，或 `tools/run_host_tests.sh all --with-real-primary`；后者包含原60例和新增7例，均逐项保留结果。真实目标逐例独立进程，B超时用例会等待实际45秒，失败退出码不会被吞掉。原入口14例仍使用StrictMock ISimManager，当前真实目标已经使用生产SimManager/Controller；完整CoreService/Stub/HDI仍BLOCKED。

F5新增 `SIM-028: real C0 (WSL, known lifetime failure)`，CLI支持GTest精确筛选和LLDB。TestMate列表发现可在CLI核实，VS Code GUI仍MANUAL；真实目标整组运行使用Task，以保证进程隔离。编辑/断点使用Debug构建和 `.host-out/build/compile_commands.json`，不使用Windows编译程序。
