# SIM 目标架构：源码结构与功能详细设计

| 项目 | 内容 |
| --- | --- |
| 文档版本 | 0.3，双模型独立裁决后的稳态组织与实际源码双视图（PROPOSED） |
| 更新日期 | 2026-10-30 |
| 源码范围 | 当前`services/multisim/`：73文件（50头/23源、4984行）；完整稳态还承接`services/sim/include`61头与`src`55源 |
| 输入基线 | 本地`main@e295540fe81ca7dd28f248d0b76470497062aa64`，编辑前工作区及暂存区干净；两篇目标文档无已有本地修改；两份报告的基线仅作历史 |
| 当前状态 | main中的73文件仍未接生产GN/装配/活动写权；§3.1/§21为实际源码，§3.2为尚未移动的目标；本轮仅修订两篇设计 |
| 权威范围 | 落实主设计的源码组织、功能映射与依赖规则；上位边界由SIM-TGT-001维护，公共契约和功能/WU清单仍由原文维护，未增加批准状态 |

**文件存在、语法可检查和有限纯规则通过，均不等于目标业务已经实现。** 原生产仍位于`services/sim/`。本文保留原21个显式导航锚点，区分当前与稳态目录、刷新方法/构造及实现状态；不据源码目录数量建立新的上下文、SO或进程。

源码内容摘要：`a344fbfc70b406013d0048e17a645561deead6b50c332a09f69fdcc713c88f7d`（SHA-256；按根目录内相对路径字典序，依次连接UTF-8路径、NUL、文件原始字节、NUL）。该摘要固定本文所述工作树，不代替最终Git提交号。

## 目录

