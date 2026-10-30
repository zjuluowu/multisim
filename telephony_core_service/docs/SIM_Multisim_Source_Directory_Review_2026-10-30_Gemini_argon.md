# OpenHarmony 多 SIM 重构：面向长期演进的源码目录架构独立评审报告

- **评审对象**：`telephony_core_service` 下 `services/multisim/` 最终稳态源码目录架构（目标完全替代 `services/sim/`）
- **主设计基线**：`SIM-TGT-001` v1.2（Git Commit `3fd0f815991fe6d5f9ba807dbc9d034c8594cb83`）
- **源码证据基线**：主仓固定证据 Commit `6017003eff50c1bc8c5dfa72336a4bfe42e948b0`（F01～F31）、`67c1b318e4f165c1671b0daac8ede6e38d2117cc`（F32～F36）、`3fd0f815991fe6d5f9ba807dbc9d034c8594cb83`（F40～F46）及本地评审工作树 `2448b42e8976ffb75e2bfcd45b27c86eb1e3965d`；历史静态分析基线 `cd3bca9ca9b1b0c163ba05a3cef5bba5ae2ac038`
- **跨仓核对基线**：`telephony_data_storage`（`843a8be7`）、`telephony_ril_adapter`（`d039c7ce`）、`telephony_cellular_data`（`29d6fa9c`）、`telephony_sms_mms`（`2336a3c3`）、`telephony_call_manager`（`3cba6085`）
- **评审性质**：只读独立架构评审（严格区分已证实代码事实、已确认目标设计、待验证外部假设与未来变化推演）

---

## 1. 核心结论

当前 `services/multisim/` 目录结构**需要调整**。现有 8 个一级目录仅服务于早期未接入构建的目标骨架，在同一层级混杂了**业务限界上下文**（`topology`、`assignment`）、**水平技术分层**（`application`、`infrastructure`、`model`）与**运行/集成机制**（`runtime`、`compatibility`、`assembly`）三个不同维度。当原 `services/sim/` 全部生产功能迁入并彻底移除旧目录后，`runtime/` 将因承载状态机、多制式文件、电话簿、短信、STK、eUICC、运营商配置及无线协议等 $70\%$ 以上代码而严重失衡，`infrastructure/` 易演变为跨上下文耦合中心，`application/card_access/` 与 `runtime/` 的分离则拉长了简单卡操作的调用链。

推荐采用**方案 C（分层与模块边界正交架构）**：一级目录收敛为 `foundation/`、`topology/`、`assignment/`、`runtime/`、`compatibility/`、`assembly/` 6 个正交模块；将专属持久化 Adapter 内聚回 `topology/` 与 `assignment/`，将 `runtime/` 按“治理内核 + 6 大卡侧能力子域 + 短请求入口”结构化分包，并通过 GN `visibility` 与 C++ 命名空间强制锁定单向依赖与唯一事实写权。

---

## 2. 现有目录评价

### 2.1 评审输入基线与事实边界声明

1. **代码与文档事实核实**：
   - 既有规模基线表明：192 个 SIM 相关文件中有 187 个出现 `slot` 相关术语（覆盖率约 $97.4\%$）；`multi_sim_controller.cpp` 单文件达 2,126 行；非测试代码中直接检索 `SIM_SLOT_2`、`SLOT_3_INDEX`、`DUAL_SLOT_COUNT`、`VSIM_SLOT_ID` 等扩卡硬编码命中 21 个文件、49 行。
   - **代码事实（FACT / LEGACY）**：当前生产构建 `BUILD.gn` 仍将 `services/sim/`、`services/core/`、`services/tel_ril/` 同编入 `libtel_core_service.z.so`，独立生产共享库 `libtel_sim_service.z.so` 尚未物理拆出；`services/multisim/` 是 2026-10-29 建立的目标源码骨架（共 36 个头文件、15 个源文件），尚未接入构建或切换生产写权。
   - **目标设计裁决（PROPOSED）**：主设计 `SIM-TGT-001` v1.2 已裁定业务上下文仅保留 `T`（Topology）与 `A`（Assignment）两个唯一事实 Owner；`O`（Orchestration）无独立业务不变量，降级为纯 Application 编排机制，进行中状态分别归属 `T`（激活进行中标志 `isSetActiveSimInProgress`）和 `A`（主卡切换状态 `AssignmentTransition`）。
2. **分析限制声明（OPEN-EXTERNAL）**：
   - 独立 eSIM 服务 SA `66250` 的服务端完整实现、外部数据提供方（DataShare Provider）跨行原子事务能力、厂商闭源扩展库 `libtelephony_ext_service.z.so` 内部逻辑及目标设备多端口并发交错行为不在当前主仓静态源码中体现，本文明确标注其验证边界，不将推测当作事实。

### 2.2 现有 8 个一级目录职责与演进风险分析

| 现有目录 | 当前骨架中的主要元素与职责定位 | 职责独立性判断 | 全量生产迁入后的核心问题与演进风险 |
| :--- | :--- | :--- | :--- |
| **`topology/` (T)** | 含 `{api, application, domain, ports}`；承载 `TopologyService`、`SetShowNumberUseCase/Plan`、`ShowNumberQuery`、`ActivateSimUseCase`、`AccountRegistry`（双投影）、`BindingIndex`。 | **高（应保留并闭环）**。拥有账户身份、资料、位置绑定及激活进行中标志的唯一写权。 | 其专属存储实现 `account_store_adapter` 被剥离在顶层 `infrastructure/`，导致修改账户字段或持久化计划需跨两个一级目录同步。 |
| **`assignment/` (A)** | 含 `{api, application, domain, ports}`；承载 `AssignmentService`、`AssignmentAggregate`、`AssignmentTransition`、默认角色视图与参数/Radio/输出端口。 | **高（应保留并闭环）**。拥有默认角色偏好、有效选择、Primary 切换准入与唯一业务重试权。 | 专属存储与参数适配 `assignment_store_adapter` 外置在顶层 `infrastructure/`，割裂了角色与主卡上下文的修改局部性。 |
| **`application/`** | 含 `orchestration/`（`RequestContext`、`EffectKnowledge`、`RuntimeOperationStore`）与 `card_access/`（`local_card_requests.h`）。 | **低（应拆解重归属）**。与 `topology/application/`、`assignment/application/` 形成水平技术层与垂直业务域的命名冲突。 | `orchestration/` 仅含 3 个无业务主权的辅助原语，独占一级目录属过度分层；`card_access/` 处理无需 `simId` 的 PIN/EF/本地 Profile 短请求，本质是 `runtime/` 的受控入口，拆到顶层拉长了调用链。 |
| **`runtime/` (R)** | 含 `{session, records, resource}` 及 `card_runtime.h/.cpp`（`ProductionSlotBindings`），对成熟卡侧对象仅定义复用契约，尚未迁入实体代码。 | **高，但二级结构严重缺失**。承担卡协议、EF/Records、设备会话与共享资源屏障。 | 当 `services/sim/` 被完全替代时，`SimStateManager/Handle/Tracker`、`SimFileManager`、4 套 `IccFile` 与 `FileController`、电话簿、卡短信、STK、`EsimManager/File/Controller`、`SimAccountManager`、`OperatorConfig*`、`RadioProtocolController` 全量迁入，现有 3 个机制子目录无法容纳如此庞大的生产代码。 |
| **`compatibility/` (C)** | 含 `legacy_sim_facade.h/.cpp`、`legacy_completion.h/.cpp`，负责旧入口签名校验、队列桥接与完成出口转换。 | **中高（应整合收口）**。作为防腐层（ACL）隔离旧 `slotId` 契约与内部模型。 | 负责执行旧通知输出的 `legacy_output_adapter` 被拆到了 `infrastructure/`，且 `MultiSimMonitor` 的平台事件监听接入在目录上缺乏显式落点。 |
| **`infrastructure/` (I)** | 含 `account_store_adapter`、`assignment_store_adapter`、`legacy_output_adapter`。 | **低（高风险耦合中心）**。按传统水平分层汇总所有外部技术 Adapter。 | 混合了 T、A、C 三个不同职责域的适配器；未来迁入 `SimRdbHelper`、`PdpProfileRdbHelper`、`TelephonyExtWrapper` 后，将同时依赖所有模块的 `ports/`，极易退化为新的跨上下文耦合中心。 |
| **`assembly/` (H)** | 含 `sim_module_factory.h/.cpp`、`host_ports.h`、`manager_publication.h`，管理同 SO 借用附着、最终独立拥有工厂及销毁排空契约。 | **高（应保留）**。作为 Composition Root 隔离 `CoreService` 宿主与 SIM 内部实现。 | 边界清晰，只需严守“仅做对象图装配与生命周期排空，不承接业务分支路由”即可保持稳定。 |
| **`model/`** | 含 `sim_identity.h`、`slot_catalog.h/.cpp`，存放纯身份/位置值类型及 H 构建的只读 `SlotCatalog`。 | **中（应更名并收拢至基础内核）**。当前严格限制为纯值类型与只读硬件目录。 | 顶层命名为 `model/` 极易诱导开发者和 AI 将数据库 DTO（如 `SimRdbInfo`）或领域实体放入其中，破坏模块封装。 |

### 2.3 阶段一七项核心问题诊断与三类问题界定

