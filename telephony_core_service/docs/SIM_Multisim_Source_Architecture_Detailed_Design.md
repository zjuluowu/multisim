# SIM 目标架构：源码结构与功能详细设计

| 项目 | 内容 |
| --- | --- |
| 文档版本 | 0.2，分支整合及评审意见落实后的源码视图 |
| 更新日期 | 2026-10-29 |
| 源码范围 | `telephony_core_service/services/multisim/`，当前64个文件（43头文件、21源文件） |
| 输入基线 | main `964dc5693badc08ce4abafd70d22f1dcd47cf727`、待合入骨架分支 `2448b42e8976ffb75e2bfcd45b27c86eb1e3965d`及本轮明确授权的契约/纯规则修订 |
| 当前状态 | 工作树源码视图，未接生产GN/装配/活动写权；不得将本轮修订归入上述历史提交 |
| 权威范围 | 解释实际类型、依赖、有限实现与未实现关系，不取代目标主设计、公共契约或功能/WU清单 |

**文件存在、语法可检查和有限纯规则通过，均不等于目标业务已经实现。** 原生产仍位于`services/sim/`。本文保留原21个显式导航锚点，更新过期目录、方法、构造及实现状态；不据源码目录数量建立新的上下文、SO或进程。

源码内容摘要：`9252a576f5b027bd88a38b990eefa8d0b658e1e6f5ea2a08789f8a1e3cc28540`（SHA-256；按根目录内相对路径字典序，依次连接UTF-8路径、NUL、文件原始字节、NUL）。该摘要固定本文所述工作树，不代替最终Git提交号。

## 目录

