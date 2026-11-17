# Windows VS Code + WSL SIM Host

当前构建、发现、单例运行与调试统一见[测试README](../README.md)，使用scripts/test.py。VS Code的Build Host构建全部业务切片；Run/Debug Selected Test只需用例名，Testing面板保持每例独立进程。手工launch仍可选择真实链接切片。

公开接口为sim_test.h与sim_protocol_test.h，平台细节在framework/internal；固定依赖见[dependency_lock.json](../framework/internal/host/configs/dependency_lock.json)。旧build_host.sh/run_host_tests.sh及compatibility Adapter已退役，不再作为开发入口。

[Full_Replacement.md](Full_Replacement.md)记录实际CLI/LLDB、生命周期失败、产品与GUI边界；2026-11-11历史结果及本页旧命令在Git历史和归档中保留，不视为当前验收。

## Testing 面板、源码定位和断点

用 VS Code 的“WSL: Reopen Folder in WSL”打开仓库根目录，在 WSL 扩展端安装推荐的 TestMate C++、CodeLLDB 和 clangd。Linux ELF 测试不能由 Windows 本地扩展端直接运行。

首次执行“Tasks: Run Task → Prepare Testing Panel”。该任务通过统一 test.py 构建七个业务测试程序，并逐个验证 GoogleTest XML 中每条用例的源码绝对路径和行号。固定依赖须先按 README 准备；首次也可向 `test.py editor` 传入 `--gtest-root`、`--openssl-root`、`--runtime-root`，验证后的路径保存在 .host-out/replacement.json，后续任务复用。任务失败时查看其输出的 Report 路径及对应 build 日志。

构建完成后打开 Testing 面板，点击刷新。启用启动自动发现、关闭枚举缓存，新增 TEST_F 后再次执行准备任务并刷新。七个程序按独立链接切片显示；这里只枚举 Host GoogleTest，Python 技术检查、架构脚本和需要设备的原生测试仍从 test.py 对应入口执行。

双击面板中的用例名称或使用“Go to Test”打开 TEST_F 所在文件及行号。在用例体或被调用的生产代码行号左侧设置断点，然后使用该用例的“Debug Test”（虫形图标或右键菜单）。普通运行按钮不启动调试器。TestMate 自动传入该用例的可执行文件、精确过滤参数和环境，不需要手填用例名；运行/调试前均增量构建，保持每例独立进程及独立 profile。宿主构建保留 DWARF 和帧指针；优化代码中的部分行或局部变量可能被优化，优先在实际执行语句处断点。

`test.py editor` 的报告包含 `editor/locations.json` 和七份原始枚举 XML，可用于检查列表数量与定位。该检查不运行业务用例，也不代表 GUI 点击或全部业务断言通过；已知业务和生命周期失败继续保留。
