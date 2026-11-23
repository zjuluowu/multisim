# AF-1 架构设计收敛检查点（2026-11-23）

> 文档状态：Operational / Pending。本文件是 AF-1（Architecture Design Sign-off）三轮工作的唯一进度交接入口。
> 它记录审计事实、对象模型分析和待裁决事项，**不是新的架构决策来源**；决策仍以目标架构 v1.2、源码详细设计、功能与 WU 及其批准记录为准。
> Host 证据不等于产品验收；本文件中的 PASS 仅指"本轮审计步骤完成"。

## 0. 进度总览

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| P0 | 代码仓基线和工作区保护 | PASS |
| P1 | 权威文档及已有架构决策核对 | PASS（无新增决策冲突） |
| P2 | 核心业务对象和数据所有权收敛 | 分析完成；2 项需架构负责人裁决（AF1-01、AF1-02） |
| P3～P6 | 接口契约、源码差距、文档修订、独立签收 | 不属于第一轮，未开始 |

本轮结论标记：`AF1_ROUND1_PARTIAL`（理由见 §5.1；P0～P2 的分析工作均已完成并持久化）。

## 第一部分：基线审计（P0）

### 1.1 Git 基线（2026-11-23 实测）

| 项 | 实测值 |
| --- | --- |
| Git 根目录 | `/mnt/e/github/multisim`（路径仅作本次记录，不作恢复依据） |
| 当前分支 | `main` |
| 本地 HEAD | `632f6a2a888cfbbc3c9387edc89732364e12a10e` |
| 本地 `origin/main` | `632f6a2a888cfbbc3c9387edc89732364e12a10e` |
| `git ls-remote origin refs/heads/main` | `632f6a2a888cfbbc3c9387edc89732364e12a10e` |
| HEAD 与远端 | 一致，无增量提交 |
| remote | `origin` = `github.com/zjuluowu/multisim`；`upstream` = `github.com/openharmony/telephony_core_service` |

近期提交（`git log --oneline -8`）：

- `632f6a2a8` feat(multisim): add account recovery and reload（T2）
- `06ca400a1` docs(sim): align architecture and migration gates for three sign-off conditions
- `5796fce68` feat(multisim): add account discovery provider loop（T1）
- `be63ae124` fix(multisim): seal core legacy shutdown path
- `50c0f4747` docs(sim): record three architecture sign-off decisions
- `bd4fbe833` docs(sim): independently review the multisim architecture baseline
- `befd76d4e` fix(multisim): linearize primary host shutdown
- `da99dd1a4` feat(multisim): add primary host drain gate

任务提示词中记录的基线 `632f6a2a` 仍然成立，无需增量核对。

### 1.2 工作区状态

- 暂存区：空。已跟踪文件：无修改。
- 未跟踪文件（用户文件，本轮不读写、不删除、不暂存）：`default.profraw`、`opencode.json`（均在仓库根）。
- 本文件创建前不存在 `docs/AF1*`，为全新创建；没有发现正在编辑的设计文档。
- 其他 worktree 存在（历史 G2a/审计分支及若干 `/tmp` prunable 项），本轮不使用、不清理。

### 1.3 AF-1 任务目标与本轮范围

- AF-1 目标：使核心对象、身份、关联、生命周期和唯一事实所有权达到可签收状态，让开发人员不依赖架构理论即可判断对象由谁创建、事实由谁维护、如何关联、用什么身份引用、何时失效销毁、谁有修改权。
- 第一轮范围：仅 P0～P2。P2 完成后停止。
- 本轮保护规则：不修改生产源码、GN、Golden/测试预期、历史报告和历史架构校验摘要；不 `git add`/commit/push；只新增或更新本文件。
  （仓库 AGENTS 的持续发布授权被本轮用户要求明确收窄，以本轮要求为准。）

### 1.4 必须遵循的已有设计决策（入口）

- SC-01 关闭责任、SC-02 Radio 身份和参与者、SC-03 完整写权：原始记录为
  `docs/history/reviews/SIM_Core_Architecture_Signoff_Convergence_20261123.md`，精确表述在第二部分核对。
- 一级目录五组（account / role_assignment / card / device / infrastructure）：`docs/SIM重构代码目录结构设计方案.md`。


### 1.5 当前代码结构事实（HEAD `632f6a2a` 实读）

以下只陈述源码事实。`services/multisim/` 全部 target 为 `testonly = true`（`services/multisim/BUILD.gn`），生产仍由 `services/sim/` 实现；头文件声明不等于实现。

| 责任 | 实际代码 | 本轮核实的事实 |
| --- | --- | --- |
| H | `infrastructure/assembly/`：`SimModule`、`sim_module_factory.*`、`SimHostPorts`、`SlotCatalog`、`ManagerPublicationSequence`、`PrimaryHostLifecycle` | 仅 `SimModule` 私有构造函数有定义；`AttachSimModuleToExisting`、`CreateOwnedSimServices`、两个 Deleter、`SimModule::StopAcceptingAndDrain` 均只有声明。`PrimaryHostLifecycle::StopAcceptingAndDrain` 有实现，但它是 Primary Host 切片的关闭入口，不属于 `SimModule` 成员。提示词中的 `SimModulePublication` 在源码中不存在，对应物为 `ManagerPublicationSequence` 与 `SimServicePublicationPort` |
| T | `account/`：`TopologyService`（持 `AccountRegistry`、`BindingIndex`、`DiscoveryObservationLedger`、`ActivationProgress`、`pendingDiscoveries_`）、`AccountStoreAdapter` | 实现 `CardObservationSink` 与 `PrimaryTopologyPort`；T1/T2 入口（`DiscoverAccount`、`ReconcileDiscovery`、`PrepareAccountReload`、`AcceptAccountReload`）存在 |
| A | `role_assignment/`：`AssignmentService`（持 `AssignmentAggregate`、自有 `RuntimeOperationStore`、`AccountRoleProtocol`、`pendingPrimary_`、`assignmentConflicts_`） | 实现 `RadioResultSink`、`ProfilePreferencePort`、`AccountRolePlanningPort`、`ActivationAssignmentPort`、`ScheduledRetrySink` |
| O | `infrastructure/request/`：`RequestId`、`StepRef`、`EffectKnowledge`、`RequestContext`、`RuntimeOperation`、`RuntimeOperationStore` | 容器按 `(RuntimeEpoch, RequestOwner)` 构造，由各 Owner 自持实例；不发令、不重试。`RetrySchedulerPort`/`ScheduledRetrySink` 声明也在同一头文件 |
| R·card | `card/runtime/CardRuntime`，`card/state/SessionRegistry`，`card/records/LoadSession`，`card/esim/LocalCardRequests` | `CardRuntime` 声明实现四个端口（含 `RadioExecutionPort`），但 `card_runtime.cpp` 只定义构造、`BindResultSinks`、`InvalidateVerifiedCardSession`、`PublishAcceptedCardDiscovery`、`EndLocalProfileWait`；`SwitchPrimaryRadio` 等运行方法无定义 |
| R·device | `device/resource/ResourceBarrier`、`ResourceRadioRuntime` | `ResourceRadioRuntime` 实现 `RadioExecutionPort`，自持 `barrier_`，经 `RadioPlatformPort` 发令；`device/radio/` 目录尚不存在（计划目录） |
| C | `infrastructure/compatibility/{api,events}`：`LegacySimFacade`、`LegacyCompletion`、`LegacyOutputAdapter` | 持入口引用和完成出口，不持业务状态 |
| I | `AccountStoreAdapter`（account）、`AssignmentStoreAdapter`/`AssignmentOutputAdapter`（role_assignment）、`ActivationHookAdapter`（account/activation） | 业务 Adapter 随 Owner，不集中在 infrastructure，与目录方案 §6.1 一致 |

## 第二部分：P1 架构决策核对

### 2.1 权威文档清单（均已核实存在）

| 代号 | 文档 | 唯一维护内容（依 `docs/README.md` 维护职责表） | 状态 |
| --- | --- | --- | --- |
| A | `docs/SIM_Target_Architecture_Design_v1.2.md`（1708 行） | 业务责任、事实写权、异步、资源、实施门禁 | Authoritative，整体仍 `draft/pending`，多数章节标 PROPOSED |
| B | `docs/SIM_Multisim_Source_Architecture_Detailed_Design.md`（824 行） | 当前与稳态路径、类、接口、逻辑 GN DAG | Authoritative |
| C | `docs/SIM重构代码目录结构设计方案.md`（686 行） | 一级物理职责、Infrastructure 内部边界 | Authoritative |
| D | `docs/migration/SIM_Architecture_Refactor_Design.md`（1381 行） | SIM-001～062、WU-01～09、字段台账 | Authoritative |
| — | `docs/SIM_Refactor_Execution_State.md` | 基线、实测证据、阻塞、下一批 | Operational，**不是目标架构** |
| — | `docs/history/reviews/SIM_Core_Architecture_Signoff_Convergence_20261123.md` | SC-01～03 决策记录与四文档矩阵 | Historical 记录；决策正文已落入 A/B/C/D 锚点 |
| — | `docs/history/reviews/SIM_Core_Architecture_Independent_Deep_Review_20261123.md` | 独立评审意见 | Historical；**意见不是决策** |

本轮只定向读取了 A §3、§4.1～4.2、§10、§12.3、§14；B §2、§3.2、§4、§5～11；C §4.2～4.4；签收收敛记录全文；深度评审 §2。未读 D 正文（只用其锚点），未通读执行状态。

### 2.2 设计主题索引

| 设计主题 | 权威位置 | 已收敛决策 | 当前是否存在冲突 |
| --- | --- | --- | --- |
| 业务对象定义（Slot/Subscription/Account/Binding/CardApplication/eUICC/Port/Profile） | A §3.1、§3.2.1；B §5 | 订阅绑定位置、访问卡应用、Radio 栈协议三种关系独立变化；用三个 Target 加受控索引表达，不新建实体表 | 无冲突。`EndpointInstance`/`ObservationSource` 明确为语义角色，源码无对应类型属预期 |
| T/A/O/R/C/I/H 职责 | A §4.1 边界表；B §2 | T、A 是仅有的两个业务上下文；O 是请求机制；R/C/I/H 为实现责任 | 无冲突。A §4.1 写"O 唯一拥有运行期请求状态"，B §8 写"每个进程所有者自己构造容器"，二者相容：O 定义类型和规则，实例由发起 Owner 持有（见 §3.3 表注） |
| Slot 与 Subscription 的关系、绑定基数 | A §3.1 图与注；B §5、§6 | 同一位置至多一个当前绑定，同一订阅至多一个当前位置（现有 `BindingIndex`）；空位置也保留 revision | 无冲突。多位置/多 Port 绑定为既有外部 OPEN（TGT-OPEN-006、G4），非本轮新增 |
| Role/Primary/Data/Voice/SMS 归属 | A §3.1、§4.1；B §7；C §4.3 | 全部归 A。Voice/SMS/Data 关联 `SubscriptionId`，Primary 涉及位置与 Radio 分配，不统一成一种标识 | 无冲突 |
| Request/Step/Session/Protocol/Transport/Resource 身份 | A §10.1（含 SC-02 锚点）、§11；B §8、§9 | 七类身份各有签发者与作用域，互不替代 | 设计内无冲突；**源码与设计不一致**（A 合成 `SessionRef`、成员取自目录），已由 B §9 记为待实现，见 AF1-03 |
| 资源屏障与租约 | A §12.3；B §9、§9.1；C §4.4 | 每个经证实的物理共享域一个准入者，租约由 R 统一管理；不得各建互不相知的屏障 | 设计内无冲突；**实例归属对象未命名**，见 AF1-01 |
| H 装配、生命周期、关闭 | A §14.2～14.3（含 SC-01 锚点）；B §11；C §5.3 | H 建图、封口、汇合收据、匹配销毁；不替任何一方消账 | 设计内无冲突；`SimModule` 与 `PrimaryHostLifecycle` 的关系未写明，见 AF1-02 |
| 写权与同键准入 | A §4.1、§13.2（SC-03 锚点）；B §7；D §5.2.1 | 每个物理键/DB 列一个业务 Owner 管准入 | 设计内无冲突；源码两个偏好入口仍直写，已由 B §7 记为待实现，见 AF1-05 |
| 目录与逻辑 target | C 全文；B §3.2、§4.1 | 五组目录；逻辑 target 与目录、SO 分开 | 无冲突。`ResourceRadioRuntime` 物理位于 `device/resource/`，B §3.2 稳态把 Radio 执行放 `device/radio/`（计划），属已知过渡 |

### 2.3 SC-01～03 状态

原始记录的结论为"三项条件在设计层已收敛，可提交架构负责人签收"。本轮逐项对照 A/B 锚点与当前源码，**没有发现需要推翻的依据，也没有改写其表述**。

| 决策 | 设计层 | 当前源码（`632f6a2a`） | 结论 |
| --- | --- | --- | --- |
| SC-01 关闭责任 | A §14.3 `#sc01-close`、B §11 `#sc01-receipts` 已明确 A/O/Scheduler/R/H 各自收据 | `PrimaryHostLifecycle::StopAcceptingAndDrain` 末段仍以 `AssignmentService::HasPendingDeviceEffects()`（`assignment_service.cpp:126`）作为 A 侧判据，没有 C0 原调用与逐键存储前缀的业务收据 | 设计已关闭，不重开；实现待做，B §11 已标"待实现" |
| SC-02 Radio 身份和参与者 | A §10.1 `#sc02-identity`、B §8/§9 `#sc02-radio` 已明确 | `assignment_service.cpp:199-200、239-240、392-393、600` 以 `bindingRevision` 和字面值 `"USIM"` 合成 `SessionRef`；`:253` 用 `catalog_.Slots()` 生成 C1 成员；`RuntimeOperationStore::RecordSent` 只有需要 `SessionRef` 的形式 | 设计已关闭，不重开；实现待做，B §8/§9 已标"待实现" |
| SC-03 完整写权 | A §13.2 `#sc03-key`、B §7 `#sc03-admission`、D §5.2.1 已明确 | `AcceptEntryPrimaryPreference`（`:659`）、`AcceptLocalProfilePreference`（`:662-664`，未使用来源 `StepRef`）直接调用 `store_.WriteUserPrimaryPreference`，未经 `AssignmentAggregate` 写集准入 | 设计已关闭，不重开；实现待做，B §7 已写明 |

收敛记录自带的 O1～O6 是产品/外仓证据类 OPEN，继续按原表保留，本轮不重复列出。

### 2.4 不需要重新讨论的已关闭事项

1. T、A 两个业务上下文的划分，以及 O 不是业务事实 Owner（A §4.1"O 的重新裁决"）。
2. 激活归 T、Primary/默认角色归 A（A §4.2）。
3. 五组一级目录（C）。
4. SC-01～03 的分责结论。
5. T1 账户发现、T2 恢复与重载的 Host 闭环设计（B §6）。
6. `bindingRevision`（T）与 `bindingGeneration`（R）互不具备签发效力（A §10.1）。
7. Core 拥有共享 RIL，SIM 不 Init/Stop 它（A §14.2）。

## 第三部分：P2 核心对象模型

阅读约定：**Owner** 指有权修改该事实的唯一责任方；"持有"指保存一份值或引用，不带修改权。类型列中"值"表示可自由复制的纯数据，"实体"表示有身份和可变状态，"机制"表示无业务事实的技术对象，"语义角色"表示设计中有此概念但源码无独立类型。

### 3.1 核心对象分类

| 类别 | 对象 | 归属 |
| --- | --- | --- |
| 一、物理与逻辑卡 | Slot、Endpoint、Port、eUICC、Profile、CardApplication、Subscription、AccountRecord、BindingRef、BindingIndex | 静态描述归 H；账户与绑定归 T；卡侧库存与应用归 R·card |
| 二、卡运行 | CardRuntime、SessionRegistry、SessionRef、LoadSession、CardApplicationTarget | R·card |
| 三、角色 | AssignmentAggregate、RoleSelection、Primary 投影、AssignmentTransition、角色写集 | A |
| 四、请求与设备执行 | RequestId、StepRef、RequestContext、RuntimeOperation、RuntimeEpoch、本地协议轮次、原生协议会话、传输 token、传输 serial、ResourceLease、fence、ResourceBarrier | 请求机制为 O（实例随发起 Owner）；设备执行归 R·device；epoch 归 H |
| 五、模块装配 | SimModule、工厂、SimHostPorts、SlotCatalog、ManagerPublicationSequence、PrimaryHostLifecycle | H |

### 3.2 表一：核心对象定义

| 对象 | 类型 | 创建者 | 唯一 Owner | 主要职责 | 生命周期 |
| --- | --- | --- | --- | --- | --- |
| Slot（`SlotId` / `SlotDescriptor`） | 值，只读配置 | H 启动时按产品输入构建 | H（静态目录） | 描述位置、端点种类、允许的 RIL 实例与 Radio 域、可见性 | 随模块实例；运行期不变 |
| Endpoint（`EndpointKind`） | `SlotDescriptor` 的属性；当前实例为语义角色 | 种类由 H；当前实例/可达性由 R 观察 | 种类 H；当前观察 R | 区分实体、eUICC、虚拟、受限、分布式 | 种类随配置；当前实例随来源出现/消失 |
| eUICC / Port / Profile | 语义角色。源码仅有 `legacyPortIndex`、`VerifiedPortRoute` | R·card 从卡侧协议观察 | R·card（库存与协议）；已接纳的账户绑定归 T；偏好归 A | Profile 库存、Port 路由 | 随 eUICC 在位；禁用 Profile 可无会话、无账户 |
| CardApplication（`CardApplicationTarget`） | 值（槽 + 应用键） | 调用入口按意图构造 | R·card 解释 | 定位一次卡应用访问目标 | 单次请求；应用键仅在当前载体内有效 |
| Subscription（`SubscriptionId`） | 值，包装旧 simId | 旧 provider 分配；T 接纳 | T | 本机业务身份 | 长期（持久）；不承诺跨删除/重装稳定 |
| AccountRecord（含 current/historical `AccountProjection`） | 实体 | T（`AccountRegistry::AcceptDiscoveredAccount` / 重载） | T | 账户资料及各字段 revision | 长期；内存投影可由重载重建 |
| BindingRef | 值（订阅 + 槽 + `bindingRevision`） | T（`BindingIndex`） | T | 标识"这次"订阅与位置的绑定 | 至下一次换绑/解绑；可被重新发现重建，但 revision 不回退 |
| BindingIndex（`SlotRuntimeEntry`） | 实体（索引） | `TopologyService` 构造 | T | 位置→当前绑定、空位置 revision、装配事实 | 随模块实例 |
| DiscoveryObservationLedger | 实体 | `TopologyService` | T | 每应用观察序列、发现与失效资格 | 随模块实例 |
| ActivationProgress | 实体 | `TopologyService` | T | 激活进行中标志（跨模块可见的业务进度） | 每次激活一条，完成后清除 |
| CardRuntime | 过渡接缝对象 | H（`SimModule` 成员） | R·card | 借用原生产对象、绑定结果出口、发布发现/失效观察 | 随模块实例；稳态退化为有限绑定接口（B §3.2） |
| SessionRegistry | 实体 | 当前由 `CardRuntime` 构造 | R·card（`card/state`） | 每卡应用当前会话、代次水位、`ExpectedTransition` | 随模块实例；代次只增不减 |
| SessionRef | 值（epoch + 应用 + `bindingGeneration`） | R·card（`SessionRegistry`） | R·card | 标识一次卡应用访问资格 | 至换卡、应用替换、路由失效或模块重建 |
| LoadSession / LoadToken | 实体 / 值 | R·card，每轮加载 | R·card（`card/records`） | 加载成员资格、一次消费、受影响读失效 | 一轮加载或 Refresh |
| AssignmentAggregate | 实体（聚合） | `AssignmentService` 成员 | A | 角色选择、切换事实、参数投影、角色写集登记 | 随模块实例；持久事实在参数/DB，可重载 |
| RoleSelection | 值（角色 + preference + effective） | A | A | 一个默认角色的偏好与生效订阅 | 长期（持久于旧参数/DB） |
| Primary / Data 投影（`lastPrimary_`、`lastData_`） | 聚合内属性，按 `SlotId` | A | A | 主卡位置与联动 Data 的参数投影 | 长期 |
| AssignmentTransition | 实体（每 `RadioDomainId` 至多一条） | A（`AdmitTransition*`） | A | 切换进行中、目标、阶段、业务尝试 | 一次切换 |
| 角色写集（`RoleWriteMember` 位集 + `roleWriteOwners_`） | 聚合内登记 | A | A | 同键写入的准入资格 | 一次请求；结果未知时保留 |
| PendingPrimarySwitch | 实体 | `AssignmentService::StartPrimarySwitch` | A | 一次 Primary 请求的冻结计划、成员、阶段、重试 | 一次请求至终态或核对完成 |
| RequestId / StepRef | 值 | 发起 Owner 的 `RuntimeOperationStore::Begin` | 发起 Owner（T/A/R 短请求） | 业务请求及其步骤/尝试的身份 | 一次请求；仅在本 epoch + Owner 命名空间内有效 |
| RequestContext / RuntimeOperation | 值 / 记录 | 同上 | 发起 Owner（机制由 O 提供） | 原目标、父关联、发送会话、完成路由、效果认知、终态 | 一次请求；证据保留期后 `RetireAfterEvidenceRetention` |
| RuntimeOperationStore | 机制 | 各 Owner 自行构造（A 的 `operations_` 等） | 持有它的 Owner | 步骤门控、一次领取、迟到证据 | 随所属服务 |
| RuntimeEpoch | 值 | H，每次模块实例创建 | H | 区分模块实例，使旧请求/会话/租约整体失效 | 一个模块实例 |
| ResourceBarrier | 实体 | 目前有两处构造（见 §3.7） | R·device | 物理域状态机 Checking/Available/InFlight/Quarantined、fence | 随 R 实例；新实例先 Checking |
| ResourceLease | 值 | `ResourceBarrier::Acquire` | R·device | 一次命令对完整资源集的执行资格 | 至设备结束且可复用的证据；Unknown 时转隔离保留 |
| ResourceRadioRuntime | 执行适配 | Host 测试装配（`SimModule` 未持有） | R·device | 取租约、每次发送签发传输 token、接纳设备证据后转交 A | 随装配图 |
| SlotCatalog | 只读值对象 | H | H | 静态允许接线与旧容量度量 | 随模块实例；以 const 引用/`shared_ptr<const>` 注入 |
| SimModule | 组合根 | 私有工厂（未实现） | H | 按依赖顺序持有 I/C 出口、R、T、A、用例、Facade | 一个模块实例 |
| SimHostPorts | 值（端口集合） | Core 宿主 | Core（H 借用） | 共享 RIL、入口、回调、号码查询端口 | 必须活到 SIM 排空结束 |
| ManagerPublicationSequence | 机制 | `OwnedSimServices` | H | 记录 eSIM→SIM 初始化与发布的阶段和控制块一致性 | 一次创建 |
| PrimaryHostLifecycle | 机制 | Host 装配 | H | Primary 切片的封口、单一关闭者、汇合 A/Scheduler/R 判据 | 一次模块实例的关闭轮 |
| LegacySimFacade / LegacyCompletion | 兼容边界 | H（`SimModule` 成员） | C | 原入口签名、原完成出口交付 | 随模块实例 |

### 3.3 唯一事实所有权矩阵

| 业务事实/对象 | 唯一 Owner | 主要关联对象 | 其他模块允许的操作 | 依据 |
| --- | --- | --- | --- | --- |
| 账户身份及 current/historical 资料 | T | `SubscriptionId` / `AccountRecord` | 查询；经 T 入口申请变更 | A §4.1；B §2、§6 |
| 订阅与位置的当前绑定、空位置 revision | T | `BindingRef` / `BindingIndex` | 查询、用 `StillMatches` 复核；R 只提供观察 | A §4.1；B §5 |
| 槽运行清单（是否已装配） | T（接纳 H/R 给出的装配事实） | `SlotRuntimeEntry` | 查询 | `TopologyService::AcceptSlotAssembly` |
| 激活进行中 | T | `ActivationProgress` | 查询（搜网、运营商配置等经旧出口） | A §4.1、§4.2 |
| 静态槽目录、端点种类、允许 Radio 域 | H | `SlotCatalog` | 只读 | A §3.2.1；`slot_catalog.h` |
| 原始卡/锁状态、Records、电话簿、STK、eUICC 库存 | R·card | 原生产对象（借用） | 观察；T 接纳发现/失效 | A §4.1；B §9 |
| 卡应用会话及代次 | R·card | `SessionRef` / `SessionRegistry` | 使用、复核是否仍当前 | A §10.1；B §9.1 |
| 加载成员资格 | R·card | `LoadSession` | 无 | A §10.2 |
| Voice / SMS / Data 默认角色（偏好与生效） | A | `RoleSelection` | 查询、申请切换 | A §4.1；B §7 |
| Primary 位置、Primary/Data 参数及用户偏好提示 | A | `AssignmentAggregate` 投影、角色写集 | 查询、在原节点经 A 入口提交 | A §4.1；SC-03 |
| Primary 切换进行中、目标、阶段、业务重试 | A | `AssignmentTransition`、`PendingPrimarySwitch` | 查询 | A §4.1 |
| 本轮 Radio 协议参与者（真实成员与路由） | R·device | 协议适配输出 | A 冻结一份本轮副本并只按该副本推进；H 的目录仅作输入 | SC-02；B §9 |
| 原生协议会话、传输关联 | R·device | protocolSession、token、serial | A 只携带本地轮次用于匹配 | A §10.1 |
| 设备资源状态与租约 | R·device（每个物理域一个 `ResourceBarrier`） | `ResourceLease`、fence | 用例步骤申请；不得自行释放 | A §12.3；实例归属见 AF1-01 |
| 请求步骤、效果认知、一次完成领取 | 发起该请求的 Owner（T/A/R 短请求），使用 O 的机制 | `RequestId` / `StepRef` / `RuntimeOperation` | R 提交证据；C 交付 | A §4.1、§4.2；B §8 |
| 外部持久化执行 | I（只执行，不拥有事实） | Store/参数/Provider | 执行已取得资格的 Owner 的写计划 | A §4.1；B §10.1 |
| 原完成出口登记与交付格式 | C（真实回调/等待对象在 Core 宿主） | `CompletionPlan` / `LegacyCompletion` | Owner 封存终态后交付一次 | A §4.1；B §10 |
| 模块实例、epoch、装配图、发布与销毁 | H | `SimModule` / `RuntimeEpoch` | 无 | A §14；B §11 |
| 共享 RIL 实例 | Core（不在 SIM 内） | `ITelRilManager` | SIM 借用引用 | A §14.2 |

表注：提示词初始框架中"请求步骤及完成 = O"一行按权威文档细化为"机制归 O，实例与其中记录归发起 Owner"。这不是新决策，是 A §4.1（O 不拥有任何对外可见业务事实）与 B §8（每个进程所有者自己构造容器）的合并表述。

### 3.4 表二：对象关联关系

