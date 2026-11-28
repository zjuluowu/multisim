# C1 整轮物理租约和 Owner 回执核对

输入 main 为 `b96b91d3e16d4ae368c241bf8af59e93aef97a72`。

在正式 SimModule 上控制真实 Owner 投递 FIFO。旧候选在 CHECK 与 UPDATE 的成员结果均已接纳、
Owner 尚未消费时，将 Radio 与槽 1 ICCID 共用的资源标为可用。两个间隙都实际出现了共享 ICCID
的 GET RESPONSE 发令。[基线原始失败](baseline-shared-ef.log)

R 将租约锚定首条 CHECK 的原 StepRef，跨 CHECK、UPDATE 和 COMPLETE 保留同一租约。后继命令
必须匹配原请求、attempt、协议会话、精确下一步、冻结成员及路由。CHECK 与 UPDATE 的回执只推进
本轮。最后 COMPLETE 的全成员终止和复用证据使物理资源可用。正式回归实测两个间隙均无共享 EF
发令，邻卡独立域仍发原 EF。原 COMPLETE 已获证而 Owner 仍持回执时，共享域已经可用于卡命令。

R 在锁外查询 Owner。初读条目为空后，第二遍仍核对原投递、帧及回执。重投单独核对查询期间
是否产生新的原投递，避免用过时的 Owner 空状态重投尚在消费的回执。确定性测试覆盖三条 Owner
查询入口及新回执在查询期间到达的场景。

Target 全量 251/251。Primary、Radio、C1 拓扑、ICCID 和卡组合专项 ASan、TSan 各 136/136。
独立逐例复跑本批三个用例，退出码均为 0。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [独立复跑](independent.log)

十二个原冻结契约双端执行 24/24。后端及 Oracle 差分为零。架构护栏、独立语法、逻辑 GN/Ninja
和冻结资产完整性均通过。
[Both 与 Oracle](both.json.gz) [架构](architecture.log) [资产完整性](integrity.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 提前归还跨阶段租约 | Owner 尚未推进时可用，同域 EF 已发令 | [变异](early_physical_release-mutated.log) [恢复](early_physical_release-restored.log) |
| 持 R 锁查询 Owner | 查询回调无法在等待期间读原资源 | [变异](locked_owner_audit-mutated.log) [恢复](locked_owner_audit-restored.log) |
| 跳过第二遍回执核对 | 新暴露的回执仍使 R 报告已排空 | [变异](one_pass-mutated.log) [恢复](one_pass-restored.log) |
| 接受过期 Owner 查询执行重投 | 新回执被重复登记为待投递任务 | [变异](stale_flush-mutated.log) [恢复](stale_flush-restored.log) |

本批未覆盖 Stop 恰落在跨阶段间隙时的全部原轮恢复来源。没有可归因的原轮终止证明时，原轮继续
保留隔离。正式全图的两遍关闭、卡字段 Refresh、跨实例原责任和独立扩展仍待。