1. [范围、依据与状态口径](#scope)
2. [整体结构与职责边界](#architecture)
3. [完整源码目录](#source-tree)
4. [分层与依赖规则](#dependencies)
5. [身份、目标与槽目录](#identity)
6. [T：账户与连接拓扑](#topology)
7. [A：角色与资源分配](#assignment)
8. [应用编排机制与短请求](#application)
9. [R：完整生产卡运行支撑](#runtime)
10. [C/I：兼容入口与技术适配](#integration)
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

生产仍使用[`services/sim/`](../services/sim/)；目标源码位于[`services/multisim/`](../services/multisim/)。本轮修正的是未接入骨架的契约、有限纯规则与说明，没有移动成熟实现、接入GN、转移活动写权或修改公开API/ABI/IDL/schema。

依据为[目标主设计](SIM_Target_Architecture_Design_v1.2.md)、[流程对照](多SIM架构重构_设计说明与业务流程对照.md)及[独立评审记录](SIM_Target_Architecture_Independent_Review_2026-10-29.md)。[功能/WU清单](SIM_Architecture_Refactor_Design.md)继续维护SIM-001～062及子项和WU-01～09；本文解释当前源码，不另建目标主规范。

主设计§4.1/§4.2的T/A事实Owner、O应用机制裁决继续有效。main已有round-3/4复审修订被保留；新报告中的风险经审视后转化为契约修正，不因报告已经合入就认定实现完成。

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
| I | 原DataShare、参数、Hook、通知和技术句柄 | 角色策略、账户缓存、虚构全局事务 |
| H | 对象身份、共享RIL、epoch、目录、端口装配、发布与匹配销毁 | 新业务Owner或第二套RIL/成熟对象 |

运行调用为`C → T/A用例 → R/I窄端口`；结果经`MsisdnResultSink/ActivationResultSink/RadioResultSink`返回发起者，R观察经`CardObservationSink`进入T。R不再引用具体应用服务。T/A跨Owner只使用消费方定义的窄入口，父子请求不嵌套等待、不持有对方聚合。

<a id="source-tree"></a>

## 3. 完整源码目录

当前完整清单为**64个文件：43个头文件、21个源文件**。`.cpp`不保证有业务实现；逐文件状态见[附录](#inventory)。

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
│   ├── sim_module_factory.cpp
│   └── sim_module_factory.h
├── assignment/
│   ├── api/
│   │   └── assignment_requests.h
│   ├── application/
│   │   ├── assignment_service.cpp
│   │   └── assignment_service.h
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
│   ├── legacy_boundary_ports.h
│   ├── legacy_completion.cpp
│   ├── legacy_completion.h
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
│   │   └── session_registry.h
│   ├── card_runtime.cpp
│   └── card_runtime.h
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

<a id="dependencies"></a>

## 4. 分层与依赖规则

Domain只包含纯模型/值/规则。Application使用Domain及所需端口，R/I实现应用声明的端口，C调用应用入口，H构造注入。新旧成熟类的具体借用只出现在R/I/H，不反向进入T/A。

| 使用方 | 窄契约 | 实现/注入 |
| --- | --- | --- |
| T发现与重载 | AccountRolePlanningPort | A实现；H在A构造后、发布前BindRolePlanner，避免构造环 |
| T激活 | ActivationAssignmentPort | A实现旧有界等待、带owner/session/domain/token的新准入、正式释放/转交、偏好和复核；T不依赖AssignmentService |
| A主卡 | PrimaryTopologyPort | T实现绑定读取及映射子请求，A不依赖TopologyService |
| T号码/激活、A切换 | 运行端口及各自结果sink | R实现运行，应用实现sink，H绑定 |
| 本地Profile短请求 | LocalCardPort、ProfilePreferencePort | R执行、A接纳提示，H注入 |
| C完成 | CallbackHostPort、LegacyOutputPort | Core宿主/I执行，C解释旧出口 |

没有include环不等于依赖已合理。此处显式消除了R到具体结果Owner、T激活到具体A服务及A到具体T服务的关系。`show_number_plan`已从Domain移入Application，兼容步骤偏序不再仅因纯C++就归入Domain。

<a id="identity"></a>

## 5. 身份、目标与槽目录

`model/sim_identity.h`保留SubscriptionId、SlotId、RadioDomainId、分类目标、RuntimeEpoch、SessionRef和BindingRef。SubscriptionId仅包装旧simId；空槽/PIN/EF不强求账户，不从slot或标签生成身份。

SessionRef由R维护epoch、应用和bindingGeneration；BindingRef为T已接受位置绑定。BindingIndex空位置也有独立revision，移除不归零。同一卡再插必须使旧引用失效，未知槽不回退槽0。

SlotCatalog只表达配置端点、允许连接、可见性、容量及可运行期加入属性。条目的rilInstance/radioDomain是配置解释，不是当前Radio分配；当前绑定归T，当前Radio观察归A。目录不证明对象已装配或支持更多并发。

Port映射到不同逻辑槽时，一槽一当前账户不必然要求新schema；确需一槽多当前绑定时再核对键、旧投影、provider与产品契约。旧GetEsimPortIndex是请求暂存，不作为可靠绑定证据。机械拆SO不强制先引入目录。

<a id="topology"></a>

## 6. T：账户与连接拓扑

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

RequestId现含`value + epoch + RequestOwner`，StepRef另含step/attempt；共享R资源不能只比较一个局部序号。RuntimeOperationStore按epoch与Owner构造，Begin可携parent，AdvanceStep/RecordSent/AcceptResult声明受控推进，不允许外部改公共Context来绕过资格。

`CompletionPlan.route`使用CallbackCompletionRoute、SynchronousCompletionRoute、InternalCompletionRoute。LegacyIntCompletionPort向原出口交付int结果，不提供等待或万能Execute。等待只在C/Core适配层，不占据产生完成所需的T/A短提交域。

LocalCardRequests是具名短请求，不是新的业务上下文。SwitchProfileRequest同时保留legacyPortIndex、forceDisableProfile及可选VerifiedPortRoute；本地短请求负责登记/结束Profile期望，R签发会话并保留Unknown恢复责任。外层int和卡enum分开，下载/OSU仍由既有66250任务边界主编排。

<a id="runtime"></a>

## 9. R：完整生产卡运行支撑

CardRuntime继续承接原State/File/Controller、安全、MSISDN、Records、SIM短信存储、电话簿、STK、运营商配置/APN/权限及eUICC能力。ProductionSlotBindings显式借用原对象；不机械复制成熟类。原单一EsimManager内部已有普通/低优先级两套EsimFile，不创建第二Manager。

CardRuntime借同一ITelRilManager，普通Radio三阶段用原控制器，专用Primary路径沿共享RIL。激活旧串行主要来自activeSimMutex_跨等待及PrimaryDone同锁，ctx_在cv.wait期间释放，不能恢复为“全程持有静态ctx_”。

| 机制 | 已表达责任 | 当前边界 |
| --- | --- | --- |
| BindResultSinks | 四类窄结果/观察出口；不认识具体T/A用例 | 模块内绑定可见；真实事件关联、weak owner/epoch/排空仍待接入 |
| SessionRegistry | Current/失效、ExpectedTransition、可信接续、确定未发撤销、等待结束 | 仅声明；G3b须桥接尚未迁入G3c的旧用例，任意READY不足以接续 |
| LoadSession | 父子/必需可选/读写成员，一次消费，受影响读失效 | 有限纯规则；实际各卡型字段及LOADED写点仍在生产 |
| ResourceBarrier | 初始Checking、完整资源Acquire、MarkSent、NotSent撤销、Complete、Quarantine/Recovery | 有限纯状态转移，不证明设备结束能力，当前未启用 |
| 发现观察流 | 每应用发现/失效共observationSequence，送T同一接纳序列 | 发送前在原写点准入；跨T的DB返回还须再次复核 |

资源租约核对epoch、完整资源集、fence及完整StepRef。未发不能伪完成，已发不能伪NotSent；写Unknown不释放。只读查询在证明结束和可复用后可释放资源，但仍保留原失败结果；开通道Get也不能绕过冲突域。

<a id="integration"></a>

## 10. C/I：兼容入口与技术适配

LegacySimFacade保留原入口差异和签名语义。号码仍先受理/排队、后callback；普通激活和Primary执行点返回原int，通过宿主先登记waiter再发起，允许结果先于Await到达。同步返回不能统一解释为所有设备已完成，第三卡旧早返回单独保持。

LegacyCompletion实现ShowNumberCompletionPort与LegacyIntCompletionPort；原通知条件和int交付分开。LegacyOutputAdapter只交付Monitor/宿主/平台结果，不持有业务缓存；CallbackHostPort保存真实调用者/回调/同步等待/内部接收者。

| I适配 | 真实使用约束 |
| --- | --- |
| AccountStoreAdapter | 独立Current/Historical旧查询；同次读取的T行与角色片段分别接纳；Match/联合发现/Removal沿原helper，不能整行反灌 |
| AssignmentStoreAdapter | 原默认卡命令、参数预写/恢复/Data/偏好；不统一传播原来忽略的参数或广播错误 |
| LegacyOutput/AssignmentOutput/ActivationHook | 原通知、阶段、两次Hook和原返回节点；void不表示硬件确认 |

平台/IO方法仍声明或伪代码，无实际provider替换或第二缓存。DisableProfile相关标签恢复提示与旧无后缀输出仍按主设计§13.2的Owner及原节点处理，不从参数名推导新业务事实。

<a id="assembly"></a>

## 11. H：SIM侧装配与宿主契约

两种工厂用途已明确分开：

| 契约 | 输入/输出及拥有关系 | 实现状态 |
| --- | --- | --- |
| AttachSimModuleToExisting | 同SO迁移附着；借用既有store/monitor/radio/slots和单一euicc；使用H的catalog与epoch | 工厂/deleter仅声明；私有构造仅表达注入，不旁路双装配 |
| CreateOwnedSimServices | G2b最终SIM侧创建原对象；只收host、LegacySimStartup、SimServicePublicationPort，返回原ISimManager/IEsimManager句柄及匹配拥有者 | 工厂/销毁仅声明，不要求Core取得内部Controller/store，也不强制先建目录 |

SimModule构造次序为I/C出口→R→T→A→号码/查询/激活/短请求→Facade。A仅依赖T窄端口；构造后BindRolePlanner(A)，随后R BindResultSinks(号码、激活、A、T)，未完成必要绑定不得发布。不能靠双向强引用弥补构造环。

Core仍创建并拥有共享RIL；最终工厂按原eSIM创建/Init/发布、SIM创建/Init/发布时点回交Core，搜网/CMI仍留Core。原OnInit false未传播的地方不因Stage诊断就新增失败出口。

原接口没有虚析构时，由生产具体类型控制块或匹配deleter销毁，不追加公开虚表。业务导出为私有创建/匹配销毁，工具链必要符号另列；不宣称绝对只有两个ELF符号。常驻拆分和StopAcceptingAndDrain增强分开，排空失败不卸载，SIM不Stop共享RIL。

<a id="set-number"></a>

## 12. 业务流程：设置显示号码

阅读入口为LegacySimFacade→SetShowNumberUseCase::StartAtExecutionPoint。保留原IsValidData/FileManager/错误优先级，不新增号码格式限制。R ReadMsisdnCache得到原session/tag，T PrepareNumberTarget记录BindingRef、身份及两个号码字段版本，O登记具名请求。

运行偏序：`R写MSISDN → T账户DB → Current号码投影 → Historical号码投影 → C原完成`。PlanShowNumberUpdate已在Application；NumberAcceptance独立记录DB/两个缓存结果。

卡成功而DB失败不回滚卡、不重放写卡；当前成功而历史失败保留部分可见效果；只有原整体成功才发适用账户通知，随后原callback。通知交付失败不反转卡/DB事实。

等待超时内部保留Unknown，晚到可信结果只供原R会话观察/核对，不继续已结束T账户步骤或重复成功完成。实际ProcessSetMsisdnDone写点必须先准入，不能先改共享lastMsisdn缓存再在外层丢弃。

G2保持旧任务执行时定位File、写库步骤才解析simId；当前执行点固定目标和陈旧拒绝属于G3b，不能直接宣称等价替换。IPC接收时提前固定另行裁决。

<a id="query-number"></a>

## 13. 业务流程：号码查询与独立回写

ShowNumberQuery保留卡缓存→账户→Core IMS IMPU原读源、错误与立即返回，不等待后台回写。NumberBackwrite保存原binding/session、身份和current/historical号码字段版本；改名或IMS偏好变化不应废除合法号码资格。

独立回写执行时由T复核并预留字段，拒绝后不补写新卡账户。该回写是新请求，不通过AcceptLateEvidence复活已终结设置请求。原闭包仅捕获slot/号码并在执行时找账户的行为作为G2基线，固定与陈旧拒绝单列G3b。

QueryImsPreference仍按ICCID走原存储读源，不因T拥有资料就改成统一内存查询。

<a id="activation"></a>

## 14. 业务流程：普通SIM激活

ActivateSimUseCase只依赖ActivationAssignmentPort，不引用具体AssignmentService。T拥有active投影和ActivationProgress；A接纳入口偏好、Primary准入/后续复核；R执行；I执行原Hook；完成经LegacyIntCompletionPort分路。

普通non-force仍为：进度置位→原4s有界等待Primary（到期可继续）→RIL→Current/Historical缓存→第一次UpdateSubState/清进度→DB→第二次缓存/Hook→A主卡复核→最终Hook→原通知/Registry及返回。DB失败保留前面效果，不统一持久后发布。

ActivationProgress以epoch、请求Owner、step、原session和slot控制开始/清理；只允许有效Activation或旧LegacyBridge请求，value为0或其他Owner不能建立资格。进度不是AccountRecord字段。清零可在DB之前，false不证明设备或共享资源空闲；旧请求不能清新槽操作。

普通force仍先RIL但不新增账户写；卫星非force的DB/缓存顺序和force仅Hook分别保持。完整设备/完成路径仍未实现，不能因纯进度规则已通过就称激活已接入。

<a id="primary"></a>

## 15. 业务流程：Primary切换与Data联动

A读取PrimaryTopologyPort的绑定/ICCID/active和R的PrimaryRadioObservation，按PrimaryDecisionFacts生成模式计划。SlotCatalog只提供静态允许连接，当前Modem观察由R提供并由A接纳。

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
| State、PIN/PUK、认证/IO，SIM-005～011 | R + 应用短请求 | 不要求simId，不复制协议/剩余次数 |
| File、Records、Refresh及卡记录，SIM-012～019 | R及LoadSession/SessionRegistry | 保留卡型/解析/原LOADED路径；不把加载放入T事务 |
| InitData/InitIccId/账户缓存，SIM-020～022 | T TopologyService/AccountRegistry/BindingIndex | 身份/账户归T，建行角色由A片段提供 |
| 名称、号码、查询回写，SIM-023及子项 | T资料/号码用例，R MSISDN，I字段存储，C完成 | 名称不写卡，号码先卡后库，查询回写独立 |
| 普通/force/卫星激活，SIM-024 | T激活用例；A准入/偏好；R设备；I Hook | active与进行中归T，各分支不统一事务化 |
| Voice/SMS/Data/Primary/Radio，SIM-025～030 | A AssignmentService/Aggregate，R协议 | 保留参数预写/条件恢复/重试与Primary→Data |
| 标签、资料、映射开始，SIM-031及子项 | T资料/映射用例；R软件状态 | 映射用例/厂商端口尚缺，人工ABSENT不证明物理拔卡 |
| 回调、Observer、重连/用户切换，SIM-032～035 | C登记、I技术token、T账户加载、R卡加载 | 当前只展开号码完成；完整订阅壳后续补齐 |
| 卡短信、电话簿、STK，SIM-036～040 | R原成熟实现 | 不迁入完整短信业务或联系人业务 |
| carrier/APN/权限/运营商规则，SIM-041～045相关项 | R原配置/匹配；T仅IMS偏好 | 不按SimAccountManager类名整类迁T |
| 下载/OSU，SIM-046 | 原EsimServiceClient/66250任务边界 | Core仅本地原语，远端完整实现仍需匹配证据 |
| 本地eUICC，SIM-047～054 | R原EsimManager/File/Controller、短请求 | 保留一个Manager内部普通/低优先级File、卡侧结果/通道/缓存 |
| DataShare/探测，SIM-055～056 | I store；T/A字段接纳；R探测 | 不直接建立新RDB或独立写缓存 |
| EXT/VSIM/卫星、serial、CMI等，SIM-057～060 | I/H/C及相应T/A/R Owner | 保留宿主门面/厂商ABI，按真实切片补端口 |
| FactoryReset、未装配缓存源，SIM-061～062 | 原网络Owner；缓存源不自动启用 | 不扩展为删Profile或新增活动writer |

这是一张源码责任定位表，完整功能断言、编号子项与审批条件仍以原功能/WU清单及主设计为准。

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

<a id="reading"></a>

## 18. IDE阅读与开发定位

1. 从[SimModule装配](../services/multisim/assembly/sim_module_factory.cpp)及两工厂声明区分附着、拥有与发布。
2. 看[LegacySimFacade](../services/multisim/compatibility/legacy_sim_facade.h)和[完成路由](../services/multisim/application/orchestration/request_context.h)，区分号码受理/callback、同步int与内部回执。
3. 沿[T号码用例](../services/multisim/topology/application/set_show_number_use_case.cpp)→[应用计划](../services/multisim/topology/application/show_number_plan.cpp)→[账户接纳](../services/multisim/topology/application/topology_service.cpp)→[字段/发现存储](../services/multisim/topology/ports/account_store.h)。
4. 看[AccountRegistry](../services/multisim/topology/domain/account_registry.cpp)、[BindingIndex](../services/multisim/topology/domain/binding_index.cpp)、[观察ledger](../services/multisim/topology/application/discovery_observation_ledger.cpp)和[激活进度](../services/multisim/topology/application/activation_progress.cpp)的有限纯实现。
5. 看[A模式计划](../services/multisim/assignment/domain/assignment_aggregate.cpp)和[运行端口](../services/multisim/assignment/ports/radio_execution_port.h)，再回A应用伪代码，勿将一个普通分支推广为所有Primary路径。
6. 沿[R窄结果桥](../services/multisim/runtime/card_runtime.h)、[LoadSession](../services/multisim/runtime/records/load_session.cpp)、[资源转移](../services/multisim/runtime/resource/resource_barrier.cpp)对照原生产真实写点。
7. 最后看[本地Profile短请求](../services/multisim/application/card_access/local_card_requests.cpp)、SessionRegistry期望和A提示出口。

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

本次已执行的检查：

- 全部64个骨架源码文件（43个头文件、21个源文件）分别以`clang++ -std=c++17 -fsyntax-only -Wall -Wextra -Werror`通过；include根为`services/multisim`。后续局部接口修订仍需复核，不以此前结果覆盖更新内容。
- 已保留可重复运行的[纯规则检查入口](../test/multisim_architecture_rules/run.py)及[说明](../test/multisim_architecture_rules/README.md)：从仓库根执行`python3 telephony_core_service/test/multisim_architecture_rules/run.py`。不依赖第三方框架，不修改生产GN，编译产物使用临时目录。
- topology真实链接SlotCatalog、BindingIndex、AccountRegistry、DiscoveryObservationLedger、ActivationProgress；覆盖目录不等装配、绑定ABA/空位置版本/重复身份、两个投影独立、改名不废号码、迟到重载按字段接纳、旧发现/错误epoch/无关槽、旧激活清理及错误Owner/无效请求开始的正负断言。
- resource覆盖完整资源集合、初始核对、发送/NotSent撤销、正常完成、Unknown隔离、读载荷未知而资源可用、错误Owner/epoch/fence及伪造租约；assignment覆盖四种Primary模式、原分支优先级、参数位置、未知资源、请求命名空间及完成分路。root实际运行三套均PASS，边界仅为真实纯规则。

本文输出检查覆盖原21个显式锚点、本地链接、目录树与附录的全文件覆盖、表格列数、代码围栏、源码组合摘要及空白。纯检查未创建生产State/File/Controller或模拟卡效果成功；通过不表示目标业务初始化或设备场景已完成。

没有运行OHOS构建、生产全链接、SA/IPC/ABI/Feature矩阵、fuzzer、设备、交错复现或外仓联调；历史51文件语法记录不替代当前内容验证。业务层没有因纯规则测试通过而获准启用屏障或改变公开行为。

<a id="inventory"></a>

## 21. 附录：逐文件功能及实现状态

下表覆盖当前源码根内全部文件。有限纯实现只指所列规则，不表示生产接入；未定义的业务方法仍按声明处理。

| 文件 | 主要类型/责任 | 实现状态 |
| --- | --- | --- |
| [application/card_access/local_card_requests.cpp](../services/multisim/application/card_access/local_card_requests.cpp) | local_card_requests | 构造与本地短请求/Profile原节点伪代码；无真实卡IO |
| [application/card_access/local_card_requests.h](../services/multisim/application/card_access/local_card_requests.h) | VerifyPinRequest、ReadEfRequest、VerifiedPortRoute、SwitchProfileRequest、EuiccPrimitiveResult等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [application/orchestration/request_context.h](../services/multisim/application/orchestration/request_context.h) | RequestOwner、RequestId、CompletionId、SynchronousWaitRef、InternalReplyRef等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [application/orchestration/runtime_operation_store.h](../services/multisim/application/orchestration/runtime_operation_store.h) | RuntimeOperationStore | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assembly/host_ports.h](../services/multisim/assembly/host_ports.h) | SimHostPorts、SimServicePublicationPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assembly/sim_module_factory.cpp](../services/multisim/assembly/sim_module_factory.cpp) | sim_module_factory | 构造/绑定及流程伪代码；业务方法未实现 |
| [assembly/sim_module_factory.h](../services/multisim/assembly/sim_module_factory.h) | ExistingSimBindings、SimModule、SimModuleDeleter、SimModuleInitFailure、LegacySimStartup等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/api/assignment_requests.h](../services/multisim/assignment/api/assignment_requests.h) | SetRoleCommand、SwitchPrimaryCommand、RadioAssignmentView、AssignmentView、AccountRolePlanningInput等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/application/assignment_service.cpp](../services/multisim/assignment/application/assignment_service.cpp) | assignment_service | 构造/绑定及流程伪代码；业务方法未实现 |
| [assignment/application/assignment_service.h](../services/multisim/assignment/application/assignment_service.h) | AssignmentService | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/domain/assignment_aggregate.cpp](../services/multisim/assignment/domain/assignment_aggregate.cpp) | assignment_aggregate | 有限纯Primary模式/参数位置计划；其他领域接纳仍按声明 |
| [assignment/domain/assignment_aggregate.h](../services/multisim/assignment/domain/assignment_aggregate.h) | TransitionPhase、PrimarySwitchMode、ParameterWritePosition、AssignmentTransition、PrimaryDecisionFacts等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/domain/role_selection.h](../services/multisim/assignment/domain/role_selection.h) | DefaultRole、RoleSelection | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/assignment_store.h](../services/multisim/assignment/ports/assignment_store.h) | PrimaryParameterSnapshot、AssignmentParameterResult、RoleStoreResult、AssignmentStore | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/primary_topology_port.h](../services/multisim/assignment/ports/primary_topology_port.h) | PrimaryTargetFacts、PrimaryTopologyPort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/profile_preference_port.h](../services/multisim/assignment/ports/profile_preference_port.h) | ProfilePreferencePort | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [assignment/ports/radio_execution_port.h](../services/multisim/assignment/ports/radio_execution_port.h) | PrimaryRadioObservation、RadioSwitchRequest、RadioSwitchResult、RadioResultSink、RadioExecutionPort等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [compatibility/legacy_boundary_ports.h](../services/multisim/compatibility/legacy_boundary_ports.h) | LegacyCallerRef、LegacyCallbackRef、EntryCheck、LegacySynchronousCall、LegacyIngressPort等 | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
| [compatibility/legacy_completion.cpp](../services/multisim/compatibility/legacy_completion.cpp) | legacy_completion | 构造/绑定及流程伪代码；业务方法未实现 |
| [compatibility/legacy_completion.h](../services/multisim/compatibility/legacy_completion.h) | LegacyCompletion | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
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
| [runtime/session/session_registry.h](../services/multisim/runtime/session/session_registry.h) | TransitionCause、ExpectedTransition、TransitionEvidence、SessionRegistry | 私有类型/端口/状态契约；有限内联构造/访问见源码 |
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
