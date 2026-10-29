# SIM 架构文档入口

一致性复审日期：2026-10-23；起点为 `zjuluowu/multisim/main@801877c95ab8d37c60cb60dc210e796be6b96599`。这是固定审阅基线，不代表持续变化的分支 HEAD。生产证据沿用主文档 B1/W1，工作树未提交内容另列；本轮仅修改文档。

## 现行设计与阅读顺序

1. [SIM 架构重构设计](SIM_Architecture_Refactor_Design.md)：架构主规范。当前实现、目标责任、SIM-001～062 及子项、写权、兼容轨迹、阶段和 Q 项均在此维护。
2. [SIM 架构分层对比与功能迁移设计](SIM_Architecture_Layering_and_Migration_Design.md)：同一方案的分层/部署补充视图，不另立业务规则或实施状态。
3. [SIM 管理对外接口说明](SIM管理对外接口说明_源码核实版.md)：接口分层、签名与本仓委托路径；声明存在性不视为外部服务运行保证。
4. [独立 Linux 测试工程](../test/systemtest/README.md)及[执行记录](../test/systemtest/docs/run_record.md)：以记录的环境、版本、命令和日期解释结果，不从目标架构图推断测试完成。

目标技术裁决由 [SIM 目标架构设计 v1.2](SIM_Target_Architecture_Design_v1.2.md) §4/§6.4 承接；功能/WU 与迁移编号保留在重构设计，分层文档为补充视图，本文为阅读索引。差异处理见主文档[补充设计复审记录 UC-01～09](SIM_Architecture_Refactor_Design.md#supplement-review)。源码决定现在如何实现；用户要求与已确认契约约束目标；提案编号和“已批准”字样不能替代批准证据。

## 统一口径

- 两个业务上下文：T 账户与连接拓扑、A 角色与资源分配；O 为各用例使用的 Application 机制。R 卡运行支撑、C 兼容实现职责、I 技术适配、H 宿主装配另列；Compatibility 不作为第四个业务上下文。
- T/A 分别拥有自己的业务事实；O 不拥有业务事实或跨模块进行中标志；R 维护卡应用会话、Records 和真实设备资源使用。C 不成为第二业务写者。按 [WU-01～09 及全部旧入口](SIM_Architecture_Refactor_Design.md#writer-cutover)切换写权，遗留用例同步委托。
- 分类为 SlotTarget、CardApplicationTarget、SubscriptionTarget；空槽、PIN/PUK 和未识别身份的卡操作不强求 simId。simId 保持旧不透明 ID 契约，不承诺跨删除、重装或设备永久稳定。
- Domain 只含纯规则/值类型；Application 协调用例和受控提交，依赖内层端口；I/R/H 实现端口。R 复用生产协议和解析，尚未纯领域化。
- 保留公开 Framework/IPC、CoreManagerInner 消费侧及条件 eSIM 入口。SIM 私有 SO 是同进程链接边界，按 G2a 依赖/所有权整理、G2b 链接拆分、G2c 关闭协议分别验收。
- 卡状态、Records、账户、激活、默认角色、Profile 和网络就绪分别表达。GetShowNumber 等查询保留既有回写/存储访问；[部分失败轨迹](SIM_Architecture_Refactor_Design.md#legacy-visibility)不能被最终快照一致掩盖。
- 内部领域事件只通知已接受事实，不替换 RadioEvent、Hub、Registry、CES、账户回调或 eSIM 回执，不统一重排旧输出。进程内总线不提供跨进程传输；SA 身份不决定地址空间，66250 保持既有服务契约。
- 不新增公开 API/IPC/SA/进程、schema、厂商 ABI 或运行期双实现开关。四槽/MEP、NULL 迁移、新身份和恢复政策须满足对应 Q 项；本轮已授权同步修改未接入骨架，仍不表示生产接入或行为增强已完成。

## 先前设计与历史资料

以下文档保留原基线、推导和历史执行语境；其中“四 Context”、固定事件数、旧阶段摘要或旧 capability 阻塞不得覆盖现行主文档：

- [2026-10-15 冲突裁决](MultiSIM/08_design_unification_decisions.md)、[V3 总体设计](SIM_Refactored_Software_Architecture_Design_Version3.md)、[V3 类设计](SIM_Layered_Architecture_Class_Design_Version3.md)。
- [旧源码基线](MultiSIM/01_sim_subsystem_baseline.md)、[演进分析](MultiSIM/02_current_architecture_and_evolution.md)、[eSIM 边界分析](MultiSIM/03_esim_slot_and_sa_boundary.md)、[目标导读](MultiSIM/04_ddd_target_architecture.md)、[类/数据映射](MultiSIM/05_detailed_class_and_data_model.md)、[汇报提纲](MultiSIM/06_refactoring_proposal_slides.md)、[进展记录](MultiSIM/07_subscription_refactoring_status.md)。
- [历史目录](history/pre_unification_20261015/README.md)、V22/V23 等二进制汇报及审阅资料。保留原文和结果，不回写为新方案运行通过。

旧 [design_alignment_manifest.json](MultiSIM/design_alignment_manifest.json) 是生成时的摘要。当前证据及来源在主文档[来源清单](SIM_Architecture_Refactor_Design.md#document-sources)与复审附录维护，不用旧摘要证明当前内容。

## 维护与检查

- 改规则先更新主文档的职责、功能/写权、兼容和阶段，再同步补充视图；历史材料通过明确适用版本解释。
- 新的源码事实提供仓库路径、符号/行号及版本；跨仓缺口保持具体 Q 项，不从类名或部署图补出调用链。
- 覆盖映射、静态语义复核、主机执行、产品/设备验证分别记录。旧容量 1 PASS、设施 16 PASS 是历史记录，原业务场景没有新的通过证明；本轮未运行产品测试。
- 链接、图表、编号与证据摘要检查不能证明目标重构已实现。源码、数据、历史执行结果的变更另按用户任务处理。
