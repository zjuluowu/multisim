SIM架构现行索引（2026-10-15）
============================

先读 ../README.md 和 08_design_unification_decisions.md。
总体规范：../SIM_Refactored_Software_Architecture_Design_Version3.md（SIM-SAD-001 v0.3.1）
类设计：../SIM_Layered_Architecture_Class_Design_Version3.md（SIM-CLASS-001 v0.3.1）

01：当前源码事实与边界，不包含其他分支的实现完成声明。
02：问题证据与演进；统计和反例分别标记证据等级。
03：eSIM、slot、66250及共宿主边界。
04：统一目标导读，使用V3四Context/三聚合，不另立规范。
05：当前类、字段、目标与兼容映射，不等同新表设计。
06：基于统一设计的评审提纲，不代表代码完成。
07：当前仓进展与真实测试记录，不采用其他仓gitlink状态。
08：新旧冲突取舍、代码依据、条件目标与待决来源。
design_alignment_manifest.json：本轮输入、归档、源码摘要和验证。

三类目标为SlotTarget/CardApplicationTarget/SubscriptionTarget；不伪造simId。
公开API、IPC、schema、部署和厂商ABI冻结；无运行期Feature Flag。
十类内部异步领域事件不替代既有通知与回执。
Phase0未就绪；容量B1 PASS、INFRA16 PASS，原五业务场景BLOCKED。

旧完整正文保存在 ../history/pre_unification_20261015/。
V22/V23二进制与审阅报告是历史评审资料，不作为并行实施规范。
方案变更同步SAD、类设计、08及专题；运行状态须有真实命令与结果。
