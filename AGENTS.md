# 多 SIM 重构任务入口

本仓库包含 OpenHarmony `telephony_core_service` fork。最终由 `services/multisim/` 完整替代 `services/sim/`，不长期保留两套 SIM 业务实现。工作包括设计、骨架、测试护栏及按授权实施的生产迁移。

- 开始前读取[部件执行指引](telephony_core_service/AGENTS.md)和[唯一执行状态](telephony_core_service/docs/SIM_Refactor_Execution_State.md)。业务架构与不变量由主设计维护，物理目录及文件映射由源码详细设计维护；本入口不复制进度或技术方案。
- 先核对 Git 根目录、remote、分支、HEAD、工作区、暂存区、worktree 和远端差异。保护已有修改，不自动 checkout/reset/clean/stash 或覆盖文件。
- 历史修订 B+ 骨架及 G2a GetRealSimCount 只读闭包已按授权保存到 main；当前目录以[五组目录方案](telephony_core_service/docs/SIM重构代码目录结构设计方案.md)和[迁移记录](telephony_core_service/docs/SIM_Directory_Migration_20261112.md)为准；主机验证、产品接入、写权切换、SO/生命周期及新能力分别验收。历史 G2a 检查点保留旧目录，精确提交、证据和下一批任务查执行状态，并以实际 Git 可达性复核。
- 不依赖旧会话、`/tmp`、固定克隆路径或本机结果恢复工作；验证产物可放仓外，但不能作为唯一交付证据。历史 PASS、目录存在和提交成功均不等于当前生产验收。
- 每批按用户授权完成实际变更、适用验证和状态同步。用户已持续授权：每完成一个有实际成果、可复核的阶段进展，及时将本批明确归属的修改提交并常规推送至 `origin/main`，无需重复请求提交/推送确认；不要将发布集中拖到任务全部结束。已知失败和阻塞须如实保存，阶段发布不等于业务或产品验收通过。
- 发布前核对远端差异、适用验证及唯一执行状态，仅逐文件暂存本批成果；保护用户已有和无关修改，不无差别暂存、不强推、不绕过保护。远端分歧或保护拒绝时保留本地成果并报告，不为推送成功擅自合并、变基或切换工作区；推送后核实远端SHA与包含关系。此持续发布授权不扩大生产修改/迁移范围；用户后续明确要求不提交、不推送或调整目标分支时，以该要求为准。
