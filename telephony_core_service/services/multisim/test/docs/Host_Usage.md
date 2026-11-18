# Windows VS Code + WSL SIM Host

2026-11-18 Host回归124/124 PASS、CI准入PASS，历史失败与修复证据见[本批归档](archive/remaining_failures_20261118/phase_b/README.md)；产品和GUI未验收。

当前构建、发现、单例运行与调试统一见[测试README](../README.md)，使用scripts/test.py。VS Code的Build Host构建全部业务切片；Run/Debug Selected Test只需用例名，Testing面板保持每例独立进程。手工launch仍可选择真实链接切片。

公开接口为sim_test.h与sim_protocol_test.h，平台细节在framework/internal；固定依赖见[dependency_lock.json](../framework/internal/host/configs/dependency_lock.json)。旧build_host.sh/run_host_tests.sh及compatibility Adapter已退役，不再作为开发入口。

[Full_Replacement.md](Full_Replacement.md)记录实际CLI/LLDB、生命周期失败、产品与GUI边界；2026-11-11历史结果及本页旧命令在Git历史和归档中保留，不视为当前验收。

## Testing 面板、源码定位和断点

用 VS Code 的“WSL: Reopen Folder in WSL”打开仓库根目录，在 WSL 扩展端安装推荐的 TestMate C++、CodeLLDB 和 clangd。Linux ELF 测试不能由 Windows 本地扩展端直接运行。

首次执行“Tasks: Run Task → Prepare Testing Panel”。该任务通过统一 test.py 构建七个业务测试程序，并逐个验证 GoogleTest XML 中每条用例的源码绝对路径和行号。固定依赖须先按 README 准备；首次也可向 `test.py editor` 传入 `--gtest-root`、`--openssl-root`、`--runtime-root`，验证后的路径保存在 .host-out/replacement.json，后续任务复用。任务失败时查看其输出的 Report 路径及对应 build 日志。

构建完成后打开 Testing 面板，点击刷新。启用启动自动发现、关闭枚举缓存，新增 TEST_F 后再次执行准备任务并刷新。七个程序按独立链接切片显示；这里只枚举 Host GoogleTest，Python 技术检查、架构脚本和需要设备的原生测试仍从 test.py 对应入口执行。

双击面板中的用例名称或使用“Go to Test”打开 TEST_F 所在文件及行号。在用例体或被调用的生产代码行号左侧设置断点，然后使用该用例的“Debug Test”（虫形图标或右键菜单）。普通运行按钮不启动调试器。TestMate 自动传入该用例的可执行文件、精确过滤参数和环境，不需要手填用例名；运行/调试前均增量构建，保持每例独立进程及独立 profile。宿主构建保留 DWARF 和帧指针；优化代码中的部分行或局部变量可能被优化，优先在实际执行语句处断点。

`test.py editor` 的报告包含 `editor/locations.json` 和七份原始枚举 XML，可用于检查列表数量与定位。该检查不运行业务用例，也不代表 GUI 点击或全部业务断言通过；业务结果以实际回归报告为准，历史失败证据保留。

## 随当前 CPP 文件筛选用例

测试树按“可执行程序 → 测试源码文件 → 用例”分组。先完成 Prepare Testing Panel 并刷新，再打开某个测试 CPP，在左侧 Testing 面板的筛选输入框输入 `@doc`，或点击该输入框右侧的漏斗并勾选“Show in Active File Only（仅显示当前文件）”。启用后，切换到另一个测试 CPP，面板会自动显示新文件中的用例，无需重新构建或逐次输入文件名；面板须保持打开。

例如打开 cases/developer_copy_test.cpp 时显示 SimTest.DualSimPinUnlockAfterWrongPin；切换到 cases/pin_failure_isolation_test.cpp 时显示 SimTest.WrongUsimPinDoesNotBlockSimPeer。用例继续支持源码跳转和 Debug Test。

`@doc` 是 VS Code 内置的测试视图筛选状态，不是 settings.json 配置项，仓库配置不能默认替用户打开该开关。需要在实际使用的 VS Code 窗口启用；若重开窗口后筛选清空，再输入 `@doc` 即可。删除 `@doc` 或取消勾选可恢复全部测试。打开不包含已发现用例的生产 CPP/头文件时列表为空是当前文件筛选的预期行为。若测试 CPP 也为空，先移除其他名称/状态筛选，再确认打开的是同一个 WSL 仓库、已构建发现用例，并刷新测试列表。

依据：[VS Code 测试筛选](https://code.visualstudio.com/docs/debugtest/testing)、[TestMate 分组配置](https://github.com/matepek/vscode-catch2-test-adapter/blob/master/documents/configuration/test.advancedExecutables.md#testgrouping)。本批仅核对配置和真实枚举映射，未声称已经在GUI中打开此筛选。

C0 主卡单例的旧实现析构/ICCID注销问题已修复，资源清理断言保持；修复前后、反向验证及当前其他失败见[本批证据](archive/c0_lifetime_fix_20261117/README.md)。查询切片只构造未初始化Manager，其Host链接边界被调用即报错，不能用这些切片证明完整对象图清理。

## 完整回归报告

VS Code任务 `Run All Tests` 使用 `test.py all --ci`；构建仍在.host-out，报告默认在.host-out/results。本次架构检查会临时在仓外执行，并将产物复制至报告的architecture/；architecture-location.json记录执行位置和保留位置。临时目录完成后移除，排查时使用保留位置及architecture.log。直接调用run_architecture.py仍使用仓外--output。

当前五项已修复缺陷必须PASS；修复策略文件缺失、删项或重复均拒绝准入，不回退到历史失败豁免。历史签名及原始报告保持原字节。