| 对象 A | 关系 | 对象 B | 基数 | 关联维护者 |
| --- | --- | --- | --- | --- |
| Slot | 当前绑定到 | Subscription | 1 — 0..1（双向） | T（`BindingIndex`） |
| Subscription | 拥有资料 | AccountRecord | 1 — 1（current、historical 各 0..1 投影） | T（`AccountRegistry`） |
| Slot | 当前可访问 | CardApplication 会话 | 1 — 0..* | R·card（`SessionRegistry`） |
| CardApplication 会话 | 包含 | LoadSession | 1 — 0..*（按 scope） | R·card |
| eUICC | 提供 | Port | 1 — 0..* | R·card 观察（产品基数 OPEN） |
| eUICC | 容纳 | Profile | 1 — 0..* | R·card 观察 |
| 启用的 Profile / 实体卡 | 呈现 | CardApplication | 1 — 1..* | R·card |
| 已发现的卡应用会话 | 被接纳为 | BindingRef | 0..1 — 0..1 | T 接纳 R 的观察（`CardObservationSink`） |
| RoleSelection（Voice/SMS/Data） | 指向 | Subscription | 每角色 1 条 — 0..1；多个角色可指向同一订阅 | A |
| Primary 投影 | 指向 | Slot | 1 — 0..1 | A |
| AssignmentTransition | 属于 | RadioDomain | 0..1 — 1（同域同时至多一个切换） | A |
| Slot | 配置属于 | RadioDomain | * — 0..1 | H（静态） |
| RadioDomain（业务冲突域） | 对应 | 物理资源域 `ResourceKey` | 未在权威文档中给出映射规则 | 见 AF1-04 |
| RequestId | 包含 | StepRef | 1 — 1 当前步（step/attempt 递增） | 发起 Owner |
| 子请求 | 关联父 | 父 StepRef | * — 0..1 | 发起子请求的 Owner |
| 一次 Primary 请求 | 冻结 | 本轮参与者集合 | 1 — 1..* | A 冻结，R 提供并验证 |
| 一个设备步骤 | 持有 | ResourceLease | 1 — 0..1（覆盖完整资源集） | R·device |
| ResourceLease | 覆盖 | 物理资源域 | 1 — 1..* | R·device |
| 一次发送 | 标识为 | 传输 token（及可选 serial） | 1 — 1 | R·device |
| SimModule | 持有 | T/A/R/C 服务与 Adapter | 1 — 各 1 | H |
| SimModule | 借用 | Core 的共享 RIL、原生产对象、HostPorts | 1 — 各 1 | Core 拥有 |

关于"ID 作为属性还是独立关联中心"：权威设计的答案是**关系由 Owner 的受控索引维护，ID 只是引用值**（A §3.1 末、§4.1.1"不引入全局映射服务"）。`BindingIndex` 是 T 内部的索引，不是面向全模块的 ID 映射中心；其他模块拿到的是带版本的 `BindingRef`，用前复核。

### 3.5 表三：身份模型

以下七类身份互不替代。数值相等不代表可互换。

| 身份 | 源码类型 | 签发者 | 持有者 | 有效期 | 失效条件 | 禁止用途 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 业务请求 | `RequestId`（value + epoch + `RequestOwner`） | 发起 Owner 的 `RuntimeOperationStore::Begin` | 发起 Owner；随 `RequestContext` 传给 R/C | 一次请求 | 终态且证据保留期结束；模块重建 | 当作设备取消序号、跨重启追踪 ID；仅按 value 识别 |
| 2 请求步骤 | `StepRef`（request + step + attempt） | 发起 Owner `AdvanceStep` | 发起 Owner；R 在发送关联中保存原值 | 一步的一次尝试 | 步骤推进、重试、终态 | 授予写权（可追溯来源，但不等于资格，B §7） |
| 3 账户绑定 | `BindingRef`（订阅 + 槽 + `bindingRevision`） | T `BindingIndex` | A 的请求计划、T 的号码/激活目标 | 至换绑或解绑 | 换卡、解绑、同槽再绑（revision 递增） | 签发或充当卡应用会话；当作全局拓扑版本 |
| 4 卡应用会话 | `SessionRef`（epoch + 应用 + `bindingGeneration`） | R·card `SessionRegistry` | 访问该应用的请求（PIN、EF、号码写卡、Profile） | 至应用被替换 | 换卡、应用替换、路由失效、模块重建 | 用于纯 Radio 步骤；由 T/A 用 `bindingRevision` 合成 |
| 5a 本地协议轮次 | `RadioSwitchRequest::protocolSession`（uint32，当前由 A 的 `nextProtocolSession_` 递增） | A（本请求内的轮次编号） | A 的 pending；R 的发送关联 | 一轮三阶段交换 | 新一轮（重试）开始 | 当作原生协议会话号；跨请求比较 |
| 5b 原生 Radio 协议会话 | 原 `RadioProtocolController` 的 sessionId（未进入 Target 类型） | 原协议/平台 | R·device 协议适配，映射到 5a | 一次原生交换 | 协议结束或被替换 | 上浮给 A 作业务身份 |
| 6 单次传输 | 传输 token：`uint64_t`，`ResourceRadioRuntime::nextTransportToken_`；RIL serial：`RadioSubmitResult::transportSerial` | token 由 R·device；serial 由平台 RIL | R·device 的 `transmissions_` | 一次发送至其结束证据 | 结果被接纳或被核对 | serial 当作跨重启或跨协议身份（可复用/回绕）；没有 serial 的通知填造 serial |
| 7 设备资源租约 | `ResourceLease`（epoch + 资源集 + holder `StepRef` + fence） | R·device `ResourceBarrier::Acquire` | 提交该命令的 R 执行适配 | 至设备结束且可复用 | 完成证据、确认未发送、恢复证据后推进 fence；模块重建后先 Checking | 由 A/T 持有或释放；用等待超时、调用返回、本地 channel 清零来释放 |
| （辅助）模块实例 | `RuntimeEpoch`（128 位） | H | 嵌入 1、4、7 | 一个模块实例 | 模块重建、进程重启 | 当作业务持久版本；由新 epoch 推断旧命令已结束 |

说明：5a/5b 的区分依据 A §10.1 与 B §9"将原生 protocolSession 映射到本地轮次。当前 Host 计数仅能关联 Host 发送"。源码中这两者及传输 token、fence 都是裸整数，没有独立类型，属于 P3 接口契约要决定是否加类型的事项（AF1-06），不是职责未决。

另有一个容易混淆的名称：`ActivationConflictLease`（`account/activation/activation_assignment_port.h`）是 **A 签发给 T 激活用例的业务冲突资格**，不是设备资源租约；A §4.2 协作规则 3 明确"资格释放不等于设备可用"。

### 3.6 生命周期模型

| 分组 | 对象 | 说明 |
| --- | --- | --- |
| 长期存在（持久事实，重启后由存储恢复） | Subscription、AccountRecord 的资料、角色偏好、Primary/Data 参数 | Owner 为 T 或 A；内存投影可通过重载重建 |
| 随模块实例存在 | SlotCatalog、RuntimeEpoch、SimModule 及其服务、BindingIndex、SessionRegistry、ResourceBarrier、各 RuntimeOperationStore | 模块重建后全部换新；epoch 使旧引用失效 |
| 随卡/载体在位存在 | 当前绑定、SessionRef、LoadSession、eUICC/Port/Profile 观察 | 换卡时：R 先经 `CardObservationSink` 让 T 撤销资格，再发布新会话（`CardRuntime::PublishAcceptedCardDiscovery`）；历史账户保留 |
| 仅一次请求期间 | RequestId/StepRef/RequestContext、PendingPrimarySwitch、AssignmentTransition、角色写集登记、ActivationProgress 条目、ExpectedTransition、ResourceLease、传输 token | 结果未知时不随调用返回而结束，保留至核对完成 |
| 由业务 Owner 管理 | 账户、绑定、激活进度（T）；角色、切换、写集（A） | 只有 Owner 的应用入口可修改 |
| 只是技术引用 | BindingRef、SessionRef、StepRef、token、serial、fence、epoch | 值类型；使用前向签发者复核 |
| Stop 时必须等待排空或移交（SC-01） | A 的已受理请求、未决角色资格、已发未确认的存储前缀；O 的已领取未返回的完成；Scheduler 的已投递/运行中任务；R 向 Owner 的在途投递；R 的在途设备命令（可在有驻留接收者时保持隔离，L3 先于 L4） | T 的在途发现写入（`pendingDiscoveries_`）、激活进度按 SC-01"其他用例由其 Owner"同样需要收据，具体形态见 AF1-07 |
| 可通过重新发现或恢复重建 | 当前绑定（T1/T2）、账户与角色投影（重载）、会话（R 重新观察）、资源状态（Checking 后凭证据恢复） | 不可重建：进程内请求身份、旧 epoch 的租约；这些不跨重启恢复（A §12.2） |

对象销毁与悬空引用：`SimModule` 成员声明顺序即创建顺序（I/C 出口 → R → T → A → 用例 → Facade），反序销毁；`TopologyService::roles_`、`CardRuntime::resultSinks_` 是构造后绑定的借用引用，`ResourceRadioRuntime` 对 sink/dispatch 用 `weak_ptr`、对 platform 用引用。设计要求（B §11）事件桥只保留 weak Owner + epoch + 在途计数；**反序销毁本身不证明异步回调已排空**（`sim_module.h` 注释），因此销毁前必须先得到 `Drained`。

### 3.7 Card 与 Device 的边界审视

先列当前 HEAD 核实到的事实：

1. `CardRuntime` 声明实现 `RadioExecutionPort`，并持成员 `ResourceBarrier resources_`（`card/runtime/card_runtime.h:104`，在 `card_runtime.cpp:26` 以 epoch 构造）。除构造外，`resources_` 在 `card/`、`infrastructure/`、`role_assignment/`、`account/` 中没有任何使用；`CardRuntime::SwitchPrimaryRadio` 等没有定义。
2. `ResourceRadioRuntime` 持自己的 `ResourceBarrier barrier_`，在 `SwitchPrimaryRadio` 中 `barrier_.Acquire(resources_, request.step)`（`resource_radio_runtime.cpp:93`），签发传输 token（`:105`），经 `RadioPlatformPort::SubmitPrimaryRadio` 发令（`:111`）。资源键由构造方传入，Host 测试使用 `"host-primary-radio:conservative-shared"`。
3. `SimModule` 的私有构造把 `runtime_`（`CardRuntime`）作为 `RadioExecutionPort` 传给 `AssignmentService`（`sim_module_factory.cpp`），`SimModule` 不持有 `ResourceRadioRuntime`。该构造没有生产或 Host 调用者。
4. Host B/C1 测试直接组合 `AssignmentService` + `ResourceRadioRuntime` + `PrimaryHostLifecycle`（`test/cases/target/primary/b_async_primary_test.cpp`）。
5. GN 中 `ResourceBarrier`、`ResourceRadioRuntime` 与 `SessionRegistry`、`LoadSession` 同属 `runtime_mechanisms`，`CardRuntime` 属 `runtime_core`。

逐题结论：

| 问题 | 结论 | 状态 |
| --- | --- | --- |
| 1. CardRuntime 管卡应用生命周期，还是也管物理 Radio 资源？ | 设计已裁决：Card 管卡应用会话、Records、安全与卡侧通道语义；真实域 lease/fence/结束资格归 `device/resource`，Radio 协议 session/phase/member 归 `device/radio`（B §9.1；C §4.2、§4.4）。`CardRuntime` 是过渡接缝，稳态退化为有限绑定接口，其 Session/Load/Resource 成员"尚未完成能力拆分"（B §3.2）。源码中它仍兼有 Radio 端口与屏障成员，属**已登记的设计与代码不一致** | 设计 PASS；代码差距入 P4 |
| 2. ResourceRadioRuntime 是否应成为设备资源的唯一执行责任方？ | 需要拆成两句。**唯一准入者**：已裁决，每个经证实的物理共享域只有一个资源准入者（A §12.3）。**唯一执行者**：未裁决。A §12.3 要求激活、Radio 协议、eSIM 开通道查询等冲突集合统一登记到 R；B §9.1 又写"Card 可直接使用注入的窄硬件能力，不强制 CardManager→DeviceManager→RIL"。因此 `ResourceRadioRuntime` 是 Radio 命令族的执行适配，文档没有说它要承接卡通道、激活等其他命令族的执行 | 准入 PASS；执行者范围 OPEN（并入 AF1-01） |
| 3. CardApplication Session 与 Radio ProtocolSession 是否完全独立？ | 已裁决为独立（SC-02；A §10.1；B §3.2"稳态不设 device/session"）。纯 Radio 步骤不得使用 `SessionRef`。源码 A 仍合成 `SessionRef`，待实现 | 设计 PASS；代码差距 AF1-03 |
| 4. ResourceLease 由谁创建、保存、释放？ | 创建：`ResourceBarrier::Acquire`，由用例步骤（`StepRef` 为 holder）或 R 的运行请求申请。保存：提交命令的 R 执行适配（现为 `ResourceRadioRuntime::Group::lease`），A/T 不持有。释放：仅 R 凭设备证据（`Complete`、`CancelConfirmedNotSent`、`AcceptRecoveryEvidence`）；Unknown 转 `Quarantine` 保留。依据 A §12.3、B §9 | PASS（Radio 命令族）；其他命令族的保存者随 AF1-01 |
| 5. 一个真实物理域是否可能存在两套互不知情的 ResourceBarrier？ | 设计禁止（A §12.3"不能按旧锁各建一个互不相知的屏障"）。**当前源码在结构上允许**：`CardRuntime::resources_` 与 `ResourceRadioRuntime::barrier_` 是两个独立实例，互无引用。目前没有任何装配图同时启用二者（前者从未被使用，`SimModule` 也不含后者），所以今天没有运行期双重独占；但没有结构性手段阻止将来同域双屏障。深度评审 §2.2 提出过同一风险，本轮以源码复核属实 | OPEN：AF1-01 |
| 6. SimModule 如何连接 Card、Radio 和 AssignmentService？ | 现状有两张互不包含的图：`SimModule`（全量声明图，A 的 Radio 端口接到无实现的 `CardRuntime`，关闭方法仅声明）和 Host Primary 图（A 接 `ResourceRadioRuntime`，关闭由 `PrimaryHostLifecycle` 实现）。设计方向明确：H 注入 device 的协议适配作为 A 的 `RadioExecutionPort`（B §9 SC-02、C §4.4），H 汇合收据（B §11）。但权威文档没有写明 `SimModule` 是否持有 `ResourceRadioRuntime`、`PrimaryHostLifecycle` 是并入 `SimModule::StopAcceptingAndDrain` 还是其下属部件 | OPEN：AF1-02 |

没有足够文档证据支持唯一答案的两点（AF1-01、AF1-02）不在本轮自行裁决。

### 3.8 核心类图

```mermaid
classDiagram
    direction LR

    class SlotCatalog {
        <<H 只读>>
        SlotDescriptor[] slots
    }
    class SimModule {
        <<H 组合根>>
        RuntimeEpoch epoch
        StopAcceptingAndDrain()
    }

    class TopologyService {
        <<T 唯一写入口>>
    }
    class AccountRecord {
        SubscriptionId subscription
        current / historical 投影
    }
    class BindingIndex {
        位置到当前绑定
        空位置 revision
    }
    class BindingRef {
        <<值 T签发>>
        subscription, slot, bindingRevision
    }

    class AssignmentService {
        <<A 唯一写入口>>
    }
    class AssignmentAggregate {
        角色写集登记
        Primary/Data 投影
    }
    class RoleSelection {
        role, preference, effective
    }
    class AssignmentTransition {
        domain, target, phase
    }

    class SessionRegistry {
        <<R card>>
        每应用当前会话与代次
    }
    class SessionRef {
        <<值 R签发>>
        epoch, application, bindingGeneration
    }
    class LoadSession {
        <<R card>>
    }

    class RadioExecution {
        <<R device 协议适配>>
        本轮参与者与路由
        传输 token
    }
    class ResourceBarrier {
        <<R device 每物理域一个>>
        state, fence
    }
    class ResourceLease {
        <<值 R签发>>
        resources, holder StepRef, fence
    }

    class RuntimeOperationStore {
        <<O 机制 实例随 Owner>>
    }
    class StepRef {
        <<值>>
        RequestId, step, attempt
    }

    SimModule *-- TopologyService
    SimModule *-- AssignmentService
    SimModule *-- SessionRegistry : 经 CardRuntime 过渡持有
    SimModule o-- SlotCatalog
    SimModule ..> RadioExecution : OPEN AF1-02 持有关系未定

    TopologyService *-- AccountRecord : 0..*
    TopologyService *-- BindingIndex
    BindingIndex ..> BindingRef : 签发
    BindingRef --> AccountRecord : 指向订阅
    BindingIndex --> SlotCatalog : 按槽

    AssignmentService *-- AssignmentAggregate
    AssignmentService *-- RuntimeOperationStore : 自有实例
    AssignmentAggregate *-- RoleSelection : Voice/SMS/Data
    AssignmentAggregate *-- AssignmentTransition : 每 Radio 域 0..1
    RoleSelection --> AccountRecord : 0..1 订阅
    AssignmentService ..> BindingRef : 复核 不持有绑定事实

    SessionRegistry ..> SessionRef : 签发
    SessionRegistry *-- LoadSession : 0..*
    SessionRegistry ..> TopologyService : 发现与失效观察

    AssignmentService ..> RadioExecution : RadioExecutionPort
    RadioExecution ..> AssignmentService : RadioResultSink
    RadioExecution --> ResourceBarrier : 申请与释放
    ResourceBarrier ..> ResourceLease : 签发
    ResourceLease --> StepRef : holder
    RuntimeOperationStore ..> StepRef : 签发

    note for ResourceBarrier "OPEN AF1-01：同一物理域的唯一实例由哪个对象持有；卡通道等命令族如何向它申请"
    note for SessionRef "不得由 bindingRevision 合成；纯 Radio 步骤不使用（SC-02）"
```

图中 `RadioExecution` 表示 device 侧实现 `RadioExecutionPort` 的协议适配这一角色；当前实现类为 `ResourceRadioRuntime`。T 的激活、号码用例及其自有 `RuntimeOperationStore` 与 A 对称，为保持图面简洁未画出。

### 3.9 核心组件依赖图

实线为编译依赖（使用方 → 被依赖的契约），虚线为运行期回调（经窄 sink，由 H 绑定）。

```mermaid
flowchart TB
    Core["Core 宿主<br/>共享 RIL、回调、原生产对象"]
    subgraph infra["infrastructure/"]
        H["assembly（H）<br/>SimModule、SlotCatalog、epoch、关闭"]
        C["compatibility（C）<br/>原入口、原完成出口"]
        O["request（O 机制）<br/>RequestId、StepRef、OperationStore"]
        V["sim_identity.h<br/>纯值"]
    end
    T["account/（T）<br/>账户、绑定、激活进度"]
    A["role_assignment/（A）<br/>角色、Primary、切换"]
    RC["card/（R·card）<br/>会话、Records、安全、eSIM 卡侧"]
    RD["device/（R·device）<br/>Radio 执行、资源屏障"]

    Core --> H
    H --> C & T & A & RC & RD
    C --> T & A & RC
    T -->|AccountRolePlanningPort<br/>ActivationAssignmentPort| A
    A -->|PrimaryTopologyPort| T
    T -->|运行端口| RC
    A -->|RadioExecutionPort| RD
    RC -->|ProfilePreferencePort| A
    T & A & RC --> O
    O --> V
    T & A & RC & RD --> V
    RC -.->|CardObservationSink<br/>号码与激活结果| T
    RD -.->|RadioResultSink| A
    RC -.->|OPEN AF1-01：卡通道命令的资源准入| RD
    RD --> Core
    RC --> Core
```

说明：

- T 与 A 之间的双向箭头都指向对方的**窄契约**（`*_contracts`），不指向对方的服务或聚合；当前 GN 图无环（B §4.1）。
- `RadioExecutionPort` 的声明在 `role_assignment/primary/`（消费方定义出站契约），由 device 实现。
- `AssignmentService → RadioExecutionPort → device` 是设计方向与 Host 图；`SimModule` 当前把该端口接到 `CardRuntime`（AF1-02）。
- 各 Owner 自带的 Store/输出 Adapter（I）位于各自目录，图中并入所属 Owner。

## 第四部分：遗留问题

类别标记：【决策】已有设计决策；【实现】已有代码实现；【不一致】设计与代码不一致；【缺证】证据不足。

| 编号 | 问题描述 | 证据路径及符号 | 影响范围 | 状态 | 需架构负责人裁决 | 进入第二轮 |
| --- | --- | --- | --- | --- | --- | --- |
| AF1-01 | 同一物理域的唯一 `ResourceBarrier` 实例由哪个对象持有并注入；卡通道（eSIM、ARA-M）、普通激活等命令族是经 device 的执行适配发令，还是 Card 持注入的硬件能力并只向同一屏障申请租约。【缺证】+【不一致】 | 决策侧：A §12.3（一个准入者、冲突集合统一登记）；B §9.1（Card 可直接用窄硬件能力）、B §9（"session/resource 按真实生命周期注入"，未命名持有者）。代码侧：`card/runtime/card_runtime.h:104` `resources_`（未使用）；`device/resource/resource_radio_runtime.h` `barrier_` | Card/Device 边界、所有设备命令的准入接口、关闭时 R 的设备收据形态 | OPEN | 是。需给出：屏障实例的持有对象；非 Radio 命令族的执行归属。eSIM 普通/低优先级 File 与 ARA-M 是否共享物理域属既有外部缺口（B §9.2），不阻塞对象归属裁决 | P3（设备准入端口）；P4（退役 `CardRuntime::resources_`） |
| AF1-02 | `SimModule` 与 Host Primary 图的装配关系：`SimModule` 是否持有 device 的 Radio 执行适配并注入 A；`PrimaryHostLifecycle` 与 `SimModule::StopAcceptingAndDrain` 的关系（合并、下属部件或并存）。【缺证】+【不一致】 | `infrastructure/assembly/sim_module_factory.cpp`（`assignment_(…, runtime_, …)`）；`sim_module.h`（`StopAcceptingAndDrain` 仅声明，返回枚举与 `PrimaryHostLifecycle::DrainResult` 同形）；`primary_host_lifecycle.h`；B §11（把收据契约写在 `PrimaryHostLifecycle` 名下，同时称 assembly 拥有单套生产对象） | H 的对象持有图、关闭入口的唯一性、A 的 Radio 端口接线 | OPEN | 是。倾向性信息：设计方向（B §9、C §4.4）已指向"H 注入 device 适配"，缺的是对装配对象关系的明文 | P3（H 装配与关闭契约） |
| AF1-03 | A 以 `bindingRevision` 和 `"USIM"` 合成 `SessionRef`，成员取自 `catalog_.Slots()`；`RecordSent` 只有卡会话形式。【决策】已定（SC-02），【不一致】 | `role_assignment/assignment_service.cpp:199-200、239-240、392-393、600`（合成会话）、`:253`（成员）；`infrastructure/request/runtime_operation_store.h` `RecordSent` | Primary B/C1 的发送登记与成员集合 | 设计 PASS；代码 OPEN | 否（B §8、§9 已写明待实现契约） | P3（Radio 专用发送登记、参与者规划结果的接口）；P4 |
| AF1-04 | 业务冲突域 `RadioDomainId`（来自 `SlotDescriptor`，A 用于同域至多一个切换）与物理资源域 `ResourceKey`（R 用于租约）之间的对应规则未在已读章节给出；Host 中二者各自取值。【缺证】 | `infrastructure/sim_identity.h` `RadioDomainId`；`device/resource/resource_barrier.h` `ResourceKey`；A §12.3"A 决定分配策略…R 统一管理租约，三者不是重复 arbiter" | A 的准入范围与 R 的独占范围是否一致覆盖；多栈产品 | OPEN | 待定：先在 P3 定向查 A §12.4、§17.1 及 D；若仍无规则再提请裁决 | P3 |
| AF1-05 | 两个偏好入口直写 Store，绕过 `AssignmentAggregate` 写集；`AcceptLocalProfilePreference` 未使用来源 `StepRef`。【决策】已定（SC-03），【不一致】 | `role_assignment/assignment_service.cpp:659、662-664`；B §7 `#sc03-admission` | 偏好键的同键准入 | 设计 PASS；代码 OPEN | 否 | P4 |
| AF1-06 | 本地协议轮次、原生协议会话、传输 token、fence 在源码中均为裸整数，无独立类型；本地轮次由 A 的 `nextProtocolSession_` 产生。【实现】 | `role_assignment/primary/radio_execution_port.h` `RadioSwitchRequest::protocolSession`、`RadioDeviceResult::transportToken`；`assignment_service.h` `nextProtocolSession_`；`resource_barrier.h` `ResourceLease::fence` | 身份误用的编译期防护；职责本身已清楚（§3.5） | OPEN（设计细化） | 否 | P3（决定是否引入强类型及其定义位置） |
| AF1-07 | SC-01 规定"其他用例由其 Owner"出具关闭收据，但 B §11 的收据契约只展开了 A + Scheduler + R(Radio)。T 的在途发现写入（`pendingDiscoveries_`）、激活进度、号码字段预留，以及 R·card 的在途投递，其收据形态未写明。【决策】原则已定，【缺证】契约细节 | A §14.3 `#sc01-close` 表首行；B §11 `#sc01-receipts`；`account/topology_service.h` `pendingDiscoveries_`、`ActivationProgress` | 完整模块关闭（不止 Primary 切片） | OPEN（设计细化） | 否（按 SC-01 原则展开即可；若展开时出现分歧再提请） | P3 |
| AF1-08 | `SessionRegistry`、`LoadSession` 的实例由过渡对象 `CardRuntime` 持有；稳态由 `card/state`、`card/records` 的哪个能力组件持有、如何注入，文档只给方向。【决策】方向已定，【不一致】过渡 | B §3.2 末段、§9、§9.1；`card/runtime/card_runtime.h` | R·card 内部对象持有图 | OPEN（设计细化） | 否 | P3 |
| AF1-09 | `ScheduledRetrySink::AcceptScheduledC1Retry` 这一业务命名的回调声明位于通用请求机制头。深度评审建议收回 `role_assignment` 或声明仅用于 C1；该建议不是决策。【实现】 | `infrastructure/request/runtime_operation_store.h`；深度评审 §2.3 | O 机制的通用性边界 | OPEN | 否（P3 接口落点问题） | P3 |
| AF1-10 | eUICC/Port/Profile 的多绑定基数、动态端点来源身份。【缺证】，既有外部 OPEN | A §3.1 图注、§3.2、§10.4；TGT-OPEN-006 | G4 新能力 | BLOCKED（外部产品证据） | 否（非本轮可裁决；沿用既有 OPEN） | 否 |
| AF1-11 | 提示词所列 `SimModulePublication` 无此类型；对应物为 `ManagerPublicationSequence` + `SimServicePublicationPort`。【实现】 | `infrastructure/assembly/manager_publication.h`、`host_ports.h` | 仅术语 | PASS（已澄清） | 否 | 否 |

## 第五部分：第二轮交接

### 5.1 本轮验收自评