1. **一级目录维度混杂（目录组织问题）**：8 个一级目录混用了“业务上下文”（`topology`、`assignment`）、“技术分层”（`application`、`infrastructure`、`model`）和“运行/集成角色”（`runtime`、`compatibility`、`assembly`）三种非正交分类基准，导致开发者难以建立一致的代码寻址心智模型。
2. **过度分层导致业务链路碎片化（目录组织问题）**：以 `SetShowNumber`（SIM-023.b）为例，一次完整修改需横跨 `compatibility/`（入口与完成）、`topology/application/`（用例与步骤计划）、`topology/domain/`（账户注册表）、`topology/ports/`（存储端口）、`application/orchestration/`（请求上下文）、`infrastructure/`（DataShare 写入与通知输出）和 `runtime/`（`UpdateMsisdnNumber` 写卡协议）共 7 个一级目录。
3. **共享目录演变为耦合中心的隐患（依赖关系问题）**：顶层 `infrastructure/` 同时服务于 T、A、C，若在同一构建目标内共享内部辅助状态，将绕过 T 和 A 的唯一写权入口。
4. **生产代码迁入后的目录失衡（目录组织问题）**：原 `services/sim/` 约四分之三的文件属于卡侧协议与运行态组件（状态、文件、安全、STK、电话簿、短信、eUICC、运营商配置、Radio 协议），现有 `runtime/{session,records,resource}` 仅定义了治理壳，缺少承载生产实现的二级功能包结构。
5. **三类架构问题的严格区分**：
   - **目录组织问题（物理内聚度与寻址成本）**：`application/` 与 `topology/application/` 重名、`card_access` 外置、专属 Adapter 散落在顶层 `infrastructure/`、`runtime/` 缺少功能子包——这些通过重组目录结构即可直接解决。
   - **职责边界问题（状态所有权归属）**：`slotId` 混兼物理位置、业务身份、拓扑关系、默认角色与无线资源五种生命周期；`MultiSimController` 同时操作账户缓存、角色参数与进行中标志；`SimAccountManager` 类名虽含 Account 却仅负责运营商配置装配——这些必须通过固化 T、A、R 的唯一事实写权（ARCH-RULE-002）解决，不能靠单纯移动文件解决。
   - **依赖关系问题（跨层与反向调用）**：SIM 模块经 `CoreManagerInner` 反调自身、5 处 detached 线程无准入并发触发主卡切换、RadioProtocol 错误分支不校验 session、激活回包共享单一 `activeResponse_`——这些必须通过 `HostPorts` 依赖反转、`RequestContext` 关联及 GN `visibility` 编译门禁解决。

---

## 3. 三种候选目录方案

### 3.1 关键特殊目录与 `Runtime` 定位的前置架构裁决

1. **`runtime/` 的真实架构定位**：`runtime/` **绝不是简单的 RIL Adapter**。底层 `TelRilManager` 仅提供基于 `serial` 的传输层消息收发；而 `runtime/` 是管理**卡应用会话（`SessionRegistry`）**、**EF 文件加载轮次与局部刷新闭包（`LoadSession` / `recordsGeneration`）**、**无线协议切换与设备互斥屏障（`ResourceBarrier` / `RadioProtocolController`）**以及 **3GPP/ETSI 卡侧状态机（UICC/USIM/ISIM/RUIM、PBR 电话簿、STK、eUICC 逻辑通道）**的有状态设备运行子系统。因此 `runtime/` 必须作为一级核心子系统存在，且内部必须将“运行治理内核”与“具体卡业务能力”解耦。
2. **`model/`、`application/orchestration/`、`application/card_access/`、`assembly/`、`compatibility/` 的存废裁决**：
   - **`model/`**：取消顶层 `model/` 命名，将其中的 `sim_identity.h` 与 `slot_catalog.h` 迁入基础内核目录（如 `foundation/`），防止其演变为容纳业务实体的万能共享模型；
   - **`application/orchestration/`**：因 O 不含业务规则与领域状态，将其下沉至基础内核的 `orchestration/` 子目录，彻底消除顶层 `application/` 目录；
   - **`application/card_access/`**：迁入 `runtime/access/`（或在方案 A 中归入网关直通层），消除简单卡操作跨顶层目录的无价值转发；
   - **`assembly/` 与 `compatibility/`**：均保留独立一级目录，分别作为唯一 Composition Root（H）和永久性外部旧契约防腐层（C）。

---

### 3.2 候选方案 A：业务上下文优先（Context-First / Vertical Slice）

#### 1) 最终稳态核心目录树

~~~text
services/multisim/
├── topology/                        # [业务上下文 T] 账户身份、位置绑定、资料与激活垂直切片
│   ├── api/                         # T 命令、查询 DTO、AccountLifecycle 只读契约
│   ├── application/                 # TopologyService, ActivateSimUseCase, SetShowNumberUseCase/Plan, ShowNumberQuery
│   ├── domain/                      # AccountRegistry(双投影), BindingIndex, DiscoveryAdmission, ActivationProgress
│   ├── ports/                       # AccountStore, ActivationRuntimePort, NumberQueryHostPort
│   └── adapters/                    # [内聚自原 infrastructure] AccountStoreAdapter, ImpuQueryHostAdapter
├── assignment/                      # [业务上下文 A] 默认角色、Primary 仲裁与切换垂直切片
│   ├── api/                         # 角色读写契约、Primary 切换命令、AssignmentTransition 只读视图
│   ├── application/                 # AssignmentService, PrimarySwitchPlan, 唯一业务重试与参数恢复编排
│   ├── domain/                      # AssignmentAggregate, AssignmentTransition, 默认角色与主卡选择纯规则
│   ├── ports/                       # AssignmentStore, RadioExecutionPort, LocalProfilePreferencePort
│   └── adapters/                    # [内聚自原 infrastructure] AssignmentStoreAdapter(默认卡URI与系统参数)
├── card_runtime/                    # [运行上下文 R] 卡侧运行态、协议栈与设备交互垂直切片
│   ├── governance/                  # CardRuntime, SessionRegistry, LoadSession, ResourceBarrier
│   ├── state_security/              # SimStateManager/Handle/Tracker, PIN/PUK 解锁, SlotMapping 合成状态管理
│   ├── uicc_files/                  # SimFileManager, IccFile/SimFile/RuimFile/IsimFile, *FileController, CardFileDetect
│   ├── euicc/                       # EsimManager/File/Controller(本地通道/APDU/Profile), EsimServiceClient(66250适配)
│   ├── telecom_apps/                # dialling_numbers/(电话簿与PBR), sim_sms/(卡短信), stk/(STK会话与命令)
│   ├── carrier_config/              # SimAccountManager(配置装配), OperatorConfig*, MatchingRule, IccOperatorPrivilege*, PdpProfile*
│   └── radio_channel/               # RadioProtocolController(三阶段协议状态机与SetActiveSimToRil设备执行)
├── legacy_gateway/                  # [防腐上下文 C] 外部旧契约门面、卡短请求直通、系统监听与兼容输出
│   ├── facade/                      # LegacySimFacade(承接 CoreServiceSim/SimManager 入口路由与槽能力检查)
│   ├── card_passthrough/            # [迁自 application/card_access] LocalCardRequests(PIN/EF/本地Profile短请求)
│   ├── monitor/                     # MultiSimMonitor(监听热插拔、用户切换、DataShare重连并分发至 T/A/R)
│   └── completion_output/           # LegacyCompletion + LegacyOutputAdapter(账户通知、Registry、CES广播输出)
├── assembly/                        # [宿主装配 H] SIM 子系统工厂、依赖注入与生命周期排空
│   ├── sim_module_factory.h/.cpp    # AttachExistingSimModule / CreateOwnedSimModule, ManagerPublication
│   └── host_ports.h                 # CoreService 注入的共享 RIL、FFRT、IMS 与单例接缝
└── common/                          # [共享内核] 最小公共值类型、只读槽目录与编排辅助原语
    ├── model/                       # sim_identity.h(SimId/SlotId强类型), slot_catalog.h/.cpp(只读硬件槽目录)
    ├── orchestration/               # [迁自 application/orchestration] RequestContext, EffectKnowledge, RuntimeOperationStore
    └── platform_helpers/            # SimRdbHelper 基础连接封装、TelephonyExtWrapper 安全调用封装
~~~

#### 2) 目录职责、业务归属与依赖边界
- **职责与归属**：`topology/` 与 `assignment/` 各自将 `{api, application, domain, ports, adapters}` 封装在单一垂直目录内，实现业务规则与专属存储适配的物理内聚；`card_runtime/` 按 7 个子域承载全部卡侧与无线运行态代码；`legacy_gateway/` 统一收口外部旧入口、短请求转发、系统事件监听与通知输出。
- **T/A 所有权与依赖约束**：`topology/domain` 与 `assignment/domain` 不含任何外部 I/O 依赖；跨上下文协作仅在 `application/` 层通过对方公开的 `api/` 或窄端口完成（如 `LocalCardRequests` 经 `LocalProfilePreferencePort` 通知 A 更新偏好）。
- **优缺点评估**：
  - **优点**：单一业务上下文（T 或 A）内部修改高度闭环，消除了顶层 `infrastructure/` 和 `application/` 的歧义。
  - **缺点**：将 `card_passthrough` 放入 `legacy_gateway/` 混淆了“旧契约翻译”与“卡侧实时访问”的职责；且 T、A、R 对底层同一套 DataShare（`SimRdbHelper`）和 `TelephonyExtWrapper` 的适配分散在多处 `adapters/` 与 `common/platform_helpers/` 中，底层技术升级时易产生跨目录联动。

---

### 3.3 候选方案 B：架构分层优先（Layer-First / Horizontal Clean Architecture）

#### 1) 最终稳态核心目录树

