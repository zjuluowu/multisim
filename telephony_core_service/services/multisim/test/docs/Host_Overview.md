# Host 测试入口

当前正式开发、构建、发现、运行和调试入口为[测试 README](../README.md)，统一命令为 scripts/test.py。普通用例只使用 sim_test.h，协议专项额外使用 sim_protocol_test.h。

[Full_Replacement.md](Full_Replacement.md)维护唯一的接口审计、前后结果、生命周期修复、平台边界及验收记录。当前已发布Host为124/124 PASS、CI准入PASS；历史失败保留原始版本。旧 Host/Primary/G2a Runner 已退役；历史命令和结果仅按其原提交解释，不参与活动执行。

产品构建、真实设备和 VS Code GUI 的状态与 Host CLI/LLDB 分开报告。依赖由 framework/internal/host/configs/dependency_lock.json 锁定，业务开发者无需理解或修改历史冻结资产。