| # | 验收标准 | 结果 |
| --- | --- | --- |
| 1 | Git 基线和工作区状态已记录 | 满足（§1.1、§1.2） |
| 2 | 权威文档及已有架构决策已核对 | 满足（§2.1、§2.2） |
| 3 | SC-01～03 没有被无依据推翻 | 满足（§2.3） |
| 4 | 核心业务对象和技术对象已分类 | 满足（§3.1、§3.2） |
| 5 | 各对象之间的身份关系清晰 | 满足（§3.5） |
| 6 | 核心对象生命周期已明确 | 满足（§3.6）；T 侧关闭收据细节见 AF1-07 |
| 7 | T/A/O/R/Device/C/I/H 职责已形成矩阵 | 满足（§1.5、§3.3） |
| 8 | 每项核心业务事实都有明确的唯一 Owner | 责任方层面满足；设备租约的责任方为 R·device，其具体持有对象未定（AF1-01） |
| 9 | Card 与 Device 的资源所有权已裁决，或明确记录 OPEN 及所需证据 | 以"记录 OPEN"方式满足（§3.7，AF1-01、AF1-02） |
| 10 | 核心 UML 类图和组件依赖图已形成 | 满足（§3.8、§3.9） |
| 11 | 第二轮工作已明确 | 满足（§5.3～5.5） |
| 12 | 成果已持久化到检查点 | 满足（本文件；未暂存、未提交、未推送） |

标记为 `AF1_ROUND1_PARTIAL` 而非 `AF1_ROUND1_P0_P2_COMPLETE` 的原因：任务规定"存在未裁决事项应标记 PARTIAL"，而 AF1-01、AF1-02 需要架构负责人裁决。P0～P2 的分析步骤本身没有遗漏。两项裁决给出后，对象模型基线即可视为稳定。

本轮验证范围：只做了文档与源码的只读核对，没有运行任何构建、测试或架构检查脚本；T1/T2 的 PASS 数字沿用既有 Host 报告，未重跑。

### 5.2 已完成与未解决

- 已完成：P0 基线与保护；P1 文档与决策索引、SC-01～03 核对；P2 对象分类、三张表、所有权矩阵、生命周期、类图、依赖图、Card/Device 六问。
- 未解决：AF1-01、AF1-02（待裁决）；AF1-04（P3 先补证）；AF1-03、05～09（设计细化或代码差距，按列分入 P3/P4）。

### 5.3 P3 需要重点设计的接口

1. **设备准入端口**（依赖 AF1-01 裁决）：命令族向唯一屏障申请/登记发送/提交结束证据的窄接口；租约保存者；`RadioDomainId` 与 `ResourceKey` 的对应（AF1-04）。
2. **Radio 参与者规划与专用发送登记**（SC-02 落地）：`RadioExecutionPort` 的只读规划结果（成员、路由、资源集、观察版本）；不依赖 `SessionRef` 的 `RecordSent` 形式；本地轮次/原生会话/传输 token 的类型（AF1-06）。
3. **H 装配与关闭契约**（依赖 AF1-02 裁决）：`SimModule` 持有图；A、T、R·card、R·device、Scheduler 各自的关闭收据类型（SC-01、AF1-07）。
4. **A 的同键准入入口**（SC-03 落地）：`AcceptEntryPrimaryPreference`、`AcceptLocalProfilePreference` 与写集的契约。
5. **R·card 稳态持有图**：`SessionRegistry`、`LoadSession` 的持有与注入（AF1-08）。
6. `ScheduledRetrySink` 的落点（AF1-09）。

### 5.4 P4 需要核对的源码

- `role_assignment/assignment_service.cpp`：`StartPrimarySwitch`、`AcceptRadioStep`、`AcceptC1RetryDue`/重试路径、`HasPendingDeviceEffects`、`CompleteOnce`、两个偏好入口。
- `device/resource/resource_radio_runtime.cpp`：`SwitchPrimaryRadio`、`AcceptDeviceResult`、`Recover`、`OwnerCallbacksDrained`、`DeviceResourcesDrained`。
- `device/resource/resource_barrier.cpp`：全部状态转移。
- `card/state/session_registry.cpp`、`card/records/load_session.cpp`。
- `account/topology_service.cpp`：`DiscoverAccount`、`ReconcileDiscovery`、`AcceptCardSessionInvalidation` 中与关闭收据相关的 pending 状态（不重审 T1/T2 业务闭环）。
- `infrastructure/assembly/primary_host_lifecycle.cpp`、`sim_module_factory.cpp`。
- `services/multisim/BUILD.gn` 中 `runtime_mechanisms`、`runtime_core`、`assembly_core` 的源与依赖。

### 5.5 下一轮需要读取的精确位置

| 目的 | 文件与位置 |
| --- | --- |
| AF1-04 资源键来源 | A（目标架构）§12.4、§12.5、§17.1；`infrastructure/assembly/slot_catalog.cpp` |
| 回调准入与关联契约 | A §11.1～11.3；`infrastructure/request/runtime_operation_store.cpp` |
| 提交协议与部分效果 | A §9.1～9.2；B §16 |
| 字段台账（SC-03 落地） | D `#sc03-ledger`（§5.2.1）、`#sc-acceptance`（§7.4～7.5） |
| 装配目录约束 | C §5.3 `#sc01-assembly-dir`、§5.4、§7.1 |
| Primary 时序 | B §15；`role_assignment/primary/primary_switch_plan.cpp` |
| A/T 协议头 | `role_assignment/account_role_protocol.h`、`assignment_requests.h`、`account/topology_requests.h`、`account/account_store.h` |
| Host 关闭证据 | `services/multisim/test/docs/Host_Lifecycle_L3_Report_20261120.md` |

### 5.6 下一轮不需要重复读取的内容

- 签收收敛记录全文、A §3、§4.1～4.2、§10、§12.3、§14.2～14.3；B §2、§3.2、§4.1～4.2、§5～11；C §4.2～4.4。其要点已进入本文件 §2、§3。
- 本文件 §1.5 与 §3.2 涉及的全部头文件（`sim_identity.h`、`request_ref.h`、`request_context.h`、`runtime_operation_store.h`、`card_runtime.*`、`session_registry.h`、`load_session.h`、`resource_barrier.h`、`resource_radio_runtime.h`、`radio_execution_port.h`、`assignment_aggregate.h`、`assignment_service.h`、`binding_index.h`、`account_registry.h`、`topology_service.h`、`sim_module*.h/.cpp`、`host_ports.h`、`slot_catalog.h`、`manager_publication.h`、`primary_host_lifecycle.*`）。若 HEAD 变化，只需对这些文件做 `git diff 632f6a2a..HEAD -- <path>`。
- T1/T2 的业务闭环与测试报告；SC-01～03 的决策过程；目录方案的历史评审。

### 5.7 下一轮开始前的检查

1. `git rev-parse HEAD` 与 `git ls-remote origin refs/heads/main`，对比 `632f6a2a`；有增量先看 `git log 632f6a2a..HEAD --stat`。
2. 确认本文件仍存在且未被改动（本轮未提交，文件为未跟踪状态）。
3. 取得 AF1-01、AF1-02 的裁决；未取得时 P3 的第 1、3 项按"待裁决的备选"书写，不自行定案。

**第一轮到此停止，未进入 P3～P6。**

---

## 第六部分：第二轮执行状态（P3～P4）

> 以上第一至第五部分是第一轮（P0～P2）的固定记录，其中的工作区状态和基线描述保持第一轮实测原貌，不随第二轮改写。

### 6.1 第二轮基线（2026-11-23 实测）

| 项 | 实测值 |
| --- | --- |
| 本地 HEAD = `origin/main`（`git ls-remote`） | `6df6973c73c05c1bd4c923733f1ccbb87e209ada` |
| `632f6a2a..HEAD` 增量 | 仅 1 个提交、1 个文件：本检查点入库（546 行）。`services/` 无变化，第一轮固定的源码事实直接复用 |
| 暂存区 | 空 |
| 未跟踪文件（用户文件，不触碰） | `default.profraw`、`opencode.json` |
| 本轮允许的修改 | 只更新本文件；不 add/commit/push |

### 6.2 阶段状态

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| P3-A | AF1-01、AF1-02 方案与接口收敛 | 完成；两项推荐方案 `PROPOSED / AWAITING_ARCHITECT_SIGNOFF`（第七部分） |
| P3-B | 核心契约、身份映射、三张时序图、P2 图示勘误 | 完成（第八部分） |
| P4-A | 源码一致性审查 | 完成（8.1 矩阵最后一列、第九部分） |
| P4-B | AF-2 最小开发清单与 P5/P6 交接 | 完成（第十部分） |

第二轮结论标记：`AF1_ROUND2_P3_P4_COMPLETE`（自评见 10.4）。AF1-01、AF1-02 的推荐方案及 8.2 的接口建议仍为 `PROPOSED`，待架构负责人签收；AF1-13 待产品证据。

## 第七部分：P3-A 两个核心 OPEN 问题的方案收敛

状态约定：本部分的推荐方案均为 `PROPOSED / AWAITING_ARCHITECT_SIGNOFF`。凡权威设计已能直接确定的原则只引用，不重新比选。

### 7.1 AF1-01 统一设备资源准入

#### 7.1.1 已由权威设计确定、无需比选的原则

| 原则 | 依据 |
| --- | --- |
| 每个经证实的物理共享域只有一个准入者，租约由 R 统一管理 | A §12.3 |
| 一次命令在同一短准入域按稳定顺序一次预留完整资源集，锁外发令，不逐把持锁等待 | A §12.3 状态表 Available 行 |
| Card 可直接使用注入的窄硬件能力，不强制 Card→Device→RIL 的转发链 | B §9.1 |
| A 的角色资格、T 的激活进度都不是设备租约 | A §4.2 协作规则 3 |
| 资源键来源不齐时用已知保守共享域，不按逻辑槽数推并发 | A §12.3 |

#### 7.1.2 本轮补充核实的源码事实

- `ResourceBarrier` **自身没有任何同步**；头文件注释要求"调用方须在同一 R 短提交域中执行"。`ResourceRadioRuntime` 用自己的 `lock_` 保护它。因此把一个屏障引用同时交给两个各持自己锁的执行适配，`Acquire` 的"检查全部键→预留"就不再原子。这正是"共享引用不等于解决并发"的具体原因。
- `ResourceBarrier::Acquire` 已实现：键排序、去重、全有或全无、不等待、holder 必须带非 `Unspecified` 的 `RequestOwner` 且 epoch 匹配（`resource_barrier.cpp`）。
- `ResourceRadioRuntime::SwitchPrimaryRadio` 在 `lock_` 内取租约和登记传输 token，**释放 `lock_` 后**才调用 `platform_.SubmitPrimaryRadio`；平台在提交过程中同步回调的结果由 `submittingTransmissions_` / `pendingSubmissionResults_` 缓冲后再接纳。向 A 的投递经 `RadioOwnerDispatchPort::PostRadioOwnerTask`，不在 `lock_` 内直接回调 sink。
- `ResourceRadioRuntime` 的资源集是构造时固定的 `resources_`，与 `RadioSwitchRequest::domain` 无关；Host 夹具中槽 0/1 的 `RadioDomainId` 分别为 `m0`/`m1`，而资源键只有一个 `host-primary-radio:conservative-shared`。

#### 7.1.3 候选方案

| | 方案 A（推荐）：屏障自带短提交域，H 建唯一实例，各命令族执行适配借用 | 方案 B：把 `ResourceRadioRuntime` 泛化为唯一设备执行者 |
| --- | --- | --- |
| 唯一实例的持有 | `ResourceBarrier` 本身就是 R·device 的准入组件（`device/resource`）。物理上作为 `SimModule` 的一个成员，声明在所有执行适配之前；事实 Owner 仍是 R·device，H 只管寿命 | 屏障留在泛化后的执行者内部 |
| 短提交域 | 屏障内置一把只保护自身 map 的叶子锁；锁内不调用任何外部代码 | 执行者的锁 |
| 命令族如何准入 | Radio（`ResourceRadioRuntime`）、卡侧通道/激活（Card 的执行适配）各自在发令前对**同一实例**调用一次 `Acquire(完整键集, StepRef)` | 所有命令族都经执行者发令 |
| Card 是否直连硬件 | 是，沿用 B §9.1；前提是先取得租约 | 否，全部改道 |
| 租约保存与释放 | 提交该命令的执行适配保存（Radio 为 `Group::lease`，Card 为其请求关联）；只凭设备证据经屏障释放 | 执行者统一保存 |
| 改动范围 | 屏障加锁；`ResourceRadioRuntime` 构造改为接收 `ResourceBarrier&`；删除 `CardRuntime::resources_` | 新建通用设备执行接口，卡通道、APDU、激活的协议语义都要穿过 device |

不推荐方案 B 的原因：与 B §9.1"不强制 Card→Device→RIL"直接冲突；卡侧通道的 AID/APDU/逻辑句柄语义属于 Card（B §9.2），穿过 device 会让 `ResourceRadioRuntime` 长成设备总管；迁移面大而对"唯一准入"没有额外收益。

#### 7.1.4 推荐方案对八个问题的回答

| # | 问题 | 回答 |
| --- | --- | --- |
| 1 | 唯一实例由谁持有 | R·device 的 `ResourceBarrier` 自身（一个模块实例内唯一），物理寿命随 `SimModule` |
| 2 | H 如何创建和注入 | H 以 `RuntimeEpoch` 构造屏障；按宿主提供的**已验证资源路由**逐键 `RegisterForInitialCheck`；把 `ResourceBarrier&` 传给各执行适配的构造函数。每个键凭 `AcceptInitialRecoveryEvidence` 才离开 Checking |
| 3 | 多命令族如何经同一物理域准入 | 各适配按自己命令实际触及的物理域给出键集，对同一屏障 `Acquire`。键集相交则后到者得不到租约，在任何平台调用之前返回 |
| 4 | Card 是否仍可直连窄硬件端口 | 可以。约束为：`Acquire` → `MarkSent` → 锁外 I/O → `Complete` / `Quarantine`，与 `ResourceRadioRuntime` 现有顺序相同 |
| 5 | 谁保存和释放租约 | 提交命令的执行适配保存；释放只经屏障的 `Complete`、`CancelConfirmedNotSent`、`AcceptRecoveryEvidence`。A/T 不持有、不释放 |
| 6 | 多命令族如何原子申请 | 一条命令只调用一次 `Acquire`，传完整键集；屏障在自己的锁内完成"全部可用才全部预留"。禁止为同一命令分两次申请 |
| 7 | 持锁跨 I/O、回调重入、锁序 | 规定锁序：Owner 提交域（A/T）→ 执行适配锁 → 屏障锁。屏障锁是叶子，不外调；适配锁不跨平台 I/O、不直接回调 sink。R·device 现状符合；**A 现状不符合**（见 9.3 的 AF1-12） |
| 8 | 不同物理域如何并行 | 屏障按键记录状态，`Acquire` 只检查所请求的键，锁只覆盖 map 操作。要真正并行，Radio 的键集必须按本轮路由得出，而不是固定全集（见 8.2） |

未裁决而保留 OPEN 的部分：普通/低优先级 EsimFile 与 ARA-M 是否同一物理域、EF/APDU 与 Radio 切换是否共享域、厂商 RIL 旁路。这些是产品/设备证据（B §9.2、A §12.3 末段）。缺证时按保守共享键处理，不承诺并发。冲突时对调用者是等待还是按原失败出口返回，属方法级行为，仍为 PROPOSED（A §12.3）。

#### 7.1.5 接口与持有关系的修改

```cpp
// device/resource/resource_barrier.h：签名不变，新增内部同步（生产实现须用 FFRT 原语）
class ResourceBarrier final { /* 现有公开方法全部沿用 */ };

// device/resource/resource_radio_runtime.h
ResourceRadioRuntime(ResourceBarrier &barrier, RadioPlatformPort &platform /*, 路由输入见 8.2 */);
// 删除成员 ResourceBarrier barrier_；改为 ResourceBarrier &barrier_

// card/runtime/card_runtime.h
// 删除成员 ResourceBarrier resources_；需要设备准入的卡侧命令族经构造注入的 ResourceBarrier& 申请
```

- GN 影响：`resource_barrier.*`、`resource_radio_runtime.*` 目前与 `session_registry.*`、`load_session.*` 同在 `runtime_mechanisms`。功能上不必拆 target 即可实现；若要用结构检查保证"只有 assembly 构造屏障"，建议把这两组源拆到独立的 device 逻辑 target，并更新 `allowed_edge`。
- 生命周期影响：屏障先于所有执行适配构造、晚于它们析构。模块级"设备已排空"判据变为：所有已注册键为 Available，且每个适配无在途组。

#### 7.1.6 验证用例

- 正常：Radio B 切换使用键 K1，同时一个卡侧通道命令使用键 K2。两者都取得租约并各自完成，互不阻塞。
- 反向：把两者映射到同一键 K。先到者 InFlight 期间，后到者 `Acquire` 失败，平台提交次数为 0；先到者结果 Unknown 时键进入 Quarantined，后到者仍不得发令，直到恢复证据被接纳。另加一条结构断言：除 assembly 外没有任何位置构造 `ResourceBarrier`。

需要架构负责人签收：是。签收点为"屏障自带短提交域 + H 持寿命 + 适配借用"以及锁序规定。

### 7.2 AF1-02 统一 SimModule 装配和生命周期

#### 7.2.1 已确定、无需比选的原则

| 原则 | 依据 |
| --- | --- |
| assembly 创建并拥有单套对象；构造次序 I/C 出口→R→T→A→用例→Facade；未完成必要绑定不得发布 | B §11 |
| H 注入 device 的协议适配作为 A 的 `RadioExecutionPort` | B §9（SC-02）、C §4.4 |
| H 在同一已封口实例汇合 A、O/完成侧、Scheduler、R 的收据；不替任何一方消账 | A §14.3、B §11（SC-01） |
| 没有已验证的接手者时原图留存 | A §14.3、B §11 |
| 产品工厂（`CreateOwnedSimServices`）与迁移附着是两件事；不为 Host 验证提前做 G2b | B §11 |

#### 7.2.2 本轮补充核实的源码事实

- `AssignmentService` 继承 `enable_shared_from_this`，重试调度用 `weak_from_this()`；`PrimaryHostLifecycle` 与 `ResourceRadioRuntime::BindResultSink` 都要求 `shared_ptr<AssignmentService>`。`SimModule` 目前按值持有 `AssignmentService`，三者接不上。
- `SimModule` 构造依赖 `ExistingSimBindings`，其中是 `SimRdbHelper&`、`MultiSimMonitor&`、`RadioProtocolController&` 等 Legacy 生产对象引用；`CardRuntime` 构造同样要求 `RadioProtocolController&`、`ITelRilManager&`。Host 无法在不引入 Legacy 对象的前提下构造。
- `CardRuntime` 覆写了四个端口的纯虚方法，但多数没有定义。按 C++ 规则推断，凡构造 `CardRuntime` 的可执行目标都会在链接期缺少虚表符号。这一点是推断，本轮没有构建验证。
- `SetShowNumberUseCase`、`ActivateSimUseCase` 只有构造函数，流程方法未定义；A 的激活相关入口以 `throw std::logic_error` 显式拒绝。
- `PrimaryHostLifecycle::StopAcceptingAndDrain` 已实现：封口先于 Owner 任务、单一清理者、重复调用读同一 `State`、用弱引用持 A 与 dispatch。

#### 7.2.3 候选方案

| | 方案 A（推荐）：`SimModule` 是唯一组合根与关闭入口，`PrimaryHostLifecycle` 作为其成员部件 | 方案 B：每个专项各有自己的 Lifecycle，由宿主逐个调用 |
| --- | --- | --- |
| 关闭入口 | 只有 `SimModule::StopAcceptingAndDrain` | 多个并列入口 |
| 同一轮封口与收据 | 一个实例、一轮、一个清理者 | 各专项各自一轮，宿主自行拼结果 |
| 复用现有实现 | `PrimaryHostLifecycle` 原样复用，负责 A + Scheduler + Radio 这一段 | 同样复用 |
| 新增工作 | `SimModule` 在其外层补 T、用例、Card、完成出口的收据 | 每加一个专项就多一个 Manager 式对象 |

不推荐方案 B 的原因：SC-01 要求 H 用"同一实例、同一轮封口"的收据判定，并列入口做不到；也违反"不成为并列的模块 Manager"。

#### 7.2.4 最小可运行 Host 对象持有图（推荐）

`SimModule` 的核心图只依赖端口；Legacy 具体对象只出现在产品附着工厂里。

| 创建次序 | 对象 | 持有方式 | 说明 |
| --- | --- | --- | --- |
| 0 | `SimHostPorts`、`SlotCatalog`、`RuntimeEpoch` | 值 / `shared_ptr<const>` | 宿主提供；见下方对 `SimHostPorts` 的窄扩展 |
| 1 | I 侧存储与输出端口（`AccountStore`、`AssignmentStore`、`AssignmentOutputPort`、旧输出） | 由工厂按变体创建后交 `SimModule` 持有 | Host 变体注入 fake/Provider bridge；产品附着变体由 `ExistingSimBindings` 构造各 Adapter |
| 2 | `LegacyCompletion` | 成员 | C 的完成出口 |
| 3 | `ResourceBarrier` | 成员 | 7.1 的唯一实例；此处注册键 |
| 4 | `ResourceRadioRuntime`（Radio 执行适配） | 成员 | 借用屏障与 `RadioPlatformPort` |
| 5 | `CardRuntime` | 成员 | 不再实现 `RadioExecutionPort`；Legacy 借用改为可选输入；未实现的方法给出显式拒绝的定义 |
| 6 | `TopologyService` | 成员 | |
| 7 | `AssignmentService` | `shared_ptr` 成员 | `radio` 参数接第 4 项 |
| 8 | 号码、号码查询、激活用例，`LocalCardRequests` | 成员 | 流程未实现的保持显式拒绝 |
| 9 | `PrimaryHostLifecycle` | 成员 | 入参为第 7、4 项及宿主的调度器与 owner dispatch |
| 10 | `LegacySimFacade` | 成员 | 最后构造，最先析构 |

构造后的绑定顺序（全部成功才允许 `LegacyEntry()` 对外可用）：

1. `topology.BindRolePlanner(*assignment)`
2. `card.BindResultSinks(numbers, activation, topology)`（去掉 Radio sink 参数）
3. `radio.BindResultSink(assignment)`；`radio.BindOwnerDispatch(host.ownerDispatch)`
4. `assignment->BindRetryScheduler(host.retryScheduler)`
5. 各资源键接纳初始恢复证据；未获证据的键保持 Checking

`SimHostPorts` 的窄扩展（宿主拥有队列与线程，头文件注释已写明"H owns queue/thread/cancel/drain"）：

```cpp
struct SimHostPorts {
    /* 现有四项沿用 */
    std::shared_ptr<RetrySchedulerPort> retryScheduler;
    std::shared_ptr<RadioOwnerDispatchPort> ownerDispatch;
    RadioPlatformPort &radioPlatform;
    // 已验证的资源路由输入，形态见 8.2
};
```

避免双向强引用与悬空回调：A 由 `SimModule` 强持有；`ResourceRadioRuntime`、调度器对 A 只持 `weak_ptr`；A 对 Radio 执行适配持引用，后者先构造后析构。`TopologyService::roles_` 与 `CardRuntime::resultSinks_` 是借用引用，其安全性依赖"得到 Drained 之后才销毁"。

#### 7.2.5 关闭：封口、排空、销毁

`SimModule::StopAcceptingAndDrain()` 的步骤：

1. **封口**：Facade 入口关门并登记在途调用；T、用例、Card 停止受理新请求；调用 `PrimaryHostLifecycle::StopAcceptingAndDrain()`，由它完成 A、调度器、Radio 的封口与 Owner 封口任务。
2. **排空判定**（可重复调用，读同一轮状态）：
   - 在途执行：Facade 在途调用、调度器已投递/运行中任务、R 向 Owner 的投递、已领取但回调尚未返回的完成。
   - 业务责任：A、T、各用例、Card 的关闭收据（形态见 8.1 的 M19）。
   - 设备：屏障所有键 Available，各执行适配无在途组。
3. **结果**：`SimModule::DrainResult` 现有三个值不足以表达"业务待完成/核对"。B §11 已允许用内部结果区分三类原因且不动公开 ABI。建议在这个私有枚举上增加 `BusinessEffectsUnresolved`。判定次序为：在途执行 → 业务责任 → 设备；全部通过才是 `Drained`。
4. **销毁**：只有 `Drained` 后才允许 Deleter 按成员反序析构。未 `Drained` 时保留原图，不 Retire、不清 pending、不解绑接收者。

T 侧与其他用例的关闭收据来源（均为现有状态，不新增业务状态）：

| 提供者 | 计入"未完成"的现有状态 |
| --- | --- |
| T 发现/恢复 | `pendingDiscoveries_` 中的条目（存储 Unknown，或已写但观察失效）；`DiscoveryObservationLedger` 中未结束的 store ticket |
| T 重载 | 已 `BeginReload` 尚未 `AcceptReload` 的尝试（`issuedLoads_` 与 `consumedLoads_` 之差） |
| T 号码、激活 | 各用例 `RuntimeOperationStore` 中未终态或未交付的请求；`ActivationProgress` 中进行中的条目；号码字段预留 |
| A | 见 9.2 的 SC-01 缺口：除 `pendingPrimary_` 外，还要计入仍被持有的角色写集、非终态的 `AssignmentTransition`、未交付的完成 |
| Card | 未结束的 `ExpectedTransition`、`msisdnAssociations_` |

可移交与必须保留原图：

| 情形 | 处理 |
| --- | --- |
| 已终态、完成已交付且回调已返回的请求 | 可回收，不需要移交 |
| 存储或参数效果 Unknown、原调用未交付、角色资格未释放 | 仅当常驻接手者确认接到原请求、完成路由、效果清单、资格和可达核对入口时可移交。当前代码与设计都没有已验证的接手机制，**默认保留原图** |
| 设备键 Quarantined | 需要仍能收证且新图无法绕过隔离的驻留方；否则保留原图（L3 与 L4 分开，A §14.3） |
| 接手者拒绝或不存在 | 保留原图，返回非 `Drained` |

#### 7.2.6 工厂范围

| 工厂 | AF-2 范围 | 说明 |
| --- | --- | --- |
| Host 装配（testonly） | 做 | 以端口和 fake 构造上表的对象图；返回 `SimModuleHandle` 或带阶段的失败 |
| `AttachSimModuleToExisting`（同 SO 迁移附着） | 不做 | 仍只声明；届时由 `ExistingSimBindings` 构造 Adapter 后复用同一核心构造 |
| `CreateOwnedSimServices`（G2b 产品工厂） | 不做 | 属于产品 SO 工程边界，不在 AF-2 |

GN 影响：`assembly_core` 需能见到 device 执行适配与调度端口（当前经 `runtime_core → runtime_mechanisms` 已可达）。全部保持 `testonly`，不触及根产品 GN。

#### 7.2.7 验证用例

- 正常：经 Host 工厂创建 `SimModule`，从 `LegacyEntry()` 发起一次 C0 和一次 B；完成后调用 `StopAcceptingAndDrain()` 得到 `Drained`，随后销毁无悬空回调。
- 反向：B 已发令、结果未回时调用 Stop，得到非 `Drained`；再次调用结果相同且平台提交次数不增加；此时不得销毁。另一条：缺少任一必要绑定或资源键未获初始证据时，工厂返回带阶段的失败或保持入口不可用，不发布半成品。

需要架构负责人签收：是。签收点为"`SimModule` 核心图端口化、A 改为共享持有、`PrimaryHostLifecycle` 成为成员部件、私有 `DrainResult` 增加业务未决值"。

## 第八部分：P3-B 核心接口、身份映射与数据流