~~~text
services/multisim/
├── api/                             # [接口与契约层] 对内对外稳定契约与共享纯模型
│   ├── legacy_facade/               # [原 compatibility] LegacySimFacade, LegacyCompletion
│   ├── topology_contract/           # [原 topology/api] T 命令、查询 DTO 与 AccountLifecycle 契约
│   ├── assignment_contract/         # [原 assignment/api] A 角色命令、Primary 契约与切换状态视图
│   └── shared_model/                # [原 model] sim_identity.h, slot_catalog.h/.cpp
├── application/                     # [应用编排层] 用例协调、编排机制与抽象端口定义
│   ├── orchestration/               # [原 application/orchestration] RequestContext, EffectKnowledge, RuntimeOperationStore
│   ├── topology_cases/              # [原 topology/application] TopologyService, ActivateSim, SetShowNumber, ShowNumberQuery
│   ├── assignment_cases/            # [原 assignment/application] AssignmentService, PrimarySwitchPlan, 角色设置与重试
│   ├── card_access/                 # [原 application/card_access] LocalCardRequests 及 MultiSimMonitor 事件分发编排
│   └── ports/                       # 集中定义 AccountStore, AssignmentStore, ActivationRuntimePort, RadioExecutionPort 等
├── domain/                          # [核心领域层] 纯业务规则、聚合根与状态不变量（零平台/I/O 依赖）
│   ├── topology/                    # [原 topology/domain] AccountRegistry, BindingIndex, DiscoveryAdmission, ActivationProgress
│   └── assignment/                  # [原 assignment/domain] AssignmentAggregate, AssignmentTransition, 角色与主卡仲裁策略
├── runtime/                         # [设备运行层 R] 有状态卡会话、文件加载、卡侧协议与无线资源执行
│   ├── governance/                  # [原 runtime/{session,records,resource}] CardRuntime, SessionRegistry, LoadSession, ResourceBarrier
│   ├── state_security/              # SimStateManager/Handle/Tracker, PIN/PUK, SlotMapping 状态过滤
│   ├── uicc_files/                  # SimFileManager, IccFile/SimFile/RuimFile/IsimFile, *FileController, CardFileDetect
│   ├── euicc/                       # EsimManager/File/Controller, 本地 Port/Profile 通道会话
│   ├── card_features/               # dialling_numbers/, sim_sms/, stk/, carrier_config/(SimAccountManager/OperatorConfig*)
│   └── radio_protocol/              # RadioProtocolController(三阶段协议切换与激活 RIL 互斥执行)
├── infrastructure/                  # [基础设施适配层 I] 外部存储、系统参数、通知输出与平台扩展实现
│   ├── persistence/                 # AccountStoreAdapter, AssignmentStoreAdapter, SimRdbHelper, PdpProfileRdbHelper
│   ├── notification_output/         # LegacyOutputAdapter, CES 广播, StateRegistry 与账户变更回调输出
│   └── platform_ext/                # 系统参数 Adapter, TelephonyExtWrapper 适配, EsimServiceClient(66250) 代理
└── assembly/                        # [装配层 H] 对象创建、依赖注入与生命周期控制
    ├── sim_module_factory.h/.cpp    # 统一装配 api -> application -> domain/runtime/infrastructure
    └── host_ports.h                 # 对接 CoreService 的宿主能力接口
~~~

#### 2) 目录职责、业务归属与依赖边界
- **职责与归属**：一级目录严格按 Clean Architecture 水平技术职责（`api`、`application`、`domain`、`runtime`、`infrastructure`、`assembly`）切分，在每一层内部再按 `topology`、`assignment`、`card` 划分子目录。
- **T/A 所有权与依赖约束**：源码依赖在顶层目录间单向流转：`api` $\rightarrow$ `application` $\rightarrow$ `domain`，`runtime` 与 `infrastructure` 反向实现 `application/ports/`。但 T 与 A 的限界上下文边界被降级为二级目录，必须依靠额外的子目录可见性规则防止 `application/topology_cases` 直接包含 `domain/assignment`。
- **优缺点评估**：
  - **优点**：技术分层边界直观，便于对整个顶层 `domain/` 目录统一禁用平台头文件，同类基础设施代码集中在 `infrastructure/`。
  - **缺点**：**违背“同一变化原因代码内聚”原则，导致严重的散弹式修改**。任何涉及 T 或 A 的常规需求都必须同时跨越 `api/`、`application/`、`domain/`、`infrastructure/` 4 个一级目录；汇总式的 `application/ports/` 与顶层 `infrastructure/` 极易重新退化为高耦合枢纽，且推翻了当前已建成的 `topology/` 和 `assignment/` 目录树，实施返工成本高。

---

### 3.4 候选方案 C：分层与模块边界正交（Orthogonal Modular Architecture — 推荐方案）

#### 1) 最终稳态核心目录树

~~~text
services/multisim/
├── foundation/                      # [基础内核层] 零业务状态的纯标识、只读硬件目录、编排原语与底层无状态通道
│   ├── types/                       # [原 model] sim_identity.h(SimId/SlotId/PortIndex强类型), 基础枚举与通用值类型
│   ├── catalog/                     # [原 model] slot_catalog.h/.cpp(H构建的只读硬件槽能力与端点分类目录)
│   ├── orchestration/               # [原 application/orchestration] request_context.h, effect_knowledge.h, runtime_operation_store.h
│   └── platform/                    # 无业务状态的底层技术客户端：rdb_client/(SimRdbHelper基础CRUD), syspara/, ext_hook_guard/
├── topology/                        # [业务上下文 T] 账户身份、位置绑定、账户资料与 SIM 激活（唯一写权 Owner）
│   ├── api/                         # T 公开命令、查询 DTO、account_lifecycle.h 及只读账户/绑定快照契约
│   ├── application/                 # TopologyService, ActivateSimUseCase, SetShowNumberUseCase, ShowNumberPlan, ShowNumberQuery
│   ├── domain/                      # AccountRegistry(双投影), BindingIndex, DiscoveryAdmission, ActivationProgress(激活进行中与槽资格)
│   ├── ports/                       # AccountStore, ActivationRuntimePort, NumberQueryHostPort
│   └── adapters/                    # [内聚自原 infrastructure] AccountStoreAdapter(按simId字段级持久化), ImpuHostAdapter
├── assignment/                      # [业务上下文 A] 默认角色、Primary 主卡仲裁、资源分配与切换重试（唯一写权 Owner）
│   ├── api/                         # 角色查询/设置契约、Primary 切换命令、AccountRoleProtocol、AssignmentTransition 只读视图
│   ├── application/                 # AssignmentService, PrimarySwitchPlan(切换准入、跨R执行协调、唯一业务重试与参数恢复)
│   ├── domain/                      # AssignmentAggregate, AssignmentTransition, 默认Voice/SMS/Data与Primary仲裁纯规则
│   ├── ports/                       # AssignmentStore, RadioExecutionPort, LocalProfilePreferencePort, CompletionPort
│   └── adapters/                    # [内聚自原 infrastructure] AssignmentStoreAdapter(默认卡URI与Primary系统参数读写)
├── runtime/                         # [设备运行子系统 R] 有状态卡会话、文件加载、卡侧协议栈与无线协议执行
│   ├── kernel/                      # [原 runtime/{session,records,resource}] card_runtime.h/.cpp, SessionRegistry, LoadSession, ResourceBarrier
│   ├── access/                      # [迁自 application/card_access] local_card_requests.h/.cpp(PIN/EF/本地Profile短请求受控入口)
│   ├── state_security/              # SimStateManager, SimStateHandle, SimStateTracker, PIN/PUK安全操作, SlotMapping合成状态管理
│   ├── uicc_file/                   # SimFileManager, IccFile/SimFile/RuimFile/IsimFile, *FileController, CardFileDetectManager
│   ├── euicc/                       # EsimManager, EsimFile(逻辑通道/APDU/BPP/SwitchToProfile), EsimController, EsimServiceClientAdapter
│   ├── radio_protocol/              # RadioProtocolController(三阶段Radio协议状态机、会话校验、SetActiveSimToRil设备执行)
│   ├── telecom_apps/                # dialling_numbers/(IccDiallingNumbers*, UsimDiallingNumbersService), sim_sms/(SimSms*), stk/(Stk*)
│   └── carrier_config/              # SimAccountManager(运营商配置装配), OperatorConfigCache/Loader, MatchingRule, IccOperatorPrivilege*, PdpProfileRdbHelper
├── compatibility/                   # [外部兼容防腐层 C] 旧 Slot 契约门面、平台事件监听接入与原通知/完成出口
│   ├── facade/                      # LegacySimFacade(承接 CoreServiceSim/SimManager 旧签名、权限检查、slot排队与MEP兼容解析)
│   ├── monitor/                     # MultiSimMonitor(监听热插拔、用户切换、Radio状态与DataShare就绪，转为对 T/A/R 的显式输入)
│   └── output/                      # LegacyCompletion + LegacyOutputAdapter(保持 NotifySimAccountChanged、Registry、CES 广播原时序)
└── assembly/                        # [宿主装配根 H] 唯一对象工厂、依赖注入图与宿主生命周期接缝
    ├── sim_module_factory.h/.cpp    # 组装 Foundation/T/A/R/C 实例，管理 Attach/Owned 模式与 OnStop 排空(manager_publication.h)
    └── host_ports.h                 # CoreService 注入的共享 TelRilManager、IMS 客户端、FFRT 与进程级单例引用接缝
~~~

#### 2) 目录职责、业务归属与正交依赖边界
- **正交解耦设计**：
  - **一级目录对齐“事实所有权与核心变化轴”**：`topology/`（T）与 `assignment/`（A）作为两个自治业务上下文，各自内聚专属 `adapters/`，**彻底废除顶层 `infrastructure/` 与 `application/`**；`runtime/`（R）独立承载全部卡侧与无线运行态机制，内部划分为治理内核（`kernel/`）、受控短请求门面（`access/`）和 6 大正交生产能力包；`compatibility/`（C）完整闭环入站门面、系统监听与出站通知；`foundation/` 提供无业务状态的基础类型、只读 `SlotCatalog`、编排原语及底层无状态客户端封装。
  - **二级目录 + GN Target + Namespace 落实分层约束**：在 `topology/` 和 `assignment/` 内部执行 Clean 分层；在 `runtime/` 内部执行“内核治理 + 协议组件”分层，拒绝为纯协议组件强套四层空壳模板。
