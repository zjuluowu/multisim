# SIM 架构文档入口

统一日期：2026-10-15。当前工作仓：`zjuluowu/multisim`，`main`，HEAD `df8f2f017f83c70145b20255b4e9042f7a5fd923`；工作树有未提交内容，所审源码摘要见文档清单。

## 现行设计与阅读顺序

1. [冲突审视与统一裁决](MultiSIM/08_design_unification_decisions.md)：记录为什么采纳或修正新旧方案，区分设计选择、源码事实和待补批准证据。
2. [SIM 重构后软件架构设计](SIM_Refactored_Software_Architecture_Design_Version3.md)：总体设计，文档 ID `SIM-SAD-001`。
3. [SIM 分层架构类设计](SIM_Layered_Architecture_Class_Design_Version3.md)：总体设计的类与接口细化，文档 ID `SIM-CLASS-001`。
4. [当前源码基线](MultiSIM/01_sim_subsystem_baseline.md)、[问题与演进分析](MultiSIM/02_current_architecture_and_evolution.md)、[eSIM 与部署边界](MultiSIM/03_esim_slot_and_sa_boundary.md)：解释重构起点，不能用目标类图替代现状。
5. [目标架构导读](MultiSIM/04_ddd_target_architecture.md)、[类与数据映射](MultiSIM/05_detailed_class_and_data_model.md)、[评审汇报提纲](MultiSIM/06_refactoring_proposal_slides.md)：使用同一目标模型，不另立竞争方案。
6. [当前进展](MultiSIM/07_subscription_refactoring_status.md)、[独立 Linux 测试工程](../test/systemtest/README.md)：只依据实际执行记录报告状态。

设计一致性规则是：**本轮裁决 → 总体设计 → 类设计 → 导读与测试设计**。源码决定“现在是什么”；用户需求与已确认契约决定验收。源码中的现有实现不自动成为永久业务规范，新文档的“已批准”字样也不能替代缺失的批准记录。

## 统一口径

- 四个 Context：Subscription Topology、Profile Operation、Radio Assignment、Compatibility。
- 三个候选聚合：TopologyAggregate、ProfileOperationAggregate、RadioAssignmentAggregate；分别由 TopologyManager、OperationCoordinator、RadioResourceArbiter 接受领域修改。
- 三类目标：SlotTarget、CardApplicationTarget、SubscriptionTarget；空槽、PIN/PUK、身份尚未识别的操作不强求 simId。
- simId 沿用为账户/订阅的不透明标识；不新增第二套公共 SubscriptionId，不把删除、重装和跨设备身份稳定性当作已确认能力。
- 一个逻辑 ACL 分入站、查询、出站职责；保留 CoreManagerInner 的消费侧契约，内部可注入端口以消除 SIM SO 到 Core SO 的反向链接。
- 十类内部异步领域事件是候选目录，不替换既有 RadioEvent、CommonEventHub、Registry、CES、账户回调及 eSIM 回执。
- 不新增公开 API/IPC/SA/进程、schema、厂商动态库 ABI 或运行期 Feature Flag；不同构建产物差分不等于运行期切换实现。
- 原产品行为冻结；四槽、ICCID 写 NULL、默认策略、超时和恢复规则均有明确准入条件。批准来源缺失时保留设计目标，不实施迁移或扩大产品能力结论。
- Phase 0 尚未就绪；当前已记录容量查询 1 PASS、设施 16 PASS，原五条业务场景仍 BLOCKED。不是 3/35，也不是设备测试完成。

## 历史资料

[历史目录](history/pre_unification_20261015/README.md)保留本轮刷新前的完整方案、分析与状态记录。V22 说明书、V22 PPTX、V23 PPTX/PDF及其审阅依据是历史评审材料；其中早期 Context、聚合、持久化、公共接口演进和 Shadow 完成结论不作为现行实施依据。

二进制汇报原稿和既有执行结果不回写为“新方案已通过”。历史摘要描述其生成时的输入；文档刷新后不能将历史 SHA256 当作当前文件摘要。

## 维护与检查

- 改模型先更新冲突裁决、总体设计和类设计，再同步专题文档；同一规则只有一个规范来源。
- 构建和测试进展必须给出命令、SHA/源码摘要、环境、退出码与执行数量；未运行不得 PASS。
- 文档统一不授权代码重构、数据迁移、权限变更或外仓修改，也不修改已冻结的历史结果。
- 本轮审视范围、源码锚点、文档状态与检查结果见 [design_alignment_manifest.json](MultiSIM/design_alignment_manifest.json)。