### 8.1 设计—接口—源码追踪矩阵

这是唯一的追踪矩阵。前七列是 P3 的契约结论，最后一列由 P4-A 填写。"沿用"表示现有声明已满足契约，不改签名。源码路径均相对 `services/multisim/`。

| ID | 接口（提供方 → 消费方） | 输入 / 输出 | 事实 Owner；身份及资格 | 同步/异步与执行域 | 失败 / Unknown | P3 结论 | P4 实现状态与证据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| M01 | `CardObservationSink`（T 实现 → R·card 调用）`account/discovery/card_observation_port.h` | 入：`CardDiscoveryObservation`（`SessionRef`、观察序号、ICCID 等）、`CardSessionInvalidation`（原 `SessionRef`、序号）。无返回值 | 观察序列归 R·card；绑定接纳归 T。资格是 R 签发的 `SessionRef` + 每应用递增的观察序号 | R 在自己的状态写点同步调用。失效必须是短调用；发现包含存储 I/O，应转入 T 的执行域，不占 R 的线程 | 存储 Unknown 或已写但观察失效：T 留 `pendingDiscoveries_`，等 `ReconcileDiscovery` | 沿用签名；补执行域约束 | T 侧 `IMPLEMENTED` + `HOST_EXECUTED`（T1/T2 报告，未重跑）。R 侧 `PublishAcceptedCardDiscovery`、`InvalidateVerifiedCardSession` `IMPLEMENTED`，但 `CardRuntime` 无任何 Host 用例构造它，R→T 的真实接线未执行。`AcceptCardDiscoveryObservation` 在调用者线程同步完成含存储 I/O 的 `DiscoverAccount`，与执行域约束不符 |
| M02 | `SessionRef` / `BindingRef`（值，`infrastructure/sim_identity.h`） | — | `SessionRef` 只由 R·card 签发；`BindingRef` 只由 T 签发 | 值传递；使用前向签发者复核 | 复核不过即拒绝，不补造 | 沿用；禁止用 `bindingRevision` 构造 `SessionRef` | 类型 `IMPLEMENTED`。`SessionRef` 被 A 以 `bindingRevision` + `"USIM"` 合成（`assignment_service.cpp:199-200、239-240、392-393、600`），违反签发规则（AF1-03） |
| M03 | `AccountRolePlanningPort`（A 实现 → T 调用）`account/account_role_planning_port.h` | 入：规划输入、已规划片段、存储证据、重载片段集。出：`AccountRoleFragment`（带预留）、布尔接纳结果、投影版本 | 角色片段与预留归 A；账户行归 T。资格是片段内的预留 | 同步短调用。T 调用时不持自己的提交锁；A 在自己的锁内完成 | 调用结束不等于可释放：Unknown 或已写但失效时预留保留到 `Finalize` 证据成立 | 沿用 | `IMPLEMENTED` + `HOST_EXECUTED`（T1/T2 报告）。A 的七个方法均在 `primaryCommitLock_` 内委托 `AccountRoleProtocol`；T 的调用点均在 `commitLock_` 作用域之外 |
| M04 | `PrimaryTopologyPort`（T 实现 → A 调用）`role_assignment/primary/primary_topology_port.h` | `ReadPrimaryTarget(slot)` → `PrimaryTargetFacts{BindingRef, iccid, active}`；`BeginRouteMapping(parent, target)` → 子 `RequestId` | 绑定与账户事实归 T。A 只拿带版本的只读事实，用前后两次读取复核 | 同步只读。锁序为 A → T；T 不得在持锁时回调 A | 读不到或版本变化：A 在首次副作用前失败 | 沿用 | `ReadPrimaryTarget` `IMPLEMENTED` + `HOST_EXECUTED`（C0 拓扑集成用例）。`BeginRouteMapping` 以 `throw std::logic_error` 显式拒绝。锁序 A→T 成立，未发现 T 持锁回调 A |
| M05 | `ActivationAssignmentPort`（A 实现 → T 激活用例）`account/activation/activation_assignment_port.h` | 旧有界等待；`Admit/Release/Transfer` 激活冲突资格；入口偏好；主卡复核 | 冲突资格归 A（`ActivationConflictLease` 不是设备租约） | 同步短调用，不嵌套等待 | 效果未知时 `Transfer…ToReconciliation`，资格保留 | 沿用；偏好入口并入 M13 的准入 | 接口 `DECLARED`。A 的四个激活相关实现及 `RecheckPrimaryAfterAccountChange` 以 `throw std::logic_error` 显式拒绝；`ActivateSimUseCase` 只有构造函数 |
| M06 | `RadioExecutionPort`（R·device 实现 → A 调用）`role_assignment/primary/radio_execution_port.h` | 入：`RadioSwitchRequest{StepRef, 成员槽, domain, mode, 目标 BindingRef, 本地轮次, phase, 成员数}`。出：观察；发送无业务返回值 | 参与者、路由、传输关联、租约归 R·device；计划与轮次编号归 A | 提交在 A 的执行域发起；R 锁外交平台；结果异步经 M07 | `HandoffUnknown` 或缺 serial：R 隔离，A 转 `NeedsReconciliation`；`ConfirmedNotSent` 才撤销 | 窄扩展：增加只读的本轮规划；提交返回是否被接受（取代"上一次提交"查询）。见 8.2 | `ResourceRadioRuntime` 的实现 `IMPLEMENTED` + `HOST_EXECUTED`（B/C1 用例）。`CardRuntime` 的同名覆写仅 `DECLARED`。`PlanPrimaryRound`、带返回值的提交不存在；`WasLastSubmissionAccepted` 依赖单个 `lastSubmission_`。产品 Radio 适配 `BLOCKED`（需 O2/O3 证据） |
| M07 | `RadioResultSink`（A 实现 → R·device 调用）同上头文件 | `AcceptRadioStep(result)`、`AcceptLateRadioEvidence(result)`；`RadioSwitchResult` 带原 `StepRef`、成员、本地轮次、phase、`notify`、效果 | 步骤与角色效果的接纳归 A；设备结束与可复用的认定归 R | 异步，经 `RadioOwnerDispatchPort` 投递到 A 的执行域；R 记录投递收据 | 重复、错阶段、错成员、过期轮次不推进；迟到证据只进 `AcceptLateEvidence`，不重放完成 | 沿用 | `IMPLEMENTED` + `HOST_EXECUTED`。A 以 `operation->context.sentSession` 非空作为接纳前提，该会话是合成值 |
| M08 | `ResourceBarrier`（R·device → 各命令族执行适配）`device/resource/resource_barrier.h` | `Acquire(键集, StepRef)` → `ResourceLease`；`MarkSent`、`Complete`、`CancelConfirmedNotSent`、`Quarantine`、`AcceptRecoveryEvidence` | 物理域状态与 fence 归 R·device。资格是租约（epoch + 键集 + holder + fence） | 同步、不等待；自身短提交域；锁外发令 | 写效果 Unknown 不释放；只读命令结束且可复用可释放 | 沿用公开方法；内置同步、唯一实例注入（7.1） | 状态转移 `IMPLEMENTED`；经 `ResourceRadioRuntime` `HOST_EXECUTED`，另有纯规则用例。无内部同步；存在两个实例（`CardRuntime::resources_` 未使用）。真实命令结束与复用语义 `BLOCKED`（产品/设备） |
| M09 | `RadioPlatformPort`（平台适配实现 → R·device 调用）；回程 `ResourceRadioRuntime::AcceptDeviceResult` | `SubmitPrimaryRadio(request, token)` → `{disposition, serial?}`；回程 `RadioDeviceResult{token, result, commandEnded, resourceReusable, …}` | 传输 token 归 R·device；serial 归平台 | 提交同步返回"交接结果"；设备结果异步回程，允许在提交返回前到达 | 三态交接；无 serial 的通知按原协议关联，不填造 | 沿用 | 接口 `IMPLEMENTED`；仅 Host 测试中的 fake 平台实现，`HOST_EXECUTED`。对接原 `RadioProtocolController`/RIL 的产品适配不存在，`BLOCKED` |
| M10 | `RuntimeOperationStore` / `StepRef`（O 机制 → T、A 用例各持实例）`infrastructure/request/` | `Begin`、`AdvanceStep`、`RecordSent`、`AcceptResult`、`AcceptLateEvidence`、`ClaimCompletion`、`TryMarkTerminal`、`RetireAfterEvidenceRetention` | 记录归持有它的 Owner；O 不拥有业务事实 | 只在所属 Owner 的短提交域内调用；无内部锁 | 步骤一次消费；终态不可覆写；Unknown 的记录在有迟到证据前不回收 | 窄扩展：纯 Radio 的发送登记（8.2）；完成"已领取未返回"的计数 | `IMPLEMENTED`；经 A `HOST_EXECUTED`。T 的号码、激活用例构造了容器但没有流程使用。只有卡会话形式的 `RecordSent`。C0 成功与前置拒绝路径 `Begin` 后不回收。没有"已领取未返回"的记录 |
| M11 | `LegacyIntCompletionPort` / `CompletionPlan`（C 实现 → T、A 调用）`infrastructure/request/request_context.h`、`infrastructure/compatibility/events/` | `CompleteLegacyInt(plan, result)` | 完成出口登记与格式归 C；"是否已终态、是否已领取"归发起 Owner | Owner 封存终态并领取后调用；调用必须在 Owner 提交锁之外；真实等待在宿主适配层 | 交付失败不反转设备或存储结果 | 沿用签名；补"锁外调用 + 在途计数" | `LegacyCompletion::CompleteLegacyInt` `IMPLEMENTED`，但 Host 用例使用的是测试自带的完成 fake，`LegacyCompletion` 本身未见 Host 执行。A 在持 `primaryCommitLock_` 时调用它 |
| M12 | `AssignmentStore`（A 的 Adapter 实现 → A 调用）`role_assignment/assignment_store.h` | 写偏好、Primary 参数、Data、默认角色；读/恢复 Primary 快照。结果带 `callFinished` | 角色与参数事实归 A；Adapter 只执行 | 同步 I/O；应在 A 提交锁之外，返回后凭原资格接纳 | `callFinished=false` 表示效果未知，不得翻译为成功或失败 | 沿用 | `AssignmentStoreAdapter` `IMPLEMENTED` + `HOST_EXECUTED`（C0 Store 集成用例，经参数端口 fake）。产品参数服务未执行。A 在持锁状态调用全部写方法 |
| M13 | `ProfilePreferencePort::AcceptLocalProfilePreference` 与 `AcceptEntryPrimaryPreference`（A 实现 → Card 本地 Profile 短请求、Core 兼容入口/T 激活调用） | 入：来源 `StepRef` + 槽；或 `userSet` | 偏好键归 A。资格是 A 的角色写集成员 `kUserPreference` | 同步；保留原调用返回节点 | 写结果未知时资格保留并由 A 核对 | 签名沿用；实现改为先经写集准入（8.2 末） | `IMPLEMENTED`，但为直写：`:659` 与 `:662-664` 直接调用 `WriteUserPrimaryPreference`，不经写集，来源 `StepRef` 未使用。`LocalCardRequests::SwitchLocalProfile` `IMPLEMENTED`，仅有规则用例引用 |
| M14 | `SimModule` 与工厂（H）`infrastructure/assembly/sim_module*.{h,cpp}` | 创建结果为句柄或带阶段的失败；`LegacyEntry()`；`StopAcceptingAndDrain()` | 模块实例、epoch、装配图归 H | 创建与关闭在宿主执行域；回调线程只请求关闭 | 未 `Drained` 不销毁 | 按 7.2 调整持有图；枚举增加业务未决值 | 私有构造 `IMPLEMENTED`；两个工厂、两个 Deleter、`StopAcceptingAndDrain` 均 `DECLARED`。无任何用例构造 `SimModule`。A 的 Radio 端口接在 `CardRuntime` 上 |
| M15 | `SimHostPorts`（Core 宿主 → H）`infrastructure/assembly/host_ports.h` | 共享 RIL、入口、回调、号码查询端口 | 对象归 Core；H 借用 | 必须活到 SIM 排空结束 | — | 窄扩展：调度器、owner dispatch、Radio 平台端口、资源路由输入（7.2.4） | 结构体 `DECLARED`；multisim 内无 Core 侧实现，Host 用例不使用它 |
| M16 | `PrimaryHostLifecycle`（H 的成员部件）`infrastructure/assembly/primary_host_lifecycle.*` | `StopAcceptingAndDrain()` → 三态 | 关闭轮状态归 H | 可并发、可重复调用；只有一个清理者 | 任一条件不满足即返回未完成 | 沿用实现；A 侧判据换成业务收据（M19） | `IMPLEMENTED` + `HOST_EXECUTED`（B 异步用例中的关闭场景）。A 侧判据是 `HasPendingDeviceEffects()`，即 `pendingPrimary_` 是否为空 |
| M17 | `RetrySchedulerPort` / `ScheduledRetrySink`（宿主提供 → A 使用）`infrastructure/request/runtime_operation_store.h` | 调度、取消、审计；到期回调 `AcceptScheduledC1Retry` | 队列与线程归宿主；是否重试的业务决定归 A | 到期回调进入 A 的执行域；sink 以弱引用持有 | 调度代次不匹配即拒绝；Stop 后不重投 | 沿用。AF1-09 的处理：保留位置，在声明处注明"当前只服务 C1"，出现第二个使用者之前不泛化、不搬迁 | `IMPLEMENTED` + `HOST_EXECUTED`（测试内的受控调度器）。产品调度实现不存在 |
| M18 | 卡运行端口 `ShowNumberRuntimePort`、`ActivationRuntimePort`、`LocalCardPort`（R·card 实现 → T 用例、本地 Profile 短请求） | 号码缓存读/写卡、普通激活设备步骤、PIN/EF、本地 Profile 期望登记与执行 | 卡状态、会话、卡侧效果归 R·card | 异步结果经各自结果 sink 回发起者 | 未实现的能力显式拒绝，不给伪成功 | 沿用声明；AF-2 只补显式拒绝定义，不做业务迁移 | `DECLARED`。`CardRuntime` 仅定义了 `EndLocalProfileWait` 一个端口方法；`SessionRegistry` 有纯规则实现与规则用例 |
| M19 | 关闭收据（T、A、各用例、Card → H） | 出：一个只读计数值，见下 | 每个计数的事实归出具者 | H 在封口后查询；可重复查询；出具者在自己的提交域内取快照 | 取不到稳定快照按未完成处理 | **新增的唯一类型**，见下 | 不存在，待 AF-2 新增 |

M19 的最小形态（纯值，建议放在 `infrastructure/request` 的协议头，不引入新的服务或注册中心）：

```cpp
struct OwnerCloseReceipt {
    RuntimeEpoch epoch;
    size_t openRequests;            // 已受理、未到终态
    size_t undeliveredCompletions;  // 已终态未领取，或已领取但原回调/同步等待尚未交还
    size_t unresolvedEffects;       // 存储、参数或设备效果 Unknown，尚无核对证据
    size_t heldQualifications;      // 仍持有的角色写集、切换、发现预留、激活进度等
    size_t acceptedHandoffs;        // 已由常驻接手者确认的项；没有接手机制时恒为 0
};
// 仅当前四项全为 0，或与 acceptedHandoffs 逐项对消时，该 Owner 才算可关闭。
```

各出具者只新增一个只读方法（例如 `ReadCloseReceipt() const`），数据全部来自 7.2.5 表中列出的现有状态。

### 8.2 身份与资源映射（AF1-03、AF1-04、AF1-06）

规则（均来自 SC-02 与 A §10.1、§12.3，本节只给落地形态）：

1. 纯 Radio 步骤的发送登记只用原 `StepRef` 加本地轮次，不出现 `SessionRef`。
2. 本轮参与者由 R·device 给出，A 在本分支首次副作用（预写、通知、发令）之前取得并冻结，同一轮不增删。
3. `RadioDomainId` 是 A 的业务冲突范围（同域至多一个切换）；`ResourceKey` 是 R 的物理资源域。两者不要求一对一，对应关系只存在于 R·device 内部，由已验证的设备路由得出；A 不需要看到 `ResourceKey`。
4. 路由未知或独立性未证时，R 使用一个保守共享键。

最小签名建议（伪代码，不改源码）：

```cpp
// role_assignment/primary/radio_execution_port.h
struct RadioRoundPlan {
    std::vector<SlotId> members;   // R 已验证、可路由、互不重复的原生协议成员；可以不含某些目录槽
    uint64_t routeRevision;        // R 的路由观察版本；变化意味着旧计划需要核对
};
class RadioExecutionPort {
    // 现有方法沿用
    virtual std::optional<RadioRoundPlan> PlanPrimaryRound(
        SlotId target, const RadioDomainId &domain, PrimarySwitchMode mode) const = 0; // 无可用路由返回空
    virtual bool SwitchPrimaryRadio(const RadioSwitchRequest &request) = 0;            // 返回是否已被接受，取代 WasLastSubmissionAccepted
};
// RadioSwitchRequest 增加 routeRevision 字段；memberCount 由 plan.members.size() 填写

// infrastructure/request/runtime_operation_store.h
bool RecordRadioSent(StepRef expected, uint32_t localRound);  // 与卡用 RecordSent(StepRef, SessionRef) 并列
// RequestContext 增加 std::optional<uint32_t> sentRadioRound；A 接纳结果时检查它，而不是 sentSession

// device 内部（不出现在 A 可见的头文件）
// 由宿主注入的已验证路由表：成员槽/RIL 实例 → ResourceKey；
// R 在 PlanPrimaryRound 时据此算出本轮键集，并在首次提交时一次 Acquire。
```

`routeRevision` 的用途：提交时 R 发现版本已变，则拒绝该提交并让 A 把原轮转入核对，不在途换路。

关于强类型（AF1-06）的结论：

| 身份 | 是否建议加包装类型 | 理由 |
| --- | --- | --- |
| 传输 token、fence | 否 | 只在 R·device 内部和平台边界出现，不跨到 A/T，误用面很小 |
| 原生协议会话号 | 否 | 留在平台适配内，由 R 映射到本地轮次 |
| 本地轮次 | 否，但建议把字段 `protocolSession` 改名为 `localRound` | `RadioSwitchRequest` 中轮次、phase、成员数是三个相邻的 `uint32_t`，且调用点用位置初始化。改名并改用具名字段赋值即可消除混淆，不需要新类型 |

SC-03 的入口准入（M13）落地形态：

```cpp
// AssignmentService 内部，签名不变
void AcceptLocalProfilePreference(const StepRef &source, SlotId slot) {
    // 1. 若 source 属于一个已持有含 kUserPreference 写集的 A 请求 → 复用该资格
    // 2. 否则以独立请求登记 kUserPreference
    // 3. 取得资格后在锁外写 Store，返回后凭同一资格接纳并释放；结果未知则保留资格
}
```

当独立提示与另一请求（例如进行中的 C1）的写集相交时，提示不得直接写 Store，这一点由 SC-03 确定。**相交之后提示是延后写入还是该组暂缓切换**，收敛记录的 O4 和 D §5.2.1 都把它留给产品证据，本轮登记为 AF1-13，不自行决定。

### 8.3 三条代表性数据流

图中 `Card`、`Radio` 分别指 R·card 与 R·device；`Store` 指 I 侧的存储 Adapter。每张图后列出真实代码对应点。

#### Flow-1 账户发现、恢复与角色接纳

```mermaid
sequenceDiagram
    participant Card as R·card
    participant T as TopologyService
    participant A as AssignmentService
    participant Store as AccountStore

    Card->>T: AcceptCardDiscoveryObservation(SessionRef, 序号, ICCID)
    Note over T: T 提交域：登记观察，签发 store ticket，记下位置 revision
    T->>Store: MatchAccountByIccid
    T->>A: PrepareAccountRoleFragment（T 不持锁）
    A-->>T: 角色片段 + 预留
    Note over T: 复核观察仍当前且位置 revision 未变
    T->>Store: WriteDiscoveredAccount（一次原写，含 T 字段与 A 片段）
    Store-->>T: 结果（callFinished / effect / assignedSubscription）

    alt 已确认写入，观察与预留仍有效
        T->>A: AcceptDiscoveredRoleProjection（A 单独接纳角色）
        Note over T: T 单独接纳账户与绑定，签发 BindingRef
        T->>A: FinalizeInitialRoleReservation（带存储证据）
    else 效果 Unknown
        Note over T: 记入 pendingDiscoveries_，不发布绑定
        Note over A: 预留保留，不因调用返回而释放
    else 已写但观察已失效（stale-applied）
        Note over T: 记入 pendingDiscoveries_，保留 assignedSubscription
    end

    opt 恢复：同一 SessionRef 再次到达
        T->>Store: MatchAccountByIccid（读回）
        T->>A: AcceptDiscoveredRoleProjection
        Note over T: 接纳账户，ReconcileStored，清 pending
        T->>A: FinalizeInitialRoleReservation
    end

    opt 重载（Current 与 Historical 各自独立尝试）
        Note over T: PrepareAccountReload：按 view 签发 ticket，记字段与角色版本
        T->>Store: LoadAccounts(view, attempt)
        Note over T: AcceptAccountReload：按字段版本逐项接纳
        T->>A: AcceptReloadedRoleProjectionSet（仅整次读取成功时）
    end
```

T 的接纳与 A 的接纳是两次独立提交，中间没有共同事务；一次原写把两者的字段合并下发，但内存投影各自接纳、各自可能失败。

代码对应点：`card/runtime/card_runtime.cpp` `PublishAcceptedCardDiscovery`、`InvalidateVerifiedCardSession`；`account/topology_service.cpp` `DiscoverAccount`、`ReconcileDiscovery`、`PrepareAccountReload`、`AcceptAccountReload`；`account/discovery/discovery_observation_ledger.*`；`role_assignment/assignment_service.cpp` 中 `AccountRolePlanningPort` 的七个实现方法；`role_assignment/account_role_protocol.h`。

#### Flow-2 Primary 角色与 Radio 执行

```mermaid
sequenceDiagram
    participant C as LegacySimFacade
    participant A as AssignmentService
    participant O as OperationStore(A 自有)
    participant T as TopologyService
    participant Store as AssignmentStore
    participant Radio as R·device
    participant P as 平台

    C->>A: StartPrimarySwitch(目标槽, expectedBinding, 完成路由)
    A->>O: Begin → RequestId；AdvanceStep
    A->>T: ReadPrimaryTarget（前后两次，夹一次 Radio 观察）
    A->>Radio: ReadPrimaryRadioObservation
    Note over A: 选分支；AdmitTransitionWithRoleWriteSet（切换 + 角色写集）

    alt C0 已在主 Modem：无设备命令
        A->>Store: 写偏好、Primary 参数、Data（逐键，各有 callFinished）
        alt 全部调用结束
            A->>C: CompleteLegacyInt(0)，释放写集
        else 任一键效果未知
            Note over A: 保留写集与切换（Reconciling），原调用尚未交付
        end
    else B 专用路径：先设备后保存
        A->>Radio: PlanPrimaryRound → 冻结成员（单成员）
        A->>O: RecordRadioSent(StepRef, 本地轮次)
        A->>Radio: SwitchPrimaryRadio
        Radio->>Radio: Acquire(本轮键集)；签发传输 token
        Radio->>P: SubmitPrimaryRadio(request, token)
        P-->>Radio: 设备结果(token, serial, commandEnded, resourceReusable)
        Note over Radio: 匹配 token/serial/成员/轮次；Complete 释放租约
        Radio-->>A: AcceptRadioStep（经 owner dispatch）
        A->>Store: 写 Primary 参数、Data
        A->>C: CompleteLegacyInt
    else C1 三阶段：先预写后设备
        A->>Store: 预写偏好与 Primary 参数
        A->>Radio: PlanPrimaryRound → 冻结成员集合
        loop CHECK → UPDATE → COMPLETE
            A->>O: AdvanceStep；RecordRadioSent
            A->>Radio: 对每个冻结成员 SwitchPrimaryRadio(phase)
            Radio->>P: 逐成员提交
            Note over Radio,P: CHECK、COMPLETE 匹配原响应；UPDATE 只接受同会话的成员 NOTIFY，不存在单独 ACK
            Radio-->>A: 该阶段全部成员结果
        end
        Note over A: 本轮失败：恢复旧参数，首轮即向原调用交付失败，按预算调度重试（新轮次）
        A->>C: CompleteLegacyInt
    end

    opt 设备结果 Unknown 或交接未知
        Note over Radio: 键进入 Quarantined，租约不释放
        Radio-->>A: 效果 Unknown
        Note over A: NeedsReconciliation；保留写集与原步骤
        Radio-->>A: AcceptLateRadioEvidence（恢复证据到达后）
        Note over A: 只记录迟到证据，不重放完成、通知或命令
    end
```

两点必须分开看：Radio 把租约释放、键回到 Available，只说明设备可复用；A 是否完成取决于它自己的参数写入、联动 Data 和原调用交付。反过来，A 已向原调用交付失败，也不代表设备已结束。

代码对应点：`infrastructure/compatibility/api/legacy_sim_facade.cpp` `RunPrimarySwitchAtExecutionPoint`；`role_assignment/assignment_service.cpp` `StartPrimarySwitch`、`AcceptRadioStep`、`AcceptLateRadioEvidence`、`AcceptC1RetryDue`、`AcceptPrimarySwitchTimeout`、`CompleteOnce`；`role_assignment/assignment_aggregate.*` `AdmitTransitionWithRoleWriteSet`；`role_assignment/primary/primary_switch_plan.cpp` `PlanPrimarySwitch`；`device/resource/resource_radio_runtime.cpp` `SwitchPrimaryRadio`、`AcceptDeviceResult`、`Recover`；`device/resource/resource_barrier.cpp`。图中的 `PlanPrimaryRound`、`RecordRadioSent` 是 8.2 建议的接口，当前不存在。

#### Flow-3 完整模块关闭

```mermaid
sequenceDiagram
    participant Host as 宿主
    participant H as SimModule
    participant L as PrimaryHostLifecycle
    participant A as AssignmentService
    participant T as TopologyService 与用例
    participant S as Scheduler
    participant Radio as R·device
    participant X as 常驻接手者(可选)

    Host->>H: StopAcceptingAndDrain()
    Note over H: 封口：Facade 关门并登记在途调用；T、用例、Card 停止受理
    H->>L: StopAcceptingAndDrain()
    L->>A: RequestStopAccepting
    L->>S: StopAcceptingAndCancel
    L->>Radio: StopAccepting
    L->>A: Owner 封口任务（取消可取消的重试）

    H->>A: ReadCloseReceipt
    H->>T: ReadCloseReceipt
    H->>S: Audit
    H->>Radio: OwnerCallbacksDrained / DeviceResourcesDrained

    alt C0 原调用仍在执行，或完成已领取但回调未返回
        H-->>Host: CallbacksStillInFlight
    else 存储写入已发、效果 Unknown（设备命令数可以为 0）
        H->>X: 提议移交（原请求、完成路由、效果清单、资格）
        alt 无接手者，或接手者拒绝
            H-->>Host: BusinessEffectsUnresolved，原图保留
        else 接手者确认
            Note over H: 仅该项计为已移交
        end
    else Radio 键仍 Quarantined
        H-->>Host: DeviceEffectsUnresolved
        Radio-->>A: 迟到证据仍可送达（接收路径保留）
    else 全部收据为零
        H-->>Host: Drained
        Host->>H: 销毁（成员反序）
    end

    Host->>H: 再次 StopAcceptingAndDrain()
    Note over H: 读同一轮状态；不重复封口、不重投、不重试、不重复回调
```

