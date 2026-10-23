# SIM 架构分层对比与功能迁移设计

日期：2026-10-23；同步后一致性复审基线：`main@801877c95ab8d37c60cb60dc210e796be6b96599`。
性质：架构主文档的补充分层与部署视图。现状证据使用主文档的 B1/W1，并在本轮工作树复核；基线间文档提交不算生产能力变化。本文只描述设计，不代表生产重构已实施或设备运行验证已完成。

## 1. 目的、范围与判定口径

本文沿用输入稿标为“用户蓝本”的 API / Application / Domain / Infrastructure / RIL 展示方式，并在[主文档](SIM_Architecture_Refactor_Design.md)的源码核实基础上补充分层和迁移对照。原蓝本未随文件提供可定位的独立版本或批准记录，不能据此增加业务约束。

1. 重构前实际分层及其与目标分层的差异；
2. 旧模块、功能和物理部署单元到目标职责/部署单元的迁移关系；
3. 目标图中容易混淆的运行时 IPC、共享库加载、Owner 写入和跨服务事件边界。

证据等级：

| 标记 | 含义 |
|---|---|
| **FACT** | 当前检出的源码、构建配置或服务配置直接支持的静态事实；不等于设备实测。 |
| **TARGET** | 目标架构建议，尚未形成当前生产实现。 |
| **OPEN** | 依赖外仓实现、产品配置、审批或运行测试才能确认。 |

