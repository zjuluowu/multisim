# SIM 多卡重构开发入口

首次阅读只需三处。先读[业务问题与重构目标 §1～2](business/多SIM架构重构_设计说明与业务流程对照.md#1-%E8%AE%BE%E8%AE%A1%E7%9B%AE%E6%A0%87%E4%B8%8E%E9%80%82%E7%94%A8%E8%8C%83%E5%9B%B4)，再读[源码导航的目录职责](../services/multisim/README.md)，最后查[执行状态页首](SIM_Refactor_Execution_State.md#current-status)。现网 SIM 由 `services/sim/` 实现，`services/multisim/` 提供目标实现和 Host 验证。具体已实现切片、生产装配和设备验收分别查执行状态。

## 按开发任务阅读

| 任务 | 阅读顺序与检查点 |
| --- | --- |
| A · 初次了解项目 | 按页首顺序理解为何要分开卡应用、账户、默认角色和设备资源，再按五组源码目录定位业务。需要了解事实写权时读[目标架构 §3～4](SIM_Target_Architecture_Design_v1.2.md#3-统一语义与身份规则)。 |
| B · 开发一个业务功能 | 用 [SIM 功能清单](migration/SIM_Architecture_Refactor_Design.md#feature-mapping)定位旧入口及接管范围，按[业务对象与时序 §4、§7～10](business/多SIM架构重构_设计说明与业务流程对照.md#4-%E6%A0%B8%E5%BF%83%E6%A6%82%E5%BF%B5%E5%8F%8A%E5%85%B6%E5%85%B3%E8%81%94)找对应流程。核对[对外接口及权限](business/SIM管理对外接口说明_源码核实版.md)与[源码详细设计 §5～17](SIM_Multisim_Source_Architecture_Detailed_Design.md#identity)。在[测试用例导航](../services/multisim/test/cases/README.md)选择对应场景，以 Oracle、Legacy/Target 支持矩阵和实际差分确认验收范围。 |
| C · 修改架构或源码目录 | 按[目标架构 §3～6、§9～14](SIM_Target_Architecture_Design_v1.2.md#3-统一语义与身份规则)确定业务事实 Owner、分类目标和依赖约束，业务专属 Adapter 的落点查[目录方案 §6.1](SIM重构代码目录结构设计方案.md#61-业务专属-adapter-不进入-infrastructure)，公共请求、装配与兼容查[§5](SIM重构代码目录结构设计方案.md#5-infrastructure基础支撑目录职责)，再查[源码详细设计 §3～4、§21](SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree)确定文件、接口及独立 GN target。按下方维护表更新对应职责文档，并运行[架构检查](../services/multisim/test/cases/architecture/README.md)。 |
| D · 测试或定位问题 | 从[统一测试入口](../services/multisim/test/README.md)选命令、后端和用例，按 [Host 使用说明](../services/multisim/test/docs/Host_Usage.md)构建、看日志或断点定位。到 [Full Replacement](../services/multisim/test/docs/Full_Replacement.md)查覆盖、契约、Oracle 和归档证据。 |
| E · 继续推进迁移 | 先看[阻塞与下一批任务](SIM_Refactor_Execution_State.md#next-work)，用 [WU-01～09](migration/SIM_Architecture_Refactor_Design.md#writer-cutover)确认全量旧入口和验收闭包，再查[源码接入映射 §17](SIM_Multisim_Source_Architecture_Detailed_Design.md)与[测试入口](../services/multisim/test/README.md)。按[阶段门禁 §22](SIM_Target_Architecture_Design_v1.2.md#22-分阶段实施门禁)更新执行状态。 |

## 维护职责

| 文档 | 唯一维护内容 | 维护角色与更新时机 |
| --- | --- | --- |
| [目标架构 v1.2](SIM_Target_Architecture_Design_v1.2.md) | 业务责任、事实写权、异步、资源和实施门禁。整体仍为 `draft/pending`。 | 架构决策负责人记录决策证据和适用范围，涉及接口、物理布局或实施切片时同步对应文档。 |
| [五组目录方案](SIM重构代码目录结构设计方案.md) | 一级物理职责和 Infrastructure 的内部边界。 | 目录变更负责人在获准的目录决策中更新，并同步详细设计和路径摘要护栏。 |
| [源码详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md) | 当前与稳态路径、文件、实现接缝、类、接口及逻辑 GN DAG。 | 模块变更负责人随源表、构造和依赖更新，同步源码导航。 |
| [功能与 WU](migration/SIM_Architecture_Refactor_Design.md) | SIM-001～062 及子项、WU-01～09、旧入口及接管闭包。 | 迁移负责人随入口、写点或承接范围变化更新，沿用原编号并关联验收。 |
| [业务流程对照](business/多SIM架构重构_设计说明与业务流程对照.md) | 显示号码、激活、主卡和 Records 的时序与部分失败对照。 | 用例负责人随流程证据或批准行为变化更新，引用功能清单与实际接口。 |
| [唯一执行状态](SIM_Refactor_Execution_State.md#current-status) | 代码基线、实测证据、阻塞和下一批任务。 | 批次负责人每次交付后更新，按输入基线链接原始记录。 |
| [对外接口说明](business/SIM管理对外接口说明_源码核实版.md) | 对外 API、调用者、权限、错误及已核实的外部依赖。 | 接口变更负责人按源表、契约和受影响消费者更新，跨仓分工查[仓库边界](knowledge/routing/repo-boundary.md)。 |

**冲突处理。** 已发布或已批准的业务契约约束行为。源码和构建配置证明当前装配，带基线的日志证明实际验证，目标设计描述计划行为。以具体批准记录和作用范围核定决策状态，再回到上表中负责该问题的文档修订。Host 组件、产品、设备各按对应证据验收。

**文档状态。** Authoritative 表示职责上的维护来源。Supporting 为补充视图和代码证据，Operational 为操作入口，Historical 为时间固定的记录，Superseded 为已替代版本，Generated 为生成或派生材料。Pending 用于待核实材料或待批准决策，设计批准状态以正文和批准证据为准。

## 按用途浏览

- 当前架构、目录方案、源码详细设计和执行状态保留稳定路径。目录迁址的[原实施记录](SIM_Directory_Migration_20261112.md)提供历史映射。
- [业务参考](business/)集中接口、四条业务流程及 [Primary 专项对照](business/SetPrimarySlotId_重构前后实现对比说明.md)。[功能迁移](migration/)维护功能和 WU。
- [知识库](knowledge/)提供跨仓路由、权限和构建等按需资料。运行指南以[测试工程](../services/multisim/test/README.md)为准。
- [历史索引与整理记录](history/README.md)登记原稿、评审、汇报及逐文件迁移清单。追溯历史判断时采用对应版本和证据基线。