设备命令排空只是其中一个条件：在"存储效果 Unknown"分支中，Radio 完全空闲，结果仍然不是 `Drained`。

代码对应点：`infrastructure/assembly/primary_host_lifecycle.cpp` `StopAcceptingAndDrain`；`role_assignment/assignment_service.cpp` `RequestStopAccepting`、`StopAcceptingAndCancelScheduledRetries`、`HasPendingDeviceEffects`；`device/resource/resource_radio_runtime.cpp` `StopAccepting`、`OwnerCallbacksDrained`、`DeviceResourcesDrained`、`FlushOwnerDeliveries`；`infrastructure/request/runtime_operation_store.h` `RetrySchedulerPort::Audit`。图中的 `SimModule::StopAcceptingAndDrain` 实现、`ReadCloseReceipt`、接手者端口当前都不存在。

### 8.4 对 P2 图示的两处勘误

§3.8 类图中有两条关系画成了目标逻辑关系，与当前代码的成员持有不一致。原图保留不改，以下为更正。

| 关系 | 当前代码关系（`632f6a2a`） | 推荐稳态关系 |
| --- | --- | --- |
| T 与账户记录 | `TopologyService` 持成员 `AccountRegistry accounts_`；`AccountRegistry` 以 `map<SubscriptionId, AccountRecord>` 持有记录。原图把 `TopologyService *-- AccountRecord` 画成直接组合，漏了 `AccountRegistry` 这一层 | 不变：`TopologyService *-- AccountRegistry *-- AccountRecord`，并列 `TopologyService *-- BindingIndex` |
| 会话与加载 | `CardRuntime` **分别**持成员 `SessionRegistry sessions_` 与 `map<string, LoadSession> loads_`。`SessionRegistry` 并不持有 `LoadSession`；`LoadSession` 只是保存一个 `SessionRef` 值。原图的 `SessionRegistry *-- LoadSession` 是目标逻辑关系，不是已实现的组合 | `card/state` 的组件持 `SessionRegistry`，`card/records` 的组件持 `LoadSession`；二者通过 `SessionRef` 值关联（`LoadSession ..> SessionRef`），不是组合。具体持有组件见 AF1-08 |

```mermaid
classDiagram
    class TopologyService
    class AccountRegistry
    class AccountRecord
    class BindingIndex
    class CardRuntime
    class SessionRegistry
    class LoadSession
    class SessionRef
    TopologyService *-- AccountRegistry
    TopologyService *-- BindingIndex
    AccountRegistry *-- AccountRecord : 0..*
    CardRuntime *-- SessionRegistry : 当前代码
    CardRuntime *-- LoadSession : 当前代码 0..*
    SessionRegistry ..> SessionRef : 签发
    LoadSession ..> SessionRef : 按值引用
```

## 第九部分：P4-A 源码一致性审查

### 9.1 核验口径

- 审查对象是 8.1 矩阵的 19 行；结果已写入该矩阵最后一列，本部分不另建矩阵。
- 状态含义：`DECLARED` 只有声明；`IMPLEMENTED` 有可读到的实现；`HOST_EXECUTED` 仓内存在执行它的 Host 用例或既有 Host 报告；`PRODUCT_EXECUTED` 产品装配中执行；`BLOCKED` 依赖产品或设备证据。各状态分别成立，互不替代。
- **本轮没有运行任何构建或测试。** `HOST_EXECUTED` 的依据是"用例源码或既有报告存在"，不是本轮复跑结果。
- 全部 19 行都没有 `PRODUCT_EXECUTED`：`services/multisim/BUILD.gn` 的 target 均为 `testonly`，生产仍用 `services/sim/`。
- 本轮实读的实现文件：`role_assignment/assignment_service.cpp`（全文）、`role_assignment/assignment_aggregate.cpp`（写集与准入部分）、`device/resource/resource_barrier.cpp`（全文）、`device/resource/resource_radio_runtime.cpp`（全文）、`account/topology_service.cpp`（59～316 行）、`infrastructure/request/runtime_operation_store.cpp`（全文）、`infrastructure/compatibility/events/legacy_completion.cpp`、`infrastructure/compatibility/api/legacy_sim_facade.cpp`、`card/esim/local_card_requests.cpp`，以及 `test/cases/target/primary/b_async_primary_test.cpp` 的夹具段。

### 9.2 SC-01～03 的代码落地情况

三项都是"部分落地"。

| 决策 | 已经落地的部分 | 尚缺的部分 | 证据 |
| --- | --- | --- | --- |
| SC-01 关闭责任 | 封口先于 Owner 任务、单一清理者、重复 Stop 读同一状态、对 A 和 dispatch 用弱引用（`PrimaryHostLifecycle`）。调度器封口与审计。R 向 Owner 的投递有逐条收据，设备排空与回调排空分开判定。A 取消可取消的重试。O 的终态不可覆写、完成只领取一次。A 在存储效果未知时**确实保留了**角色写集和非终态的切换 | ① A 的关闭判据只看 `pendingPrimary_`。C0 路径任一键 `callFinished=false` 时直接返回，不建 pending，于是 Stop 得到 `Drained`，而原调用未交付、写集未释放。② B/C1 设备成功后的参数写若效果未知，代码把请求标成 `NeedsReconciliation` 后**删除了 pending**，同样不再被关闭判据看到，原调用也未交付。③ 没有"完成已领取、回调未返回"的记录，且回调在持锁时发出。④ T、各用例、Card 没有关闭收据。⑤ 没有模块级 Stop，Facade 没有入口在途登记。⑥ 没有接手机制 | `assignment_service.cpp` `HasPendingDeviceEffects`（:126）；`StartPrimarySwitch` 末段 `retainUnknownEffect`；`AcceptRadioStep` 成功分支中 `store_.WritePrimaryParameters`（:476）与 `WriteDefaultData`（:486）之后的 `!callFinished` 分支；`CompleteOnce` |
| SC-02 Radio 身份和参与者 | R 侧：每次发送一个传输 token，serial 在同一关联中绑定；结果按 token、serial、成员、本地轮次、phase 匹配；UPDATE（phase 2）要求 `notify`，其余阶段要求非 notify；重复成员、过期传输被拒并计数。A 侧：每个请求有冻结的 `members`，重试换新轮次；目标 `BindingRef` 只附在目标成员上；结果的轮次、阶段、成员、绑定逐项核对 | ① C1 的成员来自 `catalog_.Slots()`，不是 R 给出的可路由成员。② 四处用 `bindingRevision` 合成 `SessionRef`，`RecordSent` 只有卡会话形式。③ 资源集是固定全集，与本轮路由无关。④ 没有路由版本，无法发现在途换路 | `assignment_service.cpp:253`、`:199-200、239-240、392-393、600`；`resource_radio_runtime.cpp` `SameCommand`、`AcceptDeviceResult`、构造函数 |
| SC-03 完整写权 | `StartPrimarySwitch` 用 `AdmitTransitionWithRoleWriteSet` 一次登记切换和 `kPrimaryRoleWriteSet`。`SetDefaultRole` 对 Data、Voice、SMS 分别登记，互不相交时可并行。`AdmitRoleWriteSet` 拒绝与他人相交的成员。结果未知（`callFinished=false`）时不释放。multisim 内只有 A 持有 `AssignmentStore` | ① 两个偏好入口直写 Store，不经写集。② `AcceptLocalProfilePreference` 不看来源 `StepRef`，无法区分"父请求复用"与"独立提示"。③ `SetDefaultRole` 不取 `primaryCommitLock_`，与其他方法并发时对聚合的访问没有互斥，目前依赖"Owner 串行执行"这一前提 | `assignment_service.cpp:659、662-664`、`SetDefaultRole`；`assignment_aggregate.cpp` `AdmitRoleWriteSet` |

### 9.3 本轮新增或细化的问题

延续第四部分的编号。类别标记同前。

| 编号 | 问题描述 | 证据 | 影响 | 状态 | 需裁决 | 去向 |
| --- | --- | --- | --- | --- | --- | --- |
| AF1-12 | A 在持 `primaryCommitLock_` 期间执行存储写、Radio 提交（进而进入平台）、输出发布和完成回调。目标架构要求锁内不做 DB/RIL/IPC/回调，收敛记录也写明"A 在锁外调用存储并核对结果"。【不一致】 | `assignment_service.cpp` `StartPrimarySwitch`、`AcceptRadioStep`、`AcceptC1RetryDue`、`CompleteOnce`；A §14.1；收敛矩阵 SC-03 的 B 列 | 回调重入、Stop 与在途调用的相互等待、7.1 的锁序 | 设计 PASS；代码 OPEN | 否 | AF2-07 |
| AF1-13 | 独立偏好提示与另一请求的写集相交时的处置（延后写入，或该组入口暂缓切换）。【缺证】 | 收敛记录 O4；D §5.2.1 第 1～3 行；B §7"原同步返回要求无法同时满足时暂缓该组切换" | SC-03 入口准入的冲突分支 | OPEN | 是，且依赖产品证据 | AF2-04 只验证"不旁路"；处置留 P5/P6 |
| AF1-14 | B/C1 设备成功后参数写效果未知时，A 删除 pending 且不交付原调用。属于 SC-01 缺口②的具体位置。【不一致】 | `assignment_service.cpp` `AcceptRadioStep`（:476、:486 之后的分支） | 关闭假 `Drained`；原调用悬空 | OPEN | 否 | AF2-05 |
| AF1-15 | T 的 `AcceptCardDiscoveryObservation` 在 R 的调用线程上同步执行含存储 I/O 的发现流程；`CardRuntime` 的注释要求失效通知不得排在发现的长 I/O 之后。【不一致】 | `topology_service.cpp:59-62`；`card_runtime.cpp` `InvalidateVerifiedCardSession` 注释 | R 状态写点被阻塞；失效与发现的相对顺序 | OPEN | 否 | AF2-07 |
| AF1-16 | `CardRuntime` 的多数端口方法无定义，推断任何构造它的可执行目标无法链接；`SimModule`、`LegacySimFacade`、`LegacyCompletion` 在 Host 中都没有被执行。【实现】（链接结论为推断，未构建验证） | `card_runtime.cpp`；`test/cases`、`test/framework` 中无对上述类型的引用 | Host 框架无法成图 | OPEN | 否 | AF2-02 |
| AF1-17 | `ResourceRadioRuntime` 在持 `lock_` 时调用 `dispatch->RadioOwnerCallbacksDrained()`。若 dispatch 的实现持自己的锁再回入 R，会形成锁序反转。目前未见此类回入。【实现】 | `resource_radio_runtime.cpp` `OwnerCallbacksDrained`、`StopAccepting`、`FlushOwnerDeliveries` | 潜在锁序风险 | OPEN（低） | 否 | AF2-07 顺带处理 |

第四部分各项的更新：

| 编号 | 更新后的状态 |
| --- | --- |
| AF1-01 | 推荐方案见 7.1，`PROPOSED / AWAITING_ARCHITECT_SIGNOFF` |
| AF1-02 | 推荐方案见 7.2，`PROPOSED / AWAITING_ARCHITECT_SIGNOFF` |
| AF1-03 | 接口形态见 8.2（`RecordRadioSent`、`PlanPrimaryRound`），`PROPOSED`；代码 OPEN → AF2-03 |
| AF1-04 | 已读 A §12.4、§12.5，其中没有 `RadioDomainId`→`ResourceKey` 的规则。按 8.2 处理：映射只在 R·device 内，由宿主注入的已验证路由得出，未证时用保守共享键。`PROPOSED`；真实路由证据仍属产品（O3） |
| AF1-05 | 落地形态见 8.2 末；代码 OPEN → AF2-04 |
| AF1-06 | 结论见 8.2：不加包装类型，只改名和具名赋值 |
| AF1-07 | 收据形态见 M19 与 7.2.5；代码 OPEN → AF2-05、AF2-06 |
| AF1-08 | 仍为设计细化；AF-2 不处理稳态拆分，只处理 `CardRuntime` 可构造（AF2-02） |
| AF1-09 | 结论见 M17：保留位置，注明只服务 C1 |
| AF1-10、AF1-11 | 不变 |

## 第十部分：P4-B AF-2 最小开发清单与 P5/P6 交接

### 10.1 AF-2 最小框架开发清单

范围只限于让 T/A/R·card/R·device/O/H 经真实接口组成可运行的 Target Host 框架，并验证三条数据流。全部保持 `testonly`；不改 `services/sim/`、Core、根产品 GN；不做业务迁移。已有实现一律复用。

| 编号 | 必要代码修改 | 解决的架构问题 | 依赖 | 最小 Host 验证 | 完成条件 |
| --- | --- | --- | --- | --- | --- |
| AF2-01 | `ResourceBarrier` 内置同步；`ResourceRadioRuntime` 改为借用注入的屏障；删除 `CardRuntime::resources_`；模块内只由 assembly 构造屏障并注册键 | AF1-01；M08 | AF1-01 签收 | 两个命令族（Radio 与一个测试用的第二执行适配）：不同键并行完成；同键时后到者在平台提交前被拒；先到者 Unknown 时键保持隔离直到恢复证据 | 现有 B/C1 Host 用例行为不变；全仓只有一处构造 `ResourceBarrier`；7.1.6 的两条用例通过 |
| AF2-02 | `SimModule` 核心图端口化；A 改为 `shared_ptr` 持有；Radio 执行适配注入 A；`CardRuntime` 去掉 `RadioExecutionPort`、Legacy 借用改为可选、未实现方法给显式拒绝定义；`SimHostPorts` 按 7.2.4 窄扩展；新增 testonly 的 Host 装配函数与 Deleter | AF1-02、AF1-16；M14、M15、M18 | AF2-01；AF1-02 签收 | 经 Host 装配得到 `SimModule`，从 `LegacyEntry()` 完成一次 C0；缺绑定或缺初始资源证据时创建失败并报告阶段 | `SimModule` 可在 Host 可执行目标中构造、使用、销毁；`LegacySimFacade` 与 `LegacyCompletion` 进入执行路径；两个产品工厂仍只声明 |
| AF2-03 | `RadioExecutionPort` 增加 `PlanPrimaryRound`、提交带返回值；`RuntimeOperationStore` 增加 `RecordRadioSent`；A 改用规划成员并去掉合成 `SessionRef`；R·device 按注入的路由表得出本轮键集；字段改名 `localRound` | SC-02、AF1-03、AF1-04、AF1-06；M06、M07、M10 | AF2-01 | 四端点两栈夹具：C1 每阶段只向两个成员发令；必需成员无路由时在预写之前失败，参数写次数为 0；`routeRevision` 在途变化时提交被拒并转入核对 | A 的源码不再出现由 `bindingRevision` 构造的 `SessionRef`，也不再用 `catalog_.Slots()` 生成成员；原双卡 B/C1 用例结果不变 |
| AF2-04 | 两个偏好入口改为先经 `AssignmentAggregate` 写集准入；按来源 `StepRef` 区分父请求复用与独立提示；`SetDefaultRole` 纳入同一提交域 | SC-03、AF1-05；M13 | AF2-07 的锁外 I/O 约定 | 无冲突时提示写一次并释放；父请求持有时复用；C1 持有写集期间到达的独立提示**不产生 Store 写**并给出可观察的内部结果 | 源码中 `WriteUserPrimaryPreference` 的每个调用点之前都有同键资格检查。冲突后的处置（AF1-13）不在此项完成 |
| AF2-05 | A、T、号码/激活用例、Card 各增加只读的 `ReadCloseReceipt`（M19）；A 在存储效果未知时保留可被关闭判据看到的记录（修 AF1-14 与 C0 分支）；完成出口增加"已领取未返回"计数；C0 成功与前置拒绝路径按终态回收记录 | SC-01、AF1-07、AF1-14；M10、M11、M19 | AF2-07 | C0 在"某键已写未确认"和"未写未确认"两种前态下，A 的收据非零且原调用未被伪交付；设备成功后参数写未知时同样非零；T 发现存储 Unknown 时 T 的收据非零 | 每个出具者的收据只读取现有状态；注入上述前态时没有任何路径把收据清零 |
| AF2-06 | 实现 `SimModule::StopAcceptingAndDrain`：Facade 入口关门与在途登记，复用 `PrimaryHostLifecycle`，汇合 AF2-05 的收据与屏障状态；私有 `DrainResult` 增加 `BusinessEffectsUnresolved`；可选的接手者端口只实现"不存在或拒绝则保留原图" | SC-01、AF1-02；M14、M16；Flow-3 | AF2-02、AF2-05 | Flow-3 的五种情形：C0 原调用在途；存储 Unknown 且 Radio 空闲；Radio 晚证据在 Stop 之后到达并被原接收者接纳；重复 Stop 不重复封口、不增加提交和回调次数；接手者拒绝后仍非 `Drained` | 上述五种情形都不返回 `Drained`（或在条件满足后才返回）；`Drained` 之后销毁无在途回调；不实现真实的常驻接手者 |
| AF2-07 | A 不在持提交锁时调用平台提交、完成回调和输出发布，存储写按"登记→锁外写→凭原资格接纳"改造；T 把发现流程转入自己的执行域，失效通知保持短调用；`ResourceRadioRuntime` 不在持锁时查询 dispatch | AF1-12、AF1-15、AF1-17；A §14.1；7.1.4 的锁序 | 无 | 平台在提交过程中同步回调结果；完成回调内再次发起请求或请求关闭；发现的存储调用阻塞期间到达的失效先于发现结果生效 | 上述重入场景无死锁、无重复完成；现有 C0/B/C1 与 T1/T2 用例结果不变 |
| AF2-08 | 更新架构检查（`allowed_edge`、include 规则），如拆分 device 逻辑 target 则同步 `BUILD.gn` 与源表 | 让 AF2-01～03 的结构约束可被检查 | AF2-01～03 | 运行纯规则与 `--with-gn` 架构检查 | 检查通过，或对既有失败（收敛记录 O6 所述完整迁移校验）给出与本批无关的证据 |

建议次序：AF2-07 与 AF2-01 可先行且互不依赖；随后 AF2-02 → AF2-03、AF2-04 → AF2-05 → AF2-06；AF2-08 随各项同步。

每项与 P3 的对应：AF2-01 → 7.1、M08；AF2-02 → 7.2、M14/M15/M18；AF2-03 → 8.2、M06/M07/M10、Flow-2；AF2-04 → 8.2 末、M13；AF2-05 → M19、7.2.5、Flow-1 的 Unknown 分支；AF2-06 → 7.2.5、Flow-3；AF2-07 → 7.1.4 第 7 问、M01/M11/M12；AF2-08 → 7.1.5、7.2.6。

AF-2 明确不做的事：产品 Radio 平台适配；`AttachSimModuleToExisting` 与 `CreateOwnedSimServices`；号码、激活、PIN/EF 的业务流程实现；`CardRuntime` 的稳态拆分；真实常驻接手者；任何 WU 的旧写者退出。

### 10.2 需要签收或外部证据的事项

| 事项 | 需要谁 | 不签收时的处理 |
| --- | --- | --- |
| AF1-01 推荐方案（7.1） | 架构负责人 | AF2-01 不开工；AF2-03 中"按路由得出键集"部分一并等待 |
| AF1-02 推荐方案（7.2） | 架构负责人 | AF2-02、AF2-06 不开工 |
| 8.2 的三处接口窄扩展（`PlanPrimaryRound`、提交返回值、`RecordRadioSent`）与 M19 收据类型 | 架构负责人（可与上两项合并签收） | AF2-03、AF2-05 不开工 |
| AF1-13 偏好冲突处置 | 架构负责人 + SIM 接口/eSIM/参数服务（O4） | AF2-04 只验证不旁路；产品切换该组入口继续暂缓 |
| 物理共享域、真实路由、命令结束与复用判据 | 产品/设备（O2、O3） | Host 用保守共享键与 fake 平台；不宣称产品完成 |

### 10.3 P5/P6 的输入

P5（权威文档修订）需要改动的位置。本轮未修改这些文档。

| 文档与位置 | 需要写入的内容 | 来源 |
| --- | --- | --- |
| A §12.3 | 屏障是一个模块实例内唯一的准入对象并自带短提交域；执行适配借用；锁序 | 7.1 |
| A §14.3（SC-01 表） | 收据提供者补全 T、各用例、Card；模块级结果增加"业务未决" | 7.2.5、M19 |
| B §3.2、§9、§9.1 | `CardRuntime` 不实现 `RadioExecutionPort`、不持屏障；屏障的物理持有者；Card 直连硬件时的准入顺序 | 7.1、7.2.4 |
| B §8 | `RecordRadioSent` 的签名；完成出口的在途计数 | 8.2、M10、M11 |
| B §9（SC-02） | `PlanPrimaryRound`、`routeRevision`、路由表到键集的映射、`RadioDomainId` 与 `ResourceKey` 的关系 | 8.2 |
| B §7（SC-03） | 两个偏好入口的准入步骤；冲突处置待 AF1-13 | 8.2 末 |
| B §11 | `SimModule` 持有图与绑定顺序；`PrimaryHostLifecycle` 为成员部件；Host 装配与两个产品工厂的范围划分 | 7.2 |
| C §4.4、§5.3 | device 核心对象列表中的 `RadioAssignmentView` 实际定义在 `role_assignment/assignment_requests.h`，需说明归属；assembly 持有屏障实例 | 7.1、7.2 |
| B §4.1 | 如拆分 device 逻辑 target，更新 target 表 | 7.1.5 |
| 本文件 §3.8 | 以 8.4 的勘误为准 | 8.4 |

关于 C §4.4 的 `RadioAssignmentView`：目录方案把它列为 device 的核心数据对象，全仓检索显示该类型定义在 `role_assignment/assignment_requests.h:37`，是 A 对外视图 `AssignmentView` 的成员，device 目录下没有同名类型。是文档归属需要更正，还是类型应当迁移，留给 P5 判断。

P6（独立签收）需要核对的清单：AF1-01～AF1-17 的状态；7.1、7.2、8.2 的 `PROPOSED` 项是否已签；SC-01～03 的"已落地/尚缺"表（9.2）是否随 AF-2 更新；三条数据流的代码对应点是否仍然成立。

### 10.4 第二轮完成标准自评

| # | 标准 | 结果 |
| --- | --- | --- |
| 1 | AF1-01、02 有明确推荐方案、依据和签收边界 | 满足（7.1、7.2、10.2） |
| 2 | 核心接口契约与身份传递规则完整 | 满足（8.1、8.2）。AF1-13 的冲突处置为 OPEN，已登记 |
| 3 | 三条信息流具备真实代码对应点 | 满足（8.3）；图中三处尚不存在的接口已逐一标明 |
| 4 | 模块装配和关闭责任明确 | 满足（7.2.4、7.2.5） |
| 5 | P4 已对照真实源码形成统一差距矩阵 | 满足（8.1 最后一列、9.2、9.3） |
| 6 | AF-2 最小框架开发清单明确 | 满足（10.1） |
| 7 | P5/P6 的文档修订输入明确 | 满足（10.3） |
| 8 | 成果已写入现有检查点 | 满足（第六至第十部分；未暂存、未提交、未推送） |

本轮的验证限度：

- 只做了文档与源码的只读核对，没有运行构建、测试或架构检查脚本。
- `HOST_EXECUTED` 依据用例源码或既有报告的存在，不是复跑结果。
- "`CardRuntime` 无法链接"是按语言规则的推断。
- 四张新增 Mermaid 图没有经过渲染验证。
- D（功能与 WU）只读了 §5.2.1 台账的表头和前 16 行；A 只补读了 §12.4、§12.5。

### 10.5 下一轮开始前的检查

1. `git rev-parse HEAD` 对比 `6df6973c`；有增量先看 `git log 6df6973c..HEAD --stat`，并对 9.1 列出的实现文件做 `git diff 6df6973c..HEAD -- <path>`。
2. 确认本文件第六至第十部分存在（本轮未提交，属工作区修改）。
3. 取得 10.2 前三行的签收结论，再决定进入 P5/P6 还是 AF-2。

**第二轮到此停止，未进入 P5/P6，未修改权威文档，未开始 AF-2 编码。**

---

## 第十一部分：第三轮执行状态（P5～P6）

> 第一至第十部分是前两轮的固定记录，不随第三轮改写。第三轮对前两轮结论的修正统一写在第十二部分之后，并注明被修正的位置。

### 11.1 第三轮基线（2026-11-23 实测）

| 项 | 实测值 |
| --- | --- |
| 本地 HEAD = `origin/main`（`git ls-remote`） | `08d6e6f770fc642eb49b41b511fefd299b6370b1` |
| `6df6973c..HEAD` 增量 | 仅 1 个提交、1 个文件：本检查点第六至第十部分入库。`services/` 无变化，不重读源码 |
| 暂存区 | 空 |
| 未跟踪文件（用户文件，不触碰） | `default.profraw`、`opencode.json` |
| 本轮允许修改 | 四篇权威文档（A/B/C/D）与本文件；不 add/commit/push；不改 `approval_status` |

### 11.2 阶段状态

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| P5 | 四篇权威文档修订（P5-1～P5-5） | 完成（第十二部分） |
| AF-2 清单复审 | 对 §10.1 八项做最小化审查 | 完成（第十三部分，取代 §10.1） |
| P6 | 独立一致性审查 | 完成（第十四部分） |
| 签收输出 | 最终摘要与签收材料 | 完成（第十五部分） |
| 评审意见定向修订 | 按独立架构评审七条意见修订五个文件 | 完成（第十六部分） |

AF-1 最终标记：`AF1_READY_FOR_ARCHITECT_SIGNOFF`。设计已完整，新增方案均为 `PROPOSED`，等待架构负责人批准；四篇权威文档的 `approval_status` 未改。

## 第十二部分：P5 权威文档修订记录

### 12.1 修订方式

- 四篇文档都只追加带 `af1-` 锚点的独立段落，标题统一为"AF-1 补充（PROPOSED，待架构负责人签收）"。SC-01～03 的原有表述、锚点和矩阵一字未改。
- 没有修改任何 `approval_status`、`status`、`owner`。A 的 front matter 只改了 `revision` 和 `content_review_scope` 两行，并在 §23 增加一行修订记录；B 只在表头"更新日期"一格追加说明。
- 没有引入新的业务行为、公开接口 ABI 或 schema。新增的接口形态都是 multisim 内部的私有契约建议。
- 没有把检查点里的源码分析表复制进权威文档；权威文档只在必要处用一句话指出 `632f6a2a8` 的待改现状。

### 12.2 四篇文档修订清单