- **优缺点评估**：
  - **优点**：同时实现了**业务上下文修改的高内聚**（T、A 内部闭环）、**$70\%$ 以上卡侧生产代码的结构化承接**（`runtime/` 8 个二级目录职责分明）、**底层无状态技术通道的安全复用**（`foundation/platform/`）以及**相对当前骨架的最小迁移扰动**。
  - **缺点**：一级目录不追求形式上的“同构对称”（T/A 内部按架构层组织，R 内部按运行能力子域组织），需要配套 `BUILD.gn` 的 `visibility` 规则防止跨模块越权包含私有头文件。

---

## 4. 原 SIM 功能归属映射（生产功能完整性验证）

下表基于固定提交源码证据（F01～F46）及 62 项稳定功能/写权单元（WU-01～09），验证推荐方案 C（及方案 A、B 对照）能够 $100\%$ 承载原 `services/sim/` 与 `CoreServiceSim` 的全部生产功能，且不依赖旧 `services/sim/` 内部实现：

| 生产功能域（含 WU / 关键能力） | 原 `services/sim/` 及关联源码事实来源 | 方案 A 归属 | 方案 B 归属 | 方案 C（推荐）精确目录归属 | 事实 Owner 与运行边界约束 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1. 模块初始化、按槽对象装配与停止排空** | `CoreService::OnStart/OnStop`、`SimManager::OnInit/InitMultiSimObject/InitSingleSimObject/InitTelExtraModule` (F01, F03, F41) | `assembly/` + `common/model/` | `assembly/` + `api/shared_model/` | `assembly/sim_module_factory`<br>`foundation/catalog/slot_catalog` | **H 拥有生命周期与装配权**；构建只读 `SlotCatalog` 统一解释槽容量与分类，消除分散的 `SIM_SLOT_COUNT*`。 |
| **2. 热插拔、用户切换与平台监听** | `MultiSimMonitor::InitData/RefreshData/OnUserSwitched`、`SimCacheSyncManager` (F23, F28) | `legacy_gateway/monitor/` | `application/card_access/` | `compatibility/monitor/` $\rightarrow$ 调用 `topology/api` 与 `assignment/api` | **C 仅作监听接入与反注册管理**；发现准入（`DiscoveryAdmission`）与重载写权交由 T 和 A 各自接纳。 |
| **3. 账户身份、ICCID 匹配、双缓存投影与位置绑定 (WU-01/02/09)** | `MultiSimController::InitData/InitIccId/GetListFromDataBase/GetAllListFromDataBase/SortCache/ForgetAllData/InsertEsimData` (F04, F41, F42) | `topology/{application,domain,adapters}` | `application/topology_cases/` + `domain/topology/` + `infrastructure/persistence/` | `topology/application/topology_service`<br>`topology/domain/{account_registry,binding_index}`<br>`topology/adapters/account_store_adapter` | **T 唯一拥有账户（按 `simId`）与位置绑定写权**；替代原 `localCacheInfo_[slotId]` 直接数组读写。 |
| **4. 号码修改 (SIM-023.b)、号码查询回写 (SIM-023.c)、名称与 IMS 开关 (WU-05)** | `MultiSimController::SetShowNumber/SetShowNumberToDB/GetShowNumber/GetSimTelephoneNumber/SetShowName/SaveImsSwitch`、`SimFile::UpdateMsisdnNumber/ProcessSetMsisdnDone` (F08, F09, F34) | `topology/` + `card_runtime/uicc_files/` | `application/topology_cases/` + `runtime/uicc_files/` + `infrastructure/` | `topology/application/{set_show_number_use_case,show_number_plan,show_number_query}`<br>`runtime/uicc_file/sim_file` | **T 主编排并拥有账户资料写权**；R 负责 EF_MSISDN 写卡并经 `SessionRegistry` 校验回包归属；保留“先卡后 DB、异步 IMPU 回写”兼容轨迹。 |
| **5. SIM 激活/去激活（普通、force、卫星，SIM-024）** | `MultiSimController::SetActiveSim/SetActiveCommonSim/SetActiveSimSatellite/UpdataCacheSetActiveState/UpdateSubState/IsSetActiveSimInProgress` (F05, F32, F43) | `topology/` + `card_runtime/radio_channel/` | `application/topology_cases/` + `domain/topology/` + `runtime/radio_protocol/` | `topology/application/activate_sim_use_case`<br>`topology/domain/activation_progress`<br>`runtime/radio_protocol/` | **T 拥有激活状态与激活进行中标志**；执行前核实 A 的主卡切换状态，保留普通/force/卫星差异节点与两次 `UpdateSubState` Hook。 |
| **6. 默认 Voice / SMS / CellularData 角色管理 (WU-03)** | `MultiSimController::Get/SetDefaultVoiceSlotId, Get/SetDefaultSmsSlotId, Get/SetDefaultCellularDataSlotId, SaveDefaultCellularDataSlotIdInfo` (F06, F07, F33) | `assignment/{application,domain,adapters}` | `application/assignment_cases/` + `domain/assignment/` + `infrastructure/persistence/` | `assignment/application/assignment_service`<br>`assignment/domain/assignment_aggregate`<br>`assignment/adapters/assignment_store_adapter` | **A 唯一拥有角色偏好与有效选择写权**；保留各角色不同的回退规则与 Data 忽略特定错误的兼容语义。 |
| **7. Primary 主卡仲裁、切换、参数恢复与业务重试 (WU-04/07)** | `MultiSimController::SetPrimarySlotId/SetPrimarySlotIdWithoutModemReboot/SavePrimaryCardInfo/ResumePrimaryCardInfo/RetrySetPrimarySlotId/CheckIfNeedSwitchMainSlotId` (F06, F33, F41, F44) | `assignment/` + `card_runtime/radio_channel/` | `application/assignment_cases/` + `domain/assignment/` + `runtime/radio_protocol/` | `assignment/application/{assignment_service,primary_switch_plan}`<br>`assignment/domain/assignment_transition`<br>`runtime/radio_protocol/` | **A 拥有 `AssignmentTransition` 切换准入与唯一业务重试权**，收口原 5 处 detached 线程；分开记录意图、参数投影与设备观察。 |
| **8. Radio Protocol 三阶段切换与激活设备等待 (WU-08)** | `RadioProtocolController::Init/GetRadioProtocol/SetRadioProtocol/ProcessResponseInfoOfEvent/SetActiveSimToRil` (F19, F41, F44, F45) | `card_runtime/{governance,radio_channel}` | `runtime/{governance,radio_protocol}` | `runtime/radio_protocol/radio_protocol_controller`<br>`runtime/kernel/resource_barrier`<br>`foundation/orchestration/request_context` | **R 拥有无线协议会话与资源屏障**；对成功与错误回包均先做本轮 session 准入，消除 `activeResponse_` 单变量覆盖缺陷。 |
| **9. 原始卡状态机、PIN/PUK 安全与 SlotMapping 状态过滤** | `SimStateManager`, `SimStateHandle::ProcessIccCardState`, `SimStateTracker`, `NotifySimSlotsMapping`, `CoreServiceSim::UnlockPin` (F15, F18, F35, F43) | `card_runtime/state_security/` + `legacy_gateway/card_passthrough/` | `runtime/state_security/` + `application/card_access/` | `runtime/state_security/{sim_state_manager,sim_state_handle,sim_state_tracker}`<br>`runtime/access/local_card_requests` | **R 拥有原始卡状态与 PIN/PUK 会话**；保留原状态过滤及专用 PIN 响应，短请求无需强制绑定 `simId`。 |
| **10. UICC 多制式文件、Records 加载与 Refresh (WU-06)** | `SimFileManager`, `IccFile/SimFile/RuimFile/IsimFile`, `*FileController`, `ProcessIccRefresh`, `ProcessFileLoaded`, `OnAllFilesFetched`, `CardFileDetectManager` (F16, F27, F35) | `card_runtime/{governance,uicc_files}` | `runtime/{governance,uicc_files}` | `runtime/uicc_file/{sim_file_manager,icc_file,sim_file,ruim_file,isim_file,*_controller,card_file_detect}`<br>`runtime/kernel/load_session` | **R 拥有文件内容与 `LoadSession` 加载代次**；换卡前先 fence T，同卡局部 Refresh 按 EF 依赖闭包失效，保留 `RECORDS_LOADED` 全套出口节点。 |
| **11. 本地 eSIM/eUICC 通道协议与独立 SA 66250 协作** | `EsimManager`, `EsimFile::ObtainChannelSuccessExclusive/ObtainEid/SwitchToProfile/SyncCloseChannel`, `EsimController`, `EsimServiceClient` (F10～F13, F20) | `card_runtime/euicc/` + `legacy_gateway/card_passthrough/` | `runtime/euicc/` + `infrastructure/platform_ext/` | `runtime/euicc/{esim_manager,esim_file,esim_controller,esim_service_client_adapter}`<br>`runtime/access/local_card_requests` | **R 拥有本地 eUICC 通道互斥与 APDU 状态**；本地 Profile 切换偏好经 `LocalProfilePreferencePort` 归 A 接纳，高层下载/OSU 保持 66250 边界。 |
| **12. 电话簿（含 USIM PBR）、卡短信与 STK** | `IccDiallingNumbersManager/Handler/Cache`, `UsimDiallingNumbersService`, `SimSmsManager/Controller`, `StkManager/Controller` (F25, F26) | `card_runtime/telecom_apps/` | `runtime/card_features/` | `runtime/telecom_apps/{dialling_numbers,sim_sms,stk}/` | **R 内聚卡侧标准电信应用**；保留成熟 PBR 多级加载、卡短信存储与 STK 主动式命令协议，不混入 T/A 领域层。 |
| **13. 运营商配置、匹配规则、权限控制与 APN 初始化** | `SimAccountManager`, `OperatorConfigCache/Loader`, `MatchingRule`, `IccOperatorPrivilegeController`, `PdpProfileRdbHelper` (F24, F43) | `card_runtime/carrier_config/` | `runtime/card_features/` | `runtime/carrier_config/{sim_account_manager,operator_config_*,matching_rule,icc_operator_privilege_*,pdp_profile_rdb_helper}` | **归属 R 的运营商配置子模块**（类名 `SimAccountManager` 不代表归属 T）；经窄接口只读查询 A 的切换进行中状态。 |
| **14. 存储底座、厂商 Hook、旧门面与通知输出** | `SimRdbHelper`, `TelephonyExtWrapper`, `CoreServiceSim`, `MultiSimMonitor::NotifySimAccountChanged` (F15, F21, F22, F28, F34, F46) | `common/platform_helpers/` + `legacy_gateway/` | `infrastructure/` + `api/legacy_facade/` | `foundation/platform/{rdb_client,syspara_client,ext_hook_guard}`<br>`compatibility/{facade,output}` | **C 负责旧签名入口与通知输出时序**；`foundation/platform` 仅提供无状态技术封装，严禁厂商 Hook 直写核心缓存。 |

