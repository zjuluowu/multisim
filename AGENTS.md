# 多 SIM 重构任务入口

本仓库包含 OpenHarmony `telephony_core_service` fork。多 SIM 重构是持续实施任务，包含设计、骨架、测试护栏和经授权的渐进接入。

- 开始前读取 [部件执行指引](telephony_core_service/AGENTS.md) 和 [唯一执行状态](telephony_core_service/docs/SIM_Refactor_Execution_State.md)。子目录指引细化本入口；技术决策仍由主设计维护。
- 先核对 Git 根目录、remote、分支、HEAD、工作区、worktree 及远端差异。已有本地改动优先保护，不自动 checkout/reset/clean/stash 或覆盖文件。
- 当前为 G2a 只读切片后的恢复/准入阶段；main 与工作分支具备的内容不同，以执行状态和实际 Git 可达性核对。历史 PASS 不等于当前 HEAD 通过。
- 不依赖旧会话、`/tmp`、固定克隆路径或本机结果文件。按执行状态定位远端代码检查点、参数化验证入口和下一批最小任务。
- 仅提交本次明确归属的文件；不无差别暂存，不强推或绕过保护。提交/推送按用户授权进行，完成后核实远端包含关系并更新执行状态。