| 文档 | 位置与锚点 | 对应重点 | 内容 |
| --- | --- | --- | --- |
| A | §10.1 `#af1-identity` | P5-4 | Binding、Session、Step、Protocol Round、Transport、Lease 六类身份的签发者与禁止用途；纯 Radio 发送登记；成员冻结与路由版本；UPDATE 只认 NOTIFY |
| A | §12.3 `#af1-resource-admission` | P5-1 | 原子准入范围；资源权威（R）与对象持有（H）分开；Card 与 Radio 共享同一准入；`RadioDomainId` 与 `ResourceKey`；证明边界（其他 RIL 消费者、厂商旁路、旧执行者、另一模块实例）；同域同时只允许一个存活实例 |
| A | §13.2 `#af1-preference-admission` | P5-5 | 两个偏好入口经 A 的同键准入；AF1-13 未签收前不宣称可切换生产 |
| A | §14.1 `#af1-lock-order` | P5-5 | 提交域内不做存储写、Radio 提交、完成回调；获取顺序；T/A 之间的单向规则 |
| A | §14.3 `#af1-close-receipts` | P5-3 | 出具者；逐项可追溯；移交逐项确认、不以数量对消；效果未知即未完成；领取与返回分开；模块级结果 |
| A | front matter、§23 | — | `revision`、`content_review_scope`、修订记录一行 |
| B | §3.2 | P5-2 | `CardRuntime` 不实现 Radio 执行端口、不持屏障 |
| B | §7 `#af1-preference-entry` | P5-5 | 两个入口的三步准入；`SetDefaultRole` 入提交域；指出待改的持锁调用 |
| B | §8 `#af1-radio-sent` | P5-4、P5-3 | `RecordRadioSent`；完成在途登记；`ScheduledRetrySink` 只服务 C1 |
| B | §9 `#af1-radio-plan` | P5-4、P5-1 | `PlanPrimaryRound`、三值提交结果、字段更名；路由表到键集的映射 |
| B | §9 `#af1-barrier` | P5-1 | 唯一屏障实例契约、构造签名、获取顺序 |
| B | §11 `#af1-sim-module` | P5-2、P5-3 | 持有图、绑定顺序、能力切片、关闭步骤、`CloseObligation` / `OwnerCloseReceipt`、工厂范围 |
| B | §19 | — | 一行：补充契约不等于已实现或已批准 |
| C | §4.4 `#af1-device-dir` | P5-1 | 唯一屏障归 device；两种域的归属；`RadioAssignmentView` 的实际位置 |
| C | §5.3 `#af1-assembly-dir` | P5-2 | 组合根、`primary_host_lifecycle.*` 为成员部件、屏障的寿命与事实分开、Host 装配与产品工厂分开 |
| D | §7.5 `#af1-acceptance` | P5-3～P5-5、P5-1 | 四行补充验收断言与附加切换条件；偏好三行入口在处置签收前不切换 |

变更规模（`git diff --stat`）：A +52/−2，B +103/−1，C +21，D +11。C 保持全文 CRLF，因此 `git diff --check` 对其新增行报行尾空白，这是该文件既有约定，不是新问题。

### 12.3 第三轮对前两轮结论的修正

前两轮的原文保留不动，以下列修正为准。

| 被修正的位置 | 原结论 | 修正后 | 原因 |
| --- | --- | --- | --- |
| §8.1 M19、§7.2.5 | 关闭收据是五个计数，`acceptedHandoffs` 可与其他计数对消 | 收据是逐项清单（`CloseObligation`），每项指明原请求或工作项、种类、具体键；移交只能逐项确认，不做数量对消 | 计数无法追溯"哪一个请求的哪一个效果"被谁接走，对消会掩盖未了事项 |
| §7.2.4 第 5 项、§10.1 AF2-02 | `CardRuntime` 的未实现方法补显式拒绝定义，使其可构造 | 按能力切片装配：只装配已有完整实现的能力；未实现的能力不装配、不提供入口、不补桩。同一类混有已实现与未实现端口时，按端口分出已实现部分 | 不为 Host 构建批量实现无关业务方法或桩 |
| §10.1 AF2-02 的完成条件 | `LegacySimFacade` 与 `LegacyCompletion` 进入执行路径 | 作为 AF2-02 的第二步，可后置。`LegacyCompletion` 含未定义的 `CompleteShowNumber`，`SetShowNumberUseCase`、`ActivateSimUseCase` 同样含未定义的虚方法 | 本轮查看类声明后发现与 `CardRuntime` 同类的情况 |
| §9.3 AF1-16 | 只列 `CardRuntime` | 增加上述三个类 | 同上 |
| §3.9 组件图中 `RC -.-> RD` 的 OPEN 标注 | AF1-01 未定 | 已有推荐方案：卡侧命令向 device 的唯一屏障申请（PROPOSED） | 7.1 |
| §10.3 | P5 输入清单 | 已执行，结果见 12.2 | — |

## 第十三部分：AF-2 最终开发输入清单

本部分取代 §10.1。AF-2 的唯一目标是形成可装配、可运行、可调试的核心 Target Host 框架，用真实核心组件完成代表性业务对象和信息流验证。

### 13.1 最小化审查结论

| 审查点 | 结论 |
| --- | --- |
| 是否引入第二套业务实现 | 否。T、A、R·device、O 全部复用现有类。唯一的测试替身是 AF2-01 中用于验证"两个命令族共享屏障"的第二执行适配和 AF2-06 中拒绝接管的接手者，二者只放在 `services/multisim/test/`，不进入模块源码 |
| 是否形成大而全的 DeviceManager | 否。屏障只做准入；`ResourceRadioRuntime` 仍只服务 Radio 命令族；不新增统一设备执行接口 |
| 是否批量补 `CardRuntime` 拒绝桩 | 否，已改为能力切片（12.3） |
| 是否包含产品 SO 拆分或业务迁移 | 否。两个产品工厂保持声明；号码、激活、PIN/EF、本地 Profile 的业务流程不在范围内 |
| 每项是否对应可执行的数据流或负向验证 | 是，见 13.2 最后两列 |
| 并发与回调修改是否有护栏 | 是。AF2-07 拆成四个可单独回归的小步，每步以现有 C0/B/C1 Host 用例结果不变为前提，并各带一条重入用例 |

相对 §10.1 的调整：AF2-02 缩小并分两步；AF2-05 的收据改为逐项；AF2-06 不实现真实接手者；AF2-07 拆步并前置；其余不变。

### 13.2 开发清单

全部保持 `testonly`，不改 `services/sim/`、Core 和根产品 GN。

| 编号 | 必要代码修改 | 解决的架构问题 | 依赖 | 最小 Host 验证 | 完成条件 |
| --- | --- | --- | --- | --- | --- |
| AF2-07 | 分四步：(a) A 的完成回调移出提交锁；(b) A 的 Radio 提交移出提交锁；(c) A 的存储写改为"登记→锁外写→凭原资格接纳"；(d) T 的发现流程转入自己的执行域，失效通知保持短调用。顺带让 `ResourceRadioRuntime` 不在持锁时查询投递端口 | AF1-12、AF1-15、AF1-17 | 无 | 每步后复跑现有 C0/B/C1 用例；新增：平台在提交过程中同步回调、完成回调内再次发起请求、完成回调内请求关闭、发现存储阻塞期间到达失效 | 四步各自提交；重入用例无死锁、无重复完成；既有用例结果不变。(d) 可在 AF2-02 接入卡观察发布之前完成即可 |
| AF2-01 | `ResourceBarrier` 自身保证方法原子；`ResourceRadioRuntime` 借用注入的屏障；删除 `CardRuntime::resources_` | AF1-01 | AF1-01 签收 | 不同键并行；同键后到者在平台提交前被拒；先到者 Unknown 时键保持隔离直至恢复证据 | 模块源码中只有 assembly 构造屏障；既有 B/C1 用例结果不变 |
| AF2-02 | 第一步：`SimModule` 核心图端口化，装配 T、A（共享持有）、屏障、Radio 执行适配、`PrimaryHostLifecycle`，以及卡会话观察发布（把 `CardRuntime` 中已实现的发现/失效发布连同 `SessionRegistry` 分到 `card/state` 的组件）；`SimHostPorts` 窄扩展；testonly 的 Host 装配函数与 Deleter。第二步（可后置）：把 C 的 int 完成出口和 Primary 兼容入口按端口分出并接入 | AF1-02、AF1-16 | AF2-01；AF1-02 签收 | 经 Host 装配完成一次 C0 和一次卡发现到绑定接纳；缺绑定或缺初始资源证据时创建失败并报告阶段 | `SimModule` 能在 Host 可执行目标中构造、运行、销毁；没有新增任何拒绝桩或成功桩；两个产品工厂仍只声明 |
| AF2-03 | `PlanPrimaryRound`、提交返回三值交接结果（`ConfirmedNotSent` / `HandoffAccepted` / `HandoffUnknown`）、`RecordRadioSent`、字段更名；A 去掉合成 `SessionRef` 和目录遍历成员；R·device 按注入路由表得出键集 | SC-02、AF1-03、AF1-04、AF1-06 | AF2-01 | 四端点两栈每阶段两条命令；必需成员无路由时预写之前失败且参数写为零；路由版本在途变化时提交被拒并转入核对；平台返回交接未知时 A 转入待核对，平台提交次数不增加、资格不释放 | A 源码中不再由 `bindingRevision` 构造 `SessionRef`；A 没有任何分支把 `HandoffUnknown` 与 `ConfirmedNotSent` 同等处理；原双卡 B/C1 用例结果不变 |
| AF2-04 | 两个偏好入口经写集准入；按来源 `StepRef` 区分复用与独立；`SetDefaultRole` 入提交域 | SC-03、AF1-05 | AF2-07(c) | 无冲突写一次并释放；父请求持有时复用；C1 持有期间的独立提示不产生直接写，作为 A 的未了事项保留，原调用的返回节点和返回值不变；独立 Voice/SMS 不受阻 | 每个 `WriteUserPrimaryPreference` 调用点之前都有同键资格检查。不引入等待、不丢弃提示、不改变对外结果；AF1-13 的处置不在此项内，相关生产入口不切换 |
| AF2-05 | `CloseObligation` / `OwnerCloseReceipt`；A、T、卡会话组件各一个只读出具方法；A 在 C0 和设备成功后的参数效果未知时保留可出具的记录；完成在途登记；已终态且已交还的记录按规则回收 | SC-01、AF1-07、AF1-14 | AF2-07(a)(c) | C0 的"已写未确认""未写未确认"两种前态、B 设备成功后参数写未知、T 发现存储 Unknown：对应收据各含一项且指明具体键，原调用未被伪交付 | 出具方法只读取已有状态；上述前态下没有路径使该项消失 |
| AF2-06 | `SimModule::StopAcceptingAndDrain`：入口关门与在途登记、复用 `PrimaryHostLifecycle`、汇合收据与屏障状态；私有结果增加"业务责任未了" | SC-01、AF1-02；Flow-3 | AF2-02、AF2-05 | Flow-3 五种情形：C0 原调用在途；存储 Unknown 而 Radio 空闲；Stop 之后 Radio 晚证据被原接收者接纳；重复 Stop 不增加封口、提交和回调次数；测试用接手者拒绝后仍非 Drained。另加跨实例一条（AF1-18）：旧实例的键仍隔离或未 Drained 时创建新实例，新实例的同域键停在 Checking、平台提交次数为零，旧实例取得恢复证据并 Drained 后宿主才出具初始恢复证据 | 六种情形都不提前返回 Drained 或放行；Drained 后销毁无在途回调，且销毁时 A 无其他强持有者 |
| AF2-08 | 架构检查同步（`allowed_edge`、include 规则、源表）；如拆 device 逻辑 target 则同步 `BUILD.gn` | 使 AF2-01～03 的结构约束可检查 | AF2-01～03 | 运行纯规则与带 GN 的架构检查 | 检查通过；对与本批无关的既有失败单独给出证据（见 14.3 的工具适配问题） |

建议次序：AF2-07(a)(b) → AF2-01 → AF2-02 第一步 → AF2-03 → AF2-07(c)、AF2-04 → AF2-05 → AF2-06；AF2-07(d) 在卡观察发布接入前完成；AF2-08 随各项同步。

AF-2 不做：产品 Radio 平台适配；`AttachSimModuleToExisting`、`CreateOwnedSimServices`；号码、激活、PIN/EF、本地 Profile 的业务流程；`CardRuntime` 的完整稳态拆分；真实常驻接手者；任何 WU 的旧写者退出；AF1-13 的冲突处置。

## 第十四部分：P6 独立审查

### 14.1 审查方法与限度

- 以架构负责人视角，对照四篇文档修订后的全文相关章节、本检查点第三、七、八、九部分，以及前两轮已固定的源码事实，逐项回答 12 个问题。
- 执行的轻量检查：四篇文档与本文件中全部 `#af1-` 链接的锚点解析（8 条，全部可达，13 个锚点均有定义）；本文件 6 张 Mermaid 图用 mermaid 10 的解析器做语法解析，全部通过（只验证语法，没有渲染成图）；`git diff --check`；逐文件确认修订为纯追加（A 两行 front matter、B 一格表头除外）。
- 没有运行构建、Host 回归或架构检查脚本。本轮除查看五个类的声明行外没有重读源码。

### 14.2 逐项审查意见

| # | 核查项 | 结论 | 依据与保留意见 |
| --- | --- | --- | --- |
| 1 | 核心对象关系与唯一事实 Owner | 成立 | §3.3 矩阵每行只有一个 Owner。两处需要读者注意的细化已写明：请求记录归发起 Owner 而非 O；屏障的事实归 R·device 而对象寿命归 H |
| 2 | 四篇文档是否相互矛盾 | 新增内容之间无矛盾；发现三处既有的不精确，均不阻塞 | ① B §11 既有表格称 `ResourceRadioRuntime` 的 `dispatch_` "是指针"，`632f6a2a8` 中为 `weak_ptr`，本轮未改该表。② C §4.4 把 `RadioAssignmentView` 列为 device 对象，已用补充说明更正归属。③ B §3.2 稳态树把 Radio 执行放在计划中的 `device/radio/`，而现有执行适配在 `device/resource/`；新增文字统一写"device 的 Radio 执行适配（现为 ResourceRadioRuntime）"，不预设搬迁 |
| 3 | 新增接口是否有真实使用者且必要 | 是，一处保留意见 | `PlanPrimaryRound`、提交返回值、`RecordRadioSent`：使用者 A，实现者 R·device 与 O，直接对应 SC-02。`OwnerCloseReceipt`：使用者 H，出具者 A、T、卡会话组件，对应 SC-01。`SimHostPorts` 的四项扩展：使用者 H 与 R·device。保留意见：`CloseObligation::handedOff` 目前没有生产侧的接手者去置位，AF-2 中只会由测试替身使用。保留它是因为逐项移交是签收要求的形态；在出现真实接手者之前，它恒为 false |
| 4 | 类图的逻辑关联与 C++ 实际持有是否区分 | 已区分 | §8.4 更正了两处。本轮再补一处：§3.8 中 `RadioExecution --> ResourceBarrier` 画的是推荐稳态（借用）；`632f6a2a8` 中 `ResourceRadioRuntime` 按值持有 `barrier_`，是组合。B §11 的持有图明确写了每个对象的持有方式 |
| 5 | Mermaid 语法与接口矩阵一致性 | 语法通过；内容一致，两处以 12.3 为准 | Flow-2 使用的 `PlanPrimaryRound`、`RecordRadioSent` 与 B `#af1-radio-plan`、`#af1-radio-sent` 同名同义。Flow-3 的"仅该项计为已移交"与逐项确认一致。§8.1 M19 的计数形态和 §3.9 的 OPEN 标注已被 12.3 修正 |
| 6 | ResourceBarrier 是否有新的重复权威风险 | 模块内风险已闭合；模块外有三类边界，已写入 A §12.3 第 5 条 | 模块内：只由 assembly 构造，可用结构检查约束。模块外：其他 RIL 消费者与厂商旁路、仍在运行的旧 `services/sim` 执行者、另一个模块实例。其中"旧实例未排空时又建新实例"是把屏障寿命挂在模块上之后新出现的风险，文档已规定同域同时只允许一个存活实例；产品热重建若需要驻留屏障，其执行手段没有设计，登记为 AF1-18 |
| 7 | SimModule 的强引用环与销毁顺序 | 无环；销毁有一个前置条件 | A 由 `SimModule` 共享持有，执行适配、调度器、`PrimaryHostLifecycle` 对 A 只持弱引用。已投递的任务会临时强持有 A 或结果出口，因此只有在 Drained（调度器与 Owner 投递均已清空）之后才可销毁，并在销毁时确认 A 没有其他强持有者；B §11 已写明。成员反序析构时 T 的 `roles_`、卡观察的结果出口会短暂指向已析构对象，由于此时不再有执行，属惰性悬空 |
| 8 | SC-01 的 Unknown 效果能否被关闭收据观察 | 设计上能；代码尚不能 | A §14.3 规定效果未知即未完成并须指明键。`632f6a2a8` 有两处会让该项消失（§9.2 缺口①②），由 AF2-05 修正 |
| 9 | SC-02 是否禁止伪造卡应用 Session | 是 | A §10.1 的 SC-02 原文与 `#af1-identity` 表都禁止；B 给出了不需要 `SessionRef` 的登记形式。代码中四处合成由 AF2-03 删除，其完成条件可用源码检索验证 |
| 10 | SC-03 的同键准入与不同键并发 | 保持 | 两个入口只登记偏好成员，Voice、SMS、独立 Data 的成员互不相交。相交后的处置未定（AF1-13），文档明确在此之前不切换生产入口 |
| 11 | AF1-01～17 的状态与证据是否一致 | 一致，汇总见 15.5 | §9.3 的更新表与本轮修正无冲突；AF1-16 的范围按 12.3 扩大 |
| 12 | AF-2 清单是否足以指导开发 | 足够，但受签收门控 | 每项有修改范围、依赖、验证和完成条件。AF2-01、02、03、05、06 依赖 15.3 中 PROPOSED 项的签收 |

### 14.3 本轮新增登记

| 编号 | 问题 | 证据 | 状态 | 需裁决 | 去向 |
| --- | --- | --- | --- | --- | --- |
| AF1-18 | 屏障寿命随模块实例，产品若要求模块热重建，需要跨实例驻留的屏障或等价机制；当前只有"同域同时一个存活实例"的约束，没有执行手段。【缺证】 | A §12.3 `#af1-resource-admission` 第 5 条；A §14.3 L3/L4 | OPEN，不阻塞 AF-2（Host 不做热重建） | 产品有热重建需求时需要 | G2c；签收矩阵 O2 |
| AF1-19 | 工具适配：`documentation_migration.json` 把 `docs/SIM重构代码目录结构设计方案.md` 绑定到固定摘要。实测该摘要与 HEAD（修订前）的文件已不相等，与修订后的文件也不相等。这是收敛记录 O6 所述的既有不兼容，本轮没有修改摘要或校验规则，也没有运行该校验。其余三篇文档是否被其他规则绑定未逐项核实。【实现】 | `services/multisim/test/cases/architecture/documentation_migration.{py,json}`；收敛记录 O6 | OPEN（工具） | 否 | 测试工程任务，随 AF2-08 处理 |

## 第十五部分：AF-1 签收材料

### 15.1 最终设计摘要

1. **业务事实只有 T 和 A 两个 Owner。** T 管账户身份、资料、绑定和激活进度；A 管默认角色、Primary、切换和偏好。O 只提供请求、步骤和一次完成的机制，记录归发起它的 Owner。
2. **R 分成卡运行和设备执行两部分。** 卡运行签发卡应用会话并承接卡侧协议；设备执行给出 Radio 协议参与者、保存传输关联，并通过唯一的资源屏障管理物理域租约。
3. **六类身份互不替代。** Binding、Session、Step、Protocol Round、Transport、Lease 各有签发者；纯 Radio 步骤不使用卡应用会话。
4. **同一物理域只有一个准入者。** 屏障的事实归 R，对象由 H 创建并注入；Card 与 Radio 的执行适配共享它；`RadioDomainId` 是业务冲突范围，`ResourceKey` 是物理域，映射留在 R 内。
5. **`SimModule` 是唯一组合根和关闭入口。** device 的 Radio 执行适配注入 A；`PrimaryHostLifecycle` 是它的成员部件；只装配有完整实现的能力。
6. **关闭以逐项收据为准。** 各 Owner 出具未了事项清单，H 只汇总；效果未知即未完成；完成的领取与回调返回分开；移交逐项确认。
7. **提交域内不做外部调用。** 存储写、Radio 提交、完成回调都在提交域之外；获取顺序为 Owner → 执行适配 → 屏障。
8. **写权按物理键。** 偏好键的两个入口经 A 的同键准入；不同键保持并行。

### 15.2 四篇权威文档修订清单

见 12.2。四篇文档的状态字段未变：A 仍为 `status: draft`、`approval_status: pending`。

### 15.3 既有决定与 PROPOSED 设计区分表

| 主题 | 既有决定（设计层已收敛；A 整体仍 draft/pending） | 本轮新增（PROPOSED，待架构负责人签收） |
| --- | --- | --- |
| 业务上下文 | T、A 两个业务上下文；O 为机制（A §4.1） | 无 |
| 目录 | 五组一级目录（C） | 无；仅更正 `RadioAssignmentView` 的归属描述 |
| SC-01 关闭责任 | A/O/Scheduler/R/H 各自的收据责任；无接手者则原图留存 | 收据为逐项清单及其类型形态；出具者补全 T 与卡运行；领取与返回分开的登记；模块级结果增加"业务责任未了" |
| SC-02 Radio 身份 | 身份分工、R 提供成员、A 冻结、UPDATE 只认 NOTIFY | `PlanPrimaryRound`、路由观察版本、三值提交结果、`RecordRadioSent`、字段更名 |
| SC-03 写权 | 同键同准入、父资格可复用、独立提示独立登记 | 两个入口的三步准入；`SetDefaultRole` 入提交域 |
| 资源屏障 | 每个物理域一个准入者；一次预留完整资源集；锁外发令（A §12.3） | 屏障自身保证原子并由 H 持寿命、执行适配借用；Card 与 Radio 共享；域映射留在 R 内；同域单存活实例与证明边界 |
| 装配 | assembly 拥有单套对象；构造次序；未绑定不发布（B §11） | 核心图端口化；A 共享持有；Radio 执行适配注入 A；`CardRuntime` 不实现 Radio 端口；`PrimaryHostLifecycle` 为成员部件；能力切片；Host 装配与产品工厂分开 |
| 并发 | 锁内不做 DB/RIL/IPC/回调（A §14.1） | 明确包含存储写、Radio 提交、完成回调；获取顺序；T/A 单向规则 |

### 15.4 剩余 OPEN 与外部责任方

| 事项 | 责任方 | 对 AF-2 的影响 |
| --- | --- | --- |
| 15.3 右列全部 PROPOSED 项的批准 | 架构负责人 | AF2-01、02、03、05、06 的开工条件 |
| AF1-13 偏好冲突处置（延后、等待或暂缓切换） | 架构负责人与 SIM 接口、eSIM 服务、参数服务（收敛记录 O4） | 不阻塞；AF2-04 只验证不旁路。处置签收前不切换这组生产入口 |
| 物理共享域、真实协议成员与路由、命令结束与复用判据 | 协议、厂商、RIL/HDI、产品（O2、O3） | 不阻塞；Host 用保守共享键和测试平台 |
| AF1-18 模块热重建时的驻留屏障 | 架构负责人与宿主（G2c、O2） | 不阻塞 |
| AF1-10 eUICC/Port/Profile 多绑定基数、动态端点来源 | 产品（TGT-OPEN-006、G4） | 不阻塞 |
| AF1-08 `SessionRegistry`、`LoadSession` 的稳态持有组件 | 模块负责人 | AF2-02 只分出已实现的会话观察部分，其余不处理 |
| AF1-19 文档摘要校验的工具适配 | 测试维护者（O6） | 影响 AF2-08 的"检查通过"判定方式 |
| 原出口与逐键核对的产品证据、Provider 与后台写 | 收敛记录 O1、O5 的原责任方 | 不阻塞 |

### 15.5 AF1-01～19 状态汇总

| 编号 | 主题 | 设计状态 | 代码状态（`632f6a2a8`） | 去向 |
| --- | --- | --- | --- | --- |
| AF1-01 | 唯一资源准入 | PROPOSED，待签 | 两个实例，屏障无内部同步 | AF2-01 |
| AF1-02 | SimModule 装配与关闭 | PROPOSED，待签 | 仅构造函数；关闭只声明 | AF2-02、AF2-06 |
| AF1-03 | 合成 SessionRef、目录成员 | SC-02 已定；接口 PROPOSED | 四处合成、一处目录遍历 | AF2-03 |
| AF1-04 | 业务域与物理域映射 | PROPOSED | 固定全集 | AF2-03；真实路由待 O3 |
| AF1-05 | 偏好入口旁路 | SC-03 已定；步骤 PROPOSED | 两处直写 | AF2-04 |
| AF1-06 | 强类型 | 已结论：不加包装，只更名 | 裸整数、位置初始化 | AF2-03 |
| AF1-07 | T 等出具关闭收据 | PROPOSED | 无 | AF2-05、AF2-06 |
| AF1-08 | 卡会话与加载的稳态持有 | OPEN（细化） | 由 `CardRuntime` 持有 | AF-2 之后 |
| AF1-09 | 重试回调的落点 | 已结论：保留，只服务 C1 | 不需改动 | 关闭 |
| AF1-10 | Port/Profile 多绑定 | BLOCKED（外部） | — | G4 |
| AF1-11 | 术语 `SimModulePublication` | 已澄清 | — | 关闭 |
| AF1-12 | A 持锁做外部调用 | 既有原则 + PROPOSED 细化 | 四个方法违反 | AF2-07 |
| AF1-13 | 偏好冲突处置 | OPEN，待产品证据 | — | 签收后 |
| AF1-14 | 设备成功后参数未知时丢失 pending | SC-01 已定 | 存在 | AF2-05 |
| AF1-15 | T 在调用者线程做发现 I/O | PROPOSED | 存在 | AF2-07(d) |
| AF1-16 | 含未定义虚方法的类不可构造 | 能力切片 PROPOSED | `CardRuntime` 等四个类（推断，未构建验证） | AF2-02 |
| AF1-17 | R 持锁查询投递端口 | PROPOSED | 存在，低风险 | AF2-07 |
| AF1-18 | 热重建的驻留屏障 | OPEN | — | G2c |
| AF1-19 | 文档摘要校验适配 | OPEN（工具） | 既有不兼容 | AF2-08 |

### 15.6 设计与源码仍有差距的清单

设计（含 PROPOSED）已写入权威文档、源码尚未跟上的项目。证据位置见 §9.2、§9.3。

| 差距 | 对应设计位置 | 关闭它的 AF-2 项 |
| --- | --- | --- |
| 屏障有两个实例且无内部同步 | A `#af1-resource-admission`、B `#af1-barrier` | AF2-01 |
| `CardRuntime` 声明 Radio 执行端口；`SimModule` 把它接给 A；`SimModule` 无法在 Host 构造 | B §3.2、`#af1-sim-module` | AF2-02 |
| A 合成 `SessionRef`；C1 成员取自目录；资源集固定；无路由版本 | A `#af1-identity`、B `#af1-radio-plan`、`#af1-radio-sent` | AF2-03 |
| 两个偏好入口直写；`SetDefaultRole` 不在提交域内 | A `#af1-preference-admission`、B `#af1-preference-entry` | AF2-04 |
| A 的关闭判据只看 `pendingPrimary_`；两处效果未知后不留记录；无完成在途登记；T、卡运行无收据 | A `#af1-close-receipts`、B `#af1-sim-module` | AF2-05 |
| 没有模块级关闭实现和入口在途登记 | 同上 | AF2-06 |
| A 持锁执行存储写、Radio 提交、完成回调；T 在调用者线程做发现 I/O | A `#af1-lock-order` | AF2-07 |
| `resource_barrier.h` 等源码注释仍写"R CardRuntime 使用" | B `#af1-barrier` | 随 AF2-01 |
| 全部 multisim 目标为 testonly，无产品执行 | 既有状态 | 不属 AF-2 |

