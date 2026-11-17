# Windows VS Code + WSL SIM Host

当前构建、发现、单例运行与调试统一见[测试README](../README.md)，使用scripts/test.py。VS Code的Build Host构建全部业务切片；Run/Debug Selected Test只需用例名，Testing面板保持每例独立进程。手工launch仍可选择真实链接切片。

公开接口为sim_test.h与sim_protocol_test.h，平台细节在framework/internal；固定依赖见[dependency_lock.json](../framework/internal/host/configs/dependency_lock.json)。旧build_host.sh/run_host_tests.sh及compatibility Adapter已退役，不再作为开发入口。

[Full_Replacement.md](Full_Replacement.md)记录实际CLI/LLDB、生命周期失败、产品与GUI边界；2026-11-11历史结果及本页旧命令在Git历史和归档中保留，不视为当前验收。