本文采用[主文档的职责裁决](SIM_Architecture_Refactor_Design.md#target-contexts)：T 账户与连接拓扑、A 角色与资源分配、O 业务操作三个业务上下文；R 卡运行支撑、C 兼容职责、I 技术适配、H 宿主装配另行表达。C 不拥有业务权威事实，不再作为待定的第四业务上下文。选择依据是知识和状态维护责任，非文档日期。主文档是规则来源，本文只展开视图；早期 V3/MultiSIM 作为推导资料保留，不同表述按[本轮裁决](SIM_Architecture_Refactor_Design.md#supplement-review)解释。

本轮目标不新增公开 API、IPC、SA、进程、schema、厂商 ABI 或运行期实现切换开关；保留既有卡协议和外部行为。目标 Owner/Adapter/Repository 名称不得作为当前实现证据。

## 2. 架构总览

### 2.1 设计原则

下表整理输入稿的 P1–P10 提案，并按主文档限定适用范围。规则编号及 `SIM-INV-*` 不具有独立批准证据；本表是 **TARGET**，不是当前代码行为或现行静态检查结果。

| # | 原则 | 蓝本来源 | 目标约束与本仓边界 |
|---|---|---|---|
| P1 | Subscription 身份独立于物理位置 | SIM-INV-001/006/008；ARCH-RULE-001 | 目标身份/账户不得以 `slotId` 为主键；但本轮沿用现有 `simId` 契约，不新增公共 SubscriptionId，也不承诺跨删除/设备的永久身份。 |
| P2 | 单一数据源 | SIM-INV-002/012；ARCH-RULE-002 | 每个 Aggregate 只有一个逻辑写者；迁移时不允许 Legacy 与新 Owner 对同一事实长期双写。 |
| P3 | 决策记录必要输入版本 | SIM-INV-003/004；ARCH-RULE-003 | 记录使用的 T/A/R 快照及作用域；锁外 I/O 后复核相关 bindingRevision、SessionRef、LoadScope/attempt 和因果。不存在覆盖全部状态的单一全局 Revision，不因无关资料变化拒绝合法结果。 |
| P4 | 外部回调门控 | SIM-INV-009/011；ARCH-RULE-004 | 命令结果检查 operation/step/attempt 及原 serial/session；无请求上报携带来源、路由和可证会话，不伪造 OperationId。Adapter 转观察/结果，Application/R 接收器决定接受，不直接写 Aggregate。 |
| P5 | Compatibility 只读派生 | SIM-INV-005/006/014；ARCH-RULE-005 | 兼容查询和旧格式输出从核心事实派生，不成为拓扑、角色或操作事实的第二写者。 |
| P6 | Domain 层无基础设施依赖 | DDD；ARCH-RULE-006 | Domain 不依赖具体 RIL、DB/DataShare、IPC、HDI、FFRT 或 Host singleton；Port 的归属由目标模块边界明确，避免 Domain 反向 include 实现。 |
| P7 | 厂商 Hook 经过受控用例 | SIM-INV-013；ARCH-RULE-007 | Hook 由应用用例经端口执行，保留旧调用节点、次数及部分失败结果；不一律推迟到整个操作成功后，也不从兼容层旁路修改核心事实。 |
| P8 | 领域锁内无 I/O | SIM-INV-003；ARCH-RULE-008 | 锁仅保护内存状态变更；DB、参数、RIL、IPC、Hook 和外部回调均在锁外执行。 |
| P9 | Owner 接受后发布对应事实 | SIM-INV-003/009；ARCH-RULE-009 | 内部事实事件在对应 Owner 接受后发布；兼容投影仍在旧程序节点可见。激活缓存可先于 DB 结果更新，Primary 可预写；不能等设备/存储全部成功再开放旧查询。提交不构成全局原子事务。 |
| P10 | 配置槽清单统一查询 | SIM-INV-005/014；ARCH-RULE-010 | T/SlotRegistry 收敛配置槽与合法目标来源；可见槽、实际卡/Port、并发 Modem/激活数量分别维护，不能合成一个槽数。产品开关、保留槽和能力门禁逐项核实。 |

`static_analysis` 是蓝本建议的验证方式，不代表仓库已有对应规则或 CI 检查。本轮如采纳，应将每条规则落到可执行的边界测试或静态检查，并明确豁免与结果。

### 2.2 目标分层架构（TARGET；源码依赖方向）

下图箭头从使用方指向所依赖接口；`implements` 表示实现方依赖内层端口声明。R 是封装的生产运行实现，放在纯 Domain 之外。RIL 是既有技术协作边界，运行协作另见 §9.1。

```mermaid
flowchart TB
    ENTRY["既有 API / Stub / InnerKit 兼容入口"] --> C["C：入站 / 查询 / 完成与输出适配"]
    C --> USE["Application：T/A 受控提交、O 用例协调"]
    C --> VIEW["已接受的只读视图接口"]
    USE --> VIEW
    USE --> DOM["Domain：T/A/O 纯规则、值对象与状态模型"]
    USE --> PORT["内层端口：Store / Execution / Observation / Host / 输出"]
    R["R：生产 State / EF / Security / STK / eUICC"] -.implements.-> PORT
    I["I：DataShare / 参数 / 事件 / Vendor 适配"] -.implements.-> PORT
    HOST["H：Core 组合根 / 工厂 / 回调与生命周期"] --> USE
    HOST --> R
    HOST --> I
    HOST --> HOSTIMPL["Core 私有 HostPorts 实现"]
    HOSTIMPL -.implements.-> PORT
    HOSTIMPL --> RIL["既有 TelRil / HDI / 搜网等宿主能力"]
```

端口均为私有协作抽象，非新增 IPC。`TopologyRepository` 等逻辑名称不强制接口放在 Domain：本方案 Store/Execution/Host 用例端口由应用层定义，纯 Domain 不做 I/O；若另抽纯领域 Repository 契约，须说明无平台依赖的语义。OperationContext 默认内存存储。`Subscription API`、`ISubscription` 不纳入本轮目标图。O 覆盖卡安全、号码修改、激活、主卡和本地 Profile 步骤，不限于 Profile；不在 Core 与 66250 复制同一高层任务状态机。

### 2.3 关键架构约束

| 依赖/协作边界 | 目标规则 | 状态与验证 |
|---|---|---|
| API → Application | API 负责旧契约、权限、参数/回调边界；通过应用服务处理用例，不直接改 Aggregate。 | TARGET；静态依赖检查建议。不得借此改变现有权限和错误顺序。 |
| Application → Domain | Application 编排准入、目标解析、外部效果、Owner 提交及完成输出；Domain 保持纯业务规则。 | TARGET；简单查询可走只读视图，不强制经过 OperationCoordinator。 |
| Domain / 内层端口 | Domain 只依赖纯值类型与规则；应用层定义本方案 Store/Execution/Observation/Host 用例端口；I/R/H 实现端口。 | TARGET；Domain 无 FFRT/DataShare/HDI/singleton，R 在纯领域层之外。 |
| Infrastructure → Application/Domain | Adapter 实现 Port、发出 Observation/Result；不直接调用 DomainService、改聚合或旁路 Application。 | TARGET；回调通过 ingress/Owner 门控。 |
| Compatibility → 核心事实 | Compatibility 只读派生旧接口视图和旧格式输出，不写 T/A/O 权威状态。 | TARGET；任何需要修改事实的旧 API 转换成应用命令。 |
| 跨聚合协作 | 使用 Command、不可变快照/值类型及已接受领域事件，不传共享可写 Aggregate。 | TARGET；同一端到端效果只由一个主协调者发出。 |
| DomainEventBus | 可选的同地址空间提交后通知；不替代既有 IPC、RadioEvent、Hub、Registry、CES 或回调。 | 不提供跨进程传输、全局顺序或恰好一次保证。SA ID 不等于进程 ID；66250 保持既有服务契约，不因可能共宿主而旁路。 |
| API/部署边界 | 本轮不新增公开 API、IPC、SA、进程、schema 或运行期开关。 | 当前统一设计约束；蓝本中新增接口项均须单独批准后才能成为实施目标。 |

## 3. Bounded Context 到组件映射（TARGET）

本节采用主文档 T/A/O 单写者模型，C 为兼容实现职责。Manager/Arbiter 是责任组的旧候选名：`TopologyManager` 对应应用 `TopologyService` 与领域 `AccountTopology`；`RadioResourceArbiter` 对应应用 `AssignmentService` 与纯 `AssignmentPolicy`；`OperationCoordinator` 为应用协调器，`OperationContext` 保存运行期状态。它们不要求一一建类，也不将整个协调器放进纯 Domain。

### 3.1 Subscription Topology Context

| 领域概念 | 目标组件 | 当前对应 | 状态 | 职责与迁移关键点 |
|---|---|---|---|---|
| 拓扑状态与应用提交 | `AccountTopology` / `TopologyService` | `MultiSimController` 的 ICCID/账户/映射职责（部分） | PROPOSED | 应用提交与纯规则分开，维护已接受账户/绑定及视图；保留 simId 旧契约，不承诺永久身份。 |
| 应用 Store Port | `ITopologyRepository`（候选名） | 无独立 Port；缓存嵌在 Controller | PROPOSED | T 应用层定义账户/拓扑存取契约，Domain 不访问存储；按现有 DataShare 能力实现。 |
| Repository Adapter | `TopologyRepositoryImpl` | `SimRdbHelper` + Controller 缓存 | PROPOSED | 当前 `SimRdbHelper` 实际使用 DataShare；不得据命名声称 NativeRdb 直写或 DB 与快照原子。 |
| SlotRegistry | `SlotRegistry` | `SIM_SLOT_COUNT*` 宏、`slotCount_` 与多个槽位边界检查（[当前定义](../interfaces/innerkits/include/telephony_types.h#L30-L63)） | PROPOSED | 蓝本使用的 `TELEPHONY_SLOT_COUNT` 标识未在当前源码检出；目标应收敛现有多种容量来源，并覆盖产品可配置性/分布式槽规则，不能仅替换常量名。 |
| CardDetector | `CardDetector` | `MultiSimMonitor` / `MultiSimHelper::IsEsim`（部分） | PROPOSED | 将卡型检测封装为输入观察；不得把 ATR/类型识别等同于账户身份发现。 |
| 共享 RIL 执行适配 | `RilExecutionAdapter` / HostPorts | `TelRilManager` / `TelRilSim`（部分） | PROPOSED | 由 R/I 与宿主端口协作，供各用例复用；不属于 T 的私有事实源，回调先经接收器门控。 |

**迁移关键点：**将 `MultiSimController` 中 `InitIccId`、`UpdateDataByIccId`、标签/账户查询等拓扑职责逐项迁入 T；缓存从槽索引结构整理为带身份、绑定和来源的 Snapshot。蓝本所说将槽位基数收敛至 SlotRegistry 是目标清理工作，不能借此改变产品槽位能力；`MultiSimHelper::IsEsim()` 的 ATR/类型判断进入 CardDetector，但仍与账户身份发现分开。

### 3.2 Business Operation Context（O）

| 领域概念 | 目标组件 | 当前对应 | 状态 | 职责与迁移关键点 |
|---|---|---|---|---|
| 操作状态与应用协调 | `OperationContext` / `OperationCoordinator` | Controller 激活/切换、RadioProtocolController；Core eSIM 本地步骤 | PROPOSED | O 的状态/规则与应用协调分层；关联 step/attempt，不复制 66250 高层流程。 |
| 运行期上下文 Port | `IOperationRepository`（候选名） | 无统一上下文仓 | PROPOSED | 由 O 应用层定义进程内存取，持久恢复另审；不让 Domain 访问 FFRT/DB。 |
| OperationRepository Adapter | `RuntimeOperationRepository` | Handler 等待字段、请求 map/session 分散 | PROPOSED | 收拢运行态关联。进程重启后已丢失的内存项不能再更新成 `ABORTED`；只能重查设备/持久事实，或另外设计持久日志。 |
| Core eUICC Adapter | `EuiccAdapter` | `EsimManager` + `EsimFile` + `TelRilManager` | PROPOSED | 封装本地逻辑通道/APDU/回执；保留真实卡侧调用链和错误语义。 |
| CardChannel | `ICardChannel`、可选 `PsimCardChannel` | PSIM/eSIM 文件与 APDU 路径无统一 CardChannel 抽象 | PROPOSED | 仅在确有共同契约时引入；不能把 PSIM EF I/O 与 eSIM APDU 强行合并成相同协议。 |

**迁移关键点：**在既有可用操作入口上关联内部 OperationId/step/attempt，不新增公共 IDL 参数；RIL/eUICC 回调由 ingress 做门控。Core `EsimManager`/`EsimFile` 负责本地 APDU 与卡侧原语；SA 66250 的下载/OSU 主流程保留单一远端 Owner。

### 3.3 Radio Resource Context

| 领域概念 | 目标组件 | 当前对应 | 状态 | 职责与迁移关键点 |
|---|---|---|---|---|
| 分配状态、策略与应用提交 | `AssignmentView` / `AssignmentPolicy` / `AssignmentService` | ProtocolController 与 Controller 角色/Primary 流程（部分） | PROPOSED | 角色意图、选择、资源计划与已观察资源分开；应用层提交，Domain 纯策略，不以偏好推断设备/网络成功。 |
| 应用 Store Port | `IAssignmentRepository`（候选名） | 参数/缓存散布 | PROPOSED | A 应用层按既有字段访问存储，不虚构新参数/库表。 |
| Assignment Adapter | `AssignmentRepositoryImpl` | 系统参数 + DataShare 路径（按字段不同） | PROPOSED | 参数与 DataShare 是不同外部资源，无跨资源原子性保证。 |
| RolePolicy | `RoleAssignmentPolicy` | `MultiSimController::SetDefault*` 等（部分） | PROPOSED | 保留 Voice、SMS、Data 各自读取/回退差异及 Primary→Data 联动。 |

**迁移关键点：**`SetDefaultVoiceSlotId`、`SetDefaultSmsSlotId`、`SetDefaultCellularDataSlotId` 的角色决策进入 A；`RadioProtocolController::SetRadioProtocol` 的异步设备效果由 O 关联、A 提供资源计划、RIL Adapter 执行。不能仅用新仲裁器替换现有 condition-variable 等待而丢失超时、响应、回退和设备效果核验语义。

### 3.4 Compatibility / ACL（横切实现职责）

| 领域概念 | 目标组件 | 当前对应 | 状态 | 职责与迁移关键点 |
|---|---|---|---|---|
| ACL / 旧 Slot API | `LegacySlotApiFacade` | `SimManager` + `CoreServiceSim` / Proxy / Stub（部分） | PROPOSED | 将旧 slotId 调用翻译为 SlotTarget/SubscriptionTarget 等内部类型，保持既有签名和权限行为。 |
| LegacySlotView | `LegacySlotView` | 原缓存/Manager 查询 | PROPOSED | 纯查询从对应已接受视图读取；GetShowNumber/QueryImsSwitch 走保留副作用/存储访问的查询用例，不能全部改为纯快照。 |
| VendorHook Port/Adapter | `IVendorHook` / `VendorHookAdapter` | `TelephonyExtWrapper` | PROPOSED | 保留 `dlopen` ABI，通过应用命令门控调用；函数指针接口抽象不代表厂商库变成 IPC 服务。 |

### 3.5 易遗漏功能的责任细分

下表按主文档稳定子项说明汇总类名不能覆盖的行为；事实/分支证据见[读取来源与失败轨迹](SIM_Architecture_Refactor_Design.md#legacy-visibility)，非新增接口。

| 功能 | 当前必须保留的行为（FACT） | 目标承接（TARGET） | 写权与验收关联 |
|---|---|---|---|
| [SIM-023.a](SIM_Architecture_Refactor_Design.md#sim-023-a) | 显示名资料维护，不等同于号码写卡 | T 属性；I 旧存储；C 输出 | WU-05；旧成功/失败及通知 |
| [SIM-023.b](SIM_Architecture_Refactor_Design.md#sim-023-b) | SetShowNumber 先写 MSISDN，再写 DB/缓存；只有整体成功由 Manager 通知账户 | O 用例→R 写卡→T 属性提交→C 原输出 | WU-05/06/07；写卡失败与写卡成功/DB 失败分别验收 |
| [SIM-023.c](SIM_Architecture_Refactor_Design.md#sim-023-c) | GetShowNumber 卡缓存→账户→IMPU 回退，可能异步回写 DB | T 查询用例经 R/Host 读；回写固定原目标并委托 T | WU-05；不等待回写，旧任务不能改新卡 |
| [SIM-031.a/b](SIM_Architecture_Refactor_Design.md#sim-031-a) | 标签/Profile 展示、在位资料按原来源维护 | T 资料及带来源观察；R 运行事实独立 | WU-05；标签不作永久 Profile/Port 身份 |
| [SIM-031.c](SIM_Architecture_Refactor_Design.md#sim-031-c) | 先置映射标志，再人工输入 ABSENT；后续清理消费标志 | O/remap 协调 T 标志、R 状态及 C 原输出 | WU-05/06/09；相同状态、状态调用失败、标签保留分别验证 |
| [SIM-045.a](SIM_Architecture_Refactor_Design.md#sim-045-a) | Save/QueryImsSwitch 按 ICCID 访问 DataShare | T 偏好存取用例→I/store，非 A 角色字段 | WU-05；保持缺省和返回判断 |
| [SIM-045.b](SIM_Architecture_Refactor_Design.md#sim-045-b) | IMS 能力/配置观察与偏好不同 | R/carrier；IMS 权威运行状态仍留原模块 | 保持原观察路径，不增第二写者 |

## 4. 部署模型

### 4.1 当前部署事实（FACT）

| 核对项 | 当前证据 | 边界 |
|---|---|---|
| Core SA 4010 | `sa_profile/4010.json` 指定 `process=telephony`、`name=4010`、`libtel_core_service.z.so`（[配置](../sa_profile/4010.json#L1-L11)）。 | 静态配置不证明设备已启动。 |
| Core / SIM / RIL 构建 | 根 `BUILD.gn` 把 Core、SIM、搜网、RIL 源编入 `tel_core_service`（[目标和源清单](../BUILD.gn#L30-L45)、[SIM/RIL/Core](../BUILD.gn#L67-L138)）。 | 当前没有独立生产 SIM SO。 |
| Core eSIM | `core_service_support_esim` 默认关闭，可由产品配置开启（[开关](../telephony_core_service.gni#L14-L40)、[条件源](../BUILD.gn#L139-L145)）。 | 不是所有产品都包含 Core eSIM Runtime。 |
| SA 身份 66250 | 本仓 `EsimServiceClient` 获取既有 `IEsimService` proxy；`telephony_trust.json` 的 `process=telephony` 条目同时列出 4010、66250（[客户端加载](../frameworks/native/src/esim_service_client.cpp#L65-L115)、[信任配置](../services/etc/init/telephony_trust.json#L1-L14)）。 | 这只是配置事实；不能单凭它证明设备实际进程、服务实现位置或故障隔离。 |
| `telephony_ext` | `TelephonyExtWrapper` 对 `libtelephony_ext_service.z.so` 执行 `dlopen`/`dlsym` 并调用函数指针（[wrapper](../services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp#L22-L64)）。 | 同进程插件边界，不是 IPC/SA。 |
| eSIM 两条路径 | 高层 Download/OSU 经 `EsimServiceClient → IEsimService`；本地 APDU 经 `CoreService → EsimManager → EsimFile → TelRilManager → HDI RIL`（[IDL/Client](../interfaces/innerkits/IEsimService.idl#L25-L53)、[APDU链路](../services/core/src/core_service.cpp#L1150-L1172)）。 | 不可合并成单一服务内调用链。 |

### 4.2 目标部署建议（TARGET）

```mermaid
flowchart LR
    APP["既有 Framework / API / IPC"] --> CORE
    subgraph HOST["SA 4010 / telephony 宿主；目标仍同进程"]
        CORE["CoreService / 既有 Core SO"] --> SIMSO["目标私有 SIM SO<br/>API 协作 + Application + T/A/O + R + Adapters"]
        CORE --> RIL["既有 TelRilManager / HDI 集成"]
        CORE -.dlopen/dlsym.-> EXT["telephony_ext 插件"]
        SIMSO -.运行时端口委托.-> HP["H：Core 私有 HostPorts 实现"]
        HP --> RIL
        SIMSO --> BUS["进程内 DomainEventBus"]
    end
    APP -.EsimServiceClient / 既有服务 IPC/callback.-> SA66250["既有 SA 身份 66250<br/>服务实现/进程需产品证据"]
    SIMSO --> DS["DataShare / 系统参数 / Registry / CES"]
    RIL --> MODEM["HDI RIL / Modem / UICC / eUICC"]
```

输入稿中的“`esim_service` SA 66250 独立进程、OperationCoordinator/OperationRepository 位于该进程”不能作为当前事实，且与本仓当前统一设计“保持既有 SA/进程、不新增服务；本地 OperationCoordinator 在 SIM 私有 SO，66250 保留其既有高层任务主流程”不一致。若确需把 66250 确认为独立部署并迁移 Owner，须先有产品服务配置/实现证据、跨进程协议和唯一主编排裁决；在此之前图中把 66250 画为外部既有服务契约，不断言进程位置。

同样，蓝本中的新增独立 `IEsimManager` IPC 服务、`ISubscription` 或新增 OperationId 公共参数属于契约演进；本仓已有的 `IEsimManager` C++ 协作接口必须保留。这些契约演进不属于本次目标部署；目标私有 SIM SO 作为同进程链接/责任边界，不能宣称独立故障隔离。图中 SIMSO→HostPorts 是运行委托，不是 SIM 链接 Core 具体符号；源码/链接方向按 §2.2 和主文档核验。真实的 `dlopen` 调用方向是 Core/Wrapper 加载并调用扩展库，不能将 `telephony_ext.so` 画成“IPC 端点”。

| 蓝本部署项 | 核验/裁决 | 本文采用口径 |
|---|---|---|
| Core SA 4010 留在 `telephony` | 当前 profile/构建支持 SA 4010 的配置；目标保持既有宿主。 | TARGET：Core + 私有 SIM SO 同进程，不新增 SA。 |
| eSIM SA 66250 独立进程 | SA ID 和 `process=telephony` trust 配置可见；本仓未包含该服务实现或实际设备进程清单。独立进程/故障隔离未证实。 | OPEN：保持既有 `EsimServiceClient` IPC 契约；进程位置不画成事实或已批准目标。 |
| 66250 内运行第二个 OperationCoordinator | 与“一个端到端任务只有一个主编排者”冲突，也与本仓目标由 SIM OperationCoordinator 管本地操作、远端服务保留其高层任务相冲突。 | 不采用双 Owner。若批准拆分，必须重新明确端到端操作所有权和协议。 |
| `telephony_ext` 通过 IPC 访问 | 当前实现是 `dlopen`/`dlsym` 函数指针调用。 | 同进程插件；目标通过 Host/Vendor Adapter 保持 ABI，不改画成 IPC。 |

## 5. 线程模型与回调准入

本节区分**当前实现事实**和**目标线程约束**。目标的“ffrt task”是实现选项，不要求将现有线程/Handler 全量替换，也不意味着所有回调都可安全汇入一个线程。

| 组件/边界 | 当前事实（FACT） | 目标约束（TARGET） |
|---|---|---|
| `MultiSimController` | 有 `ffrt::condition_variable` 等等待状态；账户/角色/激活/主卡流程混合。 | Application 对同一 Owner 的提交串行；等待与设备操作不持有 Aggregate 锁。 |
| `RadioProtocolController` | `SetRadioProtocol` 使用 Handler、超时事件和 `ffrt::condition_variable`（[声明/入口](../services/sim/include/radio_protocol_controller.h#L38-L86)、[实现](../services/sim/src/radio_protocol_controller.cpp#L103-L141)）。 | 将状态门控、命令发送、结果核对分开；不能以超时断言设备没有效果。 |
| `EsimFile` | 多个 APDU/配置步骤各有独立 ffrt mutex/condition variable 与完成标志（[声明示例](../services/sim/include/esim_file.h#L282-L380)）。 | 以 operation/step/attempt 关联响应；不在锁内做 APDU、IPC 或等待外部回调。 |
| RIL / eUICC 回调 | Observer、Handler、serial/session 并存。 | 命令结果检查 operation/step/attempt/原传输关联；无请求上报检查来源/路由与可证会话，不能要求不存在的 OperationId；需要时关联重查。 |
| Event 分发 | 各旧事件/回调有独立语义。 | 同地址空间内部通知不提供跨进程传输；SA 身份不决定地址空间，66250 仍使用既有服务契约。 |
| 资源生命周期 | Handler、线程、条件变量和 Observer 注销分散在多个对象。 | 关闭时停止受理、注销输入、排空在途工作后销毁；不能强杀线程解决可能仍运行的厂商调用。 |

```text
RIL/eUICC 输入
  → Adapter 解析来源、路由与原传输关联
  → Application/R ingress：区分命令结果与无请求观察
  → 校验相关 operation/step/attempt 或 SessionRef/LoadScope/因果
  → 对应 Owner 锁内接受该步骤事实或兼容投影
  → 锁外按原步骤执行所需 I/O；继续返回 ingress 复核和提交
  → 在原契约节点输出返回/通知；内部事件不充当统一完成点
```

这不是“先把最终结果提交，再补做 I/O”的统一算法。激活、Primary、号码和 Profile 各有不同提交/输出节点，按[兼容轨迹](SIM_Architecture_Refactor_Design.md#legacy-visibility)执行。查询无副作用时直接走读视图，不进入 O。

66250 请求/回执保留既有 IPC/callback；即使某产品共进程，SA ID 也不授权改用内存总线。端到端任务只有一个主编排者；本地关联状态不复制远端补偿器。

会话准入采用主文档 [SessionRef/ExpectedTransition](SIM_Architecture_Refactor_Design.md#session-transitions)：R 维护 runtimeEpoch、bindingGeneration 和按 LoadScope 的 recordsGeneration，T 只维护已接受绑定的 bindingRevision。局部 Refresh 不作废未受影响请求；合法切换新会话须符合范围、身份、因果和期限，不能仅凭同槽 READY 放行。

## 6. 持久化模型

### 6.1 Repository Port 与存储映射

| Repository / 目标状态 | Aggregate | 建议存储 | 当前对应 | 迁移约束 |
|---|---|---|---|---|
| `ITopologyRepository` / `TopologySnapshot` | Topology | 现有 DataShare 契约 + 受控内存快照 | `MultiSimController::localCacheInfo_` / `allLocalCacheInfo_`、按槽 Manager vectors 与 `SimRdbHelper`（[成员声明](../services/sim/include/multi_sim_controller.h#L193-L225)） | 蓝本提到的 `data_[slotId]` 未在本次核验的当前 Controller 中找到；目标可用现有 simId 表示账户身份、slot 表示位置绑定，但不因此迁移 schema 或改变 simId 规则。 |
| `IAssignmentRepository` | Assignment | 现有参数/DataShare 的字段映射 | Controller 默认角色/Primary 缓存、系统参数和 DataShare | Voice/SMS/Data/Primary 规则不同；参数服务与 DataShare 不构成一个事务。 |
| `IOperationRepository` / `OperationContext` | Operation | 默认进程内 Map；持久化需另行批准 | 无统一仓；Handler/RIL serial/session 分散 | 重启后内存记录已丢失，不能再将该记录更新为 `ABORTED`。若需恢复终态/同 ID 恢复，必须另设计持久日志并批准。 |
| Adapter cache / slot registry | 不属于业务 Aggregate | 配置值或可失效只读缓存 | 当前常量、Manager 数组与局部缓存 | 不得让缓存成为第二事实源；明确加载、失效与版本策略。 |

当前账户 CRUD 经 `DataShareHelper`，没有据此证明本仓直接 NativeRdb/RdbStore 写库（[SimRdbHelper](../services/sim/src/sim_rdb_helper.cpp#L94-L136)）。用户蓝本中的 `Snapshot 原子写`应解释为目标内存快照的提交/发布规则；它不能代表 DataShare、系统参数与内存快照之间具备分布式原子事务。

### 6.2 锁外 I/O、提交和恢复

1. 锁内只执行校验、状态迁移和内存 Snapshot 发布；数据库、系统参数、RIL、IPC、Hook、回调均锁外执行。
2. 同一 Aggregate 的写入按 Owner 串行；执行锁外 I/O 后复核本次依赖的绑定版本、会话/LoadScope 代次、操作 attempt 与因果，再接受并发布新快照。禁止“解锁并发写 DB，按回包先后覆盖内存”。
3. 提交事件需在 Owner 接受变更后发出；但 DB 写成功、参数写成功、快照发布和外部通知之间仍须分别定义失败/重试/核对语义。
4. 应用用例通过 Store Port 访问存储，Domain 不参与 I/O。`StoreAccessCoordinator` 或本仓锁只约束受控访问，不代表跨连接、提供方事务或 DataShare+参数原子性（[聚合提交规则](SIM_Architecture_Refactor_Design.md#writer-cutover)、[存储边界](SIM_Architecture_Refactor_Design.md#target-contracts)）。
5. 重启恢复采用已持久化事实/设备状态重查；默认不承诺同 OperationId 跨重启恢复。需要持久操作日志时，先审查 schema、数据保留、状态迁移和外部完成契约。

资源可用与操作完成分离：R 的 [ResourceBarrier](SIM_Architecture_Refactor_Design.md#resource-recovery) 维护 `Checking / Available / InFlight / Quarantined`。等待超时或操作终结不解除隔离；O 可交出精简恢复记录并释放调用者引用，由 R 继续核对。解除需分别证明效果与执行静止，不能只读到期望值就重发。重建后首次冲突写重新准入；Q-03/Q-06 缺判据时只阻塞相关用例切换，不阻塞无关查询或用例，也不向旧 API 增加 Busy/Unknown 错误。

## 7. 重构前：源码事实与实际分层

### 7.1 现状结构图

当前代码没有显式分开的 Domain、Application、ACL、Repository 等 DDD 层。下图是对已存在组件的**逻辑分层阅读**，不是代码目录或现成层接口；同一组件可能横跨多个职责。

```mermaid
flowchart TB
    subgraph ENTRY["入口与兼容面"]
        FW["JS / CJ / ANI / Native Client"]
        IPC["Core Proxy / Stub / CoreServiceSim"]
        INNER["CoreManagerInner / InnerKit 直接委托"]
    end

    subgraph SERVICE["Core 服务装配与 SIM 协调（职责混合）"]
        CORE["CoreService"]
        SIM["SimManager：门面、每槽对象装配、转发"]
        MSC["MultiSimController / MultiSimHelper：账户、角色、激活、主卡"]
        MON["MultiSimMonitor：事件监听、重试、账户回调"]
    end

    subgraph RUNTIME["卡运行时与协议执行"]
        STATE["SimStateManager / SimStateHandle / IccState"]
        FILE["SimFileManager / IccFile / 各卡型文件"]
        SUPPORT["SIM SMS / 电话簿 / STK / 运营商配置"]
        EUICC["条件 EsimManager / EsimFile"]
        RPC["RadioProtocolController"]
        RIL["TelRilManager / TelRilSim / HDI RIL"]
    end

    subgraph PLATFORM["外部平台和技术边界"]
        STORE["DataShare 提供方"]
        PARAM["系统参数"]
        REG["State Registry / CES / CommonEvent"]
        EXT["telephony_ext 动态库"]
        ES["既有 eSIM 服务 SA 66250"]
    end

    FW --> IPC
    IPC --> CORE
    CORE --> SIM
    CORE --> RIL
    INNER --> SIM
    SIM --> STATE
    SIM --> FILE
    SIM --> MSC
    SIM --> MON
    SIM --> SUPPORT
    CORE --> EUICC
    MSC --> RPC
    MSC --> STORE
    MSC --> PARAM
    STATE --> RIL
    FILE --> RIL
    RPC --> RIL
    EUICC --> RIL
    MON -.状态/文件/平台事件.-> MSC
    STATE --> REG
    MON --> REG
    MSC --> REG
    FILE -.回写公开卡状态.-> STATE
    MSC -.反查非 SIM 能力.-> INNER
    FILE -.查询宿主能力.-> INNER
    CORE -.dlopen / 符号调用.-> EXT
    FW -.既有服务客户端 IPC.-> ES
```

图中代表性实现位置：

- Core 初始化并装配 RIL、条件 eSIM、SIM、搜网和 `CoreManagerInner`：[core_service.cpp](../services/core/src/core_service.cpp#L92-L128)。
- `SimManager` 按 slot 创建多组 Manager，并拥有 Controller/Monitor：[sim_manager.cpp](../services/sim/src/sim_manager.cpp#L41-L89)、[sim_manager.h](../services/sim/include/sim_manager.h#L209-L225)。
- 账户 CRUD 实际经 `DataShareHelper`：[sim_rdb_helper.cpp](../services/sim/src/sim_rdb_helper.cpp#L94-L136)。
- RIL 状态、文件、协议控制分别通过 Observer/事件处理完成，不是统一领域总线：[sim_state_handle.cpp](../services/sim/src/sim_state_handle.cpp#L832-L877)、[multi_sim_monitor.cpp](../services/sim/src/multi_sim_monitor.cpp#L93-L115)。

### 7.2 重构前分层职责表

| 逻辑层（分析视角） | 当前实现承担者 | 当前职责及耦合点 |
|---|---|---|
| 对外/API 与服务入口 | Framework 绑定、Native Client/Proxy/Stub、`CoreServiceSim` | 参数编解码、权限/能力检查、异步结果与转发；既有 IPC 入口不等同于所有共宿主直接调用入口。 |
| 服务组合与门面 | `CoreService`、`SimManager`、`CoreManagerInner` | SA 启动装配、按槽对象构造、管理入口转发；SIM 对 `CoreManagerInner` 还有非 SIM 能力反查。 |
| 应用协调（未显式分层） | `MultiSimController`、`MultiSimMonitor` | Controller 同时承担账户缓存/写库、激活、默认角色、Primary、Modem 切换与事件；Monitor 同时承担平台监听、账户初始化重试及回调。 |
| 卡状态与应用运行 | `SimStateManager`、`SimStateHandle`、`SimFileManager`、`IccFile`、`SimFile` 等 | 状态观察、安全操作、EF IO/解析、Records 完成及状态回写；卡运行逻辑不是纯领域层或只读 Repository。 |
| 多卡资源与 RIL | `RadioProtocolController`、`TelRilManager`、`TelRilSim` | Modem/slot 协议切换、HDI 请求与 serial/callback 关联；Core SO 内的 SIM RIL 模块仍与 Call/Data/Network/SMS RIL 信令共存。 |
| 持久化与系统适配 | `SimRdbHelper`、参数 API、`TelephonyExtWrapper`、Registry/CES/Hub | DataShare CRUD、系统参数、事件/回调、厂商函数指针访问分散在业务流程中；未证实有本仓 NativeRdb 直写或跨存储统一事务。 |
| eSIM 双边能力 | Core `EsimManager`/`EsimFile`；`EsimServiceClient` | Core 本地卡侧 eUICC/APDU 与既有 SA 66250 的高层任务服务是两条协作路径，不是同一对象或同一部署实现。 |

结论：当前分层主要由调用路径和代码模块形成，而不是严格依赖规则。Controller 同时混有领域决策、流程、设备访问和存储；状态/文件 Manager 同时拥有协议执行、状态转移和输出；平台事件也直接穿行于这些模块。图中反查 CoreManagerInner 与文件回写是现状耦合，非目标依赖规则。当前整体分层基线详见 [重构设计第 3 章](SIM_Architecture_Refactor_Design.md#current-flows)。

## 8. 物理部署：当前事实与目标提案

### 8.1 当前部署图（FACT）

```mermaid
flowchart LR
    APP["Framework / 调用方"] --> SA4010
    subgraph TELEPHONY["telephony 进程配置"]
        SA4010["SA 4010<br/>libtel_core_service.z.so<br/>Core + SIM + 搜网 + RIL + 条件 Core eSIM"]
        API["libtel_core_service_api<br/>CoreManagerInner / Client / Proxy"]
        EXT["libtelephony_ext_service.z.so<br/>dlopen 插件"]
        SA4010 --> API
        SA4010 -.dlopen / dlsym.-> EXT
    end
    APP -.EsimServiceClient / 既有 IPC.-> SA66250["既有 SA 身份 66250<br/>服务实现/运行进程不在本仓核实范围"]
    SA4010 --> HDI["HDI RIL / 驱动 / Modem / UICC"]
    SA4010 --> DS["DataShare 提供方"]
    SA4010 --> SYS["系统参数 / Registry / CES"]
```

| 核对项 | 当前源码事实 | 不能据此推出 |
|---|---|---|
| Core 服务身份 | `sa_profile/4010.json` 指定 `process=telephony`、`name=4010`、`libtel_core_service.z.so`（[4010.json](../sa_profile/4010.json#L1-L11)）。 | 设备上必然已启动或目标产品已装载该 SA。 |
| Core 构建内容 | `ohos_shared_library("tel_core_service")` 包含 SIM、搜网、RIL 和 Core 实现（[BUILD.gn](../BUILD.gn#L30-L45)、[SIM/RIL/Core 源](../BUILD.gn#L67-L138)）。 | 存在独立生产 SIM SO。当前并不存在。 |
| Core eSIM 条件 | `core_service_support_esim` 默认关闭，产品 `global_parts_info` 可启用（[telephony_core_service.gni](../telephony_core_service.gni#L14-L40)）；条件实现源见 [BUILD.gn](../BUILD.gn#L139-L145)。 | 所有产品/构建都包含 Core eSIM Runtime。 |
| SA 66250 | 客户端定义 SA ID 并通过 System Ability Manager 加载 `IEsimService` proxy（[esim_service_client.cpp](../frameworks/native/src/esim_service_client.cpp#L27-L32)、[加载与 proxy](../frameworks/native/src/esim_service_client.cpp#L65-L115)）。`telephony_trust.json` 将 4010、66250 同列在 `process=telephony` 配置中（[配置](../services/etc/init/telephony_trust.json#L1-L14)）。 | 该配置本身不证明设备实际启动/服务实现位置/故障隔离；也不能将它画成 Core 同进程对象。 |
| 厂商扩展 | 当前路径为 `libtelephony_ext_service.z.so`，Core 通过 `dlopen`/`dlsym` 取函数指针（[wrapper.cpp](../services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp#L22-L64)）。 | IPC、SA 或跨进程调用。 |

**Core eSIM 两条路径必须分画：**

1. 高层 Download/OSU 等任务：Framework → `EsimServiceClient` → SA 66250 `IEsimService`；IDL/方法声明见 [IEsimService.idl](../interfaces/innerkits/IEsimService.idl#L25-L53)，客户端调用见 [esim_service_client.cpp](../frameworks/native/src/esim_service_client.cpp#L202-L210)。
2. Core 卡侧 APDU：Core `SendApduData` → `EsimManager` → `EsimFile` → `TelRilManager` → HDI RIL；见 [CoreService](../services/core/src/core_service.cpp#L1150-L1172)、[EsimManager](../services/sim/src/esim_manager.cpp#L230-L239)、[EsimFile](../services/sim/src/esim_file.cpp#L1692-L1725)、[TelRilSim](../services/tel_ril/src/tel_ril_sim.cpp#L451-L458)。

### 8.2 目标部署图（TARGET）

```mermaid
flowchart LR
    APP["既有 Framework / IPC / InnerKit"] --> CORE["SA 4010 / telephony<br/>既有 Core SO"]
    subgraph HOST["同进程、既有 SA 4010 宿主"]
        CORE --> SIMSO["目标私有 SIM SO<br/>C + Application + T/A/O + R + I"]
        CORE --> RIL["既有 RIL / 搜网实现"]
        CORE --> API["既有 InnerKit 兼容门面"]
        SIMSO --> UTILS["既有公共 utils / API"]
        CORE -.Host Wrapper / dlopen.-> EXT["既有厂商插件 ABI"]
    end
    APP -.EsimServiceClient / 既有服务 IPC.-> ESIM["SA 66250<br/>保留高层 eSIM 任务；进程位置待产品核实"]
    SIMSO --> PLATFORM["DataShare / 参数 / Registry / CES 适配"]
    SIMSO -.运行时 HostPorts 委托.-> HP["Core 私有宿主实现"]
    HP --> RIL
```

目标是同 SA/同进程内的**链接和责任边界**，不是新的故障隔离边界。目标 SIM SO 名称、工厂、生命周期、符号导出及依赖图仍须 G2 构建验证；不要把不同设计草稿中的候选库名写成既成产物。主设计对目标部署/链接关系的定义见 [第 4.4 节](SIM_Architecture_Refactor_Design.md#target-deployment)。

## 9. 目标分层与重构前分层对照

### 9.1 目标协作（TARGET；运行时）

本图箭头表示运行调用/事实流，不表示相互 include；源码依赖见 §2.2。T/A/O 方框含各自应用接收器；T→I 表示应用用例经端口执行 I/O，纯 Domain 不直接访问基础设施。

```mermaid
flowchart TB
    OLD["既有 API / Core IPC / CoreManagerInner"] --> C["C：入站 / 查询 / 输出"]
    C --> T["T 应用提交 / 账户与拓扑规则"]
    C --> A["A 应用提交 / 角色与资源策略"]
    C --> O["O OperationCoordinator / 本地业务用例"]
    C --> READ["T/A/R 已接受读视图"]
    T -.目标和相关绑定版本.-> O
    A -.资源计划.-> O
    O --> R["R：卡运行 / 会话 / ResourceBarrier"]
    R -.可信观察.-> T
    R -.资源观察.-> A
    O -.核对后提交请求.-> T
    O -.核对后提交请求.-> A
    T --> I["I/H 经端口执行存储 / 参数 / 输出 / 宿主能力"]
    A --> I
    R --> I
    T -.内部已接受事实.-> BUS["可选同地址空间内部通知"]
    A -.内部已接受事实.-> BUS
    O -.内部已接受结果.-> BUS
```

T/A/R 的兼容读视图由各唯一写者在对应旧节点发布；C 不另写事实。GetShowNumber 等查询仍经保留副作用的用例。内部总线不决定旧同步返回/查询可见点；主文档的[职责](SIM_Architecture_Refactor_Design.md#target-contexts)、[内部接口](SIM_Architecture_Refactor_Design.md#target-contracts)和[部分失败轨迹](SIM_Architecture_Refactor_Design.md#legacy-visibility)共同约束此图。

### 9.2 对照表

| 设计维度 | 重构前（FACT） | 目标（TARGET） | 迁移约束 |
|---|---|---|---|
| 入口/兼容 | Proxy/Stub、CoreServiceSim、InnerKit 直接调用并存；转换、权限、转发职责分散。 | 保留旧入口；C 按入站、查询、出站分工。 | 保持签名、错误、权限上下文、线程与通知完成点；不能把直接调用误当公开 IPC。 |
| 用例协调 | `MultiSimController` 承载多个业务流程；`MultiSimMonitor` 混合事件接入和恢复。 | Application 负责路由、准入、串行提交和流程；简单读不强制进入操作协调器。 | 业务交付按功能拆分；写权按 WU 及全部旧入口共同切换，不把旧类整体改名成一个 Owner。 |
| 领域事实 | 账户缓存、卡状态、文件状态、参数和 DB 在不同 Manager/Helper 中写入。 | T/A/O 各有一个逻辑写者；只读视图标注来源、版本、失效规则。 | 同一写权只有一个 writer；旧激活、重载、初始化和异步回写同时改为受控委托。 |
| 卡运行 | State/File Manager 同时处理状态、协议、回调、解析及部分输出。 | R 包住并复用生产卡逻辑；必要时经 Adapter 输出观察和执行结果。 | 保留 RIL serial、卡型路径、PIN 专用完成及 Records 原语义。 |
| 技术适配 | DataShare、参数、RIL、Hook、Registry/CES 直接嵌入业务调用。 | 内层定义端口，I/H 实现并注入；锁外执行 I/O。 | 本地锁/Repository 不代表跨进程、跨连接或 DB/参数原子事务。 |
| 事件 | RadioEvent、CommonEvent、Registry、CES、Hub、账户回调、eSIM 回执各自存在。 | 内部 DomainEventBus 仅为领域内异步通知候选；兼容输出仍走各旧通道。 | 不以领域总线重排外部完成顺序，不以其代替跨 SA IPC。 |
| 代码部署 | SIM、Core、搜网、RIL 同编入 `libtel_core_service.z.so`。 | 候选私有 SIM SO，与既有 Core 同进程、同 SA 宿主。 | 不声称新 SO 已构建，也不据此宣称崩溃隔离。 |
| eSIM | Core 卡侧 Runtime 与 66250 高层任务服务并存。 | 保留双通道；Core 仅管理本地卡能力/关联，远端服务保留其任务边界。 | 单个端到端任务只能有一个主编排者；Core 不复制 66250 的完整流程。 |

## 10. 旧模块到目标模块/功能的迁移映射

下表是**责任拆分映射**，不意味着源类会一对一搬迁。完整稳定功能项和逐方法证据参见 [SIM-001～SIM-062 主映射表](SIM_Architecture_Refactor_Design.md#feature-mapping)。

| 旧模块/功能 | 当前职责事实 | 目标职责承接 | 功能在目标部署中的位置 | 迁移时保持/禁止事项 |
|---|---|---|---|---|
| `CoreService`、`CoreServiceSim`、Stub/Proxy、NAPI/Native/ANI/CJ | 服务启动、外部入口、转发与调用检查。 | H 组合根及 C 入站/查询/输出 ACL。 | Core 与既有 Framework/API SO；SIM 私有 SO 通过兼容入口协作。 | 不新增 IPC/SA，不改变权限/错误优先顺序、接口签名和异步返回。 |
| `SimManager` | 每槽对象创建、统一 SIM 门面、容量/槽检查、Controller/Monitor 生命周期。 | C 兼容入口；H/Application 装配；SlotRegistry 管配置槽事实；R 保留 Manager 运行能力。 | 入口仍在旧 Core/API 面；SIM 管理实现候选进入 SIM SO。 | 不把 SimManager 全部视为 Topology；保持按槽能力、额外槽初始化和对象寿命。 |
| `MultiSimController` 账户/ICCID/身份发现/缓存 | 身份发现、账户及 DataShare CRUD；SIM-020～022。 | T 应用提交与 AccountTopology；I 旧存储。 | T/I 在 SIM SO；提供方仍外部。 | WU-01/02；建行涉及角色先委托 A 旧规则，初始化激活可保留 O/R 旧编排，但写缓存不能旁路 T。 |
| 名称/号码、标签/映射与 IMS 属性 | SIM-023.a～c、031.a～c、045.a/b，细分见 §3.5。 | T 资料/查询；O 号码写卡/remap；R 协议/状态；C 输出；IMS 偏好 T、能力 R。 | SIM SO 内跨模块协作，旧入口保留。 | 不把号码改成只写 DB、查询全改纯快照，或将映射标志当重映射完成；按 WU-05/06/07/09 收口。 |
| `MultiSimController` 默认 Voice/SMS/Data、Primary | 默认角色、参数/DB 写入、Primary/Data 联动及事件。 | A 的 RadioResourceArbiter/AssignmentRepository；O 负责多步切换；I 负责 DataShare/参数。 | A/O 在 SIM SO；系统参数与存储由外部适配访问。 | 保持 Voice/SMS/Data 不同的旧规则和 Primary→Data 联动；不将角色选择当设备/网络成功。 |
| `MultiSimController` 激活、RadioProtocol、Modem 互换 | 执行 RIL 命令、等待结果、更新状态/缓存/存储并可能触发后续主卡调整。 | O 作为流程/效果门控；A 规划资源；R/RIL Adapter 执行；T 接受已核实账户事实。 | Operation/Assignment 在 SIM SO；TelRilManager/HDI 保持现有构建边界。 | 单命令一个设备执行方；未知设备效果先核对，不能超时后盲目重试或双发。 |
| `MultiSimMonitor` | Radio/File/平台事件订阅，DataShare Ready/用户切换，账户重试和清理，账户回调/Registry 更新。 | I 事件适配和 service-watch；CommandIngress/Observation ingress；C 输出回调桥；必要的 T reconcile 用例。 | Monitor 替代职责在 SIM SO，事件提供方和 Registry/CES 保持原服务。 | 保持注册/反注册、重试条件、回调和 Registry 可见时点；不统一改成 DomainEventBus。 |
| `MultiSimHelper` | CommonEvent 组装、ICCID 哈希、SIM 数据构造。 | C 出站 CompatibilityOutput；I/数据 codec。 | SIM SO 内部适配；CommonEvent 服务外部。 | 既有载荷、sticky/ordered 属性及重复发布行为不变。 |
| `SimStateManager`、`SimStateHandle`、`IccState` | 卡状态、锁状态、安全/认证、RIL 观察、Registry/Observer 通知。 | R 保留卡状态/安全执行；Adapter 映射 Observation；C 查询层输出旧状态。 | 状态运行逻辑在 SIM SO，TelRil/Registry 保持既有边界。 | 保持六值 `SimState`、指定 ICCID/切换过滤和专用 PIN 响应；状态≠账户/Records/网络就绪。 |
| `SimFileManager`、`IccFile`、`SimFile` 等 | 卡型路由、EF 读写、解析、加载计数、Records 与状态回写。 | R 的 application/records/codec；LoadSession 和 RecordsGeneration 是渐进门控建议。 | SIM SO 内复用原生产算法；RIL 执行接口不迁入领域层。 | 不把计数/serial 当作同一代次；不改变合法刷新和对外完成时点。 |
| SIM SMS、电话簿、STK、运营商配置 | 卡内短信、ADN/FDN/PBR、STK 流程、运营商匹配/权限与 APN 初始化协作。 | R 对应 sms_storage、phonebook、stk、carrier 子模块；外部投递/联系人/数据业务留原仓。 | 卡侧代码在 SIM SO；SMS/Data/联系人提供方及业务服务仍在各自 SA/仓。 | 不扩展职责到完整 SMS、联系人、Call、Data 服务；保留 PIN2、文件多阶段和专用事件语义。 |
| Core `EsimManager` / `EsimFile` / `EsimController` | 本地 eUICC 信息、逻辑通道、APDU、认证、卡侧 Profile 原语和回执。 | R/CoreEuiccAdapter；O 仅管理本地步骤/等待关联；T 接受核实后的拓扑观察。 | 条件 Core eSIM Runtime 位于 SIM SO；HDI RIL 仍按原路径。 | 不把所有卡侧原语转给 66250；保留 Port/slot 输入及外层返回与内层 enumResult 差异。 |
| `EsimServiceClient` / 66250 | Client 加载 SA 66250 并调用既有 IDL 任务方法。 | I 的 ExistingEsimServiceAdapter/ReplyAdapter；远端任务主编排仍归既有服务。 | 66250 身份和已有 IPC 保持；trust 配置的 `process=telephony` 下同时列有 4010 与 66250，但仍须产品运行配置确认实际装载。 | 不复制下载/OSU 主流程；服务实现、重启恢复和设备实际装载需外仓/产品核验。 |
| `RadioProtocolController` | 三阶段 Radio Protocol 操作、slot/Modem 映射、回调和等待。 | A 保持期望角色/资源决策；O 关联切换流程；RilExecutionAdapter 承担既有协议执行。 | Adapter 和 Owner 在 SIM SO；RIL/HDI 路径维持原宿主。 | 不因目标接口而把当前协议状态说成无关联；保留已有 session/响应机制。 |
| `TelRilManager`、`TelRilSim` | 各信令模块适配 HDI RIL、Observer/请求回调。 | Host RIL Port/Execution Adapter + RilCallbackAdapter/CommandIngress。 | 现有 Core/RIL 构建位置可保持，不由 SIM 聚合接管全部信令。 | Adapter 的回调只映射并提交观察/结果；不直接改聚合，也不绕过 Application 调用 DomainService。 |
| `SimRdbHelper` / 参数 API | DataShare 账户 CRUD 与参数读写混在 Controller 流程。 | Topology/Assignment Repository 接口由 I Adapter 实现；参数独立 Adapter。 | SIM SO 内端口/适配代码；DataShare 服务和参数服务外部。 | 新 Repository 名称不证明新表/直写/事务；锁不保证多个连接、DataShare 与参数原子性。 |
| `TelephonyExtWrapper` | Core 调用进程内 `dlopen` 插件并取厂商符号。 | VendorHookAdapter 实现已有端口，厂商策略归调用该能力的业务 Owner。 | Wrapper 可随 Core/SIM SO 的符号和 ABI 实际依赖安排；厂商动态库仍为插件。 | 不将插件调用画成 IPC/SA，不改厂商 ABI，不在不确定效果后重复执行。 |
| 启停、同步等待、回调生命周期 | FFRT/标准同步原语、Handler、线程及析构清理分布在 Manager/Adapter。 | Application/Infrastructure lifecycle、StopAndDrain、资源 lease 与异步关联登记。 | 同进程 SO 卸载前停止受理、反注册并排空。 | 同步整理需逐路径验证；当前不存在目标全局排空保证，也不能强杀线程解决厂商调用。 |

## 11. 状态所有权、存储和事件迁移规则

### 11.1 当前事实到目标 Owner

| 当前数据/状态 | 当前主要维护位置 | 目标权威位置（TARGET） |
|---|---|---|
| 卡状态、ICCID、锁原因、CardType | `SimStateHandle` / `SimStateManager`；文件层可触发状态更新 | 卡原始运行事实由 R 保留；T 仅接受身份/绑定观察；C 投影旧公开状态。 |
| EF/Records/文件加载 | `SimFileManager`、各 `IccFile`/`SimFile` 与控制器 | R 持有卡应用会话和记录装载；R 维护 SessionRef 与按 LoadScope 的 recordsGeneration；T 单独维护 bindingRevision。 |
| 当前/历史账户缓存 | `MultiSimController` 的 `localCacheInfo_`、`allLocalCacheInfo_` 等 | T/TopologyRepository；只读副本有来源和失效规则。 |
| 默认 Voice/SMS | Controller 缓存和 DataShare 记录 | A/AssignmentRepository；保持各自旧规则。 |
| 默认 Data/Primary | Controller 参数、`lastCellularDataSlotId_`、RadioProtocol 状态 | A 为意图/选择/资源事实；I 写现有参数；O 协调物理切换。 |
| 操作状态与传输请求关联 | Handler 等待字段、RIL requestMap/serial、协议 session | O 保存本地运行期业务上下文；R 保持传输 serial/session。 |
| 66250 远端任务状态 | 由远端服务和既有回执承载；服务实现不在本仓验证 | 仍由既有服务持有；Core 只关联本地等待和可验证观察。 |

### 11.2 提交与恢复边界

- 目标 Domain 只计算和校验业务规则；DB、RIL、IPC、Hook 和外部回调都由 Application/Adapter 在领域锁之外执行。
- 同一 Owner 的提交需串行；执行外部 I/O 后复核必要的输入版本，再发布新的已接受快照。禁止“解锁并发写 DB，按回包先后覆盖内存”。以主文档的[受控写权协议](SIM_Architecture_Refactor_Design.md#writer-cutover)为准。
- `StoreAccessCoordinator`/本仓写锁只能约束本仓受控写；不意味着提供方事务、跨连接事务、DataShare 与系统参数原子性（[存储边界](SIM_Architecture_Refactor_Design.md#target-contracts)）。
- `OperationRepository` 当前目标是**运行期内存上下文**。进程重启后旧内存记录已不存在，不能再由该内存仓更新为 `ABORTED`；若要求写入可恢复终态，就需要另行批准和设计持久化日志。现阶段采用设备/持久事实重查，不承诺同 OperationId 跨重启恢复（[运行期操作说明](SIM_Architecture_Refactor_Design.md#target-contracts)）。

### 11.3 事件边界

- 进程内 DomainEventBus 是目标内部机制，只连接同一宿主地址空间内的消费者；它本身不提供跨进程传输。SA 身份与地址空间不是同一概念。SA 66250 回执必须经既有 IPC/callback 边界进入本地 Adapter/Ingress，不能画成可直接订阅 Core 内存总线。
- DomainEventBus 不替代 RadioEvent、CommonEventHub、State Registry、CES、账户回调或 `IEsimServiceCallback`；既有输出语义需要逐接口保持（[领域总线约束](SIM_Architecture_Refactor_Design.md#legacy-visibility)）。
- Adapter 的输入回调应先转换成类型化 observation/result，再提交 CommandIngress/队列，由 Application Owner 决定是否接受；Adapter 不应直接调用 DomainService 或修改聚合。
- Core 的本地 OperationCoordinator 不能与 66250 对同一高层任务各自成为主编排者。对 66250 所有的下载/OSU 任务，Core 只关联已有请求/回执；本地卡侧 APDU 操作则由 Core Runtime 按既有执行链完成（[eSIM 边界](SIM_Architecture_Refactor_Design.md#target-contexts)）。

每个状态的完整旧入口、新入口、过渡委托和删除条件以[WU-01～09](SIM_Architecture_Refactor_Design.md#writer-cutover)为准。功能汇总不能代替写权表；尤其 G3a 切 T 时，尚未迁到 G3c 的激活、初始化、重载和异步查询回写也必须委托 T，角色字段同步接入 A 的旧规则适配入口。

## 12. 迁移阶段与门禁

以下是[主文档阶段表](SIM_Architecture_Refactor_Design.md#migration-stages)的摘要，不另立完成标准。首轮结果为历史执行记录；本轮没有运行新测试。旧 capability 问题已随代码同步修正，不能从 2026-10-15 状态表重新推成当前阻塞；匹配存储契约、初始化闭包等缺口仍按切片核实。

| 阶段 | 目标/切换单位 | 进入/退出要点 |
|---|---|---|
| G0/G1 行为防护 | 依赖版本、生产初始化闭包、旧返回/状态/通知 Oracle | 真实编译/链接/运行、负控和清理；当前没有完整业务就绪证明。源码可定位不代表运行 PASS。 |
| G2a 依赖/所有权 | 原 SO 内整理 HostPorts、工厂/deleter、回调寿命 | 保留旧 writer/线程/生命周期；先通过直接调用与初始化边界，不启用新停止策略。 |
| G2b SO 机械拆分 | 移动已解耦生产源，保持旧驻留期 | Core→SIM 链接单向；Feature/CFI/符号/ABI/跨 SO 销毁与行为差分；不夹带新恢复策略。 |
| G2c 关闭协议 | 单独实现停止受理、注销、取消等待、排空与资源接收器 | 关闭超时/晚到回调/Hook 验证；无法排空不卸载。需要动态重建/卸载的产品交付前必须完成。 |
| G3a 账户/拓扑写权 | WU-01～05，包含遗留激活、初始化、重载、号码回写 | 同一状态全部旧入口同时委托；A 旧规则写入口先接入；不等待 G3c 再删旁路。 |
| G3b 卡会话/门控 | WU-06；SessionRef、LoadScope、合法转换 | 保留 PIN/解析/局部 Refresh 合法响应、人工 ABSENT 条件及原错误/输出。 |
| G3c 角色/本地操作 | WU-04/07 等对应写权，A 纯策略、O 单执行方 | 保留 Primary/Data 联动与部分失败；R 屏障/逐命令静止判据完成，旧恢复器关闭。 |
| G3d 支撑/eSIM | WU-08/09 及各支撑族 | 两组 eUICC 对象共用真实设备资源域；66250 主编排、回执和生命周期缺口先闭环。 |
| G4 新能力 | 单独批准后的产品/数据方案 | 四槽/MEP/身份/NULL 等不因内部拆分自动启用；产品协议、权限、设备矩阵分别验收。 |

切换顺序为“全部旧入口接入受控委托→验证→停止受影响写并排空/隔离→导入状态→一次切写→恢复→删除旧路径”。无法排空不切写；只读 Shadow 不写事实、发设备命令或输出旧通知。Legacy/Split 分开构建运行，不增加运行期双实现 Flag。

## 13. 目标文档审查清单

1. **总线按地址空间限定。** 进程内总线不提供跨进程传输；SA 身份不证明部署进程。66250 请求/回执保持既有服务 IPC/callback → 本地 Adapter/Ingress，不旁路为内存总线。
2. **Operation 单写者按任务界定。** 每一类端到端命令只允许一个主编排 Owner；Core 本地关联器不能复制远端任务状态机、重试和补偿。
3. **Adapter 不直接绕过应用层。** 回调适配器返回观察/结果给 ingress；由 Application 做门控、复核和提交。
4. **内存仓不能更新已丢失记录。** 重启后不能将已丢失上下文置 ABORTED；R 从 Checking 重新核对资源并准入，持久恢复同 ID 需单独方案。
5. **快照与外部存储分阶段提交。** DB 写、参数写、内存快照发布、通知发送需各自定义失败和重试语义；版本复核不能被“本地锁”或 Repository 名称替代。
6. **区分 IPC、SO 链接和动态加载。** `EsimServiceClient → SA 66250` 是服务边界；Core `dlopen` Vendor 库是进程内函数调用；目标 Core→SIM SO 是同进程链接/装载边界。图需标清依赖方向和运行时调用方向。
7. **eSIM 两条通道不可合并。** Core APDU 走本地 EsimFile→TelRilManager→HDI；LPA/下载任务走 EsimServiceClient→既有 66250。
8. **描述部署不能超出证据。** `telephony_trust.json` 是进程配置证据，不是设备运行验证；本仓没有 SA 66250 服务实现，服务主流程/重启恢复仍为 OPEN。

9. **图的箭头须有一种明确语义。** 源码依赖图不混画数据回流；运行图中的 Owner 包含应用接收器，不让纯 Domain 调具体 I。R 的存量生产逻辑不冒充纯领域代码。
10. **读接口与旧中间可见性逐项保持。** 号码查询异步回写、IMS 访问存储、映射人工 ABSENT、激活/Primary 部分失败都要有去向，不能统一成最终快照。
11. **切换按全部写入口和资源证据验收。** WU 与 G2a/b/c、G3a～d 一致；未知效果不解除设备隔离，静止依据缺失明确关联 Q 项。

## 14. 关键依据索引

规则均回到主文档，不再引用旧 V3 的某行作为另一套现行规范：

- [业务责任与 T/A/O/R/C/I/H](SIM_Architecture_Refactor_Design.md#target-contexts)、[源码依赖](SIM_Architecture_Refactor_Design.md#target-layers)、[目标部署](SIM_Architecture_Refactor_Design.md#target-deployment)。
- [当前业务调用](SIM_Architecture_Refactor_Design.md#current-flows)、[当前构建与部署](SIM_Architecture_Refactor_Design.md#current-deployment)。
- [SIM-001～062 主项及 8 个子项](SIM_Architecture_Refactor_Design.md#feature-mapping)、[WU-01～09 写权切换](SIM_Architecture_Refactor_Design.md#writer-cutover)。
- [SessionRef/LoadScope/ExpectedTransition](SIM_Architecture_Refactor_Design.md#session-transitions)、[ResourceBarrier/恢复准入](SIM_Architecture_Refactor_Design.md#resource-recovery)。
- [查询来源与部分失败轨迹](SIM_Architecture_Refactor_Design.md#legacy-visibility)、[跨 SO 生命周期](SIM_Architecture_Refactor_Design.md#so-lifecycle)、[阶段与退出条件](SIM_Architecture_Refactor_Design.md#migration-stages)。
- [来源清单](SIM_Architecture_Refactor_Design.md#document-sources)、[待决事项 Q-01～06](SIM_Architecture_Refactor_Design.md#open-decisions)、[本轮 UC-01～09 核查记录](SIM_Architecture_Refactor_Design.md#supplement-review)。

源码链接是该基线的静态定位；E 编号及摘要在主文档保留。本文完成的是文档/定向代码复核，不增加设备、SO、服务或主机测试 PASS；Mermaid 尚未做渲染器视觉验收。