1. [范围、依据与状态口径](#scope)
2. [整体结构与职责边界](#architecture)
3. [当前完整源码与最终目标目录](#source-tree)
4. [分层与依赖规则](#dependencies)
5. [身份、目标与槽目录](#identity)
6. [T：账户与连接拓扑](#topology)
7. [A：角色与资源分配](#assignment)
8. [应用编排机制与短请求](#application)
9. [R：完整生产卡运行支撑](#runtime)
10. [C/I：兼容入口、导出与技术适配](#integration)
11. [H：SIM侧装配与宿主契约](#assembly)
12. [业务流程：设置显示号码](#set-number)
13. [业务流程：号码查询与独立回写](#query-number)
14. [业务流程：普通SIM激活](#activation)
15. [业务流程：Primary切换与Data联动](#primary)
16. [状态、并发、失败与恢复](#consistency)
17. [原功能迁移与生产接入](#migration)
18. [IDE阅读与开发定位](#reading)
19. [未实现内容与局部设计缺口](#gaps)
20. [检查记录与验证限度](#verification)
21. [附录：逐文件功能及实现状态](#inventory)

<a id="scope"></a>

## 1. 范围、依据与状态口径

生产仍使用[`services/sim/`](../services/sim/)；目标源码位于[`services/multisim/`](../services/multisim/)。本轮只修改本文与目标主设计的目录/职责表达，没有移动源码、修改GN、转移活动写权、变更公开API/ABI/IDL/schema或修改评审/执行状态。既有纯规则不因此成为新生产实现。最终multisim完整替代sim；Attach/旧实现桥仅供迁移，稳态不保留两套生产执行路径。

依据为[目标主设计](SIM_Target_Architecture_Design_v1.2.md)、[流程对照](多SIM架构重构_设计说明与业务流程对照.md)及[独立评审记录](SIM_Target_Architecture_Independent_Review_2026-10-29.md)。[功能/WU清单](SIM_Architecture_Refactor_Design.md)继续维护SIM-001～062及子项和WU-01～09；本文解释当前源码，不另建目标主规范。

主设计§4.1/§4.2与§6.5的T/A事实Owner、O机制及安全裁决继续有效；本文§3.2/§4落实主设计§6.6。完整读取[Opus](SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md)及[Gemini](SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md)，同等对待建议，按外部契约→事实Owner→实际依赖→变化隔离→理解成本→调整成本裁决，见§4.3。报告合入不等于建议获批。

| 状态 | 本文含义 |
| --- | --- |
| 有限纯实现 | 确有可链接规则，例如绑定版本、字段接纳、观察资格、进度归属、Primary计划或资源状态；没有设备/平台接入 |
| 构造/绑定 | 实际C++表达依赖和窄结果出口，不表示工厂可创建并启动生产 |
| 声明/伪代码 | 有输入输出和步骤，但对应业务方法未定义，不能提供可用业务 |
| 计划复用 | 成熟能力仍在旧目录，本骨架不复制算法或暗中启用第二执行路径 |
| 外部缺口 | 匹配provider、厂商结束保证、66250任务、产品Port/并发证据等，仅阻塞实际依赖切片 |

<a id="architecture"></a>

## 2. 整体结构与职责边界

| 责任 | 唯一拥有的事实/规则 | 关键排除 |
| --- | --- | --- |
| T | 账户身份、current/historical资料投影、绑定与空位置revision、发现接纳资格、槽激活进度 | 原始设备状态、A角色、R设备租约 |
| A | 默认角色及独立投影、Primary/Data参数和意图、切换transition、业务重试 | 卡身份、完整数据连接、原协议session |
| O | 各用例私有请求/步骤/等待/效果/终态机制 | 业务进行中视图、统一发令或重试中心 |
| R | 原卡/安全/EF/Records/电话簿/STK/eUICC协议，原始观察、会话、加载成员与实际资源 | T/A事实的旁路写者 |
| C | 原入口、调用者、旧查询投影、同步/回调/内部完成分路 | 可独立写业务的镜像 |
| I | 跨Owner的技术连接/重连、参数访问等客户端；专属业务适配随Owner | 角色策略、账户缓存、虚构全局事务；不是全体Adapter的集中目录 |
| H | 对象身份、共享RIL、epoch、目录、端口装配、发布与匹配销毁 | 新业务Owner或第二套RIL/成熟对象 |

运行调用为`C → T/A用例 → R/I窄端口`，PIN/EF/本地Profile则进入R相应能力的具名应用入口；结果经`MsisdnResultSink/ActivationResultSink/RadioResultSink`返回发起者，R观察经`CardObservationSink`进入T。R不再引用具体应用服务。T/A跨Owner只用公开窄契约（入站由提供方发布、出站由消费方定义），父子请求不嵌套等待、不持有对方聚合。

<a id="source-tree"></a>

## 3. 当前完整源码与最终目标目录

### 3.1 当前工作树：已存在的完整文件清单

截至输入`e295540f`实际为**73文件：50头/23源，4984行**。统计递归包含`services/multisim/`下所有普通文件，行数按原字节分行；不混入其他worktree或G2a工作分支。目录计数为topology 27、assignment 14、runtime 9、compatibility 7、infrastructure 5、assembly 4、application 4、model 3。当前没有本区域BUILD.gn，生产GN未引用骨架，namespace仍为`OHOS::Telephony::MultiSim`；未实施下面的target或namespace边界。逐文件状态见[§21](#inventory)。

```text
services/multisim/
├── application/
│   ├── card_access/
│   │   ├── local_card_requests.cpp
│   │   └── local_card_requests.h
│   └── orchestration/
│       ├── request_context.h
│       └── runtime_operation_store.h
├── assembly/
│   ├── host_ports.h
│   ├── manager_publication.h
│   ├── sim_module_factory.cpp
│   └── sim_module_factory.h
├── assignment/
│   ├── api/
│   │   ├── assignment_requests.h
│   │   └── primary_execution.h
│   ├── application/
│   │   ├── account_role_protocol.h
│   │   ├── assignment_service.cpp
│   │   ├── assignment_service.h
│   │   ├── primary_switch_plan.cpp
│   │   └── primary_switch_plan.h
│   ├── domain/
│   │   ├── assignment_aggregate.cpp
│   │   ├── assignment_aggregate.h
│   │   └── role_selection.h
│   └── ports/
│       ├── assignment_store.h
│       ├── primary_topology_port.h
│       ├── profile_preference_port.h
│       └── radio_execution_port.h
├── compatibility/
│   ├── completion_cell.h
│   ├── legacy_boundary_ports.h
│   ├── legacy_completion.cpp
│   ├── legacy_completion.h
│   ├── legacy_profile_result.h
│   ├── legacy_sim_facade.cpp
│   └── legacy_sim_facade.h
├── infrastructure/
│   ├── account_store_adapter.cpp
│   ├── account_store_adapter.h
│   ├── assignment_store_adapter.h
│   ├── legacy_output_adapter.cpp
│   └── legacy_output_adapter.h
├── model/
│   ├── sim_identity.h
│   ├── slot_catalog.cpp
│   └── slot_catalog.h
├── runtime/
│   ├── records/
│   │   ├── load_session.cpp
│   │   └── load_session.h
│   ├── resource/
│   │   ├── resource_barrier.cpp
│   │   └── resource_barrier.h
│   ├── session/
│   │   ├── session_registry.cpp
│   │   └── session_registry.h
│   ├── card_runtime.cpp
│   ├── card_runtime.h
│   └── result_sinks.h
└── topology/
    ├── api/
    │   └── topology_requests.h
    ├── application/
    │   ├── activate_sim_use_case.cpp
    │   ├── activate_sim_use_case.h
    │   ├── activation_progress.cpp
    │   ├── activation_progress.h
    │   ├── discovery_observation_ledger.cpp
    │   ├── discovery_observation_ledger.h
    │   ├── set_show_number_use_case.cpp
    │   ├── set_show_number_use_case.h
    │   ├── show_number_plan.cpp
    │   ├── show_number_plan.h
    │   ├── show_number_query.cpp
    │   ├── show_number_query.h
    │   ├── topology_service.cpp
    │   └── topology_service.h
    ├── domain/
    │   ├── account_registry.cpp
    │   ├── account_registry.h
    │   ├── binding_index.cpp
    │   └── binding_index.h
    └── ports/
        ├── account_role_planning_port.h
        ├── account_store.h
        ├── activation_assignment_port.h
        ├── activation_runtime_port.h
        ├── card_observation_port.h
        ├── number_query_host_port.h
        ├── show_number_completion_port.h
        └── show_number_runtime_port.h
```

<a id="target-tree"></a>

### 3.2 唯一推荐的最终稳态树（计划，尚未移动源码）

一级按业务责任及支撑边界，二级按变化原因：T/A的纯模型/对外契约/平台Adapter保持可检查边界，用例、兼容计划与私有端口随场景内聚；R按卡能力组织。下列文件名为落点建议，不是存在性清单。`.h/.cpp`同场景就近放置，不机械套用全局include/src，也不改变原公共头的对外路径。BUILD/README及新api均待实施，不创建空业务成功壳。

```text
services/multisim/
├── BUILD.gn                         # 契约/纯规则/用例/适配分target，仍可同原SO
├── README.md                        # 计划的模块地图与开发导航，不是新主规范
├── include/                         # 仓内导出白名单；原公共API不复制
├── foundation/
│   ├── sim_identity.h               # 纯身份/目标值
│   ├── slot_catalog.h/.cpp          # H构建的只读能力目录
│   ├── request_ref.h                # 从Context拆出的epoch/Owner/Step/效果值
│   └── orchestration/               # 独立Application target，非值类型总依赖
│       ├── request_context.h
│       └── runtime_operation_store.h
├── topology/                       # T，唯一账户/绑定/资料Owner
│   ├── api/                         # 命令/查询/观察、跨模块执行与结果窄契约
│   ├── domain/                      # AccountRegistry、BindingIndex等纯规则
│   ├── discovery/                   # 发现/联合建行/独立重载、ledger与私有存储端口
│   ├── activation/                  # ActivateSim用例/进度、分支计划及私有端口
│   ├── profile/                     # 名称/号码/标签/IMS、查询回写及私有端口
│   ├── mapping/                     # 具名映射子请求，不成为独立事实Owner
│   ├── adapters/                    # T字段/存储映射、Hook与Host适配
│   └── topology_service.h/.cpp      # 唯一受控事实入口，不吞并各用例
├── assignment/                     # A，唯一默认角色/Primary/分配Owner
│   ├── api/                         # 角色/准入/Primary/偏好与跨模块执行契约
│   ├── domain/                      # AssignmentAggregate/Transition、纯角色规则
│   ├── roles/                       # Voice/SMS/Data用例及私有端口
│   ├── primary/                     # 兼容计划/参数恢复/唯一业务重试与私有端口
│   ├── account_roles/               # 联合建行、角色重载及T协作桥
│   ├── adapters/                    # 角色URI/参数与A输出适配
│   └── assignment_service.h/.cpp    # 受控事实入口
├── runtime/                        # R，有状态生产卡运行子系统
│   ├── api/                         # 卡能力入口/查询/观察与必要窄契约
│   ├── card_runtime.h/.cpp          # 可保留的薄委托/绑定接缝，无万能状态仓
│   ├── session/                     # 唯一会话代次、期望与可信接续
│   ├── resource/                    # 跨槽真实Modem/eUICC资源域
│   ├── state/                       # 原始卡态/PIN/PUK/认证及具名短请求
│   ├── records/                     # File/Controller/Refresh、LoadSession、EF短请求
│   │   └── codec/                   # 实证可独立的解码/TLV子集
│   ├── phonebook/                   # ADN/FDN、USIM PBR/ANR/IAP
│   ├── sms_storage/                 # 卡短信存储，非完整短信业务
│   ├── stk/                         # 主动命令、会话/终端应答及原输出
│   ├── carrier/                     # 配置/匹配/权限/APN及专属Adapter
│   ├── radio/                       # 三阶段/专用Primary/激活设备执行
│   └── euicc/                       # 本地Profile具名用例、通道/APDU/BPP/库存
├── compatibility/                  # C，持续兼容外部契约，不长期兼容旧实现
│   ├── api/                         # Core/H需要实现的入站/完成窄接缝
│   ├── legacy_sim_facade.h/.cpp     # 按account/card/esim等拆cpp，保留旧签名
│   ├── monitor/                     # 外部订阅/死亡清理/重连输入
│   └── output/                      # 完成、旧投影与通知翻译/交付
├── infrastructure/                 # I，仅实际跨Owner技术客户端，无业务缓存
└── assembly/                       # H，创建/注入/原发布顺序/匹配销毁
    ├── sim_module_factory.h/.cpp
    ├── manager_publication.h
    └── host_ports.h
```

这不是“所有一级目录都是状态Owner”：T/A是业务上下文，R是运行子系统，C/I/H是边界职责；`include`是导出视图，foundation是受限共享代码的物理父目录。保留8个一级目录并非沿用旧8目录，也不以减少数量证明解耦。三级只用于真实独立变化/依赖子集，当前仅示例编排机制和codec；不按文件数阈值强制拆层。纯codec target仅选其可独立源，不凭目录名宣称整个Records或MccPool无平台依赖。

### 3.3 原八个一级目录到稳态的调整

| 当前目录 | 目标调整 | 不随搬迁改变的边界 |
| --- | --- | --- |
| `topology/` | application按discovery/activation/profile/mapping归族，domain保留；私有ports随场景，对外契约入api；专属Adapter回归 | 账户/绑定/资料及激活进度仍归T，跨Owner经窄入口 |
| `assignment/` | application按roles/primary/account_roles归族；domain保留，专属Adapter回归 | Primary→Data、准入/恢复与唯一业务重试仍归A |
| `application/` | 撤除：关联值到foundation，容器/Context到foundation/orchestration独立target；短请求分别进入R能力包 | O仍为Application机制，不产生第三业务Owner |
| `runtime/` | 保留并完整承接成熟卡能力；CardRuntime收窄，session/resource唯一，load资格留records | 不复制Manager、共享RIL、会话及设备执行/重试 |
| `compatibility/` | 保留旧门面，区分宿主api、monitor与output | 旧API/错误/完成和通知节点继续兼容，不保留旧实现双写 |
| `infrastructure/` | 专属Adapter分别移T/A/C；只留下跨Owner技术连接/调用底座 | 技术客户端可有连接状态，不有账户/角色镜像 |
| `assembly/` | 保留工厂、发布、HostPorts；仓内可见入口由include精确导出 | Attach过渡与最终Owned模式分开，生命周期不变 |
| `model/` | 更名并入foundation的纯值/只读目录部分 | 不扩成DTO、聚合、平台工具或动态路由中心 |

新增`include/`和拟议BUILD/README承担导出/约束/导航；不存在新的运行期Facade转发层。当前到目标的具体文件映射见§6～11、§17、§21，不能把这里的计划路径当已存在链接。

<a id="dependencies"></a>

## 4. 分层与依赖规则

### 4.1 逻辑层与源码target边界（计划）

Domain只包含纯模型/值/规则。Application存在于T/A场景包、R能力包的短请求以及独立编排机制target；目录名不是逻辑层。当前具体成熟对象的借用只出现在R/I/H，不反向进入T/A。api发布窄入站命令/视图及需要外部实现的消费契约；同一场景私有port就近存放，仅被跨模块实现者使用时才选取导出，禁止包含另一模块私有场景/聚合/Adapter头。

| 拟议target边界 | 允许的源码依赖 | 必须阻止 |
| --- | --- | --- |
| `sim_values`（foundation平铺白名单） | 标准库、纯身份/关联值和只读目录 | O容器、CompletionPlan、平台、T/A/R/C实现 |
| `request_protocol`（foundation/orchestration内选取的协议头） | sim_values；CompletionPlan/RequestContext等无平台DTO/窄完成契约 | 可变Store、实际waiter/调用者对象、平台库或业务聚合 |
| `request_mechanism`（同物理目录内的容器实现） | request_protocol；Owner自有运行表 | 业务聚合、设备发令、重试、跨重启持久日志、平台等待设施 |
| T/A的`*_types`（api选取的纯契约） | sim_values；AccountReadView/DefaultRole等单一定义的纯枚举/DTO，整体成DAG | 私有聚合头、应用命令/Context、平台DTO |
| T/A的`*_contracts`（应用入站/执行契约） | 自身types、必要对方纯契约、request_protocol | 可变Store、具体Service/用例或可写聚合；不冻结“T永远高于A” |
| `*_domain` | sim_values、自身纯types及纯规则；不挂平台external_deps | 应用contracts、request_protocol/容器、callback/等待、RIL/DataShare/IPC/FFRT头 |
| `*_cases`（逻辑Application） | 本Owner domain、request_mechanism、所需T/A/R窄contracts | 对方cases/domain/adapters、R/I具体实现 |
| `*_adapters`、C输出 | 相应消费契约、实际技术客户端与平台 | 自有业务缓存、反向调用具体用例来隐藏代写 |
| R能力与治理实现 | sim_values、必要request_protocol/窄contracts，能力内用例可依request_mechanism及原平台/RIL接缝 | 具体T/A用例、C、重复共享资源Owner；纯codec不依赖整个R/RIL |
| C/monitor/facade | T/A/R及宿主窄contracts、C私有完成/输出 | 修改聚合或统一业务重试；公开权限校验搬动仍需逐入口差分 |
| H组合根 | 按构造需求连接具体实现及宿主 | 业务策略、第二对象/单例、未经排空卸载 |

api发布的是入口声明/窄服务facet，Service具体构造与聚合成员仍为核心私有、仅H装配；C持入口引用，不包含根部Service实现头。不要求每个用例新建虚接口，也不以入口声明重导出所有domain头。

当前号码/激活/Primary命令按值携带CompletionPlan，执行载荷还带RequestContext，故应用contracts必须允许依赖request_protocol，不能仅依sim_values；纯Domain仍不依赖这些命令。api现经account_registry.h/role_selection.h取得AccountReadView/DefaultRole，迁移时选取为Owner api下单一定义的纯types（Domain反向消费该纯类型），不能重导出整个私有聚合头或复制定义。

需至少分契约、纯规则/用例核心和平台Adapter，不能“每模块一个source_set”后声称domain已隔离。`deps/public_deps`仅传播必要契约，GN `public`白名单、`visibility`、`check_includes`与头扫描共同验证；没有external_deps本身不能防相对include偷越界。建议namespace为`MultiSim::Topology/Assignment/Runtime/Compatibility`，基础值与机制分别`MultiSim::Foundation`及`MultiSim::Orchestration`；现有骨架单namespace尚未改，namespace不是访问控制或运行准入。该命名建议只约束新的私有实现，原公共/仓内契约和成熟导出类型不为目录整齐机械改名或改变布局/符号，须按原ABI及消费者证据处理。

Runtime成熟能力存在真实内部互调，先可保持一个有界实现target（纯codec/表格与条件源另列），不因目录分包就宣称能力target已独立。后续拆target按实际头/回调依赖收口，例如carrier消费records快照，records的配置/语音信箱查询经窄消费端口反向注入，不为打破环制造新业务层。eSIM off/on条件矩阵不能简单把整个euicc目录排除，原EsimManager有disabled实现分支。

### 4.2 跨Owner调用、契约与注入


| 使用方 | 窄契约 | 实现/注入 |
| --- | --- | --- |
| T发现与重载 | AccountRolePlanningPort | A实现；H在A构造后、发布前BindRolePlanner，避免构造环 |
| T激活 | ActivationAssignmentPort | A实现旧有界等待、带owner/session/domain/token的新准入、正式释放/转交、偏好和复核；T不依赖AssignmentService |
| A主卡 | PrimaryTopologyPort | T实现绑定读取及映射子请求，A不依赖TopologyService |
| T号码/激活、A切换 | 运行端口及各自结果sink | R实现运行，应用实现sink，H绑定 |
| 本地Profile短请求 | LocalCardPort、ProfilePreferencePort | R执行、A接纳提示，H注入 |
| C完成 | CallbackHostPort、LegacyOutputPort | Core宿主/I执行，C解释旧出口 |

没有include环不等于target已合理。当前已用窄端口消除R到具体结果Owner、T激活到具体A服务及A到具体T服务的引用，但仍有T/A目录级双向include，下面的contracts/core拆分尚未实施。`show_number_plan`已从Domain移入Application，兼容步骤偏序不再仅因纯C++就归入Domain。

<a id="review-decisions"></a>

### 4.3 双模型意见裁决（PROPOSED，非审批记录）

两报告完整作为输入。共识不自动成立；差异先辨命名/目录深度与业务语义。下表为本轮唯一裁决清单，主设计只维护相应上位约束，不另写第三份规范。

| 议题 | Opus意见 | Gemini意见 | 最终裁决 | 原因与证据 | 影响文档 |
| --- | --- | --- | --- | --- | --- |
| 一级组织/最终替代（共识） | Owner优先，8目录含common/I/include | 6目录正交，取消I | 采纳责任优先，调整为§3.2唯一8目录 | T/A需保持事实边界；C/H不是业务Owner；目录混合标签或数量本身不证明缺陷。R完整承接，旧sim最终移除 | A§2/5/6.6；B§2/3/17 |
| T/A内部（差异） | 场景族+api/domain/adapters，取消集中ports | 统一application/domain/ports/adapters | 采用场景族，保留逻辑分层 | 当前号码和Primary步骤计划均在application；同一修改原因就近，domain零平台与adapters隔离靠target而非多层目录 | A§5.1.5/6.6；B§3/4/6/7 |
| common/foundation与O（差异） | common白名单含值与通用Store | foundation含types/catalog/O/platform | 部分采纳，统一foundation；值/编排分target，平台不入 | `RuntimeOperationStore::operations_`是Owner实例运行态，非零状态纯值；CompletionPlan不应成Domain公共依赖；不设全局O实例 | A§6.6；B§4/8 |
| RequestOwner（存疑） | 删除封闭枚举，各模块登记 | 保留请求机制 | 不直接删除；保留安全命名空间，禁止堆入具体业务步骤 | 当前枚举Assignment已覆盖多个用例，非每增用例必改；epoch+owner+序号及step/attempt是R租约/旧清理的必要资格。目录迁移不重写标识协议 | A§6.5.3/6.6；B§8 |
| Runtime能力/治理（共识及表面差异） | 能力直接二级，session/resource | kernel/access/telecom_apps+能力子域 | 采纳能力分包；不建固定kernel/access/telecom_apps层 | 会话/资源唯一；load资格归records。电话簿/短信/STK无已证明共同编排需再包一层，目录深度不产生隔离 | A§6.6；B§3/9 |
| CardRuntime（实质分歧） | 拆为各能力，另slot_runtime | 保留kernel受控门面 | 部分采纳：可保留薄委托，不建新万能SlotRuntime | 当前h持sessions/resources/loads/MSISDN关联/观察序列且实现4端口；应分责任，不能按槽复制共享RIL/eUICC/屏障或丢统一准入 | A§6.1/6.6；B§9/11 |
| 短请求（差异） | PIN/EF/Profile进各能力包 | runtime/access统一入口 | 采用能力内具名应用入口，不增加必经网关 | `local_card_requests.*`已有期望登记、A提示、双结果；其应用职责必须保留，不能因归R就变任意直通卡调用 | A§4.2/6.6；B§8/9 |
| 专属Adapter/I（共识及差异） | 专属回Owner，I收窄 | 全删I，底座入foundation/platform | 采纳Owner内聚，保留窄I技术客户端 | `legacy_output_adapter.h`实际3个独立class，不能误称已隐藏代写；技术helper有连接/错误状态，EXT是跨SIM/搜网进程单例，不能整类下沉/复制 | A§6.6；B§10/17 |
| ProfilePreferencePort（存疑） | 消费方在card_access，现位置错误 | 留A ports | 移入A api，保留A入站能力语义 | 本地短请求消费、A实现；提供方发布入站服务合理。问题是当前A ports混淆入/出站，不是必须另造消费者接口 | A§6.6；B§4.2/8 |
| C/Monitor（差异） | C平铺多cpp | facade/monitor/output，监控只读过滤事件 | 按真实职责区分api/monitor/output，Facade多cpp；不冻结单一Monitor或新过滤源 | 当前Monitor有账户重载/写入，需交T/A；原订阅/反注册、首帧和重试节点必须差分，不能为消除假想传播改变输入 | A§5.4/6.6；B§10 |
| T/A依赖（实质分歧） | 固定A api低于T api | 应用层互调窄api | 不固化永久高低；独立契约target成DAG、实现不互依 | 当前T存储载荷引用A片段，A服务实现T端口；调用双向不等于核心target循环，不共享可写聚合。纯DTO/端口拆出即可，非新上下文 | A§5.2/6.6；B§4 |
| GN/namespace（共识） | 每模块source_set+api | 细粒度target/visibility | 采纳约束目标，按契约/核心/适配及实际能力拆分 | 当前无multisim BUILD，同namespace；已有窄契约仍有收益，不能说解耦为零。一个全包target和namespace都不能阻止平台泄漏/越权写 | A§6.6/18；B§4 |
| 共享头/外部消费者（遗漏） | 新include，MccPool可下沉utils | 未完整展开导出 | 补导出白名单、独立解码/表格target；暂不全搬utils | RIL_SIM实际调用SIMUtils，搜网用MccPool，卫星innerkit含SimStateHandle；不能形成RIL→全R→RIL环或复制公共ABI | A§6.6；B§10.2/17 |
| 完整生产归属（遗漏/错误） | Tracker归state，探测拆T/R | Tracker归state、探测整类归uicc | 按实际职责修正并覆盖116文件 | `SimAccountManager::Init`创建配置Tracker；CardFileDetectManager直写/查DB；PdpProfile专属carrier；Plmn/VoiceMail并非纯codec；未装配cache_sync不启用 | A§6.2/6.6；B§9/17 |
| 未来变化（存疑） | 四卡集中目录/策略 | V1～V5零P、V5严格只改R、仅测本包 | 采纳变化轴分析，拒绝无条件零传播/缩回归 | RadioSwitchResult含phase/session；MEP、错误、结束/部分效果与外仓协议不明。静态Catalog不拥有动态绑定；以契约等值及资源共享证据限定 | A§6.6/17；B§9/17.4 |
| SO/生命周期（已裁决边界） | 与目录另行讨论 | G2a/G2b节奏重新待决 | 沿用A§22，无须重开门禁 | G2a原SO、G2b工程边界、G2c按产品需求排空；target和目录不自动独立SO/进程 | A§7/14/22；B§11/17.3 |
| 旧统计与位置（事实修正） | 73文件、F11计划位置、F12冲突 | 36头/15源及旧位置 | 当前重统计，采纳成立漂移 | 当前73=50h+23cpp，A§5.1.5旧分层路径已更正，B§18/21 Primary计划已定位application；报告56/62含sim/test，不全是生产 | A§1/5.1.5/6.4；B§1/3/20/21 |


<a id="identity"></a>

## 5. 身份、目标与槽目录

当前`model/sim_identity.h`计划进入`foundation/sim_identity.h`；其保留SubscriptionId、SlotId、RadioDomainId、分类目标、RuntimeEpoch、SessionRef和BindingRef。SubscriptionId仅包装旧simId；空槽/PIN/EF不强求账户，不从slot或标签生成身份。

SessionRef由R维护epoch、应用和bindingGeneration；BindingRef为T已接受位置绑定。BindingIndex空位置也有独立revision，移除不归零。同一卡再插必须使旧引用失效，未知槽不回退槽0。

SlotCatalog只表达配置端点、允许连接、可见性、容量及可运行期加入属性。条目的rilInstance/radioDomain是配置解释，不是当前Radio分配；当前绑定归T；设备原始Radio观察由R提供，A拥有接纳后的角色/分配视图。目录不证明对象已装配或支持更多并发。

Port映射到不同逻辑槽时，一槽一当前账户不必然要求新schema；确需一槽多当前绑定时再核对键、旧投影、provider与产品契约。旧GetEsimPortIndex是请求暂存，不作为可靠绑定证据。机械拆SO不强制先引入目录。

<a id="topology"></a>

## 6. T：账户与连接拓扑

当前实现表/协议如下，稳态位置统一为：`TopologyService`在T根部受控入口，AccountRegistry/BindingIndex留domain；ledger与发现/重载在discovery，ActivationProgress/ActivateSimUseCase在activation，号码/查询/计划及名称/IMS在profile，映射子步骤在mapping。带请求/session资格的兼容进度和存储偏序属Application，即使内部计算是纯C++也不机械移Domain。

`AccountStore`跨发现/激活/资料使用的纯载荷与外部实现契约导出到`topology/api/account_store.h`，按字段方法维护一个现有物理store，不按场景复制连接或缓存；场景专用请求/结果端口随场景，R/C需实现的部分再选取导出。这样减少寻址跳转，同时不制造并行Store或改变联合建行。

| 文件/类型 | 当前表达 |
| --- | --- |
| AccountRegistry / AccountProjection | 同一T下current/historical可空投影；name/number/active/label/IMS各自revision，身份另有identityRevision |
| BindingIndex / SlotRuntimeEntry | 已装配事实、当前BindingRef及空位置也持续的revision；同身份不隐式跨键双绑定 |
| DiscoveryObservationLedger | 固定模块epoch、每应用观察序列、发现与失效资格；R校验不替代T最终复核 |
| ActivationProgress | 独立slot/request/session进度，旧完成不能清新请求；不作为账户属性或资源空闲证明 |
| TopologyService | 唯一事实入口、资料/发现/重载/映射接口；业务IO仍声明/伪代码 |

AccountStore的LoadAccounts接AccountLoadRequest(view,attempt)，分别返回原读取的T行和StoredAccountRoleFragment。PrepareAccountReload保存相关身份、各字段及A角色投影版本；一个view失败保留该view旧值，另一个view仍可成功。纯AcceptReloadProjection对字段分别核对，无关改名不拒绝号码，旧重载不覆盖新字段。

发现协议为：同序接纳CardDiscoveryObservation→预留受影响旧/新键→锁外MatchAccountByIccid→A角色规划/资格→一次原WriteDiscoveredAccount→复核ledger/绑定键/A资格→T接纳provider身份/绑定，A接纳角色。新账户规划允许无simId，消除插入前后的因果环。写前及返回后经StillOwnsInitialRoleReservation校验角色资格，FinalizeInitialRoleReservation要求原调用结束、存储效果明确且投影已接纳或核对完成；Unknown或已写但失效不能只因调用返回就释放。

CardSessionInvalidation经同一T序列撤销原会话资格，绑定移除继续保存空位置revision。已经写库但观察失效时保留assignedSubscription和已发生效果进入核对，不能只拒绝内存发布后宣称无效果。

TopologyService构造后由H绑定AccountRolePlanningPort；它实现PrimaryTopologyPort与CardObservationSink，跨Owner不暴露可写容器。AccountStore不提供wholeAccount Save，不把字段预留当provider CAS。

<a id="assignment"></a>

## 7. A：角色与资源分配

**当前位置纠正：** `PlanPrimarySwitch`与PrimarySwitchPlan在`assignment/application/primary_switch_plan.*`，模式/参数位置/输入值在`assignment/api/primary_execution.h`，AccountRole协议在`assignment/application/account_role_protocol.h`；`assignment_aggregate.cpp`目前没有Primary计划实现。稳态分别进入primary、api、account_roles；聚合及纯角色规则留domain，Default角色用例在roles。保留逻辑Application/Domain，不把兼容步骤计划只因无I/O就移到Domain。

`AssignmentAggregate`区分角色约束、带transitionToken的切换事实和参数投影。纯PrimaryDecisionFacts显式输入userSet、专用Primary能力、当前主Modem观察及厂商换路需求，不从目录推断动态事实。

| PrimarySwitchMode | ParameterWritePosition | 原生产语义 |
| --- | --- | --- |
| ThreePhaseProtocol | BeforeDevice | 普通路径预写后协议，失败条件恢复 |
| WithoutModemRestart | AfterDevice | 专用RIL先设备，成功后保存 |
| AlreadyPrimary | WithoutDevice | 原已为M0分支不执行完整协议，仍保持原保存/Data节点 |
| VendorRouteMapping | AfterDevice | 经T映射子请求和厂商能力，旧早返回与后续步骤分别保持 |

`PlanPrimarySwitch`有有限纯实现；角色接纳、参数IO、实际切换与重试仍主要是声明/伪代码。`AssignmentService`经PrimaryTopologyPort、RadioExecutionPort、AssignmentStore、LegacyIntCompletionPort协作，并实现T角色规划/激活和本地Profile提示所需窄端口。

AccountRolePlanningInput允许existingSubscription为空；A先生成带reservation的字段片段，原Insert返回身份后分别接纳。加载返回StoredAccountRoleFragment原DB载荷，current/historical角色版本独立，不以A当前态反填旧读取。冲突资格覆盖存储调用，不能先读片段后任意整行写。

<a id="application"></a>

## 8. 应用编排机制与短请求

当前仍在`application/{orchestration,card_access}`，下述协议不因搬迁重定义。稳态基础身份/StepRef/EffectKnowledge抽到`foundation/request_ref.h`纯值target；Context/CompletionPlan在`foundation/orchestration`的request_protocol，RuntimeOperationStore在同目录另一个request_mechanism target；两者逻辑均为Application，后者依赖前者，纯Domain不依赖二者；C负责解释旧完成路由，真实等待设施由C/Core宿主实现，不进入纯值或Domain。

当前`LocalCardRequests::SwitchLocalProfile`已定义对抽象端口的登记→执行→原节点提示→结束等待顺序；PIN/EF仍仅声明，真实Manager/File/RIL未接入。稳态拆包保留原发起者签发的完整RequestContext，能力入口只消费，不各自以同一epoch/LocalCard命名空间从1重签序号；请求签发/实例寿命与回收资格须保持唯一，不能靠删枚举或分目录获得唯一性。

通用容器有可变运行态但无独立业务事实：每个进程所有者自己构造并在其短提交域使用，H注入epoch及已有Owner命名空间，禁止全局共享表自动发令/重试。现RequestOwner枚举保留完整安全含义，不采用未经定义的“各模块临时登记”；新增具体业务步骤留场景内，确需扩展Owner命名空间时须同时覆盖身份冲突、重建失效、旧回收及R租约反例，不在本次目录修改中默默删除保护。

PIN短请求落`runtime/state`，EF读写短请求落`runtime/records`，本地Profile用例/期望登记落`runtime/euicc`；各能力入口由runtime/api选取导出，不新建统一access路由。`ProfilePreferencePort`作为A入站能力移`assignment/api`由A实现，R的Profile用例依赖窄契约，不获取AssignmentService实现；T标签提示也经T入口，不由R直接写。纯PIN/EF无跨Owner效果时不用多加T/A跳转。

RequestId现含`value + epoch + RequestOwner`，StepRef另含step/attempt；共享R资源不能只比较一个局部序号。RuntimeOperationStore按epoch与Owner构造，Begin可携parent，AdvanceStep/RecordSent/AcceptResult声明受控推进，不允许外部改公共Context来绕过资格。

`CompletionPlan.route`使用CallbackCompletionRoute、SynchronousCompletionRoute、InternalCompletionRoute。LegacyIntCompletionPort向原出口交付int结果，不提供等待或万能Execute。等待只在C/Core适配层，不占据产生完成所需的T/A短提交域。

LocalCardRequests是具名短请求，不是新的业务上下文。SwitchProfileRequest同时保留legacyPortIndex、forceDisableProfile及可选VerifiedPortRoute；本地短请求负责登记/结束Profile期望，R签发会话并保留Unknown恢复责任。外层int和卡enum分开，下载/OSU仍由既有66250任务边界主编排。

<a id="runtime"></a>

## 9. R：完整生产卡运行支撑

R整体完整承接原State/File/Controller、安全、MSISDN、Records、SIM短信存储、电话簿、STK、运营商配置/APN/权限及eUICC能力；不能把这些能力都解释为稳态CardRuntime的成员和实现。当前ProductionSlotBindings显式借用原对象，CardRuntime仍集中持会话、资源、加载、MSISDN关联和观察序列；这是需收窄的骨架，不是最终万能Manager。稳态CardRuntime可仅作能力分派/结果绑定；协议/缓存和请求关联由对应能力组件拥有，session/resource按真实生命周期注入，不机械复制成熟类。原单一EsimManager内部已有普通/低优先级两套EsimFile，不创建第二Manager。

CardRuntime借同一ITelRilManager，普通Radio三阶段用原控制器，专用Primary路径沿共享RIL。激活旧串行主要来自activeSimMutex_跨等待及PrimaryDone同锁，ctx_在cv.wait期间释放，不能恢复为“全程持有静态ctx_”。

| 机制 | 已表达责任 | 当前边界 |
| --- | --- | --- |
| BindResultSinks | 四类窄结果/观察出口；不认识具体T/A用例 | 模块内绑定可见；真实事件关联、weak owner/epoch/排空仍待接入 |
| SessionRegistry | Current/失效、代次水位、ExpectedTransition、可信接续、NotSent撤销/等待结束/到期 | 当前session_registry.cpp已有有限纯规则；无真实卡事件接入；G3b须桥接旧用例，任意READY不足以接续 |
| LoadSession | 父子/必需可选/读写成员，一次消费，受影响读失效 | 有限纯规则；实际各卡型字段及LOADED写点仍在生产 |
| ResourceBarrier | 初始Checking、完整资源Acquire、MarkSent、NotSent撤销、Complete、Quarantine/Recovery | 有限纯状态转移，不证明设备结束能力，当前未启用 |
| 发现观察流 | 每应用发现/失效共observationSequence，送T同一接纳序列 | 发送前在原写点准入；跨T的DB返回还须再次复核 |

资源租约核对epoch、完整资源集、fence及完整StepRef。未发不能伪完成，已发不能伪NotSent；写Unknown不释放。只读查询在证明结束和可复用后可释放资源，但仍保留原失败结果；开通道Get也不能绕过冲突域。

### 9.1 稳态能力与治理的接口边界

- `session/`维护每卡应用唯一会话水位及期望归因；`records/`维护LoadSession、EF成员/刷新闭包和卡缓存；`resource/`维护按真实设备共享域的唯一租约。能力组件使用这些机制，不另建各自同义代次/设备锁。
- `state/`拥有原始卡态/锁因、PIN/PUK/认证，保留SenseSwitch/合成状态原过滤；`SimStateTracker`实际处理配置变化，归carrier。R原始观察到T最终接纳的共序/失效协议继续有效。
- `records/`保留卡型File/Controller与LOADED真实写点；`codec/`仅承接可独立解析/编码（如sim_char_decode、sim_number_decode、TagService及SIMUtils可分离部分）。MccPool保留records内独立表格target，含FFRT/懒初始化；PlmnFile含Parcel、VoiceMailConstants调用配置/EXT，留有依赖的records实现，不进入纯基础层。
- `phonebook/`、`sms_storage/`、`stk/`分别拥有PBR/ADN、卡短信、STK会话缓存；经records的受控EF/TLV能力协作，不能T/A复制协议。`carrier/`承接配置Tracker、匹配、证书权限和APN/PDP专属适配；records回查配置经窄端口，避免与carrier具体实现构成target环。
- `radio/`只执行三阶段/专用Primary/激活设备协议，A保留参数及业务重试。`euicc/`内单一EsimManager的普通/低优先级File共资源域，短请求保留卡enum/外层int、原port/force/提示节点；不复制66250高层任务或新的远端客户端实例。
- `CardFileDetectManager`原含数据库持久资料探测，须按R原始输入、T adapter翻译和T最终接纳拆分，不整类移records。协议状态仍沿原对象/事件链；能力目录不等于已获得独立并发、target或SO。

<a id="integration"></a>

## 10. C/I：兼容入口、导出与技术适配

LegacySimFacade保留原入口差异和签名语义。号码仍先受理/排队、后callback；普通激活和Primary执行点返回原int，通过宿主先登记waiter再发起，允许结果先于Await到达。同步返回不能统一解释为所有设备已完成，第三卡旧早返回单独保持。

LegacyCompletion实现ShowNumberCompletionPort与LegacyIntCompletionPort；原通知条件和int交付分开。当前legacy_output_adapter.h中LegacyOutputAdapter、AssignmentOutputAdapter、ActivationHookAdapter是三个独立类，并非已有统一代写中心。稳态原完成/通知翻译在`compatibility/output/`，A输出Adapter和T激活Hook分别随A/T adapters，仍只交付Monitor/宿主/平台结果、不持业务缓存；CallbackHostPort保存真实调用者/回调/同步等待/内部接收者。

| 现有适配契约 | 真实使用约束 |
| --- | --- |
| AccountStoreAdapter | 独立Current/Historical旧查询；同次读取的T行与角色片段分别接纳；Match/联合发现/Removal沿原helper，不能整行反灌 |
| AssignmentStoreAdapter | 原默认卡命令、参数预写/恢复/Data/偏好；不统一传播原来忽略的参数或广播错误 |
| LegacyOutput/AssignmentOutput/ActivationHook | 原通知、阶段、两次Hook和原返回节点；void不表示硬件确认 |

平台/IO方法仍声明或伪代码，无实际provider替换或第二缓存。DisableProfile相关标签恢复提示与旧无后缀输出仍按主设计§13.2的Owner及原节点处理，不从参数名推导新业务事实。

### 10.1 Adapter和平台输入的归属

AccountStoreAdapter与字段翻译在`topology/adapters`，AssignmentStoreAdapter在`assignment/adapters`；carrier的PdpProfileRdbHelper随carrier。跨Owner共享的DataShare连接/重连/技术CRUD可留infrastructure，业务字段计划不下沉。现SimRdbHelper混合语义不能整类当纯底座；TelephonyDataHelper有连接/错误状态，不应写“无状态”，但不持T/A事实缓存。共享底座升级可必要地影响多个Adapter，不能借此把它们再收拢成业务集中点。

C monitor保留原登记、首帧、死亡清理、重连及预算，把账户/角色重载交T/A、卡加载交R。不新增统一监听总线、不要求保持一只万能Monitor，也不未经事件差分强制只订阅过滤后的R事件。权限、调用者捕获及原排队节点留原层或等值C/宿主适配，不能因目录移动重新验证为服务身份。

### 10.2 include与仓内导出：不产生第二套公共契约

| 现有外部消费者/头 | 稳态承接 | 验收限制 |
| --- | --- | --- |
| Core的sim_manager.h/esim_manager.h | 私有工厂/宿主头从include白名单导出；消费接口仍为原ISimManager/IEsimManager | 不能要求Core装配内部Controller；原控制块、发布/销毁和eSIM off/on行为保持 |
| Core/搜网/RIL的sim_constant.h；RIL的sim_data_type.h | 保留既有必要声明的单一定义与精确导出，按语义拆内部常量 | 不复制公共枚举/错误码/ABI；不暴露可写状态或全部Runtime头 |
| RIL_SIM的sim_utils.h | 可独立解码/字符串辅助target及窄导出，R与RIL共同依赖 | 去除非必要头传递后证明无RIL依赖；不允许RIL→整个Runtime→共享RIL环 |
| 搜网的mcc_pool.h | records表格工具独立target和导出头 | 现有FFRT/懒初始化有技术依赖，不称纯值；移到公共utils须额外消费者/链接证据，不在本轮强制 |
| innerkits的operator_config_hisysevent_enum.h、Core的operator_config_matchsim_info.h | carrier契约枚举/DTO窄导出 | 保持原布局/符号及公共头路径，不导入carrier实现 |
| satellite_service_client.h的sim_state_handle.h | 当前具体头依赖记录为需收口的仓内技术债，迁移切片经窄状态/回调契约保留行为 | 不以目录文档擅改公开虚表；移除旧include路径前必须有实际卫星Feature构建证据 |

`include/`是导出入口，不另复制类型定义；通过GN public头/forwarding或原innerkits契约引用维持单一声明。内部api只服务multisim协作，不能把整个api树自动对仓外安装/公开。TelephonyExtWrapper留原进程共享定义，通过HostPorts/窄适配引用，不把它搬到foundation或在新SO复制。

<a id="assembly"></a>

## 11. H：SIM侧装配与宿主契约

当前两种工厂用途已明确分开，稳态仍由assembly创建/拥有单套生产对象，仓内创建/HostPorts入口按include白名单导出；Attach是过渡契约，不成为长期依赖旧sim的边界：

| 契约 | 输入/输出及拥有关系 | 实现状态 |
| --- | --- | --- |
| AttachSimModuleToExisting | 同SO迁移附着；借用既有store/monitor/radio/slots和单一euicc；使用H的catalog与epoch | 工厂/deleter仅声明；私有构造仅表达注入，不旁路双装配 |
| CreateOwnedSimServices | G2b最终SIM侧创建原对象；只收host、LegacySimStartup、SimServicePublicationPort，返回原ISimManager/IEsimManager句柄及匹配拥有者 | 工厂/销毁仅声明，不要求Core取得内部Controller/store，也不强制先建目录 |

SimModule构造次序为I/C出口→R→T→A→号码/查询/激活/短请求→Facade。A仅依赖T窄端口；构造后BindRolePlanner(A)，随后R BindResultSinks(号码、激活、A、T)，未完成必要绑定不得发布。不能靠双向强引用弥补构造环。

Core仍创建并拥有共享RIL；最终工厂按原eSIM创建/Init/发布、SIM创建/Init/发布时点回交Core，搜网/CMI仍留Core。原OnInit false未传播的地方不因Stage诊断就新增失败出口。

原接口没有虚析构时，由生产具体类型控制块或匹配deleter销毁，不追加公开虚表。业务导出为私有创建/匹配销毁，工具链必要符号另列；不宣称绝对只有两个ELF符号。常驻拆分和StopAcceptingAndDrain增强分开，排空失败不卸载，SIM不Stop共享RIL。

<a id="set-number"></a>

## 12. 业务流程：设置显示号码

当前阅读入口为LegacySimFacade→SetShowNumberUseCase::StartAtExecutionPoint。稳态源码依次为C门面/输出→`topology/profile/`（用例、ShowNumberPlan、查询/私有port）→T根部事实入口/domain及`topology/adapters/`；R端口实现在`runtime/records/`。跨模块R/C仅包含T导出的api结果/执行契约，不反向包含profile具体用例。保留原IsValidData/FileManager/错误优先级，不新增号码格式限制。R ReadMsisdnCache得到原session/tag，T PrepareNumberTarget记录BindingRef、身份及两个号码字段版本，O登记具名请求。

运行偏序：`R写MSISDN → T账户DB → Current号码投影 → Historical号码投影 → C原完成`。PlanShowNumberUpdate已在Application；NumberAcceptance独立记录DB/两个缓存结果。

卡成功而DB失败不回滚卡、不重放写卡；当前成功而历史失败保留部分可见效果；只有原整体成功才发适用账户通知，随后原callback。通知交付失败不反转卡/DB事实。

等待超时内部保留Unknown，晚到可信结果只供原R会话观察/核对，不继续已结束T账户步骤或重复成功完成。实际ProcessSetMsisdnDone写点必须先准入，不能先改共享lastMsisdn缓存再在外层丢弃。

G2保持旧任务执行时定位File、写库步骤才解析simId；当前执行点固定目标和陈旧拒绝属于G3b，不能直接宣称等价替换。IPC接收时提前固定另行裁决。

<a id="query-number"></a>

## 13. 业务流程：号码查询与独立回写

稳态`topology/profile/show_number_query.*`消费R records窄视图、T资料和Core Host IMPU端口（T adapter/HostPorts注入），不依赖Core实现。ShowNumberQuery保留卡缓存→账户→Core IMS IMPU原读源、错误与立即返回，不等待后台回写。NumberBackwrite保存原binding/session、身份和current/historical号码字段版本；改名或IMS偏好变化不应废除合法号码资格。

独立回写执行时由T复核并预留字段，拒绝后不补写新卡账户。该回写是新请求，不通过AcceptLateEvidence复活已终结设置请求。原闭包仅捕获slot/号码并在执行时找账户的行为作为G2基线，固定与陈旧拒绝单列G3b。

QueryImsPreference仍按ICCID走原存储读源，不因T拥有资料就改成统一内存查询。

<a id="activation"></a>

## 14. 业务流程：普通SIM激活

稳态`topology/activation/`含用例/进度及分支计划，设备执行落`runtime/radio/`、Hook落T adapters、A冲突资格落account_roles/primary，原完成交C output。ActivateSimUseCase只依赖ActivationAssignmentPort，不引用具体AssignmentService。T拥有active投影和ActivationProgress；A接纳入口偏好、Primary准入/后续复核；R执行；I执行原Hook；完成经LegacyIntCompletionPort分路。

普通non-force仍为：进度置位→原4s有界等待Primary（到期可继续）→RIL→Current/Historical缓存→第一次UpdateSubState/清进度→DB→第二次缓存/Hook→A主卡复核→最终Hook→原通知/Registry及返回。DB失败保留前面效果，不统一持久后发布。

ActivationProgress以epoch、请求Owner、step、原session和slot控制开始/清理；只允许有效Activation或旧LegacyBridge请求，value为0或其他Owner不能建立资格。进度不是AccountRecord字段。清零可在DB之前，false不证明设备或共享资源空闲；旧请求不能清新槽操作。

普通force仍先RIL但不新增账户写；卫星非force的DB/缓存顺序和force仅Hook分别保持。完整设备/完成路径仍未实现，不能因纯进度规则已通过就称激活已接入。

<a id="primary"></a>

## 15. 业务流程：Primary切换与Data联动

稳态`assignment/primary/`内聚兼容计划、步骤/参数恢复及唯一业务重试；roles承接Data保存，T mapping子请求与R radio执行经窄api注入，A adapters处理参数/输出。A读取PrimaryTopologyPort的绑定/ICCID/active和R的PrimaryRadioObservation，按PrimaryDecisionFacts生成模式计划。SlotCatalog只提供静态允许连接，当前Modem观察由R提供并由A接纳。

普通预写、免重启后写、已为M0无完整协议、第三卡映射子请求分别沿原返回节点。参数恢复不证明硬件回滚，Primary广播后Data保存不表示数据连接/驻网完成；旧忽略参数/广播错误的语义不统一改成失败。

AssignmentTransition带token，结束须匹配原资格；R错误和成功均先核对session/phase/member/step。A唯一授权同层业务重试，Unknown先核对，关闭旧Controller重试和detached CMI自调用后才启用新发令。

G2的WaitForLegacyPrimaryBeforeActivation只返回旧等待结果，不产生新租约。G3的AdmitActivationBeforeDevice接ActivationConflictRequest(step,sentSession)，返回ActivationConflictLease(owner,sentSession,domain,token)。ReleaseActivationConflict校验原租约与设备结束证据；Unknown交TransferActivationConflictToReconciliation保留责任，不能以进度false或一次bool查询代替登记/释放。

T激活等待、A切换资格和R设备租约是不同约束。跨Owner子请求有parent，不能拿一个Owner资格同步等待另一个Owner；相关冲突集合在唯一准入处一次登记。并发拒绝/排队/隔离为G3c行为增强，G2仍保持原轨迹。

<a id="consistency"></a>

## 16. 状态、并发、失败与恢复

| 层面 | 独立表达 |
| --- | --- |
| 接纳/效果/投影/存储/终态/交付 | 不合并为一个成功布尔，返回成功不证明网络可用 |
| current/historical投影 | 独立字段版本、加载attempt、失败和重试；同一Owner不要求同节点一致 |
| 发现跨Owner | R发起时会话和共序观察→T ledger→锁外Match/角色/存储→T最终复核；已写但过期仍须核对 |
| 字段冲突 | 预留覆盖锁外I/O，相关字段或角色域排序；不以拒绝迟到内存发布掩盖数据库已被覆盖 |
| 进行中/等待/租约 | 兼容进度可先清零，等待可结束，未知效果和资源责任仍保留 |
| 生命周期 | epoch、weak owner、在途计数、反注册和排空共同保护；引用计数不是卸载证明 |

重启从原存储、参数和可达设备事实重建，不恢复不存在的操作日志或同一任务ID。R资源Checking/Quarantined的解除依赖真实结束及可重用证据，预算耗尽保留局部隔离，不无限保留调用者/密码/BPP。

<a id="migration"></a>

## 17. 原功能迁移与生产接入

### 17.1 功能承接对照

| 原功能/来源 | 目标代码职责 | 保留或拆分方式 |
| --- | --- | --- |
| Core/SimManager初始化、槽能力，SIM-001～004 | H私有工厂、C桥、T运行清单、SlotCatalog | 不改变共享RIL、原发布时点和公共入口 |
| State、PIN/PUK、认证/IO，SIM-005～011 | `runtime/state/`内协议与具名应用短请求 | 不要求simId，不复制协议/剩余次数 |
| File、Records、Refresh及卡记录，SIM-012～019 | `runtime/records/`、`records/codec/`及`runtime/session/` | 保留卡型/解析/原LOADED路径；不把加载放入T事务 |
| InitData/InitIccId/账户缓存，SIM-020～022 | T TopologyService/AccountRegistry/BindingIndex | 身份/账户归T，建行角色由A片段提供 |
| 名称、号码、查询回写，SIM-023及子项 | T资料/号码用例，R MSISDN，I字段存储，C完成 | 名称不写卡，号码先卡后库，查询回写独立 |
| 普通/force/卫星激活，SIM-024 | T激活用例；A准入/偏好；R设备；I Hook | active与进行中归T，各分支不统一事务化 |
| Voice/SMS/Data/Primary/Radio，SIM-025～030 | A AssignmentService/Aggregate，R协议 | 保留参数预写/条件恢复/重试与Primary→Data |
| 标签、资料、映射开始，SIM-031及子项 | T资料/映射用例；R软件状态 | 映射用例/厂商端口尚缺，人工ABSENT不证明物理拔卡 |
| 回调、Observer、重连/用户切换，SIM-032～035 | C登记、I技术token、T账户加载、R卡加载 | 当前只展开号码完成；完整订阅壳后续补齐 |
| 卡短信、电话簿、STK，SIM-036～040 | `runtime/sms_storage/phonebook/stk`各自能力 | 不迁入完整短信业务或联系人业务 |
| carrier/APN/权限/运营商规则，SIM-041～045相关项 | `runtime/carrier/`及专属APN Adapter；T profile仅IMS偏好 | 不按SimAccountManager类名整类迁T |
| 下载/OSU，SIM-046 | 原EsimServiceClient/66250任务边界 | Core仅本地原语，远端完整实现仍需匹配证据 |
| 本地eUICC，SIM-047～054 | `runtime/euicc/`原EsimManager/File/Controller及具名短请求 | 保留一个Manager内部普通/低优先级File、卡侧结果/通道/缓存 |
| DataShare/探测，SIM-055～056 | I store；T/A字段接纳；R探测 | 不直接建立新RDB或独立写缓存 |
| EXT/VSIM/卫星、serial、CMI等，SIM-057～060 | I/H/C及相应T/A/R Owner | 保留宿主门面/厂商ABI，按真实切片补端口 |
| FactoryReset、未装配缓存源，SIM-061～062 | 原网络Owner；缓存源不自动启用 | 不扩展为删Profile或新增活动writer |

这是一张源码责任定位表，完整功能断言、编号子项与审批条件仍以原功能/WU清单及主设计为准。

#### 17.1.1 原生产头/源完整覆盖（当前文件，目标落点为计划）

按`sim/include/*.h`与`sim/src/*.cpp`枚举共116文件（61头/55源），每文件在下表归组一次，数量是迁移来源计数，不是推荐新文件数。根BUILD实际引用54源；其中3源在eSIM条件分支，EsimManager本身始终列源、内部有disabled实现。`sim/test`的头/源/GN不计生产，报告56cpp/62h不能当本轮事实。混合职责类标明拆分，覆盖不等于整类搬迁或运行验收。

| 原文件stem（include/src分别以.h/.cpp存在者计） | 头/源 | 稳态主要落点 | 处理边界 |
| --- | --- | --- | --- |
| `icc_state`、`sim_state_handle`、`sim_state_manager` | 3/3 | runtime/state | 原始卡状态、安全与PIN/PUK；协议留`runtime/state/` |
| `icc_file`、`icc_file_controller`、`sim_file`、`sim_file_controller`、`sim_file_manager`、`sim_file_init`、`sim_file_parse`、`isim_file`、`isim_file_controller`、`ruim_file`、`ruim_file_controller`、`csim_file_controller`、`usim_file_controller`、`plmn_file`、`voice_mail_constants`、`sim_char_decode`、`sim_number_decode`、`sim_utils`、`tag_service`、`usim_function_handle`、`mcc_pool` | 21/20 | runtime/records（含codec/表格子集） | 各卡型EF/Records/Refresh与MSISDN；解码子集进codec，表格独立target；不把所有辅助类视为纯函数 |
| `sim_account_manager`、`sim_state_tracker`、`operator_config_cache`、`operator_config_loader`、`operator_config_hisysevent`、`operator_config_hisysevent_enum`、`operator_config_matchsim_info`、`operator_file_parser`、`operator_matching_rule`、`icc_operator_privilege_controller`、`icc_operator_rule`、`pdp_profile_rdb_helper` | 12/10 | runtime/carrier | 运营商配置/匹配/权限/APN；Tracker实际归配置，PDP helper为专属适配 |
| `icc_dialling_numbers_cache`、`icc_dialling_numbers_handler`、`icc_dialling_numbers_manager`、`usim_dialling_numbers_service` | 4/4 | runtime/phonebook | 电话簿PBR多级加载/ADN/FDN |
| `sim_sms_controller`、`sim_sms_manager` | 2/2 | runtime/sms_storage | 卡短信存储及原错误/剩余空间 |
| `stk_controller`、`stk_manager` | 2/2 | runtime/stk | STK主动命令/会话/终端响应及原EXT/CES出口 |
| `radio_protocol_controller` | 1/1 | runtime/radio | R设备协议；A持参数/切换与唯一业务重试 |
| `esim_controller`、`esim_file`、`esim_manager` | 3/3 | runtime/euicc | 单一Manager内普通/低优先级File，协议不复制 |
| `sim_manager` | 1/1 | compatibility + assembly | 公共门面部分归C，创建/初始化发布归H，设备原语委托R |
| `multi_sim_controller`、`multi_sim_helper`、`multi_sim_monitor` | 3/3 | T/A/C/R按责任拆分 | 按账户T、角色A、订阅C、执行R拆方法，不整类迁移 |
| `sim_rdb_helper`、`sim_rdb_info`、`card_file_detect_manager`、`telephony_data_helper` | 4/3 | topology/adapters + infrastructure（及R输入） | 探测原输入R；字段/DTO翻译T adapters；技术连接I；角色片段仍由A接纳 |
| `sim_account_callback_death_recipient`、`start_osu_result_callback` | 2/2 | compatibility/monitor + output | C monitor/output保留死亡/回调；OSU服务任务仍属既有66250，File本地执行归R |
| `sim_constant`、`sim_data_type` | 2/0 | include（原声明单一定义） | 单一定义精确导出；内部常量按能力私有化，不新公开ABI |
| `sim_cache_sync_manager` | 1/1 | 未装配来源，不自动启用 | 当前未装配，不自动进入活动构建；保留来源追溯后按独立启用/清理决定处理 |
| **合计** | **61/55** | **116来源文件各一次** | **不是已迁移数量** |

关键分解依据：SimAccountManager::Init装配OperatorConfigCache及SimStateTracker（后者创建Loader）；权限控制由HasOperatorPrivileges按原调用创建。Tracker的ICC_LOADED/OPKEY回调属于配置责任。UsimFunctionHandle是UST指针/长度包装，TagService用于SimFileParse及USIM PBR。PlmnFile有Parcel，VoiceMailConstants读取配置/EXT；不将名字含Constants/Pool就当纯基础值。CardFileDetectManager与SimRdbHelper实际有持久化业务字段，必须保持一次原物理建行/更新、T/A分别接纳，不用两个Adapter各重写一次整行。

### 17.2 按WU覆盖全部写入口

| 写权单元 | 目标责任 | 迁移必须同时检查的入口 |
| --- | --- | --- |
| WU-01 | T当前/历史账户与绑定 | InitIccId、建行、缓存排序、GetList/GetAllList、Monitor/卡探测重查 |
| WU-02 | T active投影 | InitActive、普通/force/卫星、UpdateDBActiveByIccId、ForgetAllData及重载 |
| WU-03 | A Voice/SMS及角色字段 | 默认卡setter、建行片段、当前缓存/去重值、重载，不能由T整行回灌 |
| WU-04 | A Primary/Data/用户偏好 | InitMainCardSlotId、Save/Resume、last/广播去重、Core入口及EsimManager偏好 |
| WU-05 | T名称/号码/标签/IMS资料 | InitShowNumber、设置、查询异步回写、eSIM建行、卡探测、用户/provider重连 |
| WU-06 | R状态/Records/卡缓存 | 物理和软件输入、卡型File完成、Refresh、ClearData、LOADED回写 |
| WU-07 | 各用例步骤；T激活/A切换进行中 | 超时、旧重试、全部detached自调用、回包、失败恢复；O不接管业务字段 |
| WU-08 | R eUICC资源与缓存 | 普通/低优先级File共域、查询开通道、关闭重试、Profile结果与库存 |
| WU-09 | C登记/I token/T账户加载/R卡加载 | 首帧、死亡清理、用户切换、服务重连和在途注销，不保留双writer |

### 17.3 生产接入边界

先形成可验证的原行为切片，再按WU收口入口与唯一Owner。G2依赖治理保持旧解析时点、错误及原活动对象；G3按方法单独验收固定目标、准入、资源隔离与重试收紧。骨架不能直接作为完整G2替换实现。

接管前停止受影响受理并排空或隔离在途，导入原语义字段状态后一次转移写权，关闭旧独立writer及重复发令。只读影子比较只使用无副作用输入/纯规则，不能双发设备、双写DB/参数或双发Hook/通知。

本批没有新增生产构建目标、SA、进程、schema、厂商ABI或运行期双实现开关。是否拆私有SIM SO是工程部署决策，不由源码目录数量决定。

### 17.4 长期变化与测试传播

主设计§6.6的六场景是条件性隔离分析，不是设备功能承诺。四卡需同时核对SlotCatalog/装配、T绑定、A角色候选/资源域、R协议硬编码、C权限投影；逻辑端点数、Port数、激活配额和Modem栈数分别建模，不能只扩数组。MEP动态Port/Profile绑定由T接纳R观察，catalog不保存当前映射；新增一槽多绑定是否改schema依外部契约，非目录决定。

初始化/激活新硬件主要触及R state/records/radio和T activation；Primary握手/恢复主要触及A primary及R radio/resource。若设备协议下沉RIL，可优先替换R Adapter，但现RadioSwitchResult含phase/protocolSession，只有返回、错误、部分效果、通知/完成等值时才可认为T/A/C稳定；功能上移也须先证明底层原来承担的责任，不能虚构掉卡回退归属。共享会话、资源与条件编译会扩大必要回归，不能预先承诺“潜在传播为零”或“只测一个目录”。实际验证按切片依赖、原可见轨迹和Feature矩阵选择。

<a id="reading"></a>

## 18. IDE阅读与开发定位

1. 从[SimModule装配](../services/multisim/assembly/sim_module_factory.cpp)及两工厂声明区分附着、拥有与发布。
2. 看[LegacySimFacade](../services/multisim/compatibility/legacy_sim_facade.h)和[完成路由](../services/multisim/application/orchestration/request_context.h)，区分号码受理/callback、同步int与内部回执。
3. 沿[T号码用例](../services/multisim/topology/application/set_show_number_use_case.cpp)→[应用计划](../services/multisim/topology/application/show_number_plan.cpp)→[账户接纳](../services/multisim/topology/application/topology_service.cpp)→[字段/发现存储](../services/multisim/topology/ports/account_store.h)。
4. 看[AccountRegistry](../services/multisim/topology/domain/account_registry.cpp)、[BindingIndex](../services/multisim/topology/domain/binding_index.cpp)、[观察ledger](../services/multisim/topology/application/discovery_observation_ledger.cpp)和[激活进度](../services/multisim/topology/application/activation_progress.cpp)的有限纯实现。
5. 看[A模式计划](../services/multisim/assignment/application/primary_switch_plan.cpp)和[运行端口](../services/multisim/assignment/ports/radio_execution_port.h)，再回A应用伪代码，勿将一个普通分支推广为所有Primary路径。
6. 沿[R窄结果桥](../services/multisim/runtime/card_runtime.h)、[LoadSession](../services/multisim/runtime/records/load_session.cpp)、[资源转移](../services/multisim/runtime/resource/resource_barrier.cpp)对照原生产真实写点。
7. 最后看[本地Profile短请求](../services/multisim/application/card_access/local_card_requests.cpp)、SessionRegistry期望和A提示出口。

| 要改的功能 | 稳态主入口（计划） | 协作边界 |
| --- | --- | --- |
| 发现/重载/身份或绑定 | topology/discovery + domain | A account_roles、R session/records、T adapter |
| 号码/名称/标签/IMS | topology/profile | R records、T adapter、Core IMPU Host、C output |
| 激活/force/卫星 | topology/activation | A准入、R radio、T Hook、C同步完成 |
| 默认角色/Primary/Data重试 | assignment/roles + primary | T mapping、R radio/resource、A参数/输出 |
| PIN/EF/本地Profile | runtime/state、records、euicc各自具名入口 | session/resource；Profile另有T/A提示与双结果 |
| 卡文件/电话簿/短信/STK/配置 | runtime对应能力 | 原协议及受控跨能力查询，不绕路T/A |
| 回调登记、首帧、死亡/重连、通知 | compatibility/monitor + output | 平台技术token与T/A/R加载入口 |
| 外部头/符号/对象装配 | include + assembly；窄codec/表格target | Core/搜网/RIL/innerkits/卫星各自构建与原控制块 |

上述为目标导航；前面的可点击链接和§21仍全部指当前存在文件，不能要求IDE跳转尚未实施的计划路径。

IDE转到定义只会落到已有定义。业务方法未定义是当前状态，不能推断它已隐藏在Adapter中；附录逐文件列出有限实现与声明边界。

<a id="gaps"></a>

## 19. 未实现内容与局部设计缺口

| 尚未具备的内容 | 不应宣称 | 对应接入条件 |
| --- | --- | --- |
| 真实业务IO与原写点准入 | 号码/激活/Primary/Records目标实现已运行 | 按原函数和错误分支闭合，逐WU关闭旧写者 |
| 工厂/匹配销毁/真实宿主完成 | 构造可读即已可初始化或安全卸载 | 原对象身份、发布时点、失败路径及代码寿命验收 |
| 匹配provider、外部写者与可重入协议 | 本地Store或锁等于全局CAS/事务 | 匹配fork命令/schema、默认角色及外部写者证据 |
| 厂商命令结束/通道静止、细分并发域 | bool证据、超时或本地清零证明硬件结束 | 命令级因果、结束/可靠失效、资源可重用及预算 |
| 66250完整任务、产品Port/MEP能力 | Core复制下载/补偿或目录容量代表设备能力 | 匹配服务/产品/设备证据，只阻塞相关切片 |

本批已修正的接口表达不等于这些生产条件已关闭；保留草案/未接入标记。报告记录继续保留历史输入，不回写为本批设备成果。

<a id="verification"></a>

## 20. 检查记录与验证限度

### 20.1 历史记录与当前基线

原0.2稿记录过64文件的严格C++17语法和topology/resource/assignment三套纯规则，但输入工作树及统计已过期，不能将其写成当前HEAD全部通过。`5eed2181`后的原评审/执行状态记录四套规则及73文件检查，维持各自历史基线。本轮只做两篇设计，不运行/晋升源码测试。

当前73文件实际组合摘要见页首，CURRENT实际树和§21与磁盘逐项比对；PlanPrimarySwitch位于application，SessionRegistry已含有限纯实现，CompletionCell/结果sink/Profile双结果新增文件已补入清单。G2a只读真实链在另一个远端工作分支，不属于本地主分支的这73文件；主机PASS与完整OnInit BLOCKED、产品/设备范围分别见既有[执行状态](SIM_Refactor_Execution_State.md)，本轮未改状态或接入代码。

### 20.2 本轮文档验证（2026-10-30）

已按输入`e295540f`核对：两篇目标/现状视图、逻辑层与Owner、全部73骨架文件清单/逐文件状态、116个生产include/src来源（其中GN列54源、3源条件eSIM、未装配缓存源除外）、仓内外部头依赖及典型流程落点。静态依赖规则/target/namespace仍是拟议，不声称已编译实现。

本轮实际完成：434个本地链接及引用锚点、63处表格列数、8个围栏、原21个显式导航锚点保留、73文件实际树/附录及116来源文件各一次覆盖、目标一级/二级路径跨文档一致、源码组合摘要和`git diff --check`。4幅PlantUML经本机PlantUML/JRE21检查并渲染SVG，1幅Mermaid经本机解析通过；曾发现旧评审§10锚点失效，已在主设计修正并复验。按明确契约依赖检查拟议target DAG，非GN构建。1363个跟踪文件编辑前摘要对照仅本文及主设计变化，HEAD未变、暂存区为空。

没有运行OHOS构建、生产链接、SA/IPC/ABI/Feature矩阵、fuzzer、业务/设备交错或外仓联调。没有移动任何目录或创建新生产对象。外部provider、设备结束、66250/Port/MEP仍按§19及主设计§21限定切片，不妨碍本轮目录推荐闭合。

<a id="inventory"></a>

## 21. 附录：逐文件功能及实现状态

下表覆盖当前源码根内全部73文件，链接均指真实路径。有限纯实现只指所列规则，不表示生产接入；未定义业务方法仍按声明处理。目标路径按§3.2及下表分族映射，不伪造计划文件已存在。

| 当前文件族/具体文件 | 稳态位置（计划） | 搬迁时仍保留的契约 |
| --- | --- | --- |
| model/* | foundation平铺 | 身份/目录只读，不合并动态事实 |
| application/orchestration/* | foundation/request_ref.h及orchestration/*拆依赖 | epoch/Owner/step/效果值与Context/运行表分target |
| application/card_access/local_card_requests.* | runtime/state、records、euicc按请求拆分，必要命令导出runtime/api | 原参数、期望、A/T节点和双结果，不新增直通业务写 |
| topology/application/topology_service.* | topology根部 | 唯一事实入口，不吞并各用例 |
| topology/application/discovery_observation_ledger.* | topology/discovery | 共序观察、最终资格、已生效存储核对 |
| topology/application/activate_sim_use_case.*及activation_progress.* | topology/activation | slot/session/request清理资格 |
| topology/application/set_show_number_use_case.*、show_number_plan.*、show_number_query.* | topology/profile | 号码专属字段版本、原读源及步骤/完成偏序 |
| topology/domain/* | topology/domain | 纯规则/值，不引平台或O容器 |
| topology/ports/* | 场景私有port随用例；跨模块AccountStore/结果/执行/观察契约导出topology/api | 生产者/使用者/注入及一次物理写不变 |
| assignment/application/assignment_service.* | assignment根部 | 唯一角色/分配事实入口 |
| assignment/application/primary_switch_plan.*、account_role_protocol.h | assignment/primary、account_roles | Application偏序/角色reservation，不误归Domain |
| assignment/api/*与domain/* | 保持语义分层；共享纯枚举/DTO选入Owner api types单一定义 | Domain只依纯types，应用命令依request_protocol，不传递私有聚合头 |
| assignment/ports/* | A入站ProfilePreference移api；其余私有随roles/primary，外部执行/存储契约选取导出api | A偏好与参数Owner不变 |
| runtime/card_runtime.*及result_sinks.h | R薄接缝及能力局部桥，按导出需要入api | 窄sink、weak owner/epoch/排空，不指具体用例 |
| runtime/session/*、resource/*、records/* | 原能力/治理目录保留 | 唯一会话/资源/加载Owner |
| infrastructure/account_store_adapter.*、assignment_store_adapter.h | topology/adapters、assignment/adapters | T/A字段计划，不复制物理store或反灌缓存 |
| infrastructure/legacy_output_adapter.* | C output、A/T adapters按三独立类拆分 | 原通知/Hook/完成节点不合并延后 |
| compatibility/* | 门面多cpp；宿主契约api；完成/双结果output | 同步/callback/内部回执分路 |
| assembly/* | assembly；仓内入口由include精确导出 | Attach过渡/Owned最终、原发布/匹配销毁 |


| 文件 | 主要类型/责任 | 实现状态 |
| --- | --- | --- |
| [application/card_access/local_card_requests.cpp](../services/multisim/application/card_access/local_card_requests.cpp) | local_card_requests | 构造及SwitchLocalProfile对抽象端口的实际期望/提示/结束委托；PIN/EF仅声明，无真实卡IO |
| [application/card_access/local_card_requests.h](../services/multisim/application/card_access/local_card_requests.h) | VerifyPinRequest、ReadEfRequest、VerifiedPortRoute、SwitchProfileRequest、EuiccPrimitiveResult等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [application/orchestration/request_context.h](../services/multisim/application/orchestration/request_context.h) | RequestOwner、RequestId、CompletionId、SynchronousWaitRef、InternalReplyRef等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [application/orchestration/runtime_operation_store.h](../services/multisim/application/orchestration/runtime_operation_store.h) | RuntimeOperationStore | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assembly/host_ports.h](../services/multisim/assembly/host_ports.h) | SimHostPorts、SimServicePublicationPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assembly/manager_publication.h](../services/multisim/assembly/manager_publication.h) | ManagerPublicationSequence、PublicationStage、初始化/发布记录 | 有限原对象地址/共享控制块匹配规则，不执行Core原发布 |
| [assembly/sim_module_factory.cpp](../services/multisim/assembly/sim_module_factory.cpp) | sim_module_factory | 构造/绑定及流程伪代码；业务方法未实现 |
| [assembly/sim_module_factory.h](../services/multisim/assembly/sim_module_factory.h) | ExistingSimBindings、SimModule、SimModuleDeleter、SimModuleInitFailure、LegacySimStartup等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/api/assignment_requests.h](../services/multisim/assignment/api/assignment_requests.h) | SetRoleCommand、SwitchPrimaryCommand、RadioAssignmentView、AssignmentView、AccountRolePlanningInput等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/api/primary_execution.h](../services/multisim/assignment/api/primary_execution.h) | PrimarySwitchMode、ParameterWritePosition、PrimaryDecisionFacts | 模式/输入值契约，无执行或仲裁服务 |
| [assignment/application/account_role_protocol.h](../services/multisim/assignment/application/account_role_protocol.h) | AccountRoleProtocol | 有限角色reservation/finalize接纳规则，不接provider写入 |
| [assignment/application/assignment_service.cpp](../services/multisim/assignment/application/assignment_service.cpp) | assignment_service | 构造/绑定及流程伪代码；业务方法未实现 |
| [assignment/application/assignment_service.h](../services/multisim/assignment/application/assignment_service.h) | AssignmentService | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/application/primary_switch_plan.cpp](../services/multisim/assignment/application/primary_switch_plan.cpp) | PlanPrimarySwitch | 有限四模式兼容步骤/参数偏序计划，非Domain或生产切换 |
| [assignment/application/primary_switch_plan.h](../services/multisim/assignment/application/primary_switch_plan.h) | PrimaryAction、PrimarySwitchPlan | Application兼容步骤计划契约，依赖api/primary_execution.h |
| [assignment/domain/assignment_aggregate.cpp](../services/multisim/assignment/domain/assignment_aggregate.cpp) | assignment_aggregate | 仅include及领域职责说明；无Primary兼容计划实现，后者在application/primary_switch_plan.cpp |
| [assignment/domain/assignment_aggregate.h](../services/multisim/assignment/domain/assignment_aggregate.h) | TransitionPhase、AssignmentTransition、TransitionAdmission、AssignmentAggregate | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/domain/role_selection.h](../services/multisim/assignment/domain/role_selection.h) | DefaultRole、RoleSelection | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/assignment_store.h](../services/multisim/assignment/ports/assignment_store.h) | PrimaryParameterSnapshot、AssignmentParameterResult、RoleStoreResult、AssignmentStore | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/primary_topology_port.h](../services/multisim/assignment/ports/primary_topology_port.h) | PrimaryTargetFacts、PrimaryTopologyPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/profile_preference_port.h](../services/multisim/assignment/ports/profile_preference_port.h) | ProfilePreferencePort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/radio_execution_port.h](../services/multisim/assignment/ports/radio_execution_port.h) | PrimaryRadioObservation、RadioSwitchRequest、RadioSwitchResult、RadioResultSink、RadioExecutionPort等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [compatibility/completion_cell.h](../services/multisim/compatibility/completion_cell.h) | CompletionCell | 有限先登记/结果可先到达及一次完成纯规则，不是真实FFRT等待 |
| [compatibility/legacy_boundary_ports.h](../services/multisim/compatibility/legacy_boundary_ports.h) | LegacyCallerRef、LegacyCallbackRef、EntryCheck、LegacySynchronousCall、LegacyIngressPort等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [compatibility/legacy_completion.cpp](../services/multisim/compatibility/legacy_completion.cpp) | legacy_completion | 构造/绑定及流程伪代码；业务方法未实现 |
| [compatibility/legacy_completion.h](../services/multisim/compatibility/legacy_completion.h) | LegacyCompletion | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [compatibility/legacy_profile_result.h](../services/multisim/compatibility/legacy_profile_result.h) | AssignLocalProfileLegacyResult及本地Profile结果桥 | 有限外层int/可选enum赋值规则，不生成卡结果 |
| [compatibility/legacy_sim_facade.cpp](../services/multisim/compatibility/legacy_sim_facade.cpp) | legacy_sim_facade | 构造/绑定及流程伪代码；业务方法未实现 |
| [compatibility/legacy_sim_facade.h](../services/multisim/compatibility/legacy_sim_facade.h) | LegacySimFacade | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [infrastructure/account_store_adapter.cpp](../services/multisim/infrastructure/account_store_adapter.cpp) | account_store_adapter | 构造/绑定及流程伪代码；业务方法未实现 |
| [infrastructure/account_store_adapter.h](../services/multisim/infrastructure/account_store_adapter.h) | AccountStoreAdapter | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [infrastructure/assignment_store_adapter.h](../services/multisim/infrastructure/assignment_store_adapter.h) | AssignmentStoreAdapter | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [infrastructure/legacy_output_adapter.cpp](../services/multisim/infrastructure/legacy_output_adapter.cpp) | legacy_output_adapter | 构造/绑定及流程伪代码；业务方法未实现 |
| [infrastructure/legacy_output_adapter.h](../services/multisim/infrastructure/legacy_output_adapter.h) | LegacyOutputAdapter、AssignmentOutputAdapter、ActivationHookAdapter | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [model/sim_identity.h](../services/multisim/model/sim_identity.h) | SubscriptionId、SlotId、RadioDomainId、SlotTarget、CardApplicationTarget等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [model/slot_catalog.cpp](../services/multisim/model/slot_catalog.cpp) | slot_catalog | 有限目录构造/查找；不自动发现或装配 |
| [model/slot_catalog.h](../services/multisim/model/slot_catalog.h) | EndpointKind、VisibilityClass、SlotDescriptor、LegacyCapacityMeasures、SlotCatalog | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [runtime/card_runtime.cpp](../services/multisim/runtime/card_runtime.cpp) | card_runtime | 构造/绑定及流程伪代码；业务方法未实现 |
| [runtime/card_runtime.h](../services/multisim/runtime/card_runtime.h) | SimStateManager、SimFileManager、SimSmsManager、IccDiallingNumbersManager、StkManager等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [runtime/records/load_session.cpp](../services/multisim/runtime/records/load_session.cpp) | load_session | 有限加载成员登记/一次消费/局部失效；无EF IO或LOADED发布 |
| [runtime/records/load_session.h](../services/multisim/runtime/records/load_session.h) | LoadToken、LoadMemberState、LoadAccess、LoadMember、LoadSession | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [runtime/resource/resource_barrier.cpp](../services/multisim/runtime/resource/resource_barrier.cpp) | resource_barrier | 有限纯资源状态转移；证据输入不证明设备能力 |
| [runtime/resource/resource_barrier.h](../services/multisim/runtime/resource/resource_barrier.h) | ResourceState、EffectRequirement、ResourceKey、ResourceLease、InitialRecoveryEvidence等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [runtime/result_sinks.h](../services/multisim/runtime/result_sinks.h) | RuntimeResultSinks | 有限窄结果/迟到证据交付，不引用具体用例，不是事件总线 |
| [runtime/session/session_registry.cpp](../services/multisim/runtime/session/session_registry.cpp) | SessionRegistry成员实现 | 有限Current/水位/期望登记、撤销、等待结束/到期及可信一次接续；无设备I/O |
| [runtime/session/session_registry.h](../services/multisim/runtime/session/session_registry.h) | TransitionCause、ExpectedTransition、TransitionEvidence、SessionRegistry | 会话/期望/代次水位契约；cpp有有限纯实现，无设备发布闭包 |
| [topology/api/topology_requests.h](../services/multisim/topology/api/topology_requests.h) | SetShowNumberCommand、NumberSource、ShowNumberRead、ActivationKind、ActivateSimCommand等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/activate_sim_use_case.cpp](../services/multisim/topology/application/activate_sim_use_case.cpp) | activate_sim_use_case | 构造/绑定及流程伪代码；业务方法未实现 |
| [topology/application/activate_sim_use_case.h](../services/multisim/topology/application/activate_sim_use_case.h) | ActivateSimUseCase | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/activation_progress.cpp](../services/multisim/topology/application/activation_progress.cpp) | activation_progress | 有限请求Owner/epoch/session归属和进度规则 |
| [topology/application/activation_progress.h](../services/multisim/topology/application/activation_progress.h) | ActivationTransition、ActivationProgress | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/discovery_observation_ledger.cpp](../services/multisim/topology/application/discovery_observation_ledger.cpp) | discovery_observation_ledger | 有限观察序列/失效/最终资格规则 |
| [topology/application/discovery_observation_ledger.h](../services/multisim/topology/application/discovery_observation_ledger.h) | DiscoveryObservationLedger | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/set_show_number_use_case.cpp](../services/multisim/topology/application/set_show_number_use_case.cpp) | set_show_number_use_case | 构造/绑定及流程伪代码；业务方法未实现 |
| [topology/application/set_show_number_use_case.h](../services/multisim/topology/application/set_show_number_use_case.h) | SetShowNumberUseCase | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/show_number_plan.cpp](../services/multisim/topology/application/show_number_plan.cpp) | show_number_plan | 有限兼容步骤计划；无IO或业务成功返回 |
| [topology/application/show_number_plan.h](../services/multisim/topology/application/show_number_plan.h) | NumberUpdateNode、ShowNumberPlan | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/show_number_query.cpp](../services/multisim/topology/application/show_number_query.cpp) | show_number_query | 构造/绑定及流程伪代码；业务方法未实现 |
| [topology/application/show_number_query.h](../services/multisim/topology/application/show_number_query.h) | ShowNumberQuery | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/application/topology_service.cpp](../services/multisim/topology/application/topology_service.cpp) | topology_service | 构造/绑定及流程伪代码；业务方法未实现 |
| [topology/application/topology_service.h](../services/multisim/topology/application/topology_service.h) | ReloadAccountInput、ReloadRequest、AccountFieldReservation、TopologyService | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/domain/account_registry.cpp](../services/multisim/topology/domain/account_registry.cpp) | account_registry | 有限独立投影/字段版本/重载接纳纯规则 |
| [topology/domain/account_registry.h](../services/multisim/topology/domain/account_registry.h) | AccountReadView、AccountFieldRevisions、AccountProjection、AccountRecord、AccountAcceptance等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/domain/binding_index.cpp](../services/multisim/topology/domain/binding_index.cpp) | binding_index | 有限装配标记/位置版本/绑定唯一性/ABA规则 |
| [topology/domain/binding_index.h](../services/multisim/topology/domain/binding_index.h) | SlotRuntimeEntry、BindingIndex | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/account_role_planning_port.h](../services/multisim/topology/ports/account_role_planning_port.h) | AccountRolePlanningPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/account_store.h](../services/multisim/topology/ports/account_store.h) | AccountNumberPatch、AccountActivePatch、AccountNamePatch、AccountLabelPatch、AccountImsPreferencePatch等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/activation_assignment_port.h](../services/multisim/topology/ports/activation_assignment_port.h) | ActivationAssignmentPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/activation_runtime_port.h](../services/multisim/topology/ports/activation_runtime_port.h) | ActivationDeviceRequest、ActivationDeviceResult、ActivationResultSink、ActivationRuntimePort、ActivationHookPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/card_observation_port.h](../services/multisim/topology/ports/card_observation_port.h) | CardObservationSink | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/number_query_host_port.h](../services/multisim/topology/ports/number_query_host_port.h) | ImpuNumberResult、NumberBackwrite、NumberQueryHostPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/show_number_completion_port.h](../services/multisim/topology/ports/show_number_completion_port.h) | ShowNumberCompletion、ShowNumberCompletionPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [topology/ports/show_number_runtime_port.h](../services/multisim/topology/ports/show_number_runtime_port.h) | MsisdnWriteRequest、MsisdnWriteResult、MsisdnResultSink、CardNumberView、ShowNumberRuntimePort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