---

## 5. 六类未来变化影响矩阵

### 5.1 六类演进场景的业务与技术假设界定

为保证验证严谨性，明确 V1～V6 的场景定义与边界假设（其中 V5、V6 为面向架构解耦能力的验证假设，不宣称为已批准项目计划）：
- **V1（三卡扩展至四卡）**：逻辑 SIM 端点由 3 个扩展至 4 个（如双物理槽 + eSIM 多端口或 VSIM/卫星受限槽），底层激活 Modem Radio 栈仍为 2 个（4 订阅/端点 : 2 逻辑 Modem 栈的 $N:M$ 映射）。要求清除 21 个文件中的 `SIM_SLOT_2`、`SLOT_3_INDEX`、`DUAL_SLOT_COUNT`、`VSIM_SLOT_ID` 等硬编码分支。
- **V2（eSIM 规格及 Port/Profile 能力扩展）**：支持完整 MEP（单物理 eUICC 槽同时启用两个 Port 上的 Profile），扩展 Port/Profile 映射、APDU 通道隔离及本地切换能力，且旧 `GetSimId(slotId)` 必须按冻结兼容规则解析（禁止盲目任取首条账户），不污染通用默认卡逻辑。
- **V3（新硬件改变 SIM 初始化及激活流程）**：新基带平台调整卡发现、复位、PIN 解锁后的 EF 探测与硬件激活握手顺序及异常恢复条件，但上层业务对 `READY`/`LOADED` 与 `IsSimActive` 的稳定语义不变。
- **V4（硬件资源约束改变 Primary 切换流程）**：新硬件平台改变 Radio 资源并发约束，将 Primary 切换从“三阶段重启式 `SetRadioProtocol`（`CHECK/CHANGE/COMPLETE`）”调整为“带活动会话保护与硬件侧回滚的免重启快速重绑协议”。
- **V5（部分功能由 Telephony 下沉至 RIL —— 明确技术假设）**：**假设将“Radio 协议切换的三阶段状态推进与超时控制（现位于 `RadioProtocolController`）”及“eUICC 逻辑通道独占打开、APDU 分包与关通道重试（现位于 `EsimFile`）”下沉至 RIL / Modem 侧实现**；Telephony 仅发起高层原子设备请求并接纳最终执行结果，业务事实所有权（T/A）不下沉。
- **V6（部分功能由 RIL 上移至 Telephony —— 明确技术假设）**：**假设底层 RIL 不再在驱动内自动处理“多卡物理槽切换时的过渡期状态屏蔽（现配合 `NotifySimSlotsMapping` 注入合成状态）”及“掉卡后主卡自动回退决策”，改由 Telephony 直接接收原始硬件槽插拔观测，在软件侧完成拓扑重绑、过渡期状态门控与主卡重选编排**。

---

### 5.2 候选方案 A（业务上下文优先）变化影响矩阵

| 演进场景 \ 方案 A 逻辑模块 | `common/model` (`SlotCatalog`/ID) | `common/orchestration` & `helpers` | `topology/{api,app,domain}` | `topology/adapters` | `assignment/{api,app,domain}` | `assignment/adapters` | `card_runtime/governance` | `card_runtime/{state,uicc,apps,carrier}` | `card_runtime/euicc` | `card_runtime/radio_channel` | `legacy_gateway/{facade,passthrough,monitor,output}` | `assembly/` |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **V1：三卡扩四卡** | **M** | S | **M** | P | **M** | P | S | P | S | **M** | **M** | **M** |
| **V2：eSIM/MEP 扩展** | **M** | S | **M** | P | S | S | **M** | S | **M** | ? | **M** | P |
| **V3：新硬件初始化/激活** | S | S | **M** | P | S | S | **M** | **M** | S | **M** | P | S |
| **V4：Primary 切换约束变化** | S | S | S | S | **M** | **M** | **M** | S | S | **M** | P | S |
| **V5：协议/通道下沉至 RIL** | S | S | S | S | S | S | **M** | S | **M** | **M** | S | S |
| **V6：映射/状态门控上移** | **M** | S | **M** | S | **M** | S | **M** | **M** | S | **M** | P | S |

- **主要变化传播路径说明**：
  - **M（必要修改）**：V1 集中在 `SlotCatalog`、T 绑定、A 主卡仲裁与 R 无线通道；V2 集中在 `SlotCatalog`、T 的 MEP 绑定、`card_runtime/euicc` 与 `legacy_gateway` 的兼容路由；V3/V4/V5 的设备协议变化主要落在 `card_runtime` 内部及对应的 T/A 用例中。
  - **P（潜在联动）**：由于方案 A 将 `card_passthrough`（原 `card_access`）和 `monitor` 混装在 `legacy_gateway/` 中，且底层存储/平台辅助拆散在各上下文 `adapters/` 与 `common/platform_helpers/`，导致 V1/V2/V3 的存储扩展和 V3/V4/V6 的运行态事件变化容易向 `legacy_gateway/` 和各处 `adapters/` 传播。

---

### 5.3 候选方案 B（架构分层优先）变化影响矩阵

| 演进场景 \ 方案 B 逻辑模块 | `api/shared_model` | `api/{legacy,topology,assignment}` | `application/orchestration` | `application/topology_cases` | `application/assignment_cases` | `application/card_access` & `ports` | `domain/topology` | `domain/assignment` | `runtime/governance` | `runtime/{state,uicc,features}` | `runtime/euicc` | `runtime/radio_protocol` | `infrastructure/{persistence,output,ext}` | `assembly/` |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **V1：三卡扩四卡** | **M** | **M** | S | **M** | **M** | P | **M** | **M** | S | P | S | **M** | P | **M** |
| **V2：eSIM/MEP 扩展** | **M** | **M** | S | **M** | S | **M** | **M** | S | **M** | S | **M** | ? | P | P |
| **V3：新硬件初始化/激活** | S | S | S | **M** | S | P | **M** | S | **M** | **M** | S | **M** | P | S |
| **V4：Primary 切换约束变化** | S | P | S | S | **M** | **M** | S | **M** | **M** | S | S | **M** | **M** | S |
| **V5：协议/通道下沉至 RIL** | S | S | S | S | S | P | S | S | **M** | S | **M** | **M** | P | S |
| **V6：映射/状态门控上移** | **M** | P | S | **M** | **M** | **M** | **M** | **M** | **M** | **M** | S | **M** | P | S |

- **主要变化传播路径说明**：
  - **M（必要修改）被水平拆散**：在方案 B 中，同一个业务变化（例如 V1 扩卡或 V4 调整 Primary 切换策略）被迫分散到 `api/`、`application/`、`domain/`、`infrastructure/` 四个顶层目录中。
  - **P（潜在联动）显著增多**：因为 `application/ports/` 集中定义了所有模块的端口，且 `infrastructure/` 集中实现了所有模块的 Adapter，任何单一模块端口签名或外部适配的微调（V1～V6）都会污染共享的 `application/ports/` 与 `infrastructure/`，引发跨上下文重编译与潜在联动。

---

### 5.4 候选方案 C（分层与模块边界正交 —— 推荐方案）变化影响矩阵与深度论证

| 演进场景 \ 方案 C 逻辑模块 | `foundation/types` & `catalog` | `foundation/orchestration` & `platform` | `topology/` (含 api,app,domain,ports,adapters) | `assignment/` (含 api,app,domain,ports,adapters) | `runtime/kernel` (session,records,resource) | `runtime/access` (短请求门面) | `runtime/state_security` & `uicc_file` | `runtime/euicc` | `runtime/radio_protocol` | `runtime/telecom_apps` & `carrier_config` | `compatibility/` (facade,monitor,output) | `assembly/` |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **V1：三卡扩四卡** | **M** | S | **M** | **M** | S | S | S | S | **M** | S | **M** | **M** |
| **V2：eSIM/MEP 扩展** | **M** | S | **M** | S | **M** | **M** | S | **M** | ? | S | **M** | S |
| **V3：新硬件初始化/激活** | S | S | **M** | S | **M** | S | **M** | S | **M** | S | S | S |
| **V4：Primary 切换约束变化** | S | S | S | **M** | **M** | S | S | S | **M** | S | S | S |
| **V5：协议/通道下沉至 RIL** | S | S | S | S | **M** | S | S | **M** | **M** | S | S | S |
| **V6：映射/状态门控上移** | **M** | S | **M** | **M** | **M** | S | **M** | S | **M** | S | P | S |

