# SIM 架构文档入口

当前状态整理日期：2026-11-05。最新G2a分支及合入说明已发布到main，代码测试点与文档发布点在[合入记录](SIM_Refactor_Execution_State.md#main-integration-20261105)分别列出；本次只核对发布和证据、更新状态文档，没有重新运行业务或产品测试。最新分支、已完成项、失败/待裁决、产品阻塞及下一批任务统一查[唯一执行状态 §0](SIM_Refactor_Execution_State.md#current-status)，不从本文推断已完成产品替代。

修订B+骨架、G2a只读闭环及SIM Guard两轮已保存到main；最新G2a构建/执行护栏强化及状态整理也已按本批授权合入main，并在干净合入代码点重验。当前生产成熟业务仍由services/sim承担。此前2026-10-23一致性复审起点 `801877c95ab8d37c60cb60dc210e796be6b96599` 是历史审阅基线，不是当前HEAD或最新测试基线。

## 现行设计与阅读顺序

1. [执行状态](SIM_Refactor_Execution_State.md)：先区分当前交付、实际测试基线与未完成门禁，再选择下一批任务。
2. [目标主设计 v1.2](SIM_Target_Architecture_Design_v1.2.md)：业务边界、唯一写权、协议及阶段门禁；整体仍draft/pending，不覆盖已批准业务契约。
3. [源码详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md)及[源码导航](../services/multisim/README.md)：B+实际/稳态目录、文件与GN映射、日常任务入口。
4. [SIM 架构重构设计](SIM_Architecture_Refactor_Design.md)及[分层/迁移补充视图](SIM_Architecture_Layering_and_Migration_Design.md)：沿用SIM-001～062、WU-01～09、兼容轨迹与Q项，不另立架构裁决或当前实施台账。
5. [SIM 管理对外接口说明](SIM管理对外接口说明_源码核实版.md)：接口分层、签名与本仓委托路径；声明存在性不视为外部服务运行保证。
6. [G2a/Linux主机切片](../services/multisim/test/systemtest/README.md)、[SIM跨架构契约防护网](../services/multisim/test/sim_guard/README.md)、[架构规则](../services/multisim/test/multisim_architecture_rules/README.md)：分开阅读对应证据，主机/组件与产品/设备验证不互相替代。

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
- 覆盖映射、静态语义复核、主机执行、产品/设备验证分别记录。旧容量1 PASS/设施16 PASS是历史切片；G2a七场景及SIM Guard兼容/缺陷/设施使用各自证据与基线，不与旧统计相加。本批合入重跑G2a、审计与架构/GN检查，SIM Guard仍引用其既有记录；没有运行产品测试。
- 链接、图表、编号与证据摘要检查不能证明目标重构已实现。源码、数据、历史执行结果的变更另按用户任务处理。