### 15.7 AF-2 最终开发输入

见第十三部分。开工前提：15.3 右列对应项取得批准。

### 15.8 结论

`AF1_READY_FOR_ARCHITECT_SIGNOFF`

- 没有阻塞性的设计歧义。AF1-13、AF1-18 和产品证据类事项各自有明确的责任方和缺证时的处理，不影响其余设计。
- 所有新增方案保持 `PROPOSED`；未修改任何批准状态字段。
- 本轮验证限度：文档锚点、Mermaid 语法、空白检查和纯追加核对；没有构建、测试或架构脚本运行；"四个类不可构造"仍是推断。
- 工作区：修改了 A、B、C、D 和本文件共五个文件，全部未暂存、未提交、未推送；`default.profraw`、`opencode.json` 未触碰。

**AF-1 到此停止，未进入 AF-2。** 下一步的精确动作：架构负责人审阅 15.3 右列与 12.2 所列锚点并给出批准或修改意见；批准后按第十三部分的次序从 AF2-07(a)(b) 开始。

## 第十六部分：独立架构评审意见的定向修订

### 16.1 范围

本部分只处理独立架构评审的七条意见，没有重做架构分析，也没有重读源码。基线仍为 HEAD `08d6e6f77`，第三轮的五个文件此前均未提交，本次在其上继续修改。评审的"原则认可"按评审意见记录，不等于正式审批：四篇文档的 `approval_status` 未改，实施方案继续标 `PROPOSED`。

对本文件的处理方式：第一至第十部分已入库，保持原文；第十二、十三、十五部分属第三轮未提交内容，其中与评审意见直接相关的几处已就地更正，下表逐条列出。

### 16.2 独立审查意见—具体修订—待正式批准事项

| # | 独立审查意见 | 具体修订 | 待正式批准事项 |
| --- | --- | --- | --- |
| 1 | AF1-01 原则认可。保留跨实例及域外旁路限制，不宣称产品级全局独占 | A `#af1-resource-admission` 第 5 条增加：新实例的键从 Checking 起步、旧实例未安全退出时宿主不得出具初始恢复证据，并写明"是限制而不是保证，不据此宣称产品级全局独占已经实现"。C `#af1-device-dir` 增加"唯一性限于同一模块实例，不构成产品级全局独占"。D `#af1-acceptance` 资源行原有"不以屏障存在宣称该域独占"保留 | 屏障自身保证原子、H 持寿命、执行适配借用这一实施方案（B `#af1-barrier`）；获取顺序 |
| 2 | AF1-02 原则认可。Host 只装配需要验证的真实核心组件，不批量加拒绝桩 | 无需新增修订。B `#af1-sim-module` 的"能力切片"与本文件 12.3、13.1、AF2-02 已按此表述；AF2-02 的完成条件含"没有新增任何拒绝桩或成功桩" | 核心图端口化、A 共享持有、`PrimaryHostLifecycle` 为成员部件、私有关闭结果增加"业务责任未了"、能力切片的具体分法 |
| 3 | 认可 `PlanPrimaryRound`、路由版本、Radio 专用发送登记；不认可用 bool 表示提交状态，须保留三种语义 | B `#af1-radio-plan`：`SwitchPrimaryRadio` 的返回类型由 `bool` 改为该头文件已有的 `RadioSubmitDisposition`，并增加三值含义与 A 的处理表；明确 `HandoffUnknown` 不得当作未发送，R 在交接前的拒绝归入 `ConfirmedNotSent`。A `#af1-identity` 增加三种语义与"交接未知不得当作未发送"。D `#af1-acceptance` SC-02 行增加相应断言。本文件 AF2-03 的修改内容、验证和完成条件同步更正；12.2、15.3 的措辞同步。§8.1 M06 与 §8.2 中的 `bool` 建议属已入库的第二轮记录，以本行为准 | `PlanPrimaryRound`、`routeRevision`、三值返回、`RecordRadioSent`、字段更名的最终签名 |
| 4 | M19 原则认可。逐请求、逐效果清单；移交逐项确认；完成已领取未返回仍属未完成 | 无需新增修订。A `#af1-close-receipts`、B `#af1-sim-module` 的 `CloseObligation`、D `#af1-acceptance` SC-01 行、本文件 12.3 与 AF2-05 已按此表述。§8.1 M19 的计数形态属已入库的第二轮记录，已由 12.3 修正 | `CloseObligation` / `OwnerCloseReceipt` 的字段类型与各出具方法的签名 |
| 5 | SC-03 原则认可。同键统一准入；锁内不做外部 I/O、Radio 提交、完成回调 | 无需新增修订。A `#af1-lock-order`、`#af1-preference-admission`，B `#af1-preference-entry` 已按此表述；对应 AF2-07、AF2-04 | 两个入口的三步准入实现；AF2-07 四步的具体改法 |
| 6 | AF1-13 保持 OPEN。AF-2 可验证冲突下不旁路；无产品证据前不批准新增等待、丢弃提示或改变对外结果，不切换相关生产入口 | A `#af1-preference-admission`、B `#af1-preference-entry`、D `#af1-acceptance` SC-03 行都增加：Host 只验证不旁路；提示作为 A 的未了事项保留；不引入等待、不丢弃、不改变原调用的返回节点和返回值。本文件 AF2-04 的验证与完成条件同步更正，去掉了"给出可观察的内部结果"这一可能被理解为新对外结果的说法 | 冲突后的处置本身（延后写入、等待或暂缓切换），需产品行为证据（O4）；在此之前相关生产入口不切换 |
| 7 | AF1-18 保持产品门禁。AF-2 Host 验证旧实例未安全退出时新实例不得绕过同域隔离；不扩大为产品热重建保证 | A `#af1-resource-admission` 第 5 条与 B `#af1-barrier` 写明放行机制沿用现有 `InitialRecoveryEvidence`，不新增进程级注册表；A、D 均写明该 Host 断言不等于产品热重建安全，后者属 G2c。本文件 AF2-06 增加第六种情形（跨实例）及相应完成条件 | 产品热重建所需的驻留屏障或等价机制（G2c、O2）；AF-2 不涉及 |

### 16.3 一致性核对

| 核对点 | 结果 |
| --- | --- |
| 三值提交结果在 A、B、D 与 AF2-03 中的表述 | 一致。A、B、D 中不再有以布尔值表示提交状态的建议；本文件仅已入库的 §8.1、§8.2 保留旧建议，并由 16.2 第 3 行覆盖 |
| Flow-2 时序图 | 不需要改图。图中"设备结果 Unknown 或交接未知"分支已把交接未知归入隔离与待核对，与三值语义一致 |
| AF1-13 在 A、B、D 与 AF2-04 中的表述 | 一致：只验证不旁路，提示保留为未了事项，不切换生产入口 |
| AF1-18 在 A、B、D 与 AF2-06 中的表述 | 一致：Host 断言与产品门禁分开 |
| 提示保留为未了事项与关闭收据 | 一致：该项由 A 的收据出具，种类为资格仍被持有或效果待定，受 AF2-05 的形态约束 |
| 新增内容的锚点与空白 | 没有新增或更名锚点；`git diff --check` 仅报 C 的 CRLF 行（21 行，文件既有约定） |

### 16.4 状态更新

| 编号 | 更新后的状态 |
| --- | --- |
| AF1-01 | 原则经独立评审认可；实施方案 `PROPOSED`，待正式批准 |
| AF1-02 | 原则经独立评审认可；实施方案 `PROPOSED`，待正式批准 |
| AF1-03、AF1-04 | 方向经独立评审认可；提交结果改为三值；签名 `PROPOSED` |
| AF1-07（M19） | 原则经独立评审认可；类型形态 `PROPOSED` |
| AF1-05、AF1-12 | 原则经独立评审认可；实现 `PROPOSED` |
| AF1-13 | 保持 OPEN。AF-2 仅验证不旁路；不新增等待、丢弃或对外结果变化；相关生产入口不切换 |
| AF1-18 | 保持产品门禁（G2c）。AF-2 Host 增加跨实例隔离的负向验证 |
| 其余 | 不变，见 15.5 |

AF-1 标记保持 `AF1_READY_FOR_ARCHITECT_SIGNOFF`。

本次验证限度：只做了文本一致性检索、锚点与空白检查；没有运行构建、测试或架构检查脚本，没有重新解析 Mermaid（各图未改）。工作区仍是 A、B、C、D 与本文件五个文件有未提交修改，未暂存、未提交、未推送；`default.profraw`、`opencode.json` 未触碰。

**到此停止，未进入 AF-2 编码。**

## Part 16：H1 架构决策完整性独立审查

**签收意见：暂缓。** T/A 两个业务上下文、按发起者持有请求、按物理域控制发令的分工可以保留。阻止签收的是一个具体的数据丢失反例：连续两轮发现可以留下两笔未决写入，而设计指定的收据来源只能保留该位置最后一笔的完整票据。另有会话失效向 Binding 撤权的缺口。两者均不需要增加业务上下文。详见 Part 18。

**审查基线与定位。** 本地 main 与实查 origin/main 均为 `08d6e6f770fc642eb49b41b511fefd299b6370b1`，恢复工具刷新后为 CHECKPOINT_VERIFIED。起始暂存区为空，工作区有五份已修改文档及未跟踪 default.profraw、opencode.json。本轮读取的是这些文档的工作区版本。前面已有的“第十六部分”是既有定向修订，下文以英文 Part 标识本轮交付。继承 §15 与定向修订 §16.2，不重新执行 P0～P6。

本轮表中的源码相对路径默认以 `telephony_core_service/services/multisim/` 为根；以 `services/`、`docs/` 开头则以 `telephony_core_service/` 为根。A、B、C、D 文档路径见 §2.1；**C（兼容层）**沿用 A §4.1 的正式符号，任务中要求审查的“C 卡侧”在权威设计中是 **R·card**，下表分别列出，R·device 对应设备侧。证据 SOURCE 限于可读的定义和调用；下述交错是按源码推演，未经执行。历史 HOST 仅沿用 `docs/SIM_Refactor_Execution_State.md` 的 current-status、L3.1/L3.2 及其引用报告所记录的切片。PRODUCT、DEVICE 本轮均无新增证据。

### 16-H1.1 事实所有权与写界面

|架构对象|核心职责|拥有的事实|允许修改|对外能力、读者与副本裁决|禁止承担；关闭责任|依据|
|---|---|---|---|---|---|---|
|T|发现、匹配、接纳及撤销|本机 Subscription 身份、两种资料视图及字段版本、位置绑定及空位版本、已接纳观察、激活进度|同一 T 提交域接纳对应票据；存储效果先作为输入|A 读 PrimaryTargetFacts，兼容查询读参与本入口的 view。卡报告、DB 行与 A 冻结的快照均不能直接替换 T 的当前绑定|不判 Radio 成员、角色；T 保留失效写的核对责任并出具明细|SOURCE account/topology_service.cpp:46 ReadPrimaryTarget、:64 DiscoverAccount、:242 AcceptCardSessionInvalidation；DESIGN B §5、§11|
|A|角色决策、冲突准入及业务重试|Voice/SMS/Data preference/effective，Primary/Data 参数投影，按业务域的切换、角色写集、已冻结成员|A 接纳变更，发合格写计划，按原节点确认各部分效果|C 查询 AssignmentView，T 请求角色片段；操作者用 A 的资格。DB 原读源仍可具有与参数不同的旧语义，不用一个缓存覆盖两种读源|不签设备租约或卡会话；继续持有未决偏好、效果、完成责任|SOURCE role_assignment/assignment_service.cpp:132 StartPrimarySwitch、:309 ReadLegacyAssignment、:324 SetDefaultRole、:659 AcceptEntryPrimaryPreference；DESIGN A §13、§14.3|
|O|每个发起者的请求/步骤与领取机制|发起 Owner 私有的 request/step/attempt、终态、迟到证据及完成领取|仅在对应 epoch、Owner、步骤与终态下更新本请求记录|发起者读原上下文；R 投递证据，C 交付；请求中的 Binding、效果观察均无权反写业务对象|不作全局状态中心；发起者对领取后未返回及效果未明逐项负责|SOURCE infrastructure/request/runtime_operation_store.cpp:18 AcceptResult、:34 AcceptLateEvidence、:55 ClaimCompletion；DESIGN A §14.3|
|R·device|协议参与者、发送与物理准入|协议观察、每发 token/serial、全套资源键、fence、结束/可复用证据|执行端登记、屏障全套预留/隔离/恢复；平台只提供观察|A 使用本轮成员及结果副本；发送关联以 R 原登记为准，业务重试由 A 决定|不决定谁任主卡；保留隔离与投递责任；未证设备结束不能销毁接收路径|SOURCE device/resource/resource_radio_runtime.cpp:82 SwitchPrimaryRadio、:190 AcceptDeviceResult、:422 Recover；device/resource/resource_barrier.cpp:51 Acquire、:142 Complete；DESIGN A §12.3|
|R·card（卡侧）|卡访问、卡应用会话及加载|原卡/锁/Records、库存观察、应用代次、期望接续、加载成员资格|卡侧实际接纳点依原会话更新；向 T 发发现或失效观察|T 接受的是观察；A 的绑定版本不构成应用代次。加载缓存服从对应会话和 scope|不认领账户、角色或代替设备证明通道可复用；未了期望由卡侧出具|SOURCE card/runtime/card_runtime.cpp:34 InvalidateVerifiedCardSession、:44 PublishAcceptedCardDiscovery；card/state/session_registry.cpp:42 StillCurrent；card/records/load_session.cpp:35 TryConsumeRead；DESIGN B §9|
|C（兼容层）|旧签名、入口校验与完成交付|原出口路由、订阅/等待句柄的兼容关联|按原节点交付；查询按各入口原读源翻译|宿主回调持有真实出口，C 取用 Owner 投影；等待者退出不判设备静止|不写持久化偏好；封入口后在途交付仍需等返回|SOURCE infrastructure/compatibility/api/legacy_sim_facade.cpp:28 RunPrimarySwitchAtExecutionPoint；DESIGN A §4.1、§14.3|
|I|适配 DB、参数、厂商等外部输入输出|技术句柄、原调用返回及已知写效果|只执行 T/A 批准的字段片段，记录实际返回|T/A 各自接纳，可共享一次 Insert，不能整行回灌；无法确认的效果保持 Unknown|不分配业务资格；未决调用与所属 Owner 配对留存|SOURCE account/account_store_adapter.cpp:38 MatchAccountByIccid、:43 WriteDiscoveredAccount；role_assignment/assignment_store_adapter.cpp 中 AssignmentStoreAdapter；DESIGN B §10.1|
|H|装配、注入、封口与排空|静态目录、实例 epoch、发布与关闭状态；RIL 实例归 Core|只写装配/生命周期状态，通知 T 接纳装配；无权清空 T/A/R|注入只读目录及窄端口；汇总各 Owner 收据，不能据空队列替代收据|无逐项接手确认则保留原图；共享 RIL 由宿主管理|SOURCE infrastructure/assembly/sim_module_factory.cpp:23 SimModule::SimModule、infrastructure/assembly/primary_host_lifecycle.cpp:15 StopAcceptingAndDrain；DESIGN B §11、D #af1-acceptance|

关闭时不把责任默认转给下一实例。M19 允许驻留接收者逐项确认；无接收者即原 Owner 连同依赖存活。此为 DESIGN，现有 Primary 切片的排空判断不能代表整图。

### 16-H1.2 四种依赖与资源并发

|核查面|实际证据与结论|
|---|---|
|编译|SOURCE BUILD.gn:32 assembly_core 依赖各 cases/adapters，:81 assignment_cases 依赖 topology_contracts，:431 topology_cases 依赖 assignment_contracts，两者不互依具体 cases。:334 runtime_core 依赖两域契约。按该文件 source_set/group 的本地 deps/public_deps 提取 26 个节点可拓扑排序；对测试目录外 66 个本地头的显式引号 include 同样可排序。此检查不展开外部宏/产品 GN。|
|运行期调用|SOURCE account/topology_service.cpp:96 DiscoverAccount 向 A 取角色资格，role_assignment/assignment_service.cpp:149 StartPrimarySwitch 向 T 取目标事实。是双向调用；前者释放 T 锁后调用，后者持 A 锁跨 T 查询与外部调用。锁域及回调重入仍受 AF1-12/15/17 约束。PrimaryTopologyPort 返回值拷贝，无 registry 可变引用（role_assignment/primary/primary_topology_port.h，class PrimaryTopologyPort）。|
|C++ 持有|SOURCE infrastructure/assembly/sim_module.h 的 SimModule 成员与 sim_module_factory.cpp:23 构造注入：T/A/runtime 当前按值，T 借 roles_，A 借 topology_/radio_，结果 sink 后绑定，不形成相互强持有。device/resource/resource_radio_runtime.h 的 ResourceRadioRuntime 按值含 barrier_、弱持 sink/dispatch。DESIGN B §11 的 H 持共享屏障、A 共享持有、Card 与 Radio 分离仍是 PROPOSED。|
|事实写入与旁路|SOURCE services/core/src/core_service_sim.cpp:516 SetPrimarySlotId 校验后直写 USER_PREFER_PRIMARY_SLOT；旧 eSIM 路径见 A §13.2/F50。骨架 AssignmentService::AcceptEntryPrimaryPreference / AcceptLocalProfilePreference 两入口也直接调 Store，均跳过写集。SetDefaultRole 有登记但未与 Primary 同一提交锁。路径不同、物理键相同；adapter 不能自行补做业务冲突裁决。另见 Part 18 的同一 Owner 内缓存失配。H 构造内绑定端口，未见以装配权直接覆盖角色或 Binding。|

业务并发同时受 **A 的业务域及写集**、**R 的完整物理资源集**约束。域不同且物理独立，仅在角色/参数写集也不相交时才可并行；两个 Primary 即使走不同 Modem，仍可能共享全局偏好/Data 参数。相同业务域的相斥角色切换仍应被 A 拦截。独立设备动作可以并行，前提是 R 有可信的独立键。SOURCE role_assignment/assignment_aggregate.h:38 RoleWriteMember、device/resource/resource_barrier.cpp:51 Acquire 和 device/resource/resource_radio_runtime.cpp:10 构造时固定 resources_。目录中的 radioDomain、RIL 实例数和 slot 数均不能证明物理独立。

AF1-18 的反证：M1 在共享 RIL 上交接命令，停止等待时仍隔离 K；M2 的独立屏障以 Checking 起步。传入 M2 自身 epoch 和两个 true 后，AcceptInitialRecoveryEvidence 可以放行；它没有读取 M1 的 fence、未决命令或驻留收据。阻止第二次发令的必要条件是宿主不签发无依据的初始证据，并有能接收旧结束证据的 Owner。屏障实例本身证明不了该条件。SOURCE device/resource/resource_barrier.cpp:30 RegisterForInitialCheck、:39 AcceptInitialRecoveryEvidence；DESIGN A §12.3 第5条、§14.3 L3/L4，C 文档 #af1-device-dir、D #af1-acceptance。旧 SIM、厂商直发及其他 RIL 消费者也在本实例控制范围外。G2c/O2 保留，不能由 Host 两实例负例升级为产品保证。

### 16-H1.3 决策状态复核

“原则收敛”取自 §15.3 和既有 §16.2；正式批准以 A §1.3 的记录为准。本轮未发现对下列新增方案的正式批准。表中条件与源码证据是独立维度。

|决策（含 §15.5 编号）|设计状态与理由|SOURCE 或证据缺口；责任方|
|---|---|---|
|T/A 两域、五组目录；06/09/11|既有收敛。位置与账户同属 T；Primary 联动 Data 同属 A；复用机制不取得业务写权。字段更名、C1 专用重试、发布术语沿既有结论|BUILD.gn:81/431 及上表；A §4.1、B §3、C 五组方案。尊重原范围|
|SC-01、07/14、M19|原效果与原完成均需归责；逐项收据类型 PROPOSED。B §11 “来自各自已有的状态”需要修正限定|SOURCE TopologyService::ReconcileDiscovery、AssignmentService::HasPendingDeviceEffects；Part 18 B1，T/A、架构负责人在 AF-1 裁决|
|SC-02、03/04|R 出成员，A 冻结；PlanPrimaryRound、路由版本、三值交接与 RecordRadioSent PROPOSED。交接未知不能冒充未发|SOURCE AssignmentService::StartPrimarySwitch 当前合成 USIM、遍历目录；RIL/厂商真实成员 OPEN O3|
|SC-03、05/13|同物理键同资格；三步登记 PROPOSED。冲突处置 OPEN；Host 可检验不旁路，生产切换 CONDITIONAL 于旧出口/提示行为证据|SOURCE AssignmentService::SetDefaultRole、:659–665 两直写；架构、SIM 接口/eSIM/参数服务 O4|
|01/18|完整资源集单权威原则收敛；实例内屏障构造、共享和同步 PROPOSED；跨实例复用 CONDITIONAL 于宿主恢复证据|SOURCE ResourceRadioRuntime::Initialize、ResourceBarrier::Acquire；宿主/设备方 G2c、O2|
|02/08/16|SimModule 单装配、关闭；按能力切片、持有方式 PROPOSED。SessionRegistry/LoadSession 稳态成员 OPEN|SOURCE SimModule::SimModule、CardRuntime 构造；未做链接试验。模块负责人；产品工厂另验|
|10|多绑定、动态端点及 MEP 基数 OPEN；没有可信映射时不补默认 Port|SOURCE card/esim/profile_requests.h:26 VerifiedPortRoute；产品/66250 G4|
|12/15/17|锁内无外部调用为既有原则，锁序、投递和执行改法 PROPOSED|SOURCE TopologyService::DiscoverAccount、AssignmentService::StartPrimarySwitch、ResourceRadioRuntime::OwnerCallbacksDrained；Owner 实施前复核|
|19|文档摘要工具 OPEN，历史证据用途不变|既有 §14.3；services/multisim/test/cases/architecture/documentation_migration.json 及同名 .py；维护者 AF2-08。本轮未执行或修改该规则|

四篇对三值交接、偏好门禁和跨实例限制的修订以既有 §16.2 为准；目录归属不授权写权。新增风险集中在 M19 可枚举性、失效证据到绑定的因果关联和 A 内两种投影的版本联系。架构原则有依据，但 §15.8 的“没有阻塞性的设计歧义”不足以覆盖下述反例。

## Part 17：H2 核心对象模型独立审查

### 17.1 对象清单

表内类、成员和方法是 SOURCE 定位；无对等类的语义对象以 DESIGN A §3.1～3.2、§4.1.1、§17 为依据，不能由声明推断产品已实现。属性、身份引用和投影不新增独立实体。

|对象 / 类别|业务含义及唯一 Owner|核心属性与身份|生命周期|当前源码映射|
|---|---|---|---|---|
|Subscription / 身份|T 接纳的本机订阅身份，provider 分配|SubscriptionId 包装既有 simId；ICCID 是匹配证据|可跨解绑保留；删除/重建、跨设备连续性 OPEN|infrastructure/sim_identity.h:29 SubscriptionId；account/topology_service.cpp:76 DiscoverAccount|
|Account / 业务实体|T 管理该身份的资料|subscription、iccid、identityRevision；current/historical 各自可缺、有 active 和字段版本|发现或重载接纳；成员移除与字段变更分别推进水位|account/account_registry.h:30 AccountProjection、:41 AccountRecord；account/account_registry.cpp 中 AcceptReload|
|Binding / 限时业务关系|T 接纳某位置现在关联哪个订阅|BindingRef = subscription、slot、bindingRevision；不含 Session 或 epoch|建/撤均升位置版本，空位保留水位；重建仅在新图内解释|account/binding_index.cpp:59 AcceptBinding、RemoveBinding；infrastructure/sim_identity.h:58 BindingRef|
|Card / 载体|R·card 管本端原始状态；代理端只拥有本端观察|载体实例、卡应用、可缺的 ICCID；CardId 依提供方解释|插拔、来源重连、替换分别使相关会话失效|card/runtime/card_runtime.h:48 ProductionSlotBindings 借原生产对象；无统一 CardId 实体|
|Physical Port / 硬件接入点|产品提供物理事实，H 管静态配置，R 管已验证当前路由|物理位置、eUICC 域内逻辑 Port、旧逻辑 slot 三者可不同；无统一整数键|接线随配置；动态路由随实证更新|infrastructure/assembly/slot_catalog.h:29 SlotDescriptor；card/esim/profile_requests.h:26 VerifiedPortRoute|
|eUICC / 卡载体；Profile / 库存项|R·card 管库存；高层任务属外部服务|eUICC 作用域内的 Profile 身份、启停、可选 Port；请求的 iccid/port 是意图|库存中的禁用 Profile 可长期存在而无会话、账户绑定|services/sim/src/esim_file.cpp:737 GetEsimPortIndex、:2853 ProcessSwitchToProfile；card/esim/profile_requests.h:31 SwitchProfileRequest；A §17.1/F49|
|vSIM / 端点观察及外部会话|厂商拥有启停/业务会话；R 管访问实例，T 决定是否有本机账户|端点种类、来源、代次；隐藏槽不等于订阅身份|启停、重连后重新验证；无身份依据只给兼容视图|infrastructure/assembly/slot_catalog.h:27 EndpointKind；services/sim/src/multi_sim_controller.cpp:1480 IsSetPrimarySlotIdAllowed；DESIGN A §3.2|
|Device / 执行基础设施|Core 持共享 RIL；R·device 管 SIM 发令与效果|RIL 实例/允许接线、真实协议路由、ResourceKey 分别定域|宿主和硬件活动可长于模块实例|card/runtime/card_runtime.h 的 sharedRil_；device/resource/resource_radio_runtime.cpp:82 SwitchPrimaryRadio|
|Role / 策略属性|A 管默认角色选择|Voice/SMS/Data 的 preference、effective 指向可空 Subscription；Primary 有位置和参数语义|随策略、存储读源、切换阶段而变|role_assignment/role_selection.h 的 RoleSelection；role_assignment/assignment_service.cpp:309 ReadLegacyAssignment|
|Assignment / 聚合及一次变更|A 的稳定选择与一次切换分开|AssignmentAggregate 的角色、修订、写集；AssignmentTransition 的 domain、资格、阶段；PendingPrimarySwitch 的冻结目标、步骤/轮次|投影可重载；一次变化至终态或核对|role_assignment/assignment_aggregate.h:38 RoleWriteMember、:51 AssignmentAggregate；role_assignment/assignment_service.cpp:132 StartPrimarySwitch|
|AssignmentView / 只读投影|A 汇集旧读口需要的结果|lastPrimary、lastData、进行中标志、assignmentRevision|返回时的快照；不是新准入依据|role_assignment/assignment_service.cpp:309 ReadLegacyAssignment|
|CardApplication Session / 运行实例|R·card 签发卡应用访问资格|SessionRef = epoch、slot+applicationKey、bindingGeneration|按应用替换、路由失效、epoch 终止；可信期望接续另验|card/state/session_registry.cpp:42 StillCurrent、:131 AcceptExpectedContinuation|
|LoadSession / 加载轮|R·card 拥有 Records 某范围的成员|session、scope、recordsGeneration、member token|一轮加载或局部 Refresh；失效的读不得写新记录|card/records/load_session.cpp:23 构造、:35 TryConsumeRead、:54 InvalidateAffectedReadMembers|
|RuntimeOperation / 运行记录|发起者拥有，O 提供容器|上下文、原目标、步骤、效果、等待/终态、一次领取|请求接纳至责任完成及证据保留结束|infrastructure/request/request_context.h:40 RequestContext、:48 RuntimeOperation；runtime_operation_store.cpp 的 AcceptLateEvidence|
|Request / 请求身份；Step / 可执行尝试|同一发起者|RequestId = epoch、owner、value；StepRef 另加 step/attempt；可有父步|请求可跨多步；旧步不再推进，新步仍需单独授权|infrastructure/request/request_ref.h 的 RequestId、StepRef；role_assignment/assignment_service.cpp:389 AdvanceStep 调用|
|ProtocolSession / 协议轮；Transport / 单发关联|A 保存本轮本地关联；R 拥有原生协议及传输事实|本地 protocolSession、成员/phase；一次发送的 token、可选 serial 及 fence|一轮可多成员、多发送；迟到数据仍属原发送|role_assignment/primary/radio_execution_port.h:35 RadioSwitchRequest、:55 RadioDeviceResult；device/resource/resource_radio_runtime.h 的 Group、Transmission|
|ResourceLease / 设备执行资格|R·device 在完整键集上签发|epoch、holder StepRef、keys、fence、效果要求|未发可凭证撤销；已发需结束/复用证据，否则隔离|device/resource/resource_barrier.h:31 ResourceKey、:35 ResourceLease；resource_barrier.cpp:142 Complete、:171 AcceptRecoveryEvidence|