#### 方案 C 矩阵要素逐项深度解释

1. **每个主要 M（必要修改）的业务或技术原因**：
   - **V1（三卡扩四卡）**：
     - `foundation/catalog`（**M**）：扩展 `SlotCatalog` 以配置第 4 逻辑端点及 VSIM/受限槽能力，替代分散在 21 个文件中的 `SIM_SLOT_2`、`SLOT_3_INDEX`、`DUAL_SLOT_COUNT` 字面量（ARCH-RULE-010）；
     - `topology/`（**M**）：扩展 `BindingIndex` 的多端点映射及 `InitData/InsertData` 标签计算规则；
     - `assignment/`（**M**）：将 Primary 仲裁与候选选择从双卡比较扩展为 4 逻辑端点对 2 Modem 栈的 $N:M$ 仲裁；
     - `runtime/radio_protocol`（**M**）：移除 `RadioProtocolController` 内部 `DUAL_SLOT_COUNT` 的双槽数组限制；
     - `compatibility/` & `assembly/`（**M**）：按 `SlotCatalog` 动态容量装配单槽运行实例并校验旧入口 slot 范围。
   - **V2（eSIM 规格及 Port/Profile 能力扩展）**：
     - `foundation/types` & `catalog`（**M**）：在端点标识中显式表达 `PhysicalSlotId + PortIndex`；
     - `topology/`（**M**）：`BindingIndex` 支持同一物理槽下的多个 Port 同时绑定不同 `SimId` 账户，消除 `localCacheInfo_[slotId]` 一槽一卡假设；
     - `runtime/euicc`、`runtime/kernel`、`runtime/access`（**M**）：扩展多端口 APDU 通道会话管理与本地 Profile/Port 操作契约；
     - `compatibility/`（**M**）：在 `LegacySimFacade` 中按冻结兼容规则处理 MEP 下 `GetSimId(slotId)` 的解析歧义。
   - **V3（新硬件初始化与激活流程变化）**：
     - `runtime/state_security`、`runtime/uicc_file`、`runtime/kernel`、`runtime/radio_protocol`（**M**）：修改底层卡复位探测、EF 加载完成条件与 `SetActiveSimToRil` 设备握手流程；
     - `topology/`（**M**）：仅在激活业务步骤或前置准入策略变化时调整 `topology/application/activate_sim_use_case`，而 `AccountRegistry` 账户模型保持稳定。
   - **V4（硬件资源约束改变 Primary 切换流程）**：
     - `assignment/`（**M**）：在 `AssignmentService`、`PrimarySwitchPlan` 与 `AssignmentTransition` 中调整切换准入、活动会话保护及参数提交/恢复策略；
     - `runtime/radio_protocol` & `runtime/kernel`（**M**）：适配新硬件的免重启无线重绑协议与 `ResourceBarrier` 完成判定。
   - **V5（协议/通道下沉至 RIL）**：
     - **严格封闭在 `runtime/` 内部（M）**：仅需简化 `runtime/radio_protocol`（移除本地三阶段状态推进）和 `runtime/euicc`（移除本地通道分包重试），并调整 `runtime/kernel` 的会话粒度。**上层 `topology/`、`assignment/`、`compatibility/` 完全保持稳定（S）**，验证了“设备执行下沉不撼动业务事实 Owner”的隔离能力。
   - **V6（映射与状态门控由 RIL 上移至 Telephony）**：
     - `foundation/catalog`、`runtime/state_security`、`runtime/radio_protocol`、`runtime/kernel`（**M**）：在 `runtime/state_security` 中新增映射切换过渡期的状态门控（承接原 `NotifySimSlotsMapping` 合成状态过滤职责）；
     - `topology/` & `assignment/`（**M**）：在 `topology` 中承接物理槽到逻辑端点的动态重绑，在 `assignment` 中承接掉卡后的主卡重选规则。
2. **每个 P（潜在联动）的传播路径及消除方法**：
   - 方案 C 在 V1～V5 中将潜在联动（P）降为 **0**；唯一一处 **P** 出现在 **V6 的 `compatibility/monitor`**：若 `MultiSimMonitor` 直接监听底层未过滤的原始卡状态事件，当状态门控从 RIL 上移至 Telephony 时，`MultiSimMonitor` 可能被过渡期伪 `ABSENT` 误触发清理动作。**消除方案**：强制规定 `compatibility/monitor` 只能订阅经 `runtime/state_security` 准入过滤后的受控状态通知，即可将该 **P** 彻底转化为 **S**。
3. **S（保持稳定）成立所依赖的接口或业务不变量**：
   - **V2 中 `assignment/` 保持稳定（S）**：依赖 ARCH-RULE-001（身份、位置与会话分开），A 模块基于抽象订阅/端点标识与 `RadioExecutionPort` 决策角色与主卡，不耦合端点底层是物理卡还是 eSIM Port。
   - **V3/V4 中跨模块保持稳定（S）**：依赖 ARCH-RULE-002 与 ARCH-RULE-006，T 的激活流程只通过窄接口查询 A 的 `IsSetPrimarySlotIdInProgress` 准入状态，不感知 A 内部切换协议细节；增值应用（`runtime/telecom_apps`、`carrier_config`）只消费稳定的 `READY/LOADED` 与只读状态，不耦合底层初始化步骤。
   - **V5 中 `topology/` 与 `assignment/` 保持稳定（S）**：依赖 `ActivationRuntimePort` 与 `RadioExecutionPort` 采用业务意图级契约（如 `ObservePrimaryExecution`），不泄漏底层 RIL 三阶段命令或 APDU 通道参数（TGT-OPEN-002）。
4. **关键 `?`（待验证项）所需补充的代码或硬件信息**：
   - **V2 中 `runtime/radio_protocol` 的 `?`**：在 eSIM MEP 双 Port 同时激活场景下，切换或绑定第二个 Port 到逻辑 Modem 栈是否需要修改 `RadioProtocolController::SetRadioProtocol` 及底层 HDI 接口参数，取决于具体基带与 Vendor RIL 的硬件契约，需实机与 RIL 仓证据确认。

---

## 6. 方案横向比较与推荐

### 6.1 变化隔离五维分析与 AI 辅助编程评估

1. **变化隔离能力五维比较**：
   - **修改局部性**：对于本质上跨领域的业务扩展（如 V1 四卡扩展同时涉及硬件目录、T 绑定、A 仲裁、R 协议栈），方案 C 如实反映必要的跨模块修改（M），但**彻底消除了同一上下文内部跨 `api/application/domain/infrastructure` 顶层目录的架构附加修改成本**；对于设备与协议边界变化（V3、V4、V5），方案 C 将修改高度收敛在 `runtime/` 对应二级子目录内。
   - **依赖传播**：方案 C 将 `ports/` 和 `adapters/` 封装在 `topology/` 与 `assignment/` 内部，避免了方案 B 中共享 `application/ports/` 与顶层 `infrastructure/` 引发的头文件重编译扩散。
   - **状态稳定性**：在 V1～V6 全部演进场景中，T（账户/绑定/激活进行中）、A（角色/主卡切换进行中）、R（会话/加载代次/资源屏障）的三方状态所有权边界保持恒定，无需迁移事实 Owner。
   - **验证范围**：配合细粒度 GN Target，修改 `runtime/euicc`（V2/V5）或 `assignment`（V4）只需重跑对应子模块契约测试与状态机测试，无需全量回归无关的电话簿、STK 或卡短信模块。
   - **理解成本**：6 个一级目录语义正交，消除了顶层 `application/` 与 `topology/application/` 的同名困惑，开发人员与 AI 在 2 层目录深度内即可精确定位任意业务功能。
2. **AI 辅助编程友好度验证**：
   - **防止上下文拥挤（Context Crowding）**：既有工程分析表明，跨模块分散实现极易占满 AI 上下文窗口，导致“关键不变量丢失 $\rightarrow$ 生成仅能编译的局部补丁”。方案 C 通过将 T 的全栈代码内聚在 `topology/`、A 的全栈代码内聚在 `assignment/`、R 的各卡侧协议分包在 `runtime/*/`，使 AI 执行单一切片任务时只需加载主责任目录与协作方 `api/` 头文件。
   - **清晰的写权与边界护栏**：AI 可直接从目录归属识别唯一事实 Owner（禁止在 `runtime/` 或 `compatibility/` 中直接修改 T/A 缓存），且 GN `visibility` 可提供确定性的机器编译期拦截反馈。

### 6.2 十维综合评价表

