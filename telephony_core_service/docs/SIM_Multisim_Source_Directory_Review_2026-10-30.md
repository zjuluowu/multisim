# services/multisim 目标源码目录结构独立架构评审

| 项目 | 内容 |
| --- | --- |
| 评审对象 | [`services/multisim/`](../services/multisim/) 在完全替代 [`services/sim/`](../services/sim/) 后的最终稳态源码目录组织 |
| 评审基线 | `main` = `origin/main` = `5eed2181628e9e74f618231007d927ebd5cb92cd`，工作区无未提交修改 |
| 输入资料 | [目标架构设计 v1.2](SIM_Target_Architecture_Design_v1.2.md)、[源码结构详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md)、[SIM 架构重构设计](SIM_Architecture_Refactor_Design.md)、生产源码及 [`BUILD.gn`](../BUILD.gn) |
| 文档性质 | 独立评审意见（PROPOSED），供架构师决策；不是批准记录，也不修改目标稿、功能清单或源码 |
| 证据标记 | 〔事实〕已用固定提交的代码或统计核实；〔推断〕由事实推导；〔待验证〕需要补充证据 |
| 验证限度 | 仅做静态文件统计和 `#include` 统计；未运行 OHOS 构建、链接、单测或设备验证 |

## 目录

1. [核心结论](#1-核心结论)
2. [当前目录评价](#2-当前目录评价)
3. [Runtime 与公共机制的归属](#3-runtime-与公共机制的归属)
4. [候选方案](#4-候选方案)
5. [业务场景验证](#5-业务场景验证)
6. [AI 辅助编程友好度](#6-ai-辅助编程友好度)
7. [生产能力完整性检查](#7-生产能力完整性检查)
8. [方案横向比较](#8-方案横向比较)
9. [最终推荐目录结构](#9-最终推荐目录结构)
10. [尚需架构师裁决的问题](#10-尚需架构师裁决的问题)
11. [附录：证据与统计方法](#11-附录证据与统计方法)

---

## 1. 核心结论

**需要优化，但不是“删几个目录”，而是调整投入方向和边界机制。**

1. **目录投入方向反了。** 〔事实〕旧 `services/sim` 约 34.5K 行，约 75% 属于 R（卡运行支撑：EF/记录、eSIM、运营商配置、卡状态、电话簿、STK、Radio 协议）；T/A 混合控制器约占 13%。现骨架却在 T/A 上铺了 `api/application/domain/ports` 四层，R 只有 3 个治理子目录，缺少容纳生产代码的组织方案。
2. **一级目录抽象层次混杂。** 业务模块（topology、assignment）、分层名（application、infrastructure）、DDD 角色（model）和实现职责（runtime、compatibility、assembly）并列。顶层 `application/` 与 `topology/application/` 同名但含义不同。
3. **边界只靠约定。** 〔事实〕骨架没有 GN target，全部代码在同一个 namespace；`topology` 与 `assignment` 在目录层面互相依赖；`CardRuntime`、`request_context.h` 和 `LegacySimFacade` 正在变成新的集中点。

**推荐**：一级目录按“状态 Owner”组织，模块内部按用例族组织；分层只在能机械检查的地方用目录表达（`api/`、`domain/`、`adapters/`）；解耦交给 GN target、模块 namespace 和“跨模块只能 include `api/`”规则。

---

## 2. 当前目录评价

### 2.1 关键事实

| # | 事实 | 证据 |
| --- | --- | --- |
| F1 | 骨架实际 73 个文件、4,984 行；详细设计仍写 64 个文件，已经漂移 | 文件统计；[详细设计 §3](SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree) |
| F2 | 骨架未进入任何 GN target；全部代码在同一 namespace `OHOS::Telephony::MultiSim` | [`BUILD.gn`](../BUILD.gn) 只引用 `services/sim`；namespace 统计 |
| F3 | 旧 sim 各功能域占比：EF/记录/codec 28.1%、eSIM 14.7%、T/A 控制器 13.2%、运营商 8.8%、卡状态 8.4%、电话簿 8.1%、入口 5.6%、工具 3.4%、存储 3.2%、STK 3.1%、Radio 1.8%、短信存储 1.6% | 按文件分类统计，见[附录 11.2](#112-旧-servicessim-按功能域统计) |
| F4 | 骨架规模：topology 27 个文件/1,962 行，assignment 14/816，runtime 9/915（只有 session/records/resource 三个子目录） | 见[附录 11.3](#113-骨架按一级目录统计) |
| F5 | `CardRuntime` 同时实现 4 个端口，并持有 sessions、resources、loads、MSISDN 关联、观察序号等全部 R 状态 | [`runtime/card_runtime.h`](../services/multisim/runtime/card_runtime.h) 第 70、97 行 |
| F6 | `request_context.h` 被 include 12 次，是骨架中最高的；其中定义了封闭枚举 `RequestOwner { Unspecified, ShowNumber, Activation, Assignment, LocalCard, LegacyBridge }` | [`application/orchestration/request_context.h`](../services/multisim/application/orchestration/request_context.h) 第 28 行 |
| F7 | T 与 A 在目录层面双向依赖 | [`topology/ports/account_store.h`](../services/multisim/topology/ports/account_store.h) 第 26 行 → `assignment/api`；[`assignment_service.h`](../services/multisim/assignment/application/assignment_service.h) 第 30～31 行 → `topology/ports`；[`topology_service.h`](../services/multisim/topology/application/topology_service.h) 第 32 行 → `assignment/ports` |
| F8 | 端口位置违反“消费方定义”：`ProfilePreferencePort` 在 `assignment/ports`，实际消费者是 `card_access` | [`local_card_requests.h`](../services/multisim/application/card_access/local_card_requests.h) 第 25 行 |
| F9 | `legacy_output_adapter.h` 同时适配 T、A、C 三方出口 | [`infrastructure/legacy_output_adapter.h`](../services/multisim/infrastructure/legacy_output_adapter.h) 第 23～24 行及对 `compatibility` 的引用 |
| F10 | ISimManager 有 122 个虚方法，IEsimManager 有 33 个；`LegacySimFacade` 通过构造函数注入具体 T/A 用例类 | [`i_sim_manager.h`](../interfaces/innerkits/include/i_sim_manager.h)；[`legacy_sim_facade.h`](../services/multisim/compatibility/legacy_sim_facade.h) 第 31 行 |
| F11 | 纯计划代码反复在 domain 与 application 目录之间搬移：`show_number_plan` 已移到 Application；`PlanPrimarySwitch` 现位于 `assignment/application/primary_switch_plan.h` 第 43 行，详细设计 §7/§21 仍写在 `assignment_aggregate.cpp` | [`primary_switch_plan.h`](../services/multisim/assignment/application/primary_switch_plan.h)；[详细设计 §7](SIM_Multisim_Source_Architecture_Detailed_Design.md#assignment) |
| F12 | 三份文档的目录主张互相冲突：目标稿 §5.1.5 是分层优先（`domain/topology`、`application/topology`）；目标稿 §6.4.1 与重构设计 §4.4 是上下文优先；重构设计 SIM-023.a 行又写成分层优先路径 | [目标稿 §5.1.5](SIM_Target_Architecture_Design_v1.2.md#515-抽象的最小性)、[§6.4.1](SIM_Target_Architecture_Design_v1.2.md#641-目录与职责裁决)；[重构设计 §4.4](SIM_Architecture_Refactor_Design.md#44-目标模块建议目录与部署边界) |
| F13 | 旧 SIM 头文件被仓内其他模块使用：Core 用 `sim_manager.h`、`esim_manager.h`、`sim_constant.h`；搜网用 `mcc_pool.h`、`sim_constant.h`；innerkits 用 `operator_config_hisysevent_enum.h`、`sim_state_handle.h`。骨架没有对外头文件的位置 | 见[附录 11.5](#115-仓内其他模块对旧-sim-头文件的引用) |
| F14 | `topology/ports/` 已有 8 个端口文件，只覆盖号码设置、号码查询、激活、发现 3～4 个用例族 | [`topology/ports/`](../services/multisim/topology/ports/) |

### 2.2 八个一级目录逐项评价

| 目录 | 性质 | 是否有独立价值 | 问题与风险 |
| --- | --- | --- | --- |
| `topology/` | 业务模块（T） | 有：拥有账户/绑定写权 | 内部四层按“文件类型”划分；`ports/` 会膨胀成与用例脱节的长列表（F14，〔推断〕） |
| `assignment/` | 业务模块（A） | 有：拥有角色/Primary 写权 | 同上；与 T 双向依赖（F7） |
| `application/` | 分层名 | 弱 | 与模块内 `application/` 重名；`orchestration` 实际是跨 T/A/R/C 的共享词汇（F6）；`card_access` 是 R 的短请求入口 |
| `runtime/` | 支撑子系统（R） | 强：约 75% 代码，拥有 WU-06/08 事实 | 没有容纳生产代码的能力目录；`CardRuntime` 正在变成新的 SimManager（F5） |
| `compatibility/` | 入站适配（C） | 有：约 155 个旧方法的语义位置 | 直接依赖具体用例类；Facade 会持续膨胀（F10） |
| `infrastructure/` | 技术层 | 中 | 已出现跨 Owner 混装（F9），容易变成杂物箱 |
| `assembly/` | 组合根（H） | 有：唯一可见全部具体类型的地方 | 合理 |
| `model/` | 共享内核 | 有，但名称与内容不一致 | 请求关联类型放在别处；“model”与 domain 语义重叠 |

### 2.3 对评审问题的直接回答

1. **当前一级目录是否混合了不同抽象层次？** 是，见 2.2。
2. **哪些体现业务模块、分层或技术机制？**
   - 业务模块：T（topology）、A（assignment）。
   - 支撑子系统：R（runtime）。
   - 边界机制：C（compatibility）、H（assembly）。
   - 分层名：application、infrastructure。
   - 共享内核：model。
3. **哪些目录有明确的独立价值？** topology、assignment、runtime、compatibility、assembly。
4. **哪些可能是不必要的分层或过度抽象？** 顶层 `application/`；T/A 内部按文件类型划分的 `ports/`，以及 api/application/domain/ports 全量四层的统一套用。
5. **哪些公共目录可能演变为耦合中心？** `request_context.h`（含业务枚举 `RequestOwner`）、`CardRuntime`、`LegacySimFacade`、`infrastructure/`。
6. **是否便于快速定位和修改？** T/A 尚可，但一次修改要在四个类型目录之间跳转；R 的生产能力无法按目录定位。
7. **完整迁入后是否仍然合理？** 不合理。R 约 25K 行无处安放；C 需承载约 155 个方法；缺少对仓内其他模块的导出头位置（F13）。

### 2.4 合理设计、冗余抽象、职责交叉与耦合风险汇总

| 类别 | 内容 |
| --- | --- |
| 合理设计 | T/A 分别为独立业务模块并拥有唯一写入口；R 单列而不是被归为 Adapter；C 单独承载旧契约语义；H 作为唯一组合根；共享身份类型集中定义 |
| 冗余抽象 | 顶层 `application/`；T/A 内部统一四层；每个用例单独一个端口文件且集中放在 `ports/` |
| 职责交叉 | `card_access` 短请求（R 入口）放在 Application；`ProfilePreferencePort` 定义位置与消费者不一致；`legacy_output_adapter` 混合 T/A/C |
| 潜在耦合 | `RequestOwner` 业务枚举在共享头文件中；`CardRuntime` 汇聚全部 R 状态；`LegacySimFacade` 构造注入所有用例；T/A 双向依赖；没有 GN target 和模块 namespace，任何规则都无法在编译期强制 |

---

## 3. Runtime 与公共机制的归属

| 对象 | 判断 | 理由 |
| --- | --- | --- |
| `runtime/` | **作为独立一级模块，不并入 Infrastructure** | R 拥有自身状态（卡状态、Records 缓存、会话、资源屏障）和 WU-06/08 事实，包含协议规则，并占约 75% 代码；它不是无状态 Adapter。并入 Infrastructure 会把大部分代码藏在“技术层”标签下，并诱使 T/A 逻辑渗入。R 内部按卡能力组织，不套 DDD 层，与目标稿“R 的生产协议与解析不机械改名为纯领域模型”一致 |
| `application/orchestration/` | **拆分**：值类型和通用容器移入 `common/`；完成路由作为不透明令牌放入 `common/`，解释权留在 C | `RequestId/StepRef/EffectKnowledge` 被 T、A、R、C 共同使用（F6），实质是跨模块词汇，而不是 Application 层私有机制 |
| `RequestOwner` 枚举 | **删除封闭枚举**，改为各模块自行登记 owner 标识 | 每新增一个用例都要修改共享头文件，是隐性的中央注册表 |
| `application/card_access/` | **移入 R 的能力目录**（PIN 进 `state/`，EF 进 `records/`，本地 Profile 进 `euicc/`），经 `runtime/api` 暴露 | 约六成旧 API（PIN、电话簿、STK、短信、运营商、EF、eSIM 原语）不涉及 T/A 事实；多一层 Application 只增加跳转。需要 T/A 接纳的结果经 T/A 的 `api/` 入站入口提交 |
| `model/` | **合并改名为 `common/`**，并设置白名单 | 只放跨至少 2 个模块、且不含业务决策的值类型 |
| `compatibility/` | **保留** | 是 ISimManager/IEsimManager 语义的唯一落点；只依赖各模块 `api/` |
| `infrastructure/` | **收窄** | 只放跨 Owner 共享的技术包装；Owner 专属 adapter 移到对应模块的 `adapters/` |
| `assembly/` | **保留** | 唯一允许看见全部具体类型的地方 |

**原则：** 不因为某段代码被多个模块使用，就自动创建共享模块。只有满足下列全部条件，才放入 `common/`：

- 被至少 2 个模块使用；
- 不含业务决策；
- 属于稳定的值类型或通用容器。

其余情况由定义它的 Owner 在自己的 `api/` 中发布。

---

## 4. 候选方案

四个方案共用以下解耦机制，比较的只是目录维度：

- 每个模块一个 GN `source_set`，并声明 `deps` 与 `visibility`；
- 每个模块独立 namespace；
- 一个 include 规则检查脚本。

### 4.1 方案 A：业务上下文优先，内部统一分层（现骨架的规整化）

```text
multisim/
├── shared/
├── topology/{api,application,domain,ports,infrastructure}/
├── assignment/{api,application,domain,ports,infrastructure}/
├── runtime/{api,application,domain,ports,infrastructure}/   ← 强行套层
├── compatibility/
└── assembly/
```

| 项目 | 说明 |
| --- | --- |
| 一级目录 | 业务上下文 |
| 二级目录 | 架构分层 |
| 三级目录 | 通常不需要 |
| T/A 边界保护 | 路径可见；跨模块只能用 `api/` |
| Runtime 组织 | 被迫套用五层，协议、解析代码归类困难 |
| 公共代码控制 | `shared/` 白名单 |
| 业务定位 | 先找上下文，再在 4～5 个类型目录之间跳转 |
| 依赖规则 | 跨模块只能 `api/`；`domain` 不依赖平台 |
| 优点 | T/A 边界在路径上可见；与现骨架接近，调整成本低 |
| 缺点 | R 的 EF/APDU/协议既非纯 Domain 也非 Adapter，硬套层会产生空层或错误归类；一个用例分散在 4～5 个类型目录；`ports/` 规模扩大后难以定位；F11 那种在 domain/application 间反复搬文件的情况会持续 |

### 4.2 方案 B：架构分层优先

```text
multisim/
├── domain/{common,topology,assignment}/
├── application/{orchestration,topology,assignment,card_access}/
├── runtime/{state,records,euicc,...}/         ← 不是“层”，只能并列
├── infrastructure/{store,parameter,events,vendor}/
├── compatibility/
└── assembly/
```

| 项目 | 说明 |
| --- | --- |
| 一级目录 | 软件分层 |
| 二级目录 | 业务上下文或技术类别 |
| 三级目录 | 用例族较大时需要 |
| T/A 边界保护 | 只能靠二级目录名；T/A 同在一个父目录下，互相引用更容易 |
| Runtime 组织 | 不属于任何层，只能并列，使分层图失真 |
| 公共代码控制 | `domain/common` 与 `application/orchestration` 两处共享区 |
| 业务定位 | 需先理解分层理论，再到 3～4 棵顶层树中分别查找 |
| 依赖规则 | 路径即层，“`domain/**` 不依赖平台”最容易检查 |
| 优点 | 设计者熟悉；层规则直观 |
| 缺点 | 一个 T 功能分散在 3～4 棵顶层树；T/A 边界弱；占 75% 代码的 R 不在分层体系中；业务开发者认知成本高 |

### 4.3 方案 C：按 SIM 功能平铺，目录与模块边界完全解耦

```text
multisim/
├── account/  number/  activation/  default_card/  primary/  label/
├── pin/  records/  phonebook/  stk/  sms_storage/  carrier/  esim/  radio/
├── legacy_api/  storage/  assembly/  common/
```

| 项目 | 说明 |
| --- | --- |
| 一级目录 | SIM 功能（约 18 个） |
| 二级目录 | 按需 |
| 三级目录 | 一般不需要 |
| T/A 边界保护 | 只在 GN target 与 namespace 中表达，路径上不可见 |
| Runtime 组织 | 天然按能力划分 |
| 公共代码控制 | 难：AccountRegistry 被 number、activation、account 共享，必然落入 `account/` 或 `common/` 形成新中心 |
| 业务定位 | 熟悉 SIM 的开发人员最快 |
| 依赖规则 | 功能 target 数量多，依赖网状化 |
| 优点 | 定位最快，最贴近业务语言 |
| 缺点 | 状态 Owner 与唯一写入口在路径上不可见，违反“保护状态所有权”约束；一级目录过多 |

### 4.4 方案 D（推荐）：Owner 优先 + 用例族内聚 + 薄分层

| 项目 | 说明 |
| --- | --- |
| 一级目录 | 状态 Owner（topology、assignment、runtime、compatibility、assembly），外加三个边界明确的支撑目录（common、infrastructure、include） |
| 二级目录 | T/A：`api/`、`domain/`、`adapters/` 三个目录承载可检查的分层规则，其余按用例族建目录。R：按卡能力建目录，不套 DDD 层 |
| 三级目录 | 只在有独立构建条件（euicc）或纯算法子集（`records/codec`）时建立 |
| T/A 边界保护 | 一级目录即 Owner；跨模块只能 include 对方 `api/`；GN target 强制 |
| Runtime 组织 | `state/records/phonebook/sms_storage/stk/carrier/radio/euicc` 加治理目录 `session/resource`；各能力目录分别实现对应契约，取代万能 `CardRuntime` |
| 公共代码控制 | `common/` 白名单；owner 专属 adapter 回到 owner；output adapter 按 Owner 拆分 |
| 业务定位 | “改号码”进入 `topology/profile/`；“改 Primary”进入 `assignment/primary/`；“改 PIN”进入 `runtime/state/` |
| 依赖规则 | 见 [9.3](#93-依赖方向与约束) |
| 优点 | 用例、计划与私有端口集中；跨模块规则只有一条；R 有真实组织方式；状态 Owner 在路径上可见 |
| 缺点 | T/A 与 R 采用两种内部组织方式，需要一页 README 说明；owner 专属 adapter 放进业务模块后，必须靠 GN target 禁止用例代码直接依赖平台 |

---

## 5. 业务场景验证

### 5.1 推荐方案 D 的代码落点

| 场景 | 业务规则 | 用例执行 | 设备操作 | 兼容/平台 | 修改需求需要理解的代码 |
| --- | --- | --- | --- | --- | --- |
| 1. SIM 插入、发现与激活 | `topology/domain`（AccountRegistry、BindingIndex） | `topology/discovery/`；`topology/activation/`；A 的建行角色与准入在 `assignment/account_roles/` | `runtime/state/` 卡状态；`runtime/records/` 加载；`runtime/radio/` SetActiveSim | `topology/adapters/` 写库；`infrastructure/` 厂商 Hook；`compatibility/` 旧返回 | 改发现规则：T 的 discovery 与 domain；改激活时序：T 的 activation 与 R 的 radio |
| 2. 默认 Data 设置与 Primary 切换 | `assignment/domain`（AssignmentTransition、冲突集合） | `assignment/roles/`；`assignment/primary/`（计划、重试、Data 联动）；槽 3 换路调用 `topology/api` 的映射入口 | `runtime/radio/`（三阶段协议、专用 RIL） | `assignment/adapters/`（参数、默认卡 URI、广播） | 主体集中在 `assignment/primary/` |
| 3. SIM 号码设置与查询 | `topology/domain`（字段版本） | `topology/profile/`（设置用例、步骤计划、查询、独立回写） | `runtime/records/`（MSISDN EF） | `topology/adapters/`；`compatibility/` 负责完成与通知 | 主体在 `topology/profile/` |
| 4. eSIM Profile 切换及 Port 管理 | 绑定接纳在 `topology/domain` | `runtime/euicc/` 负责具名短请求和期望登记；结果进入 `topology/api` 观察入口、`assignment/api` 偏好入口 | `runtime/euicc/`；`runtime/resource/` 资源屏障；`runtime/session/` 期望转移 | `compatibility/` 负责外层 int 与卡 enum 双结果 | 主体在 `runtime/euicc/`；若 G4 引入 Port 感知绑定，变化在 `topology/domain` |
| 5. 双卡扩展至三卡、四卡 | `common/slot_catalog` 与具名策略；`assignment/domain` 冲突域 | 不新增目录 | 按 SlotCatalog 装配每槽 R 对象 | `assembly/` 装配；`compatibility/` 槽 3 旧投影；`infrastructure/` 按槽参数 | 修改集中在目录配置与具名策略，可扫描特殊槽号字面量 |

### 5.2 各方案对比

| 场景 | 现状 | A 上下文优先 | B 分层优先 | C 功能平铺 | D 推荐 |
| --- | --- | --- | --- | --- | --- |
| 1. 插卡/发现/激活 | T 四个子目录、A、`runtime/card_runtime`、`infrastructure`、`application/orchestration`、C | T/A/R/C，各自内部跨 4～5 层 | domain、application、runtime、infrastructure、C 五棵树 | account、activation、records、esim、legacy_api；写权不可见 | T 的 discovery/activation 加 A 准入、R 能力目录、C |
| 2. Default Data / Primary | 约 6 个一级、10 个以上子目录 | 约 4 个一级、9 个子目录 | 约 5 个一级 | primary、default_card、radio、legacy_api | 4 个一级，主体在 `assignment/primary/` |
| 3. 号码设置/查询 | T 的 application/ports/domain/api、R、I、C | T 内 4 层加 R/I/C | `application/topology`、`domain/topology`、`infrastructure/store`、`runtime/records`、C | number、records、storage | 主体在 `topology/profile/`，加 `runtime/records` |
| 4. eSIM Profile/Port | `application/card_access`、`runtime/card_runtime`、`assignment/ports`、C | `runtime` 内被迫分层 | `application/card_access`、`runtime/euicc`、`domain/topology` | esim、account | 主体在 `runtime/euicc/` |
| 5. 2→3→4 卡 | model、assembly、T、A、R | 同左 | domain、application、runtime | 无统一位置，易散落 | `common/slot_catalog` 加具名策略 |

### 5.3 必要拆分与跳转成本

- **必要拆分**：业务事实写入（T/A）与设备协议（R）分开；T 与 A 分开；兼容语义（C）与业务分开。场景 1 中跨 T/A/R 的跳转反映真实业务协作，不是目录造成的成本。
- **只增加跳转成本的拆分**：按文件类型划分的 `ports/`、`api/`、`application/`；顶层 `application/`；`infrastructure` 中混装多个 Owner 的出口。
- **过度拆散的风险**：方案 B 对 T 功能风险最高；方案 C 对状态所有权风险最高；方案 D 把用例、计划和私有端口放在同一用例族目录中，风险最低。

---

## 6. AI 辅助编程友好度

| 关注点 | 现状 | 方案 D |
| --- | --- | --- |
| 单任务读取范围 | 一个用例需读 api、application、domain、ports 四个目录及多个跨模块端口 | 一个用例族目录 + 本模块 `domain/` + 对方 `api/` |
| 依据目录定位 | T/A 可以；R 无法定位，需从 `CardRuntime` 注释反推 | Owner 与能力名称直接对应 SIM 业务术语 |
| 业务上下文集中度 | 中 | 高 |
| 跨目录跟踪抽象接口 | 每个用例一个端口文件，集中放在 `ports/` | 端口跟随使用者，只有跨模块契约放入 `api/` |
| 用少量规则限制错误修改 | 没有 GN target，规则不可执行 | 四条可检查规则：只能 include 对方 `api/`；`domain` 无平台头；只有 `adapters` 接平台；事实只能经 `*_service` 写入 |
| 按业务特性拆分独立开发任务 | 难：一个用例横跨多个共享文件 | 用例族目录天然适合作为任务单元 |

**说明：** 目录层级越多、分层越严格，并不意味着 AI 编程效果越好。决定性因素是：

- 单个任务需要读取的上下文是否集中；
- 越界修改能否被机械规则拦截。

---

## 7. 生产能力完整性检查

| 旧代码（行数为约值） | 推荐归属 | 说明与风险 |
| --- | --- | --- |
| `sim_manager`（1.6K） | `compatibility/`（ISimManager 实现）+ `assembly/`（Init 与发布） | 现骨架只有 C 的局部设计 |
| `multi_sim_controller`（2.3K） | 拆到 `topology/{discovery,activation,profile}`、`assignment/{roles,primary,account_roles}`、`runtime/radio`（SetActiveSimToRil） | 必须按 WU 拆分，不能整类迁移 |
| `multi_sim_monitor`、`multi_sim_helper`、`card_file_detect_manager` | `topology/discovery`（重载、探测接纳）；Observer 登记归 `compatibility/`；R 探测输入归 `runtime/state` | 现骨架没有 SIM-032/033 订阅登记的位置 |
| `sim_cache_sync_manager`（SIM-062，未装配） | 不迁移，或另行裁决 | 不因源码存在就启用 |
| `sim_state_manager/handle/tracker`、`icc_state` | `runtime/state/`（含 PIN/PUK/认证） | 现骨架缺 |
| `sim_file_manager`、`icc_file`、各卡型 File/Controller、`sim_file_init/parse`、`plmn_file`、`voice_mail_constants`、`tag_service`、`mcc_pool`、`sim_number_decode`、`sim_char_decode` | `runtime/records/`，纯解码与表格进入 `records/codec/` | 现骨架缺；`usim_function_handle` 的归属〔待验证〕 |
| `icc_dialling_numbers_*`、`usim_dialling_numbers_service` | `runtime/phonebook/` | 现骨架缺 |
| `sim_sms_*` | `runtime/sms_storage/` | 现骨架缺 |
| `stk_*` | `runtime/stk/` | 现骨架缺 |
| `sim_account_manager`、`operator_*`、`icc_operator_*` | `runtime/carrier/`；HiSysEvent 可归 `infrastructure/` | 不能按类名中的“Account”归入 T |
| `radio_protocol_controller` | `runtime/radio/` | 现骨架只在 CardRuntime 中借用 |
| `esim_manager/file/controller`、`start_osu_result_callback` | `runtime/euicc/`，作为独立条件编译 target | 现骨架没有条件构建边界 |
| `sim_rdb_helper`、`telephony_data_helper`、`pdp_profile_rdb_helper` | `infrastructure/` | 共享技术包装 |
| `sim_utils`、`sim_constant`、`sim_data_type` | 槽相关部分归 `common/slot_catalog`；仓内对外常量归 `include/`；其余归 `runtime/records/codec` | 现骨架缺对外导出位置（F13） |
| `sim_account_callback_death_recipient` | `compatibility/` | 回调登记与死亡清理属于 C 登记职责 |
| Core 的 `core_service_sim.cpp` | 保留在 `services/core`（IPC 侧） | 与目标稿一致 |

**完整性结论：**

- T/A 的归属基本完整。
- 主要缺口在三处：R 的能力目录、C 的规模化组织、对仓内其他模块的导出头位置。
- 不合理归属有两处：`card_access` 放在顶层 Application；owner 专属 adapter 混在 `infrastructure`。

---

## 8. 方案横向比较

| 维度 | 现状 | A 上下文优先 | B 分层优先 | C 功能平铺 | D 推荐 |
| --- | --- | --- | --- | --- | --- |
| 代码可理解性 | 中（混层） | 良 | 中 | 优 | 良（需一页 README） |
| 代码定位效率 | 中 | 中 | 差 | 优 | 优 |
| 业务内聚性 | 中 | 中 | 差 | 良 | 优 |
| 架构层次清晰度 | 中 | 良 | 优 | 差 | 良（只表达可检查的层） |
| 模块解耦程度\* | 差（无 target） | 取决于机制 | 取决于机制 | 取决于机制 | 取决于机制 |
| 目录组织复杂度 | 中 | 偏高 | 中 | 一级过多 | 中 |
| 功能完整性 | 差（R 缺位） | 中（R 被扭曲） | 中 | 良 | 优 |
| 长期演进（多卡、eSIM） | 中 | 中 | 中 | 中 | 良 |
| AI 编程友好度 | 中 | 中 | 差 | 中（写权易误改） | 优 |
| 调整成本 | — | 低 | 高 | 高 | 中（约 73 个文件的搬移，现在最便宜） |

\* **三类收益必须分开看。**

- **目录组织收益**：表现为定位效率和业务内聚。方案 D 最高，方案 C 次之。
- **模块解耦收益**：只来自 GN target、namespace、`api/` 规则和唯一写入口，与选用哪个目录方案无关。现状为零，因为根本没有 target。
- **分层收益**：来自 `domain` 与 `adapters` 的编译隔离。方案 B 在路径上最直观，方案 D 通过 target 实现同等约束。

**目录数量减少本身不是解耦效果。**

**适用条件：**

| 方案 | 适用条件 |
| --- | --- |
| A | 只求最小改动，并能接受 R 长期没有组织方案 |
| B | 团队坚持路径即分层，且接受 R 作为非层的并列目录、业务内聚受损 |
| C | 不需要在路径上体现状态所有权（与本次约束冲突） |
| D | 以状态 Owner 为边界、以开发定位效率为优先；若 G4 Port 感知绑定导致 T 拆成两个上下文，只需增加一个一级模块 |

---

## 9. 最终推荐目录结构

### 9.1 完整核心目录树

```text
services/multisim/
├── BUILD.gn              # 每模块一个 source_set + deps/visibility = 可执行的依赖规则
├── README.md             # 一页：模块地图、依赖规则、“改X去哪里”表
├── include/              # 只放 multisim 外（core/搜网/innerkits）需要的头：工厂入口、常量等
├── common/               # 原 model/ + 请求关联值类型（白名单）
│   ├── sim_identity.h    # SubscriptionId/SlotId/Target/SessionRef/BindingRef
│   ├── slot_catalog.h/.cpp
│   ├── request_ref.h     # RequestId/StepRef/EffectKnowledge/终态；不列业务用例
│   └── operation_store.h # 原 RuntimeOperationStore（进程内，非持久）
├── topology/             # T  ns MultiSim::Topology
│   ├── api/              # 其他模块唯一可 include：命令/视图/入口类 + 要求他方实现的契约
│   ├── domain/           # AccountRegistry、BindingIndex 等纯规则（target 无平台依赖）
│   ├── discovery/        # 发现、建账、重载、观察 ledger      SIM-020~022、035、056
│   ├── activation/       # 激活用例与进度                     SIM-024
│   ├── profile/          # 名称/号码/标签/IMS、查询回写       SIM-023、031.a/b、045.a
│   ├── mapping/          # 槽3映射子请求                      SIM-031.c
│   ├── adapters/         # T 字段写计划 → DataShare
│   └── topology_service.h/.cpp   # 唯一事实写入口
├── assignment/           # A  ns MultiSim::Assignment
│   ├── api/
│   ├── domain/           # RoleSelection、AssignmentTransition、冲突集合
│   ├── roles/            # 默认 Voice/SMS/Data                SIM-025~027
│   ├── primary/          # Primary/Radio/DSDS、计划、重试、Data 联动  SIM-028~030
│   ├── account_roles/    # 建行角色片段、激活准入（实现 T 的契约）
│   ├── adapters/         # 参数/默认卡 URI/广播
│   └── assignment_service.h/.cpp
├── runtime/              # R  ns MultiSim::Runtime（按卡能力组织，不套 DDD 层）
│   ├── api/              # C 直达的卡操作入口与只读视图
│   ├── slot_runtime.h/.cpp  # 每槽对象组装与路由（替代万能 CardRuntime）
│   ├── session/          # 会话/代次/期望转移
│   ├── resource/         # 共享设备资源屏障
│   ├── state/            # 卡状态/锁/PIN/PUK/认证            SIM-005~011
│   ├── records/          # EF/File/Controller/Refresh/LoadSession/MSISDN  SIM-012~019
│   │   └── codec/        # 纯解码与表格（无 I/O，可独立测试）
│   ├── phonebook/        # SIM-036~040（电话簿部分）
│   ├── sms_storage/      # SIM-036~040（卡短信存储部分）
│   ├── stk/              # SIM-036~040（STK 部分）
│   ├── carrier/          # 运营商配置/匹配/权限/APN            SIM-041~044、045.b
│   ├── radio/            # 三阶段协议、专用 Primary RIL、激活 RIL
│   └── euicc/            # 本地 eUICC/Profile/Port、短请求     SIM-047~054（条件 target）
├── compatibility/        # C：ISimManager/IEsimManager 实现，完成路由、旧投影、Observer 登记
│                         #    单类多 .cpp：legacy_sim_facade_{account,card,esim}.cpp，不再建子目录
├── infrastructure/       # I：只放跨 Owner 共享的技术包装（RdbHelper/参数/CES/Hook/HiSysEvent）
└── assembly/             # H：工厂、注入、发布次序
```

### 9.2 一级、二级目录职责与三级目录规则

| 一级目录 | 职责 | 二级目录原则 |
| --- | --- | --- |
| `include/` | 仓内其他模块可见的头文件 | 平铺 |
| `common/` | 跨模块值类型与通用容器 | 平铺；白名单管理 |
| `topology/` | T：账户、绑定、资料、激活；唯一写入口 `topology_service` | `api/`、`domain/`、`adapters/` 加用例族目录 |
| `assignment/` | A：角色、Primary、Radio 分配与切换；唯一写入口 `assignment_service` | 同上 |
| `runtime/` | R：卡运行支撑与设备执行 | `api/`、治理目录加卡能力目录 |
| `compatibility/` | C：旧接口、完成路由、旧投影、订阅登记 | 不设子目录，用多个 `.cpp` 拆分大类 |
| `infrastructure/` | I：共享技术包装 | 平铺；规模扩大后按技术类别拆分 |
| `assembly/` | H：组合根与发布次序 | 平铺 |

**三级目录规则：**

- T/A 默认只有两级。
- 三级目录只在两种情况下建立：
  - 有独立构建条件，例如 euicc；
  - 有纯算法子集，例如 `records/codec`。
- 单个能力目录超过约 25 个文件时，先拆 `.cpp` 文件，再考虑拆子目录。

### 9.3 依赖方向与约束

以下规则由 GN target 和一个检查脚本共同执行。

1. **跨模块只能 include 对方 `api/` 和 `common/`。** `assembly` 例外，它是唯一可以看见全部具体类型的模块。
2. **模块依赖必须是 DAG，`common` 在最底层：**
   - 两个 `api/` 之间：`assignment/api` 位于 `topology/api` 之下，T 的 api 可以引用 A 的 api，反过来不行。
   - T/A 核心只依赖 `common` 和两个 `api/`。
   - `runtime` 依赖 T/A 的 `api/`，用于实现契约和调用入站入口。
   - `compatibility` 依赖三个 `api/`。
   - `infrastructure` 只被 `*/adapters` 和 `runtime` 依赖。
   - T/A 核心**永不依赖** runtime、compatibility、infrastructure、assembly。
3. **`*/domain/` 的 target 没有 `external_deps`。** 这样平台、RIL、DataShare、FFRT、IPC 头文件在编译层面无法引入。
4. **T/A 中只有 `adapters/` 可以依赖 `infrastructure` 和平台库。**
5. **T/A 事实只能经 `*_service` 写入。** `domain` 的可变方法只对 service 可见，并配合字段赋值扫描检查。
6. **`common/` 白名单。** 新增文件必须被至少 2 个模块使用，且不含业务决策；禁止出现列举业务用例的枚举。
7. **`api/` 中放入口类声明即可。** 不要求每个用例一个虚接口，避免为解耦而过度抽象。

```text
                 common
                   ▲
             assignment/api
                   ▲
              topology/api
           ▲       ▲        ▲
 topology 核心  assignment 核心   runtime ──▶ infrastructure
           ▲       ▲        ▲          ▲
           └── compatibility ┘     */adapters
                   ▲
               assembly（可见全部）
```

图中箭头含义：被指向的是被依赖方（源码 include 方向）。运行时调用方向可以与之相反，例如 R 回调 T/A 的入站入口。

### 9.4 关键业务功能的归属

| 业务功能 | 主要位置 |
| --- | --- |
| 卡发现、建账、账户重载 | `topology/discovery/` |
| 激活（普通、force、卫星） | `topology/activation/`；准入在 `assignment/account_roles/` |
| 名称、号码、标签、IMS 偏好 | `topology/profile/` |
| 槽 3 映射子请求 | `topology/mapping/` |
| 默认 Voice/SMS/Data | `assignment/roles/` |
| Primary/Radio/DSDS 切换、业务重试 | `assignment/primary/` |
| PIN/PUK/认证 | `runtime/state/` |
| EF、Records、MSISDN、Refresh | `runtime/records/` |
| 电话簿、卡短信、STK | `runtime/phonebook/`、`runtime/sms_storage/`、`runtime/stk/` |
| 运营商配置/APN/权限 | `runtime/carrier/` |
| 本地 eUICC、Profile、Port | `runtime/euicc/` |
| 旧 API 签名、错误优先级、完成与通知格式 | `compatibility/` |
| 工厂、注入、发布次序 | `assembly/` |

### 9.5 当前八个一级目录的处理建议

| 目录 | 建议 |
| --- | --- |
| `topology/`、`assignment/` | **保留**。解散 `ports/`：对外契约放入 `api/`，私有端口跟随用例；`application/` 改为用例族目录；新增 `adapters/` |
| `application/` | **删除**。orchestration 的值类型和容器移入 `common/`；`card_access` 的 PIN、EF、Profile 短请求移入 `runtime/{state,records,euicc}`，经 `runtime/api` 暴露；`ProfilePreferencePort` 改为 `assignment/api` 的入站入口 |
| `runtime/` | **保留并扩展**。按能力组织；拆分 `CardRuntime`，由各能力目录分别实现对应契约 |
| `compatibility/` | **保留**。只依赖各模块 `api/`；Facade 按特性拆成多个 `.cpp` |
| `infrastructure/` | **收窄**。owner 专属 adapter 移到对应模块的 `adapters/`；`legacy_output_adapter` 按 Owner 拆分 |
| `assembly/` | **保留** |
| `model/` | **合并改名为 `common/`** |
| 新增 | `include/`、`BUILD.gn`、`README.md` |

### 9.6 相比现有设计的实质改进

1. 约 75% 的生产代码（R）有了可定位的归属，`CardRuntime` 不会成为新的万能 Manager。
2. 一级目录语义统一，消除 `application/` 的双重含义。
3. 用例代码、计划和私有端口集中在一个目录中，跳转明显减少。
4. 边界从约定变为编译期可执行：GN target、namespace、`api/` 规则。
5. 集中点受控：common 白名单；删除 `RequestOwner`；按 Owner 拆分 output adapter；Facade 按特性拆文件。
6. 补齐对仓内其他模块的导出头位置（F13）。
7. 只在有机械规则的地方用目录表达分层，避免文件反复在 domain 与 application 之间搬移（F11）。

---

## 10. 尚需架构师裁决的问题

| # | 问题 | 默认建议 | 影响范围 |
| --- | --- | --- | --- |
| D1 | 短请求（PIN、EF、本地 Profile）归 R 入口，还是保留独立的应用模块 | 归 R | `runtime/api`、`compatibility` |
| D2 | owner 专属 adapter 放在模块内 `adapters/`，还是放在 `infrastructure/<owner>/` | 放在模块内 | T/A、I |
| D3 | `common/` 的命名，以及 `RequestOwner` 的替代方式 | 名称待定；改为各模块自行登记 owner 标识 | 全部模块 |
| D4 | `mcc_pool`、`sim_constant` 被搜网使用：放 `multisim/include`，还是下沉到 `utils/` | 下沉，避免搜网依赖 SIM | 搜网、Core |
| D5 | `innerkits/satellite/satellite_service_client.h` include 服务内部头 `sim_state_handle.h` | 需核实外部契约〔待验证〕 | innerkits、卫星 |
| D6 | `.h`/`.cpp` 同目录，还是沿用 OpenHarmony 惯例 include/src 分开 | 同目录；对外头由 `include/` 承担 | 全部模块 |
| D7 | T/A 的 api 单向依赖（`assignment/api` 不依赖 `topology/api`）是否作为长期规则 | 作为长期规则 | T/A |
| D8 | GN 粒度，以及是否形成私有 SO | 作为部署决策单独讨论 | 构建、部署 |
| D9 | 统一目录主张后，同步修订目标稿 §5.1.5、§6.4.1 与重构设计中互相冲突的路径（F12） | 以最终裁决为准统一修订 | 文档 |

---

## 11. 附录：证据与统计方法

### 11.1 统计方法

- 旧代码：统计 `services/sim/src` 与 `services/sim/include` 的全部文件行数（不含 `test/`），并按文件名人工归入功能域。混合职责文件（如 `multi_sim_controller`）整体计入一个类别，因此比例是近似值。
- 骨架代码：统计 `services/multisim/` 全部文件。
- 依赖关系：解析 `#include "..."`，并按一级目录、或 T/A/application/runtime 的二级目录聚合，只统计骨架内部的引用。
- 外部引用：在 `services/core`、`services/network_search`、`frameworks`、`interfaces`、`utils`、`common`、`services/telephony_ext_wrapper` 中检索对 `services/sim/include` 头文件名的引用。

### 11.2 旧 services/sim 按功能域统计

| 功能域 | 行数 | 占比 |
| --- | --- | --- |
| EF/记录/codec | 9,682 | 28.1% |
| eSIM/eUICC | 5,086 | 14.7% |
| 账户与角色控制器（T/A 混合） | 4,556 | 13.2% |
| 运营商配置/权限 | 3,047 | 8.8% |
| 卡状态/安全 | 2,886 | 8.4% |
| 电话簿 | 2,777 | 8.1% |
| 入口/装配 | 1,944 | 5.6% |
| 公共工具/常量 | 1,189 | 3.4% |
| 存储适配 | 1,112 | 3.2% |
| STK | 1,055 | 3.1% |
| Radio 协议（Primary） | 616 | 1.8% |
| SIM 短信存储 | 543 | 1.6% |
| **合计** | **34,493** | 100% |

旧模块共 56 个 `.cpp` 与 62 个 `.h`，采用平铺的 `include/` 加 `src/` 结构，全部编入 `tel_core_service` 共享库。

### 11.3 骨架按一级目录统计

| 目录 | 文件数 | 行数 |
| --- | --- | --- |
| topology | 27 | 1,962 |
| assignment | 14 | 816 |
| runtime | 9 | 915 |
| compatibility | 7 | 344 |
| assembly | 4 | 301 |
| application | 4 | 271 |
| infrastructure | 5 | 216 |
| model | 3 | 159 |
| **合计** | **73** | **4,984** |

骨架根层级无文件，全部文件均位于上述八个一级目录中。

### 11.4 骨架内部跨目录 include 依赖（节选）

| 使用方 | 被依赖方 | 次数 | 评价 |
| --- | --- | --- | --- |
| `topology/ports` | `assignment/api` | 3 | 与下行共同构成 T↔A 双向依赖 |
| `assignment/application` | `topology/ports` | 2 | 同上 |
| `topology/application` | `assignment/ports` | 1 | 同上 |
| `runtime` | `topology/ports` | 6 | R 实现 T 契约，方向合理，但全部集中在 `CardRuntime` |
| `runtime` | `assignment/ports` | 2 | 同上 |
| `runtime/session`、`runtime/resource` | `application/orchestration` | 各 1 | 说明请求关联类型实为共享词汇 |
| `infrastructure` | `compatibility` | 1 | I 依赖 C，属于混装 |
| `compatibility` | `topology/application`、`assignment/application` | 3、1 | C 依赖具体用例类 |
| `application/card_access` | `assignment/ports` | 1 | 端口定义位置与消费者不一致 |

被引用次数最高的头文件：`application/orchestration/request_context.h`（12 次）、`topology/api/topology_requests.h`（8 次）、`model/sim_identity.h` 与 `assignment/api/assignment_requests.h`（各 6 次）。

### 11.5 仓内其他模块对旧 SIM 头文件的引用

| 引用方 | 被引用头文件 |
| --- | --- |
| `services/core/src/core_service.cpp` | `sim_constant.h`、`sim_manager.h`、`esim_manager.h` |
| `services/core/src/core_service_sim.cpp` | `sim_constant.h` |
| `services/core/include/core_service_hisysevent.h` | `operator_config_matchsim_info.h` |
| `services/network_search/include/operator_name.h` | `sim_constant.h` |
| `services/network_search/src/network_search_manager.cpp`、`network_search_handler.cpp` | `mcc_pool.h` |
| `interfaces/innerkits/include/i_operator_config_hisysevent.h` | `operator_config_hisysevent_enum.h` |
| `interfaces/innerkits/satellite/satellite_service_client.h` | `sim_state_handle.h` |

### 11.6 未执行的验证

本报告没有执行以下验证：

- OHOS 构建、生产链接、导出符号或 `NEEDED` 检查；
- 单元测试、fuzzer、设备或多卡场景验证；
- 对推荐目录的试迁移。

各项结论限于固定提交的静态证据。目录建议不代表业务批准或实现完成。
