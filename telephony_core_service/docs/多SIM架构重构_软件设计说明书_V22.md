# V22软件设计说明书：历史资料入口

状态：SUPERSEDED，2026-10-15已由Version3总体设计、类设计及当前裁决刷新。

完整V22正文、81页逐页内容和演讲备注保存在[历史说明书](history/pre_unification_20261015/多SIM架构重构_软件设计说明书_V22.md)。原稿不删除；历史PPTX仍保留。

现行设计请阅读[文档入口](README.md)、[冲突审视](MultiSIM/08_design_unification_decisions.md)、[SAD v0.3.1](SIM_Refactored_Software_Architecture_Design_Version3.md)及[类设计](SIM_Layered_Architecture_Class_Design_Version3.md)。

V22的四业务上下文/六聚合候选、角色归Subscription、持久化受理与Outbox、公共Subscription接口、数据库替换、实现切换或另一分支Shadow进度均不是本阶段实施规范。当前采用四Context/三聚合/分类目标，冻结公开协议、schema和部署，无运行期Feature Flag，不承诺跨重启同OperationId重放。

V23 PPTX/PDF与代码审阅说明是Version3整合前的审查成果；其源码风险诊断仍可追溯，但目标Owner划分和未来路线由本轮裁决取代。历史统计不作为本轮复算，历史摘要不当当前文件摘要。