| 评价维度 | 现有 8 目录骨架 | 方案 A（业务上下文优先） | 方案 B（架构分层优先） | 方案 C（分层与模块边界正交 · 推荐） |
| :--- | :--- | :--- | :--- | :--- |
| **1. 代码可理解性** | 中（顶层维度混杂，`application` 重名，`runtime` 未分包） | 高（按垂直业务切片寻址直观） | 中低（理解单个业务流程需跨 4 个水平层目录） | **高**（6 个一级目录职责正交，`runtime` 二级能力包清晰直观） |
| **2. 日常维护效率** | 中低（改单功能常跨 `topology`、`infrastructure`、`application`） | 高（T/A 内部闭环修改） | 低（典型散弹式修改，跨顶层目录频繁） | **高**（T/A 闭环修改，卡侧短请求直达 `runtime/access`） |
| **3. 业务内聚性** | 中（专属 Adapter 与 `card_access` 外置） | **高**（领域与专属 Adapter 紧密内聚） | 低（按技术层拆散同一业务变化原因） | **高**（T/A 内聚专属 Adapter，R 内聚全部卡侧协议与短请求） |
| **4. 架构清晰度** | 中（8 个目录平铺，主次与层次关系不显式） | 中高（业务边界清晰，但底层共享技术通道分散） | 高（经典水平分层图景） | **高**（纵向切分事实 Owner，横向用子目录 + GN 落实分层） |
| **5. 模块解耦** | 中（顶层 `infrastructure/` 易演变为新耦合中心） | 高（消除顶层大杂烩 `infrastructure/`） | 中低（汇总式 `ports/` 与 `infrastructure/` 耦合重） | **高**（私有 `ports/adapters` + 无状态 `foundation` + GN 强门禁） |
| **6. 变化隔离（V1～V6）** | 中低（`runtime` 未细分，硬件与协议变化易扩散） | 中高（`legacy_gateway` 与多处 `adapters` 有少量 P 联动） | 低（水平层切分导致 V1～V6 产生广泛跨层 P 联动） | **最高**（V1～V5 消除 P 级联动，V5 完全封闭在 `runtime` 内） |
| **7. 长期扩展能力** | 中低（无法平顺容纳原 `services/sim/` $70\%$ 以上卡侧代码） | 高（`card_runtime` 二级扩展清晰） | 中（新增特性需穿透全部水平层） | **高**（平滑支撑 N 卡、MEP 及 Telephony/RIL 边界双向迁移） |
| **8. AI 编程友好度** | 中（跨目录检索多，易混淆 `model/` 与 `domain/`） | 高（垂直切片上下文集中） | 低（单任务需加载多个顶层目录，易触发上下文拥挤） | **最高**（最小完整上下文聚焦，配合 GN `visibility` 自动防越权） |
| **9. 结构复杂度** | 中（8 个一级目录，含轻量空壳目录） | 中（6 个一级目录，底座封装略分散） | 中高（跨层接口与转发结构偏多） | **中低**（6 个一级目录，裁剪冗余转发层，层级克制） |
| **10. 实施成本** | 基准（当前已建骨架） | 中（需拆分 `infrastructure`、重整 `legacy_gateway`） | 高（需推翻重建现有 `topology/` 与 `assignment/` 目录） | **低中**（完整保留现有 `topology`/`assignment`/`assembly` 主体，仅做局部收拢与 `runtime` 分包） |

### 6.3 核心比较结论与关键取舍

- **最有利于代码理解与日常维护的方案**：**方案 C**（及方案 A）。按业务责任与运行子系统寻址显著优于方案 B 的跨水平层寻址。
- **最有利于长期变化隔离与抽象成本控制的方案**：**方案 C**。通过裁撤顶层 `application/` 与 `infrastructure/`，将 `orchestration` 归入 `foundation/`、将 `card_access` 归入 `runtime/access/`、将专属 Adapter 归入 `topology/` 和 `assignment/`，既消除了方案 B 的跨层耦合，又避免了方案 A 将实时卡访问混入兼容网关以及重复封装 `SimRdbHelper` 的弊端。
- **目录组织收益 vs 模块职责/接口解耦收益的界定**：
  - **目录组织贡献的收益**：提升代码定位直觉、收敛单次修改与 AI 阅读的文件范围、消除同名目录歧义、为原 `services/sim/` $70\%$ 以上卡侧生产代码提供清晰分包结构。
  - **模块职责与接口解耦贡献的收益**：确立 T/A 唯一事实写权（ARCH-RULE-002）、解耦 `slotId` 五重语义（ARCH-RULE-001/010）、建立请求与会话/加载轮次准入（ARCH-RULE-004）、消除 `CoreManagerInner` 循环依赖与 detached 线程竞态。
- **推荐方案 C 的核心取舍**：放弃了方案 B 在顶层目录形式上的“全局水平分层对称性”，允许业务规则密集的 `topology/`、`assignment/` 采用 `{api,application,domain,ports,adapters}` 分层，而协议状态密集的 `runtime/` 采用 `{kernel,access,能力子域}` 分包，以极小的构建规则配置成本换取最高的开发维护效率与变化隔离度。

---

## 7. 最终推荐目录设计（方案 C 完整规范）

### 7.1 最终稳态核心目录树

~~~text
services/multisim/
├── BUILD.gn                                 # 定义细粒度 ohos_source_set、visibility 白名单与编译门禁
├── foundation/                              # [1. 基础内核层] 零业务状态、零领域规则、零上层依赖
│   ├── types/                               # [迁自 model] sim_identity.h(SimId/SlotId/PortIndex强类型), 基础枚举与错误码
│   ├── catalog/                             # [迁自 model] slot_catalog.h/.cpp(由 H 构建的只读物理槽/端点分类与容量目录)
│   ├── orchestration/                       # [迁自 application/orchestration] request_context.h, effect_knowledge.h, runtime_operation_store.h
│   └── platform/                            # 无业务状态的底层平台技术客户端封装
│       ├── rdb_client/                      # SimRdbHelper 基础 DataShare 连接与参数化读写封装（严禁持有业务缓存）
│       ├── syspara_client/                  # 系统参数底层读写工具封装
│       └── ext_hook_guard/                  # TelephonyExtWrapper 动态加载与受控调用封装（严禁直接写核心缓存）
│
├── topology/                                # [2. 拓扑与账户上下文 T] 账户身份、位置绑定、资料与激活的唯一事实 Owner
│   ├── api/                                 # T 公开契约：account_lifecycle.h、账户/绑定只读快照 DTO、激活/号码命令与查询接口
│   ├── application/                         # T 用例编排：topology_service.h/.cpp, activate_sim_use_case.h/.cpp,
│   │                                        #            set_show_number_use_case.h/.cpp, show_number_plan.h/.cpp, show_number_query.h/.cpp
│   ├── domain/                              # T 纯领域层：account_registry.h/.cpp(按simId与slot双投影), binding_index.h/.cpp,
│   │                                        #            discovery_admission.h(发现最终资格), activation_progress.h(激活进行中与槽资格)
│   ├── ports/                               # T 依赖端口：account_store.h, activation_runtime_port.h, number_query_host_port.h
│   └── adapters/                            # [迁自 infrastructure] account_store_adapter.h/.cpp(按simId字段级更新), impu_host_adapter.h/.cpp
│
├── assignment/                              # [3. 角色与分配上下文 A] 默认角色、Primary 主卡与切换状态的唯一事实 Owner
│   ├── api/                                 # A 公开契约：AccountRoleProtocol、默认角色读写契约、Primary 切换命令、AssignmentTransition 视图
│   ├── application/                         # A 用例编排：assignment_service.h/.cpp, primary_switch_plan.h/.cpp(切换准入、唯一重试与参数恢复)
│   ├── domain/                              # A 纯领域层：assignment_aggregate.h/.cpp, assignment_transition.h/.cpp,
│   │                                        #            默认 Voice/SMS/Data 回退规则与 N:M Primary 候选仲裁纯策略
│   ├── ports/                               # A 依赖端口：assignment_store.h, radio_execution_port.h, local_profile_preference_port.h, completion_port.h
│   └── adapters/                            # [迁自 infrastructure] assignment_store_adapter.h/.cpp(默认卡 URI、Primary 参数与偏好持久化)
│
├── runtime/                                 # [4. 卡与无线运行子系统 R] 有状态设备会话、加载代次、卡协议栈与无线执行
│   ├── kernel/                              # R 运行治理内核：统一管控会话归属、加载范围有效性与共享资源互斥
│   │   ├── card_runtime.h/.cpp              # ProductionSlotBindings 与 R 对内受控门面
│   │   ├── session/                         # [原 runtime/session] session_registry.h/.cpp(卡会话生命期与请求归属校验)
│   │   ├── records/                         # [原 runtime/records] load_session.h/.cpp(EF加载代次/LoadScope、局部Refresh闭包与消费资格)
│   │   └── resource/                        # [原 runtime/resource] resource_barrier.h/.cpp(共享无线/通道资源正常/撤销/隔离出口)
│   ├── access/                              # [迁自 application/card_access] local_card_requests.h/.cpp(PIN/EF/本地Profile短请求受控入口)
│   ├── state_security/                      # 卡状态与安全：sim_state_manager, sim_state_handle, sim_state_tracker,
│   │                                        #              PIN/PUK/PIN2 安全认证与 NotifySimSlotsMapping 合成状态过滤
│   ├── uicc_file/                           # 多制式文件系统：sim_file_manager, icc_file, sim_file, ruim_file, isim_file,
│   │                                        #                *_file_controller, card_file_detect_manager
│   ├── euicc/                               # 本地 eUICC 运行态：esim_manager, esim_file(通道独占/APDU/BPP/SwitchToProfile),
│   │                                        #                   esim_controller, esim_service_client_adapter(对接独立 SA 66250)
│   ├── radio_protocol/                      # 无线协议与激活执行：radio_protocol_controller(三阶段协议切换、错误分支session准入、
│   │                                        #                    SetActiveSimToRil 请求级关联等待)
│   ├── telecom_apps/                        # 卡侧标准增值应用
│   │   ├── dialling_numbers/                # icc_dialling_numbers_manager/handler/cache, usim_dialling_numbers_service(PBR加载)
│   │   ├── sim_sms/                         # sim_sms_manager, sim_sms_controller
│   │   └── stk/                             # stk_manager, stk_controller(含 EXT 通知分支与 CES SessionEnd)
│   └── carrier_config/                      # 运营商配置与权限：sim_account_manager(仅装配配置), operator_config_cache/loader,
│                                            #                  matching_rule, icc_operator_privilege_controller, pdp_profile_rdb_helper
│
├── compatibility/                           # [5. 外部兼容与防腐层 C] 隔离旧 Slot API、平台事件监听与历史通知时序契约
│   ├── facade/                              # legacy_sim_facade.h/.cpp(承接 ISimManager/IEsimManager/CoreServiceSim 入口、
│   │                                        # 权限与槽能力检查、异步队列桥接与 MEP 兼容路由解析)
│   ├── monitor/                             # multi_sim_monitor.h/.cpp(监听热插拔、用户切换、DataShare重连，转为对 T/A/R 的显式输入)
│   └── output/                              # legacy_completion.h/.cpp + [迁自 infrastructure] legacy_output_adapter.h/.cpp
│                                            # (保持原账户回调、StateRegistry 与 CES 广播的先后顺序与部分失败可见性)
│
└── assembly/                                # [6. 宿主装配根 H] 唯一对象创建工厂、依赖注入与生命周期管理
    ├── sim_module_factory.h/.cpp            # 构建 SlotCatalog，支持 AttachExistingSimModule / CreateOwnedSimModule 及停止排空
    ├── manager_publication.h                # 发布稳定门面引用，管理反注册与销毁顺序
    └── host_ports.h                         # 封装 CoreService 注入的共享 TelRilManager、IMS 客户端、FFRT 与单例唯一定义处引用