### 17.2 逻辑关系与 C++ 持有对照

|起点 → 终点|关系与基数|业务判定；实际持有/数据来源|
|---|---|---|
|Account → 两种资料视图|Composition，各 0..1；读取结果为 Projection|AccountRecord 按值含两个 optional；并存合法。“Historical”是历史/全量读源，不能简单解释为互斥的已注销状态。SOURCE account/account_registry.cpp 的 ReadProjection、AcceptReload|
|Account → Subscription；Binding → Subscription/位置|Reference，每账户一个本机身份；Association，每个当前关系连一身份与一位置|注册表按身份存记录，BindingIndex 按位置存 optional 引用，不拥有卡或订阅的生命周期。空位版本与账户字段版本独立。SOURCE account/binding_index.cpp 的 AcceptBinding、RemoveBinding|
|Card/Profile → Account；Session → Binding|Association，观察经接纳才成立，允许 0；BindingRef 为 Reference|通过卡观察、ICCID 匹配、provider 返回身份；一个账户可历经多次绑定。T 需能判别哪次接纳源于哪轮观察；现有索引没有存原 Session，见 M1。SOURCE TopologyService::DiscoverAccount、BindingIndex::AcceptBinding|
|eUICC → Profile；eUICC → 逻辑 Port；Port → 位置|Composition（本卡库存条目作用域）；Association（可提供多个 Port）；Association（经核实路由）|语义包含不意味着 SimModule 拥有硬件。SwitchProfileRequest 仅按值含可选 VerifiedPortRoute 和原端口参数。实际并发基数 OPEN，不能按一个 slot 反推一个物理卡槽。DESIGN A §17.1；SOURCE card/esim/profile_requests.h:26|
|Device/代理 → 同一卡；vSIM → Profile|Association（多来源观察）；Association（产品允许时外部级联或互斥）|不按相同 ICCID 合并不同设备账户；没有 vSIM 永久拥有 Profile 的组合关系。DESIGN A §3.2、§14；SOURCE SlotDescriptor、IsSetPrimarySlotIdAllowed 只证明本机种类和分支|
|RoleSelection → Subscription；Primary → Slot|Reference，默认角色各 0..1；Primary 参数为位置 Projection|角色可共指同一订阅；Primary 变更冻结 Binding 及 ICCID，不以卡 ID 充当全部角色的键。SOURCE RoleSelection、AssignmentService::StartPrimarySwitch|
|Assignment → 角色及切换记录；Assignment → AssignmentView|Composition（A 内的记录）；Projection（查询）|AssignmentService 按值持聚合及 pending 容器，View 是独立拷贝；存储快照不能反过来提交业务决策。SOURCE role_assignment/assignment_service.h 的 AssignmentService、.cpp:309|
|Request → Step；子请求 → 父 Step；Step → Binding/Session|Composition（运行记录内当前步骤）；Reference；Reference（按步骤可空）|RequestId 是值，不拥有 C++ Step 对象；StepRef 内嵌 RequestId。RuntimeOperation 持上下文；Radio 可有目标 Binding，无需卡应用 Session。SOURCE request_ref.h、request_context.h；DESIGN A §10.1 SC-02|
|CardApplication → Session → LoadSession|Association（当前访问实例）；Composition（加载资格受该会话寿命限制）|C++ SessionRegistry 与 loads_ 均是 CardRuntime 成员，LoadSession 自持 SessionRef；无 Session→LoadSession 强指针。SOURCE card/runtime/card_runtime.h:103、card/records/load_session.cpp:23|
|Step → 协议轮 → 多次发送；Transport → Lease|Association；Association；Reference|A 持冻结计划；R 的 Transmission 存 request 副本和 fence，Group 持 lease 与成员结果。一个 serial 不足以定位业务步骤。SOURCE device/resource/resource_radio_runtime.h 的 Group/Transmission 与 .cpp:190|
|Lease → 资源键；屏障 → 租约登记；H → 执行图|Reference（全套键）；Composition；Composition（模块组件）、Reference（外部 RIL）|持有 Lease 值的副本不能释放任意键；屏障核对原登记。H 持对象不取得其业务写权。SOURCE ResourceBarrier::MatchesAll、SimModule::SimModule；目标和现状差异见 Part 16|

### 17.3 四条实际调用链的反证

|链|实际读写顺序及 ID 的作用|结论|
|---|---|---|
|SetPrimarySlotId|SOURCE services/core/src/core_service_sim.cpp:516 按 slot 查能力、在位、active，先写偏好，再走 services/sim/src/sim_manager.cpp:469、services/sim/src/multi_sim_controller.cpp:1494；按 slot 分厂商、专用 RIL、现有 Modem、协议分支。目标骨架的 LegacySimFacade::RunPrimarySwitchAtExecutionPoint 接受已构造 command，AssignmentService::StartPrimarySwitch 按 slot 取目录/T 事实，对 expectedBinding、ICCID、active 二读后冻结目标|生产链仍由位置连接多次读取，不能当作对象中心已接管。目标的 slot 查找有边界目的；二读的有效期止于其提交点。A 锁不锁住 T。|
|Account Discovery|SOURCE services/sim/src/multi_sim_controller.cpp:425 InitIccId 由 slot 找 File、取 ICCID、查 DB 并更新或新增。目标 CardRuntime::PublishAcceptedCardDiscovery → TopologyService::DiscoverAccount → BeginStore → Store 匹配 → A 角色预留 → Store 写 → T/A 各自接纳；新 simId 可在 Insert 返回前未知|ICCID 是查找条件，存储返回是身份输入，Session/sequence/admission 各有独立作用。局部已对象化；重叠两笔效果却按位置归档会重新引入隐式相等（B1）。|
|Binding 建立/变更|SOURCE account/discovery/discovery_observation_ledger.cpp 的 MatchesStore/CompleteStored 按原观察、admission、位置 revision 复核；BindingIndex::AcceptBinding 查该 slot 和其他已接纳位置；RemoveBinding 必须匹配原三元组。失效观察走 TopologyService::AcceptCardSessionInvalidation，仅改观察台账|正常接纳/显式撤销已由业务关系驱动。缺少从原 Session 到已接纳 Binding 的撤销闭环；不能用“此时 slot 上是哪张卡”补推原关系。|
|Primary B/C1 Radio|SOURCE AssignmentService::StartPrimarySwitch 专用不重启 B 保存 pending 并发单成员；C1 预写参数、遍历目录冻结成员。AcceptRadioStep:357 验成员、阶段、UPDATE 的 NOTIFY；:385 继续下一轮；:467 终端再读 T。ResourceRadioRuntime::AcceptDeviceResult 按 token、可选 serial、原请求、fence 定位原 Group|Step/Transport/Lease 已提供显式关联；目录仍代替真实成员，四处以绑定版本合成 USIM 会话，SC-02 只有部分实现。轮次继续、失败回滚与最终新角色提交的资格不同，不能据最后一次复核证明所有阶段。|

### 17.4 生命周期、失效与跨形态承载

|场景|对象仍存在|仍有效的事实|仍可执行的操作|未完成责任|
|---|---|---|---|---|
|拔卡、相同 ICCID 再插|历史资料、旧请求可留存|旧代次只解释旧观察；新会话和空位置以后的 revision 分别签发|新请求凭新绑定；读取历史资料不授权访问卡|已发写库/设备操作仍归旧 T/R 与发起者。SOURCE BindingIndex::RemoveBinding、SessionRegistry::InvalidateForVerifiedReplacement；撤销缺口见 M1|
|Profile 禁用、另一 Profile 启用|eUICC、库存、历史订阅可留存|请求端口字段不证明路由；可信新会话可重新发现，active 字段不等于绑定或卡就绪|仅原期望接续条件满足时可续步；A 另行判偏好/角色|外部 66250 管任务，卡侧管通道效果，T 管发现写；本地返回不释放租约。DESIGN A §10、§12.3；SOURCE SwitchProfileRequest、SessionRegistry::AcceptExpectedContinuation|
|等待超时、设备迟到|原记录、租约或隔离可留存|原 token/step 对原效果仍有意义|只补原效果认知；无新请求资格或第二次完成|A 留业务部分效果，R 留结束/复用责任，M19 逐项直到返回或接手。SOURCE AssignmentService::AcceptLateRadioEvidence、ResourceBarrier::AcceptRecoveryEvidence|
|模块重建、共享端点重连|旧平台动作可活过旧图；同一数值 ID 可再出现|epoch 隔离请求/会话；BindingRef 自身无 epoch，外带引用必须受原图入口/请求寿命限制|新模块不得用旧证据开新屏障；T 仅接纳新来源的可信绑定|无接手者保留旧图；静默死亡的证据、检测窗口 OPEN。DESIGN A §10、§14.3；SOURCE ResourceBarrier::AcceptInitialRecoveryEvidence、infrastructure/sim_identity.h:53|

扩展判断：共享卡在助理机上的事实与受益机代理实例可用 Association 表达，两个设备各自接纳本地订阅；同一模块内观测两个端点不授权同时认领一个当前订阅。eUICC 多库存、一张卡多应用、多个角色共用一订阅均能表达。多个同时启用的 Port 若映射到同一逻辑 slot，当前 BindingIndex 的键容不下多个 current；若映射到不同 slot 则无此基数冲突。vSIM 与 Profile 的级联、互斥属于外部产品策略，不能由事件相邻、共享偏好参数或相同 ICCID 推断。Legacy 的 IsSetPrimarySlotIdAllowed 还依赖厂商状态和 Modem 数，且专用 RIL 分支先于该检查。现有模型可以分派这些责任，尚不能据此签收真实动态端点来源、跨服务互斥、角色可选性或物理并发。DESIGN A §3.2、§17.1、§21 的 OPEN-006/007/011；SOURCE services/sim/src/multi_sim_controller.cpp:1480–1511、services/sim/src/esim_file.cpp:737/2853、card/esim/profile_requests.h:26。产品、厂商和服务方在 G4 提供端点/路由/故障轨迹。

## Part 18：H1/H2 反例与缺陷清单

### 18.1 十项指定反例的处理结论

“已有保护”指列出的方法体及其前提。关闭一项须由表内责任方满足相应判据；同一缺陷可影响多个场景。

|反例|预期裁决、Owner 与资格|现有保护及反证边界|归类、责任与去向|
|---|---|---|---|
|1 同一 Account 同时被两个 Binding 接纳|T 在本机同一索引内拒绝不同位置认领同一 Subscription，移动先撤旧|SOURCE account/binding_index.cpp 的 AcceptBinding 检查目标位置装配/版本、遍历其他位置；原位置同订阅重复是幂等。TopologyService::DiscoverAccount 在 T 锁内最终接纳。该结论限于同一 T 实例|单索引规则成立；T 保持，产品多绑定基数另走 AF1-10|
|2 两个物理端点认领一个 Subscription|R 供带来源的观察，T 对 ICCID/已匹配身份及位置联合接纳；多个设备各自观察不等于两条本地当前绑定|SOURCE DiscoveryObservationLedger::BeginStore 用 activeIccids_ 拦不同应用位置的同 ICCID 在途；TopologyService::DiscoverAccount:85 查其他绑定，BindingIndex 有最终检查。若两物理来源折叠成同一 slot/applicationKey，索引无法辨认独立端点；相同 ICCID 也不是物理唯一证明|本机不同逻辑位置有保护；物理来源、别名及多 Port 为 OPEN，T/产品 G4|
|3 Binding 更新后旧 Request 提交角色变化|A 以原 epoch/step、原 Binding、A 写集并复核 T 当前目标；无新角色提交资格|SOURCE AssignmentService::AcceptRadioStep:467 比较订阅、位置、revision、ICCID。显式解绑且在该读前完成可拒；T 锁在返回时释放，外部写入前后仍可变。C1 的 :385–413 在终端复核前即可发后继阶段；失败恢复须区分旧责任与新决策|部分保护；M1/M2，T/A 在 AF-1 明确接纳点，不能把二读当跨域锁|
|4 旧 Session 回调修改新 Card|R·card 在原状态写点查 epoch/application/generation；Records 同时查 scope/generation/member|SOURCE SessionRegistry::StillCurrent、LoadSession::TryConsumeRead。后者仅与它自己持有的会话比较；若还持旧 LoadSession，消费通过也不等于允许写新 Records。card/runtime/card_runtime.cpp 的 BeginRecordsLoad 说明仍为注释|纯规则已有，真实 Records/EF 写点为实现缺口，R·card 在 G3 对换卡、局部刷新分别验证|
|5 Primary 与 Data 同改一份偏好/联动事实|先辨认物理键。Primary 与独立 Data 重叠的是 Data 参数/通知；入口提示与 Profile 提示才直接冲突于 USER_PREFER_PRIMARY_SLOT。A 只对实际交集统一准入|SOURCE role_assignment/assignment_aggregate.h:38–50 位集；AssignmentService::SetDefaultRole:324 有准入但无 Primary 提交锁；:659–665 两个偏好提示直写。不能把 Voice/SMS 或所有 Data 提示一律串行|SC-03 原则充分；AF1-05/12 实现缺口。AF1-13 OPEN，由 A/服务方在迁入口前决定冲突后的兼容行为|
|6 RadioDomainId 不同、ResourceKey 相同|A 可有独立业务资格；R 须对相同物理键只准一个发令者|SOURCE ResourceBarrier::Acquire 只认全套键的 Available；同一受串行保护的屏障内可拒第二次。CardRuntime::resources_ 与 ResourceRadioRuntime::barrier_ 各自登记则互不相见|AF1-01，R/H 的 AF-2 实现缺口；外部旁路仍属 O2|
|7 RadioDomainId 相同、物理资源独立|若同域两次角色变化相斥由 A 拦；若是无业务冲突的独立命令，R 不应仅因业务域同名而互斥|SOURCE AssignmentAggregate 的 AdmitTransitionWithRoleWriteSet 与 ResourceBarrier::Acquire 的两个判断不同。ResourceRadioRuntime::SwitchPrimaryRadio 使用构造时固定 resources_，不能证明按命令取得精确路由|AF1-04，R/协议方；Host 条件验证与产品独立域证据分别交付|
|8 重建后两个屏障|H 禁止新实例越过旧隔离，R 新键停 Checking；Core 的 RIL 可继续执行旧命令|SOURCE services/core/src/core_service.cpp:101–123 同一 telRilManager_ 分给 eSIM/SIM/搜网；ResourceBarrier::AcceptInitialRecoveryEvidence 仅检本实例状态及来证；详见 Part 16 的 M1/M2 实例时序（此处 M1/M2 指模块实例）|AF1-18 OPEN；宿主、RIL/厂商 G2c 证明跨实例证据来源/旧执行权退出。无 PRODUCT/DEVICE 证明|
|9 历史 Account 误投影为当前|T 独立 view/attempt/成员/字段水位；当前可操作还须绑定与适用 active|SOURCE AccountRegistry::ReadProjection:66、BeginReload:262、AcceptReload:272 分 view；TopologyService::ReadPrimaryTarget:46 要求 current。反例：ReconcileDiscovery:223–229 在缺记录时由一次 ICCID 匹配同时创建两个投影，未检查当前观察或 legacyPosition 是否仍有效。该路径可暴露不当 current，不能单靠查询区分 view 防住错误写入|T 的正常重载已有保护；恢复入口属 M1/B1，AF-1 补明读源资格，AF-2 才可实施|
|10 Account 失效后迟到设备证据改当前 Assignment|R 验原 token/fence，A 验原步/终态；仅可认领原效果，不复活完成|SOURCE ResourceRadioRuntime::AcceptDeviceResult:190、AssignmentService::AcceptLateRadioEvidence:539 分开接纳；若仍在 Open，正常结果走 :467，检查 Binding/ICCID 而未比较 active。T 已删除 current 时会拒；只有 active 已变、或失效尚未撤绑定时，仍可能走成功提交。C1 的旧参数恢复另有旧节点语义|终态后的迟到路径有保护；正常回包与资格变化见 M1/M2。是否因 active 变化改变旧出口须由 A/产品裁决|

### 18.2 按严重程度登记

**BLOCKER B1 — T 的发现责任不能从现有状态逐笔恢复。** 在同一 slot 及 applicationKey 上依次观察卡 X、Y，二者已有不同本机订阅、不同 ICCID。使 X 的已发 DB 写返回 Unknown，尚未完成核对；更换卡后 Y 的写也返回 Unknown。第二次匹配不被第一笔 A 资格挡住：AccountRoleProtocol::Prepare 对已有身份按 subscription 预留，仅未知身份才按位置互斥。T 的两次写均执行 `pendingDiscoveries_.insert_or_assign({slot, applicationKey}, ...)`。第二笔替换第一笔的 ticket、rolePlan、stored；观察台账也按该位置替换。activeIccids_ 或 A 中残留的预留可以说明“还有占用”，却还原不了原 admission、原存储效果及二者的配对。两个 Unknown 不需要同时在线程中发生，顺序换卡即可构造条件。

SOURCE account/topology_service.cpp:118–185 DiscoverAccount、:188–240 ReconcileDiscovery，account/topology_service.h 的 PendingDiscovery 与 pendingDiscoveries_；account/discovery/discovery_observation_ledger.cpp 的 AcceptDiscovery、BeginStore、ReconcileStored；role_assignment/account_role_protocol.h:28 Prepare。另一个窗口是慢核对在锁外进行，返回后忽略 ReconcileStored 的判定，并按位置 erase；原项与当前项可能已不是同一项。以上是可达控制流推演，未声称观察到产品丢单。

**签收影响与关闭判据。** DESIGN B §11 指定从现有 pending/台账出具 M19；在上述输入上无法列全 X、Y 两项。问题在可归责信息的基数，给收据增加字符串字段也无法恢复已丢的数据。T 负责人和架构负责人须在 AF-1 明确每笔未结效果在观察被替换后仍可按原资格定位，结束/移交只消费对应项；定义核对返回时资格失效的处理。证明可以是两项并存，或既有策略确有充分依据禁止第二项形成。签收例须逐笔定位 X/Y，结清旧项后新项仍在，且不产生对新卡或原出口的第二次副作用。具体容器与实现留后续。

**MAJOR M1 — 缺少从卡源失效到已接纳关系的可核查撤权条件。** T 的“观察失效”和“位置解绑”有独立入口。R 发现替换会走前者，后者需要 BindingRef。前者只执行 AcceptInvalidation，ReadPrimaryTarget 仍可得到旧 active 账户和旧 Binding；同位置同订阅再次发现还可能被 AcceptBinding 按幂等处理而不升版本。反方向，简单按 slot 解绑又会误删已接手的新应用/绑定。SOURCE card/runtime/card_runtime.cpp:34–54、account/topology_service.cpp:242–256、account/binding_index.cpp 的 AcceptBinding；test/cases/target/primary/c0_topology_integration_test.cpp:606、:626 由夹具直接调用 AcceptCardRemoval。DESIGN A §6.5.2、B §5 写明期望效果，但缺少该回调路径可用的“原接纳来自哪次观察”的依据。T/R·card 在 AF-1 明确可信移除、仅应用刷新、同卡再插分别撤销哪项；用 R 真实观察入口推演，不以手工先解绑完成验收。核对旧 DB 写不能顺便把旧观察重新发布为 current（反例 9）。

**MAJOR M2 — 对 A 来说，“证据属于原操作”与“仍可提交当前角色”的时间点没有完全闭合。** B 模式冻结 active 账户后，T 可在回包前仅更新 active 而保留 Binding。AcceptRadioStep 的 bindingCurrent 比较不含 active，且复核与后续锁外存储之间没有跨 Owner 的持续资格。C1 的下一轮发令和恢复旧参数又是两个不同效果节点。SOURCE account/topology_service.cpp:268 AcceptActivationProjection，role_assignment/assignment_service.cpp:149、:161、:385–413、:420、:467–501、:509–522；DESIGN B 的 C0 拓扑集成说明（含“二读后失效”）已提示窗口。A/T 在 AF-1 明确哪些条件只在接纳时取样，哪些在下一发令、业务提交、原效果核对时必须仍成立，并列明失效后的已写部分。需保留兼容出口差分，不能自行规定一律回滚。属于签收语义缺口；已有末端检查不覆盖过程。

**MINOR N1 — A 内部角色重载的版本联动在成功 setter 路径未落实。** 先取 Current 角色读取票据，再使 SetDefaultRole 的 store 返回成功，随后提交旧读取。setter 更新 AssignmentAggregate，未调用 AccountRoleProtocol::NoteLocalRoleMutation；AcceptReloadSet 仍可能按旧版本接纳过时行。它污染的是 A 的旧读源投影，不能据此断言已覆盖聚合或生产 DB。SOURCE role_assignment/assignment_service.cpp:324–336、:687–697 与方法后关于 NoteLocalRoleMutation 的注释；role_assignment/account_role_protocol.h:68–97。当前具体 AssignmentStoreAdapter::WriteDefaultRole 在 role_assignment/assignment_store_adapter.cpp:91 返回失败，故触发依赖能成功的执行端口。既有设计已区分 Current/Historical 更新；A 在 AF-2 对成功 setter 与 Current 重载的交错复核，独立 Historical 继续按自身读源。

**OPEN — 外部条件不合并为架构签收。**

|门禁/具体反例|缺少的证据|责任方与阶段|
|---|---|---|
|AF1-18/O2：旧 RIL 命令未结束而新模块收到可复用证明|跨实例、其他 RIL 消费者、厂商发令者覆盖；原始结束/资源复用的 PRODUCT、DEVICE。SOURCE ResourceBarrier::AcceptInitialRecoveryEvidence；DESIGN A §12.3/14.3|宿主/厂商/RIL，G2c 及相应 WU|
|AF1-04/10：两个 Port 同 slot，或两个业务域同一 Modem|物理映射、成员来源、来源失效、库存与读回的对应。SOURCE services/sim/src/esim_file.cpp:737/783/837（请求 getter、可选库存标签、请求赋值）；DESIGN A §17.1|产品、协议、66250、共享卡/vSIM 提供方，G4 与匹配产品切片|
|AF1-13：eSIM 已返回，但其偏好提示与 Primary 持有相同键|冲突时原返回、时序及结果；DESIGN B #af1-preference-entry、D #af1-acceptance；SOURCE AssignmentService::AcceptLocalProfilePreference|SIM 接口、参数服务、eSIM/产品与 A，生产写权切换前|
|AF1-19：修改设计后固定摘要校验仍不同|见既有 §14.3 与 §15.4 的工具边界，未以新摘要替换旧证据|测试维护者，AF2-08|

## Part 19：下一轮 H3 交接

**本轮状态：`AF1_HARDENING_H1_H2_PARTIAL`。** H1/H2 的审查工作与十项反例分析已完成。B1 阻止以现有收据来源签收；M1/M2 需要 AF-1 明确资格与生效节点；N1 属实现核对。§15.3 和前一轮 §16.4 的 PROPOSED 保持原审批状态。此结论不授权框架开发。

**H3 的第一步（待下一轮授权）。** 核对 HEAD、工作区中本检查点及 A～D 后，先针对 B1 在纸面沿两个不同已匹配订阅复用同一位置的发现链，逐行填写“原 ticket → T 未决项 → A reservation → Store 效果 → 收据 subject → 精确终结/移交”。保留第二项后再核对第一项；若一个接口只能按当前位置返回或清除，记录无法完成的那一步，交 T 与架构负责人裁决，再审其余接口。以下路径均沿用 Part 16 的路径根。

|优先接口|精确源码位置|H3 必须回答的问题|
|---|---|---|
|DiscoverAccount / ReconcileDiscovery / ReconcileStored / FinalizeInitialRoleReservation|account/topology_service.cpp:64、:188；account/topology_service.h 的 PendingDiscovery；account/discovery/discovery_observation_ledger.cpp 的 ReconcileStored；role_assignment/account_role_protocol.h:28–66|谁保存跨代未结责任，谁验证最终移除的确是原项，哪项能出具 M19？|
|AcceptCardSessionInvalidation / AcceptCardRemoval / ReadPrimaryTarget|card/runtime/card_runtime.cpp:34；account/discovery/card_observation_port.h 的 CardObservationSink；account/topology_service.cpp:46、:242；account/binding_index.cpp 的 RemoveBinding|可信载体替换如何定位原绑定；另一个应用的失效、同卡再插、旧回包分别还能改什么？|
|StartPrimarySwitch / AcceptRadioStep / AcceptLateRadioEvidence / SetDefaultRole|role_assignment/primary/primary_topology_port.h 的 PrimaryTargetFacts；role_assignment/assignment_service.cpp:132、:324、:339、:539；role_assignment/assignment_aggregate.h:38；role_assignment/account_role_protocol.h:68|逐个判定业务取样点、预写/后写/恢复资格、字段版本与旧返回节点；列出未返回的完成和未明效果|
|PlanPrimaryRound / RecordRadioSent（提案）、平台提交/回包/恢复|role_assignment/primary/radio_execution_port.h:35–95；infrastructure/request/runtime_operation_store.h 的 RecordSent；device/resource/resource_radio_runtime.cpp:82、:190；device/resource/resource_barrier.cpp:39、:51、:142|纯 Radio 步骤如何冻结真实成员与路由；ConfirmedNotSent、已交接、HandoffUnknown 分别留下何种证据；跨实例谁可证明可复用？|
|模块关闭与各 Owner 的逐项收据（提案）|infrastructure/assembly/sim_module.h 的 SimModule；infrastructure/assembly/sim_module_factory.cpp 的构造函数；infrastructure/assembly/primary_host_lifecycle.cpp:15；DESIGN B §11、A §14.3|如何将未决发现、已领取仍未返回、设备隔离分别关联回原 Owner，何时才允许销毁？|

本轮仅为工作区文档及源码静态审查。未重跑 Host、未执行产品构建或设备试验。H3～H6 及 AF-2 均未开始。