~~~

### 7.2 原 8 个一级目录调整方式汇总

1. **撤除 3 个顶层目录（`model/`、`application/`、`infrastructure/`）**：
   - 原 `model/` 迁入 `foundation/types/` 与 `foundation/catalog/`；
   - 原 `application/orchestration/` 迁入 `foundation/orchestration/`，原 `application/card_access/` 迁入 `runtime/access/`；
   - 原 `infrastructure/account_store_adapter` 迁入 `topology/adapters/`，原 `infrastructure/assignment_store_adapter` 迁入 `assignment/adapters/`，原 `infrastructure/legacy_output_adapter` 迁入 `compatibility/output/`，底层无状态 `SimRdbHelper`/`TelephonyExt` 封装沉入 `foundation/platform/`。
2. **保留并强化 5 个核心目录 + 1 个基础内核（共 6 个一级目录）**：
   - `foundation/`、`topology/`、`assignment/`、`runtime/`、`compatibility/`、`assembly/`。

### 7.3 关键依赖约束、扩展点与反过度泛化护栏

1. **关键依赖约束（通过 `BUILD.gn` `visibility` 与头文件检查强制执行）**：
   - `foundation` 零上层依赖，严禁持有业务可变状态或领域缓存；
   - `topology/domain` 与 `assignment/domain` 仅依赖 `foundation/{types,catalog}`，不含任何 DB / RIL / IPC / FFRT 头文件（ARCH-RULE-006）；
   - `topology` 与 `assignment` 仅允许在 `application/` 层通过对方 `api/` 或窄端口（如 `AccountRoleProtocol`、`IsSetPrimarySlotIdInProgress` 查询）协作，严禁跨上下文包含 `domain/` 或 `adapters/`；
   - `runtime` 实现 `topology/ports`、`assignment/ports` 与 `runtime/access` 窄契约，内部协议组件统一经 `runtime/kernel/{session,records,resource}` 校验准入，严禁反向依赖 `compatibility` 或直写 T/A 缓存；
   - `compatibility` 无业务写权（ARCH-RULE-005），仅单向调用 `topology/api`、`assignment/api` 与 `runtime/access`。
2. **预留的最小必要扩展点**：
   - `foundation/catalog/slot_catalog.h`：集中承载槽数量、端点种类、VSIM/卫星受限槽与 MEP Port 拓扑解释（ARCH-RULE-010）；
   - `topology/ports/activation_runtime_port.h` 与 `assignment/ports/radio_execution_port.h`：隔离上层业务规则与底层 Modem/RIL 协议演进；
   - `foundation/platform/ext_hook_guard/`：约束厂商 `TelephonyExtWrapper` 扩展为受控查询/执行边界，禁止绕过 Owner 直写核心缓存（ARCH-RULE-007）。
3. **明确禁止提前引入的公共抽象**：
   - **禁止引入中央 `OperationCoordinator` 或跨系统持久化 `OperationAggregate` / Event Sourcing（TGT-OPEN-005）**：O 仅保留轻量 `RequestContext` 与运行期操作表，多步用例由目标事实 Owner（T 或 A）唯一主编排；
   - **禁止构建包揽 pSIM/eSIM 所有卡操作的万能 `ICardChannel` 大接口（TGT-OPEN-002）**：保留成熟 EF/APDU/安全/电话簿/STK 生产协议，按用例抽取窄端口；
   - **禁止在 `runtime/` 各卡侧协议组件内部强套 `{api,application,domain,ports}` 四层空壳模板**。

---

## 8. 架构决策与待验证事项

### 8.1 建议直接采纳的目录组织原则
1. **一级目录收敛为 6 个正交模块**（`foundation/`、`topology/`、`assignment/`、`runtime/`、`compatibility/`、`assembly/`），消除水平技术分层目录与垂直业务上下文目录在顶层并存的架构歧义。
2. **专属 Adapter 随业务 Owner 内聚，无状态底座沉入 `foundation/platform/`**：确保 T 和 A 的字段计划、存储协议与领域模型在同一目录内演进。
3. **`runtime/` 采用“治理内核（`kernel/`）+ 短请求入口（`access/`）+ 6 大生产能力子域”二级结构**：平顺承接原 `services/sim/` 全量卡侧代码，为最终物理删除 `services/sim/` 提供完备落点。

### 8.2 需要架构师进一步裁决的设计分歧
1. **`MultiSimMonitor` 的最终收敛形态**：建议保留在 `compatibility/monitor/` 作为统一的平台/Radio/文件事件监听与反注册接入点，将其内部原有的账户重载与状态写入拆交 T/A 应用入口（对应方案 C），需架构师确认是否维持单一 Monitor 监听类以确保初始化重试时序与旧实现严格等值。
2. **`SimAccountManager` 的命名与内部归属**：源码事实（F24）证明该类仅负责装配单槽 `OperatorConfigCache/Loader`，不管理多卡账户。建议迁入 `runtime/carrier_config/`，并由架构师裁决在移除 `services/sim/` 时是否同步更名为 `SlotCarrierConfigManager` 以彻底消除命名误导。
3. **独立共享库 `libtel_sim_service.z.so` 的拆分节奏（G2a vs G2b）**：需裁决是在原 `libtel_core_service.z.so` 内先通过 GN `source_set` `visibility` 完成 `services/multisim/` 全量切换（G2a），还是同步推进物理拆库并执行产物级 `NEEDED`/导出符号与停止排空门禁（G2b/G2c）。

### 8.3 尚缺少代码或设备证据的关键假设（待核实清单）
1. **MEP 双端口激活下的 `RadioProtocol` 硬件契约（对应 V2 中的 `?`）**：单物理槽双 eSIM Port 同时绑定两个 Modem 协议栈时，底层 RIL/HDI 的 `SetRadioProtocol` 是否需要扩展端口参数，需目标硬件与 RIL 仓实证。
2. **独立 eSIM SA `66250` 的远端任务恢复与本地通道互斥（TGT-OPEN-001）**：66250 服务仓的完整实现不在本仓，其下载/OSU 流程与 Core 内 `runtime/euicc/` 本地通道操作的并发交错需跨仓联调验证。
3. **数据提供方（DataShare Provider）的跨行原子事务能力（TGT-OPEN-004）**：外仓 `telephony_data_storage` 虽有事务调用线索，但与当前默认卡跨行更新的命令/schema 尚不匹配，近期必须保留按字段分步更新与原恢复条件，不能假定底层具备跨表/跨存储原子回滚。
4. **共享资源屏障 `ResourceBarrier` 的实机启用前提**：`runtime/kernel/resource/resource_barrier.h` 的拦截与隔离逻辑需在获得真实设备并发交错测试证据后再正式启用，防止改变既有超时与错误返回表象。

### 8.4 应坚定保留的现有核心设计（避免无意义地重新设计）
1. **保留主设计 v1.2 对 `T`、`A`、`O`、`R` 的责任切分与单写者裁决**：`T` 与 `A` 作为仅有的两个业务上下文，各自维护领域进行中状态（`ActivationProgress` 归 T，`AssignmentTransition` 归 A）；`O` 保持为轻量应用编排机制。
2. **保留 `SlotCatalog` 只读目录与 `AccountRegistry` 双投影模型**：继续沿用不透明 `simId` 契约（TGT-OPEN-006），通过 `SlotCatalog` 收口容量与特殊槽判断，通过双投影隔离按 `simId` 与按 `slot` 的查询视图。
3. **保留“Owner 逐节点接纳的兼容提交协议”（§9 / ARCH-RULE-009）**：继续保持 `SetShowNumber`、`SetActiveSim`、`SetPrimarySlotId`、`OnAllFilesFetched` 中原有的缓存更新、DB 写入、两次 `UpdateSubState` Hook 及通知先后顺序，不强行引入破坏外部可观测时序的伪全局回滚。
4. **保留原 `services/sim/` 成熟卡侧协议实现**：在迁入 `runtime/` 各二级能力目录时，直接复用 `SimStateHandle`、`IccFile/SimFile/RuimFile/IsimFile`、`UsimDiallingNumbersService`、`StkController`、`EsimFile` 的既有状态机与解析代码，仅在会话/加载准入（`SessionRegistry`/`LoadSession`）和请求关联处接入 `runtime/kernel/`。