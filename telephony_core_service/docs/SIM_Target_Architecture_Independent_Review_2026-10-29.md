# 多 SIM 目标架构与源码骨架独立评审

> - 评审日期：2026-10-29
> - 输入 HEAD：`36deb95c3d6a76c356ec75a572d53e8c0701b1e8`
> - 评审性质：设计、源码、构建及配置的定向静态审视与桌面推演。
> - 文档状态：本轮评审记录与修正建议，不是新的目标主规范，也不表示设计批准、实现完成或设备验证通过。

本文整理本轮独立审视结论。原审视阶段只读；随后按用户要求新增本文，不修改原设计、源码、构建、接口或测试。问题编号仅用于本报告导航，不替代既有 SIM 功能编号、WU 写权单元或 TGT-OPEN 决策编号。

> **main 发布说明**：本报告仍以 `36deb95c3d6a76c356ec75a572d53e8c0701b1e8` 为评审输入；发布前的 main 为 `cad822c387c11f6ab68c140cb4c747ad19a086ca`，两者已有分支差异，main 尚未合入被评审骨架。本次仅发布报告，将文档及代码引用固定到原评审输入，不合入骨架或其他分支提交，也不对 main 的并行修改作新一轮架构裁决。文中操作与验证记录保留原审视、文档输出阶段的语境。

> **后续处理入口**：本报告保留原评审基线。分支整合时已对未处理意见作逐项审视，当前裁决、骨架同步及原评审需限定之处见[目标稿 §6.5/§20.6](SIM_Target_Architecture_Design_v1.2.md#consolidated-contracts)。报告发布时的main说明为历史记录，不表示当前main仍缺少骨架。

## 1. 总体结论与可信判断基础

### 1.1 是否足以指导下一阶段

**目标方向值得保留，但当前骨架主要具备职责导航价值，尚不足以直接分工开展大规模实现。** 主要障碍不是函数尚未实现，而是账户投影、联合建行、跨 Owner 接纳、同步完成、Primary 分支及工厂所有权等协议尚未闭合。直接按现有接口填充实现，存在后期重新调整边界和生产语义丢失的风险。

| 下一步 | 评审判断 | 条件与边界 |
| --- | --- | --- |
| 继续深化骨架 | 可以 | 优先修正本文 P1 协议及状态模型，不继续扩充缺乏真实使用方的空类 |
| 建立可编译闭包 | 可以开始 | 先在原 SO 内闭合真实装配和一条完整调用链；未实现能力不以成功桩补齐 |
| 启动首批生产迁移 | 可以开展 G2a 依赖收口；暂不宜直接转移业务写权 | G3 写权迁移前关闭对应协议缺口，覆盖该状态单元全部旧入口，而非只迁公开 setter |
| 独立 SIM SO | 有条件可行 | 当前借用已有对象的工厂不能直接作为最终创建边界，须先明确对象创建、拥有、发布及匹配销毁 |
| 并发准入增强 | 单独按方法验收 | 排队、拒绝、隔离和抑制旧重试均可能改变行为，不夹带进机械结构迁移 |
| 动态卸载或模块重建 | 单独验收 | 排空、代码寿命和未知设备效果分别处理；不作为常驻 SO 拆分的统一前置条件 |

本轮没有发现需要推倒 T/A 划分、恢复中央 Coordinator 或重写成熟卡协议的依据。骨架尚未接入生产，本文不把其表达缺口描述为正在导致生产故障。

### 1.2 输入、范围与适用指引

主要输入：

1. [SIM 目标架构设计 v1.2](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/docs/SIM_Target_Architecture_Design_v1.2.md)，下称“主设计”。
2. [多 SIM 架构重构：设计说明与业务流程对照](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/docs/多SIM架构重构_设计说明与业务流程对照.md)，下称“流程对照”。
3. [现有生产实现](https://github.com/zjuluowu/multisim/tree/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/sim)与[目标源码骨架](https://github.com/zjuluowu/multisim/tree/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim)。

按真实调用关系追踪了 Core 装配、CoreServiceSim、CoreManagerInner、公共管理器接口、GN、版本脚本、存储、RIL、搜网、厂商 Hook 以及相关外仓入口。未无目标重扫全仓，也未把范围限制在两个 SIM 目录内部。

已读取并遵守 [AGENTS.md](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/AGENTS.md) 的任务边界、证据分类和改动保护要求。本文记录结论，原主设计、功能/WU 清单和迁移文档继续承担各自职责。

### 1.3 本仓基线与工作区

| 项目 | 核实结果 |
| --- | --- |
| 当前 HEAD | `36deb95c3d6a76c356ec75a572d53e8c0701b1e8` |
| 当前分支 | `docs/publish-local-doc-updates-20261029` |
| 原审视输入工作区 | 925 项跟踪文件修改；逐文件比较确认均仅为 CRLF/LF 差异；暂存区为空，无未跟踪文件 |
| 主设计历史源码证据 | `6017003eff50c1bc8c5dfa72336a4bfe42e948b0` |
| 主设计最近独立复核输入 | `3fd0f815991fe6d5f9ba807dbc9d034c8594cb83` |
| 主设计 §6.4 骨架补充输入 | `efbefd48cb20c2c19e5c1586528af435de518cbd` |
| 相关代码变化 | 上述基线至当前 HEAD，所追踪生产源码、接口、构建和配置无内容变化；目标源码新增于 `services/multisim/`，共 36 个头文件、15 个源文件 |
| 本轮操作 | 原审视只读；文档输出阶段仅新增本文，不暂存、不提交、不推送 |

本文代码行号对应上述输入的当前工作树；其已有行尾变化不影响行号。设计文档原有历史基线保持原义，不批量更新为当前 HEAD。

### 1.4 外仓定向核实

| 外仓 | 本轮读取 HEAD | 核实内容与限制 |
| --- | --- | --- |
| `telephony_data_storage` | `843a8be7f8d1dc304870a3999b754a03c6cce57d` | 定向读取 `sim/` 的 SetCard、建表、字段和升级路径；相关证据文件与 HEAD 仅有行尾差异。确认该版本与本 fork 的命令及 schema 不匹配，不能直接作为匹配 provider |
| `telephony_cellular_data` | `29d6fa9c6105c6e39a9a765048868e4631ffbb8f` | 定向读取默认 Data 公共 setter、客户端及通话状态机直接入口；相关文件忽略行尾后与 HEAD 一致。不证明产品镜像匹配或联调完成 |
| `telephony_ril_adapter` | `d039c7ce8518487bfcec54c9777e6172c0c9f684` | 读取 `services/hril/src/hril_sim.cpp` 的 serial/session、RadioProtocol 和通道关闭转发；该文件忽略行尾后与 HEAD 一致。转发能力不证明厂商硬件已静止 |

旁挂目录为 `/mnt/e/github/telephony/`。这些结果是定向事实，不继承文档中的历史设备或测试结论。

### 1.5 依据与冲突处理

| 依据性质 | 本轮采用方式 |
| --- | --- |
| 已发布或有批准证据的契约 | 约束兼容行为；本轮没有新增业务批准证据，不虚构 ACCEPTED 状态 |
| 当前源码、接口、构建与配置 | 证明签名、调用、装配、编译输入和静态行为；旧行为作为 LEGACY 护栏，不自动成为永久业务不变量 |
| 目标设计 | 提出未来结构与协议，仍需按切片实现和验证 |
| 骨架代码 | 表达类型、参数、成员、依赖和流程意图；声明存在不等于实现或生产行为 |
| 桌面交错 | 从代码和接口推导风险，未复现不写成已发生故障 |

主设计 §4.1/§4.2 已明确裁决：T/A 为业务上下文，O 为 Application 编排机制。流程对照已同步这一结论；AGENTS 及部分历史材料仍保留“三上下文”口径。这里采用有具体理由的裁决，不按日期或文件名决定权威，也不把该裁决当成业务批准。

### 1.6 骨架实际完成范围

目前有少量纯目录查询、号码步骤计划、加载成员资格示例和构造注入实现；入口、业务写入、设备方法、结果桥、工厂、deleter、排空等主要为声明或流程伪代码。骨架没有接入生产 GN，没有转移写权，没有启用第二套活动业务对象、设备执行或重试。

本轮不因这些未实现内容单独判缺陷。重点是：接口能否准确表达其声称承接的责任，以及按此实现是否会遇到状态、依赖或兼容协议障碍。

## 2. 建议保留的设计

| 设计 | 解决的问题及依据 | 保留条件 |
| --- | --- | --- |
| 身份、位置、角色与设备会话分开 | 生产 slotId 同时承担对象索引、端点种类、路由和权限投影；第三实体卡、VSIM 又有特殊分支。SubscriptionId、BindingRef、SessionRef 与只读 SlotCatalog 的区分有实际价值 | 不由目录容量推导 Modem、Port、MEP 或并发能力；近期沿用既有 simId，不新造公共身份 |
| T 管账户与绑定，A 管角色、Primary 和业务重试 | InitIccId/ForgetAllData 涉及账户同行字段；Primary 实际联动默认 Data。近期合并这些相关规则有利于形成完整写权 | 补齐联合建行及重载协议；当 Port 不变量或产品联动真正改变时再评估重开边界 |
| O 降为 Application 机制 | 请求步骤、等待、效果认知不是独立业务事实。让事实 Owner 主编排可减少 A/O 或 T/O 的往返与隐藏代写 | 通用容器不拥有业务进度、不发令、不自行重试，不演变为中央万能 Coordinator |
| R 复用成熟生产能力 | 安全、EF、各卡型 Records、电话簿、STK、运营商配置及 eUICC 协议有成熟生产责任；类名不能决定领域归属 | 真正接管原写点，不只在外层加 token；不机械复制一套同名空壳 |
| 逐节点兼容可见性 | 号码为写卡后写库；普通激活的缓存和 Hook 又在 DB 之前，均存在部分效果 | 保留各节点与独立错误，不用统一事务或最终成功布尔抹平 |
| 高层 eSIM 任务与本地原语分开 | Framework/服务任务边界与 Core 卡侧通道/APDU 是不同责任；Core 不应复制 66250 全流程 | 保留本地双结果和原偏好节点，明确本地短请求的会话及资源责任 |
| 分阶段迁移与独立工程门禁 | 写权、设备准入、SO 打包和动态卸载解决不同问题 | 按 WU 覆盖全部旧入口，机械结构迁移与行为增强分别验收 |

骨架没有普遍出现接口套接口、平台设施进入 Domain 或公共可写聚合引用的问题，也没有恢复万能 Execute/Coordinator。少数具体反向依赖及 Domain 放置问题见 §3.3。

## 3. 优先问题清单

### 3.1 分类和优先级

类别沿用本次任务定义：

1. 目标设计不合理或不完整。
2. 骨架偏离合理设计，或未能表达必要责任。
3. 文档之间或文档与骨架口径冲突。
4. 骨架阶段可接受的未实现内容。
5. 需要外仓、产品或设备证据确认的局部缺口。

P1 表示相关切片实现或接管前应修正；P2 表示深化时应处理。下列问题均属于当前表达缺口、未来实现风险或边界不清，不表示 multisim 骨架正在造成生产故障。

### 3.2 P1 问题

#### P1-1：账户加载与模型压缩了当前／历史投影

**分类：2，兼有 3。修改对象：主要为骨架，同步主设计 §6.4 承接说明。**

- **设计依据**：主设计 §8.2、§9.3、§13.2 要求保持旧读源及各投影可见节点。
- **生产事实**：`MultiSimController::GetListFromDataBase`（`services/sim/src/multi_sim_controller.cpp:733`）调用 QueryAllValidData，更新当前缓存；`GetAllListFromDataBase:759` 调用 QueryAllData，更新历史缓存，二者分别失败、重试和替换。普通激活的 `UpdataCacheSetActiveState:956` 还可能先改当前 active，再因历史索引错误退出。
- **骨架事实**：[AccountStore](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/ports/account_store.h):33、42、57 只有单组加载行、单组结果和无投影参数的 LoadAccounts；[AccountRecord](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/domain/account_registry.h):30 仅号码有双投影，名称和 active 等为单值。

**触发与影响**：current 加载成功而 all 失败时，旧实现保留不同新旧程度的数据；DB 成功、当前缓存成功、历史缓存失败也会产生部分可见效果。当前契约没有明确承接这些独立读源、独立结果和字段片段。

**架构原因**：这是兼容状态模型和接纳边界问题。只在 Adapter 注释中写“沿用原两读源”，不能说明如何分别返回与接纳；实现者可能将两个结果合并，或在端口之外维护隐藏状态。

**最小修正**：为加载指定 Current/Historical 类别、独立尝试和结果；对确有可观察差异的字段保留必要投影片段。它们仍由同一个 T Owner 管理，不新增第二业务写者。

**验证**：current 成功/all 失败及反向组合；DB 成功、当前成功、历史失败；只刷新一个投影时另一个保持原值；独立重试不覆盖较新字段。检查旧返回、查询和通知轨迹，不只检查容器最终值。

#### P1-2：T/A 联合建行与重载缺少可执行协议

**分类：1＋2。修改对象：设计与骨架。**

- **设计依据**：主设计 §4.2、§6.3、§13.2 要求 T 发现身份，A 生成角色片段，组合一次明确建行计划；重载按字段交各 Owner。
- **生产事实**：`InitIccId:421` 匹配旧账户或进入 InsertData；`InsertData:517` 调存储插入，`SimDataBuilder:560` 根据单卡等配置写初始四类角色列。新身份在插入阶段才得到落实。
- **骨架事实**：[TopologyService](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/application/topology_service.h):53 的 AcceptCardDiscovery 要求已有角色片段；[AssignmentService](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/assignment/application/assignment_service.h) 的 PrepareAccountRoleFragment 要求 SubscriptionId。AccountStore 没有匹配、建行返回身份或绑定/解绑计划入口；AccountStorageRow 排除了角色，AssignmentStore 又没有相应角色加载入口。

**触发与影响**：首次发现新卡时，角色准备需要新 simId，而新 simId 又来自后续插入。初始化或 provider 重连时，也没有明确的角色读取和分别接纳通路。实现者可能绕过端口、重复查整行或让 T 代写 A。

**架构原因**：发现、身份分配、角色规划、同行存储与分 Owner 接纳之间的因果关系未闭合。单纯声明“角色片段由 A 提供”不足以指导实现。

**最小修正**：T 准备身份匹配/建行上下文；A 按发现上下文与角色版本规划片段，不强求尚未产生的新 simId；I 执行原 Insert/Update；返回后 T 接纳身份与绑定，A 接纳角色投影。明确该计划的冲突资格、失败分支和重载分发，不新建上下文或 schema。

**验证**：首次建行、旧卡恢复、单卡初始角色、建行期间角色变化、provider 失败、current/all 独立重载及重载与本地字段更新交错。验证单一活动写者，不以本地锁宣称 provider 原子性。

#### P1-3：R 的有效发现观察跨到 T 后缺少最终接纳资格

**分类：1＋2；空位置版本另属 2。修改对象：设计与骨架。**

- **设计依据**：主设计 §10.2、§10.3、§11.2；流程对照 §10.3 要求最终准入与状态接纳位于受控边界。
- **骨架事实**：[AcceptCardDiscovery](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/application/topology_service.h):53 只有 slot、ICCID、角色片段，没有 SessionRef、观察序号或预期绑定版本。AcceptCardRemoval 带 BindingRef，发现入口没有相应资格。[LoadSession::TryConsumeRead](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/runtime/records/load_session.cpp):35 只负责该加载会话的成员消费。

**触发推演**：A 的 ICCID 已在 R 合法接纳；发现任务或建行查询未完成时同槽换成 B；随后 A 的旧发现进入 T。T 无法从当前参数证明它仍有资格更新当前绑定。R 在此前做一次正确校验，不能覆盖跨 Owner 投递和锁外 DB 窗口。

**架构原因**：R 拥有设备会话有效性，T 拥有业务绑定接纳；两者间只有业务载荷，没有明确的资格与失效排序。这里需要跨边界协议，不能靠回包时读取当前会话补造关联。

**最小修正**：传递带 SessionRef、观察序号的不可变发现输入；明确发现、替换、撤销在 T 的接纳序列或等效短提交握手，并在最终发布及 DB 返回后复核。不要只增加一次 T→R 同步检查。已发生持久效果要进入字段核对。

[SlotRuntimeEntry](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/domain/binding_index.h):28 也应在空位置保留独立 revision。当前版本只在可空 BindingRef 中，尚未明确承载移除后的版本延续；构造时容器为空本身不是问题。

**验证**：A 发现延迟→B 替换/发现→A 返回；建行已写但绑定资格失效；重复观察；同一卡 `rev1→空 rev2→再插 rev3`；无关槽加载继续。检查 DB 和内存两种效果，不只断言拒绝发布。

#### P1-4：激活进行中状态的键与清理资格不准确

**分类：2，设计作用域需细化。修改对象：骨架及主设计 §4.2/§9.3。**

**事实与依据**：生产 `IsSetActiveSimInProgress`（`multi_sim_controller.cpp:2049`）和 `UpdateActiveSimInProgress:2260` 按 slot 读写，搜网 `network_search_handler.cpp:578、839` 使用它决定 Radio 行为。骨架却将 activationInProgress 放在 [AccountRecord](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/topology/domain/account_registry.h):39，并通过 `AcceptActivationProgress(SubscriptionId,bool)` 更新。

**场景与架构风险**：换卡、解绑或账户移动后，当前账户属性不能直接表达原槽会话责任；旧请求的无资格清理也可能影响新请求。Owner 留 T 合理，但不应把槽设备操作进度等同于历史账户属性。

**最小修正**：T 内以 request、slot、sentSession 维护轻量激活 transition，按旧节点投影槽进度，开始和清理由请求资格控制；账户 active 投影仍单独保存。通用运行容器不代写该业务状态。

**验证**：换卡、解绑、同账户移动、超时后第二次激活与旧清理交错；原搜网查询仍在正确节点读到槽进度，不把资源未知和旧进度清零合为一项。

#### P1-5：Primary 端口未完整表达生产模式

**分类：2＋3。修改对象：主要为骨架，并限定流程对照图的适用分支。**

**事实与依据**：生产 `SetPrimarySlotId`（`multi_sim_controller.cpp:1483`）区分槽 3 换路、免重启专用 RIL、已为 M0、普通三阶段。`SetPrimarySlotIdWithoutModemReboot:2071` 先执行设备，成功后保存；普通路径先预写参数。主设计 §9.3 已要求两类 RIL；[RadioSwitchRequest](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/assignment/ports/radio_execution_port.h):25 仅有 step/target/domain，当前 A 伪代码和流程对照 §9.3/§9.4 却主要统一表达预写与 RadioProtocol 路径。

**场景与架构风险**：支持免重启的产品由用户切换主卡时，现有端口没有明确模式、能力和设备观察输入。实现可能丢失原顺序，或隐藏反调旧混合 Controller，使 A 的参数写权重新散落。

**最小修正**：增加具名 Primary 模式和真实能力/设备观察输入，R 承接专用 Primary RIL 与三阶段机制，A 按模式安排预写或后写；已为 M0 不执行协议的分支也进入计划。第三卡换路继续按已承认的 T/厂商局部缺口处理。

**验证**：普通、免重启、已为 M0 及第三卡各自的返回点、参数、设备、Data、广播失败偏序；成功和错误均先核对发起归属。不能以一个三阶段测试覆盖全部产品路径。

#### P1-6：原同步结果缺少明确完成接缝

**分类：2。修改对象：骨架及设计的方法级完成协议。**

**事实与依据**：`CoreServiceSim::SetPrimarySlotId`（`services/core/src/core_service_sim.cpp:456`）和 CMI 入口同步返回 int32_t，无 caller callback；[LegacySimFacade](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/compatibility/legacy_sim_facade.h):39 与 A 启动方法返回 RequestId，设备端口为异步 void。[CompletionPlan](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/application/orchestration/request_context.h):36 只有 callback 索引与成功账户通知布尔。

**场景与架构风险**：现有表达未说明如何在旧节点取回原返回值。临时把受理当结果，或在状态接纳执行域中阻塞等待，都可能破坏兼容与进展性。同步返回本身也不能被统一解释成所有设备效果已经完成，第三卡等分支有自己的早返回语义。

**最小修正**：C 明确区分旧同步结果、旧异步 callback 和内部回执；应用向对应窄出口完成。等待适配器保持原节点、预算和错误，不占用完成所需执行域，不新增公开 API。

**验证**：Primary 各分支原 int 返回；号码受理与 callback；普通激活；CMI 直接入口；超时、调用者死亡和晚到只核对、不重复完成。

#### P1-7：迁移附着工厂不能直接作为独立 SO 的最终创建边界

**分类：2＋3。修改对象：装配骨架与主设计 §6.4/§7.1/§14.2 的对应说明。仅阻塞相应拆库切片。**

**事实与依据**：[ExistingSimBindings](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/assembly/sim_module_factory.h):33 要求外部提供 store、monitor、radio 和槽对象，SimModule:45 只提供 LegacyEntry。生产 [CoreService::Init](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/core/src/core_service.cpp):101 创建共享 RIL，随后具体 EsimManager/SimManager；Controller/store 又在 Manager 内部创建。当前版本脚本将 Core 符号设为 local。

**场景与架构风险**：若把当前借用工厂直接用于 §7.1“SIM 只导私有工厂”的拆分，Core 仍需要 SIM 具体构造符号和内部对象取得路径。问题是两种工厂用途未区分，不是借用本身或工厂未定义就构成缺陷。

**最小修正**：保留同 SO 的迁移附着接缝；最终 SIM 侧工厂创建并拥有原对象，返回原管理器句柄和匹配控制块/deleter，Core 不接管 Controller/store 内部装配。保留原初始化、eSIM 反查和发布顺序。

**验证**：消费侧对象身份一致、共享 RIL 无第二实例、失败清理、私有导出、无 Core↔SIM 具体反链、Feature/CFI/PAC/LTO 等产物性质。常驻拆分不必等待动态卸载能力，但不能声称已有安全卸载。

#### P1-8：本地 Profile 参数与 A 副作用缺少承接关系

**分类：2，局部为 1。修改对象：骨架及主设计 §4.2/§7.2/§10.2 的本地短请求说明。**

**事实与依据**：[SwitchProfileRequest](https://github.com/zjuluowu/multisim/blob/36deb95c3d6a76c356ec75a572d53e8c0701b1e8/telephony_core_service/services/multisim/application/card_access/local_card_requests.h):29 未保留 forceDisableProfile，只有 optional verifiedPortIndex；LocalCardRequests 仅注入 R。生产 `EsimManager::SwitchToProfile`（`services/sim/src/esim_manager.cpp:318`）在 File 返回后仍写偏好，即使卡 enum 失败，外层也可成功。`EsimFile::SwitchToProfile:2728` 保存原 port/ICCID/force，但当前 APDU 编码分支未消费 force，不能夸大为删除它已改变设备命令。

**场景与架构风险**：原 Port 实参与经证实路由被混为一项，未知验证结果无法替代旧透传值；A 提示只有注释，无明确节点调用者。Profile ExpectedTransition 又要求有人在发起前登记，但本地短请求的登记和清理责任未展开。

**最小修正**：保留原参数，包括 force；将原 portIndex 与可选验证证据分开。明确本地短请求→R 原语结果→A 原节点提示→双结果返回的责任，绑定由后续带资格观察交 T。由本地短请求明确登记与清理期望，不复制 66250 高层状态机。R 私有状态继续区分效果和通道关闭，不改公开双结果。

**验证**：非法槽不提示；卡 enum 失败仍保持原提示节点；APDU 超时但卡可能已切换；卡成功而关闭未知；切换后旧 EF 结果；高层服务超时后 Core 不自行重跑完整任务。

### 3.3 P2 与表达问题

| 问题及分类 | 事实、触发与影响 | 最小修正、修改对象及验证 |
| --- | --- | --- |
| P2-1：号码依赖集过粗，类别 2/3 | AccountRecord.version、NumberExecutionTarget.accountVersion 与 TopologyService 伪代码只有账户版本；写卡期间改名、label 等无关属性变化可能使号码结果被判过期，与主设计 §10.3 的实际依赖要求不一致 | 骨架分开绑定、会话和号码字段冲突资格；必要时细化相关字段版本。验证写卡期间改名可继续、号码冲突有序、换卡确实拒绝旧提交；无需每字段新建聚合 |
| P2-2：R 结果路由认识具体用例，类别 2 | CardRuntime::BindResultOwners（runtime/card_runtime.h:84）直接引用 SetShowNumberUseCase、ActivateSimUseCase、AssignmentService。没有 include 环不等于依赖只到端口，替换设备实现仍需认识具体用例 | 改窄结果 sink，由 H 绑定；同步主设计 §5.2/§6.4。验证 R 实现可替换且无需引用具体应用服务；不新增总线 |
| P2-3：资源协议正常出口未表达，类别 2 | ResourceBarrier（runtime/resource/resource_barrier.h:44）仅有 Acquire、Quarantine、Recovery 等，没有明确正常完成、NotSent 撤销和初始核对转移；与主设计 §12.3 状态表不完整对应 | 补状态转移契约，验证正常释放、超时隔离、旧证据不释放新租约。当前未启用，不称已造成死锁，不阻塞 G2 |
| P2-4：兼容步骤计划放入 Domain，类别 1/2 | PlanShowNumberUpdate（topology/domain/show_number_plan.cpp）主要返回卡→DB→缓存的兼容编排偏序，纯 C++ 不自动使其成为领域规则 | 移至 Application 的具名计划即可，保留真实纯规则于 Domain；以依赖和场景检查验证，不为小移动新增重复实现测试 |
| P2-5：eSIM 对象借用映射不准确，类别 2 | ExistingSimBindings 与 CardRuntime 用两个 EsimManager 指针表达普通/低优先级；生产 Core 只创建一个 Manager，其内部每槽两套 EsimFile（esim_manager.cpp:45）。当前没有重复装配事实 | 使用一个 Manager 引用或明确其内部 File 视图，共享 eUICC 资源域。修改骨架装配说明，核对普通/低优先级通知与资源身份 |

### 3.4 类别 4：可接受的未实现内容

方法声明、流程伪代码、未接 GN、工厂/deleter 未定义、完整激活完成端口和卫星端口待展开、资源屏障未启用，本身均可接受。主设计 §6.4 已主动声明大部分边界。

它们在接管相关切片前仍需完成，但不能仅据“没有函数体”推断架构不可行。P1/P2 关注的是已写出的类型与关系是否准确，以及接口是否承载其宣称的责任。

### 3.5 类别 5：真实外部缺口及局部阻塞范围

| 缺口 | 已核实的边界 | 阻塞范围与关闭条件 |
| --- | --- | --- |
| 匹配 fork 的 provider、部署及外部写者契约 | 旁挂 provider 的 sim/src/sim_ability.cpp:200 SetCard 要求 SLOT_INDEX/CARD_TYPE；本 fork sim_rdb_helper.cpp:274、294 发送 SIM_ID/CARD_TYPE。旁挂 SIM 表缺 fork 使用的 IS_ESIM、SIM_LABEL_INDEX、OPKEY 列；其独立 opkey 表不能替代 SIM 表缓存列。SetDefaultCardByType 确有事务，但不能据此宣称匹配 | 阻塞依赖匹配 schema、默认角色事务或跨进程唯一写者保证的切片；不阻塞 G2a、纯规则或依赖收口。需匹配 provider 版本、部署和字段/失败/外部写者证据 |
| 设备结束、通道静止及厂商换路证据 | RIL adapter hril_sim.cpp:314 转发 session/phase，:352 转发关闭；仅能证明调用路径，不证明厂商硬件结束 | 阻塞依赖结束判据的新隔离/重试/更细并发域。需命令级因果、结束/失效证据、预算与错误映射 |
| 66250 匹配任务实现与恢复契约 | 本仓有接口/客户端和本地原语，未核实匹配服务的完整步骤、重试、补偿和完成点 | 阻塞依赖这些保证的高层任务切片；不把所有本地原语转办服务，也不让 Core 复制任务 |
| 真实 Port/MEP、N 槽与并发能力 | 原参数可透传、产品分支存在，不证明目标模型可同时承载任意数量的当前 Profile/Radio | 阻塞新增能力的 G4 切片，需设备、产品、存储与旧投影契约；目录可容纳 N 项不是能力证明 |

## 4. 代表场景桌面推演

以下均为静态责任与交错推演。成功、失败、超时和部分效果按旧方法分别讨论，不作为设备复现记录。

### 4.1 设置显示号码

**入口与解析**：CoreServiceSim::SetShowNumber（`services/core/src/core_service_sim.cpp:502`）保持 SystemApp、权限、空值及调用者槽能力检查，再排队捕获 slot/number/callback。任务执行时定位 File，账户在写卡后的 SetShowNumberToDB 才解析 simId。骨架在执行接纳点固定目标是 G3b 推荐增强，不是 G2 等价替换；前移到 IPC 接收时固定还需独立裁决。

**责任流转**：C 保持入口与原完成出口 → T 号码用例主编排 → R 执行并接纳 MSISDN → T 更新账户 → C 完成。卡号码归 R，账户号码归 T，I 只执行所属字段存储。

**状态与副作用**：写卡→DB→当前缓存→历史缓存。Manager 仅整体成功发原账户通知，Core 随后 callback；不保证通知消费者与 callback 消费者的全局到达顺序。

| 分支/交错 | 推演要求与骨架关系 |
| --- | --- |
| 写卡成功、DB 失败 | R 卡效果保留；T 返回原失败，不回滚或重放写卡；不发整体成功账户通知。NumberAcceptance 与 LegacyCompletion 已有合理表达 |
| DB 成功、历史缓存失败 | 保留已发生的 DB/当前缓存片段，P1-1 需使模型准确表达，不能包装成整体未发生 |
| 超时后旧回包到达 | false 包含超时，内部为 Unknown；原会话可信结果只作 R 观察/核对，不继续旧账户步骤或重复 callback。最终准入须放到原 ProcessSetMsisdnDone 写点之前 |
| 公共请求排队期间换卡 | G2 保留旧执行点解析；不能默认接收时固定。G3b 固定和陈旧拒绝须单列差分 |
| 查询回写排队期间换卡 | GetShowNumber 卡→账户→IMPU，查询不等回写；独立回写保存发起绑定/会话，执行时复核，不写新卡账户，也不复活旧设置请求 |

**对应元素**：LegacySimFacade、SetShowNumberUseCase、ShowNumberQuery、NumberExecutionTarget、NumberAcceptance、LegacyCompletion；缺口见 P1-1、P1-3、P2-1。

**迁移关闭点**：SetShowNumberToDB、InitShowNumber、两类查询异步回写、InitIccId、current/all 重载、Monitor/provider/用户重连、卡探测、名称/IMS/标签及相关 eSIM 账户写入口。仅迁一个 setter 不构成 WU 闭包。

### 4.2 普通 SIM 激活

**入口与解析**：公开 CoreServiceSim 入口在原节点写 Primary 偏好，即使后续失败；CMI、初始化入口不具有完全相同的前置轨迹。目标与会话在明确执行点取得，不能统一复制公开权限检查。

**责任流转**：T 主编排并拥有账户 active 和激活进行中事实；A 接纳偏好、协调 Primary 和后续主卡复核；R 执行设备命令并维护原保守共享串行域；I 执行原 Hook。

普通 non-force 的关键偏序为：进度置位 → 有界等待 Primary → RIL → 当前/历史缓存 → 第一次 UpdateSubState/清进度 → DB → 第二次缓存/Hook → 主卡复核 → 最终 Hook → Manager 账户通知及 active Registry。旧等待一次 4 s 超时后仍可继续，不可在 G2 偷换为新 DomainBusy。

| 分支/交错 | 推演要求与缺口 |
| --- | --- |
| 全量索引或 DB 失败 | 保留前面缓存与已发生 Hook，返回原失败，不改成最后统一发布 |
| force、卫星 | 有独立轨迹：普通 force 仍先 RIL；卫星又有不同 DB/缓存节点，不能套普通模板 |
| 超时后旧响应与下一次激活交错 | 原单值响应缺请求关联，错配为 DERIVED 风险；新发起关联与错误准入合理，但等待结束不证明设备可复用 |
| 换卡/解绑后旧请求清理 | T 仍是合理 Owner，须按原 slot/session/request 控制进度清理，不能仅对当前 Subscription 写 bool |

**对应元素**：ActivateSimUseCase、ActivationRuntimePort、ActivationHookPort、A 准入及复核入口。完整完成、投影片段和卫星端口尚未展开本身属类别 4；P1-1、P1-4、P1-6 是需要先修正的契约表达。

**迁移关闭点**：InitActive、普通/force/卫星、拔卡/初始化 active、失败清理与相关 Hook 一同登记；后续主卡复核交 A，新旧不得同时重试发令。

### 4.3 主卡切换与 Data 联动

**入口与解析**：公开、CMI 和内部重试分别保持原参数、检查与完成点。A 在原执行点取得相关绑定和设备观察，选择普通三阶段、免重启、已为 M0 或第三卡换路分支。

**责任流转**：A 拥有参数/last、角色、AssignmentTransition 和唯一业务重试；R 拥有原始协议 session、设备观察和执行租约；T 提供绑定事实及必要映射用例；I 执行原参数和输出协议。

**外仓直接入口**：cellular_data 的 `services/src/cellular_data_service.cpp:490` 公共 setter 检查 system/权限/在位/active，:515 同步调用 CMI，没有等待数据连接或驻网；`state_machine/incall_data_state_machine.cpp:265` 等内部路径直接调 CMI。A 不能统一增加公共权限检查，也不能把 Data setter 成功解释为连接完成。

| 分支/交错 | 推演要求与缺口 |
| --- | --- |
| 普通成功 | 参数预写→设备协议→主卡广播→Data 参数/last/广播→原 Done/结果及适用通知；部分参数和广播失败不统一升级返回错误 |
| 免重启/已为 M0 | 免重启设备在前，成功后保存；已为 M0 不执行完整协议。P1-5 必须承接不同模式 |
| 两个切换请求冲突 | 旧第二请求可能因 R 已通信走失败恢复、Done 和重试，提前清除第一请求进度；文档已识别该风险，未复现。新准入收紧单列 G3c |
| 超时、旧 session 错误、重试 | 成功/错误均在修改阶段标志与计数前校验；参数恢复不证明硬件回滚。重试归 A 唯一授权，Unknown 不自动释放 R 责任 |

**对应元素**：AssignmentService、AssignmentAggregate/Transition、AssignmentStore、RadioExecutionPort、AssignmentOutputPort。主要缺口是模式/观察、同步完成和附着装配边界。

**迁移关闭点**：InitMainCardSlotId、Save/Resume/SavePrimarySlotIdInfo、Data 保存、入口及 eSIM 偏好、延后目标、重试事件、detached CMI 自调用及外围 SavePrimarySlotId。不能只替换公开 SetPrimarySlotId。

### 4.4 换卡与 Records/Refresh

**入口与解析**：READY/Refresh 进入原 File/Controller，R 决定卡型、依赖闭包、scope 和加载成员；发起时固定会话/token，不在回包时贴当前值。T 的发现用例匹配或建账并接纳绑定，A 提供角色和主卡复核。LOADED 不是建账唯一入口。

**责任流转**：R 接纳具体字段、加载成员与卡型完成；T 接纳账户 loaded 与身份绑定；C/I 保持原输出。SIM 的完成路径包含 mapping 检查、UpdateLoaded、Records observer、Registry、CES、语音信箱和 EXT，RUIM/ISIM 另有自身条件，不能统一为所有必需文件成功。

| 分支/交错 | 推演要求与缺口 |
| --- | --- |
| 同槽换卡后旧读到达 | 在原实际字段写点之前撤销旧资格；不能先解析写入新缓存再在外层丢弃 |
| 同卡局部 Refresh | 只撤销受影响 EF/PBR/ANR/IAP 依赖闭包的读取，其他 scope 继续；LoadSession 已有成员和一次消费表达 |
| 重复/失败结果 | 一次消费成员资格，保留各卡型原完成条件与允许的外部通知，不以计数归零充当成功 |
| Refresh 时已有写卡 | 旧写仍保留效果和资源责任，不按陈旧读取直接丢弃或自动重写 |
| R 合法观察后 T 才换卡 | R 的一次校验不能替代 T 最终接纳，须关闭 P1-3 跨 Owner 窗口 |

**迁移关闭点**：具体卡型解析字段、ProcessFileLoaded、ClearData/Refresh、卡型重建、IccFile::UpdateLoaded、电话簿闭包，以及 Monitor 的 InitData/RefreshData、Controller 身份/解绑和 T 重载入口。只给外层包 token 不构成接管。

### 4.5 本地 eSIM/Profile 切换

**入口与解析**：高层 Framework→EsimServiceClient→66250 继续属于服务任务边界；本地调用经 CMI→EsimManager→EsimFile。原 portIndex 是调用实参，验证路由是另一个事实，不由缺证据推导默认 Port=0。

**责任流转**：本地短请求关联和期望登记 → R 开通道/APDU/等待/关闭 → A 在原节点接纳偏好提示 → 保持外层 int/卡 enum 返回。T 从后续带资格的运行观察接纳绑定，不凭原语外层成功直接更新账户或宣布 Radio 就绪。

| 分支/交错 | 推演要求与缺口 |
| --- | --- |
| 有效槽、卡 enum 失败 | 原 Manager 仍在 File 返回后写提示，外层可成功；不能改成卡成功才提示或合并双结果 |
| APDU 超时但卡已切换 | R 保留 Unknown 与因果证据；不从超时推断未执行，不让 Core 重跑 66250 完整任务 |
| 卡成功、关闭未知 | 卡结果、通道结束和资源可重用分别判断；本地 channel 清零不证明设备已关闭 |
| 新 Profile 会话与旧 EF 回包交错 | 明确本地短请求登记/清理 ExpectedTransition，旧 EF 不因允许后续观察接续而重新获权 |
| 普通/低优先级调用竞争 | 两套 EsimFile 不证明两个物理 eUICC 域；涉及开通道的 Get 也参加真实资源判断 |

**对应元素与缺口**：EuiccPrimitiveResult 双结果合理；P1-8 的参数、提示和期望责任需补齐，ResourceBarrier 正常出口尚未表达。未知结束证据只阻塞相关新屏障，不阻塞其他结构收口。

**迁移关闭点**：原 EsimManager 偏好直写、普通/低优先级 EsimFile 的共享资源入口、通道清理及 Profile 观察桥按切片交接；高层服务任务保留单一主编排者和同层重试责任。

## 5. 最小修正方案与变更影响

### 5.1 目录和组件关系

不建议新增上下文或建立平行架构。目录基本保留，修正职责与协议：

```text
services/multisim/
├── topology/
│   ├── application/   发现、重载、号码、激活；接纳序列与进度作用域
│   ├── domain/        必要账户投影片段、持续绑定版本、纯规则
│   └── ports/         独立投影读取、身份/建行计划、窄结果出口
├── assignment/        模式明确的Primary编排、角色片段、观察及重试
├── application/       请求机制；具名本地短请求及其完成协议
├── runtime/           原成熟协议、会话、加载、真实资源和结果桥
├── compatibility/     旧同步/异步完成、入口及接口投影
├── infrastructure/    原存储、参数、通知协议
└── assembly/          区分迁移附着与最终创建/拥有/匹配销毁
```

运行协作为 C→T/A 用例→R/I 窄端口；R 经结果 sink 回到发起用例，经带资格观察进入 T。H 构造并绑定关系。T/A 不持有 R/I 具体实现，R 也不依赖具体用例类。跨 Owner 只传不可变输入和正式应用提交，不持有对方可写聚合。

联合建行补“准备输入→角色计划→一次原存储调用→分别接纳”的协议；完成机制补各旧出口。不要继续向万能 Context 增加公共可写字段，也不要求新增总线、持久操作聚合或日志。

### 5.2 必要私有接口示例

以下仅说明需要表达的语义，未在本轮实现，不是新增公共 API 的授权。示例类型尚需结合前述接纳协议确定，不应直接复制为完整实现。

```cpp
struct CardDiscoveryObservation {
    SessionRef session;
    uint64_t observationSequence;
    std::string iccid;
};

enum class AccountReadView { Current, Historical };

// 原参数与经证实路由分别保存；VerifiedPortRoute 为待定义私有证据类型。
struct LocalProfileArguments {
    int32_t legacyPortIndex;
    bool forceDisableProfile;
    std::optional<VerifiedPortRoute> verifiedRoute;
};
```

还需明确四组接口约束：

1. 发现/撤销/替换观察的排序及 T 最终接纳资格，而非只有数据结构。
2. 建行前角色规划不依赖尚未产生的新 simId；存储后返回身份、效果和独立接纳结果。
3. Primary 模式、能力和设备观察足以决定原参数/设备偏序；R 不代写 A 参数。
4. 同步结果、异步 callback、内部回执具有不同完成出口，且不占据完成所需执行域。

### 5.3 扩槽与 Port 组合的修改面

| 变化 | 预计修改的目标责任 | 判断 |
| --- | --- | --- |
| 增加已证实的逻辑槽 | H/SlotCatalog、Core/SIM 装配与 RIL 路由、R 原硬编码和厂商分支、A 产品策略、C 可见性/旧 DTO、验证矩阵 | T/A 普通规则应以目录和键遍历承接，避免再次散落特殊槽号。仍须逐产品复现旧容量，槽数不证明更多 Modem 或并发 |
| 引入一槽多 Port/多当前 Profile | 上述责任之外，还包括 CardApplicationTarget/BindingRef、T 查找/发现/撤销、A 决策依赖、SessionRegistry/LoadScope 键、provider schema 与旧 DTO 投影 | 这是跨契约演进，不能仅称“改 SlotCatalog、BindingIndex、R 三处”。近期不必预建无证据的完整模型，但 G4 应准确列明影响面 |

### 5.4 替换存储或设备适配的修改面

| 变化 | 预计修改的目标责任 | 判断 |
| --- | --- | --- |
| 替换存储实现而保持原语义 | I 的 Account/Assignment Adapter、H 注入、协议能力与错误映射、相应验证 | T/A 用例原则上稳定；若 schema、事务、字段或完成保证变化，则需相应应用协议调整，不能假称只换 Adapter |
| 替换设备适配而保持原契约 | R 的 File/RIL/eUICC 执行桥、H 装配和资源键、错误/结束证据映射、产品矩阵 | 先修正 R 对具体结果 Owner 的依赖，T/A 才可稳定。当前借用原具体类型是合理迁移接缝，不为假想替换复制整套 File 接口；能力不足只影响对应切片 |

## 6. 迁移与实施可行性

### 6.1 写权、执行和重试按完整单元交接

新目录可通过原门面及私有应用入口连接旧实现，不必一次替换整个 SIM 子系统。连接之前应区分两种动作：

- G2 依赖收口仍由原 writer、原设备执行者和原重试者工作，保持解析、读源、错误、完成节点与通知。
- G3 按 WU 接管状态及全部入口；更早已转移的字段从切换时就委托新 Owner，不能等整条业务迁完才关闭旁路。

旧入口先委托、核对全部路径、受影响在途排空或隔离、导入分字段状态、一次交接写权，再删除旧直写。只读 shadow 不发设备命令、不写存储、不发原通知。未迁移用例也要遵守已经转移字段的唯一写权，不能形成长期双轨。

### 6.2 公共兼容与对象寿命

| 维度 | 接入要求 |
| --- | --- |
| 对外签名与 ABI | 保持原 API/IPC/IDL、虚表与消费侧接口；私有工厂不成为新公共 InnerKit。现有接口无虚析构时使用原控制块或匹配 deleter |
| 权限、错误与完成 | 按公开、CMI、内部入口分别保持检查和返回点；受理不等于业务完成，阶段结束不等于设备成功 |
| 读源与通知 | current/all、参数/last、卡缓存/账户/IMPU、存储查询分别保护；保留通知适用条件、次数与局部偏序 |
| Core/SIM 装配 | 保持共享 RIL→原 eSIM 初始化及发布→SIM 初始化/发布→搜网/CMI 的原时点；修正工厂创建边界，消费侧得到同一对象身份 |
| 共享 RIL | Core 拥有其生命周期，SIM 只释放自身注册与引用，不停止搜网等仍使用的实例 |
| 回调寿命 | weak owner、epoch、在途计数和排空共同保护；不能只看 shared_ptr 数量卸载含回调代码的 SO |
| 隔离与销毁 | 等待结束、设备效果未知、对象可安全销毁和 SO 可卸载分别判断；排空失败不卸载 |

### 6.3 四类实施门禁分别判断

| 门禁 | 当前可行性 | 不应混入的前提 |
| --- | --- | --- |
| 原 SO 内依赖收口 G2a | 可开始，保持旧执行与行为 | 不等待所有聚合、Port、provider CAS 或动态卸载 |
| 独立 SO G2b | 工厂、唯一定义处、符号与初始化关系闭合后推进 | 不以编译成功代替产物依赖与对象寿命证明，也不强制提前实现全部动态卸载 |
| 业务写权与并发增强 G3 | 对应 P1、旧入口和方法轨迹闭合后分批接管；增强单列 | 不用“以后适配”掩盖接口当前已承诺但无依据的能力 |
| 动态卸载/重建 G2c 等生命周期增强 | 有实际需求时单独验收 | 不把本地表清空当设备静止，也不把设备恢复日志自动升级为所有结构工作的前提 |

## 7. 下一批任务与验收条件

下表按依赖组织任务，不构成全部 P1 必须先关闭的总门禁。无关业务协议可并行深化；原 SO 内保持原行为的依赖收口不等待所有业务修订或外部证据。每个切片只以前述实际依赖为前置条件。

| 顺序 | 修改对象与范围 | 验收条件 |
| --- | --- | --- |
| 1. 协议修订 | 两份主要文档及相关骨架接口；关闭 P1-1～P1-8，按相同责任同步 P2 | 五场景的输入、事实 Owner、执行者、接纳点、部分效果、完成及旧路径关闭表均可完整推演；不新增上下文、公开 API 或占位成功 |
| 2. 原 SO 内 G2a 装配闭包 | Core HostPorts、原对象身份、共享 RIL、单例唯一定义处、迁移附着用途 | 原 writer/执行者/重试者唯一；原初始化和发布顺序保持，无新增反向具体依赖，不夹带目标固定或并发收紧 |
| 3. 真实可编译、可运行最小闭包 | 按主设计已有无卡/读入口方案连接真实生产对象；未实现能力不纳入目标 | 分别取得链接、初始化、真实事件输入及查询结果证据；平台未提供明确失败，不用业务成功桩替代 |
| 4. 首批完整状态单元迁移 | 账户切片覆盖初始化、重载、后台回写和角色片段；匹配 provider 未闭合时可先选不依赖它的 R 局部读取接纳切片 | 全部旧 writer 委托、实际写点准入、无双发/双写/双重试；正常、部分失败、迟到和交错轨迹通过对应差分 |
| 5. 独立 SO 工程验收 | 最终创建工厂、匹配销毁、导出、反链、对象身份和 Feature 产物性质 | 私有导出及依赖成立、失败路径受控、共享 RIL 身份不变；常驻拆分的限制明确，不宣称已有动态卸载 |
| 6. 逐用例启用行为增强 | 陈旧回写拒绝、切换准入、未知效果隔离、安全重试；动态卸载另列 | 每方法有旧/新轨迹、因果与结束证据、错误映射、预算及回退条件。外部证据不足保留相应用例旧路径，不扩大阻塞范围 |

## 8. 实际验证与本轮限度

原审视实际完成了基线/行尾核实、目标类型与成员检查、生产调用链、GN/版本脚本、定向外仓证据和五场景桌面推演。文档输出阶段检查本地引用路径、章节、Markdown 结构、空白及改动范围，并以输出前摘要核对已有跟踪文件未被改变。

本轮未执行生产编译、完整链接、OHOS 构建、业务 GoogleTest、fuzzer、LSP、IPC/ABI 实验、设备测试、真实交错复现或产品联调。主设计记载的历史语法检查与临时规则测试未在本轮重跑，不继承为本轮 PASS；接口示例、目录建议和验收条件均为后续建议，未实施修正。


## 9. 本轮裁决与修正记录

> 本节保留main整合前本地2448b42e工作树的实施记录；其中符号、测试路径和72单元结果对应当时工作树。与6697ff7a并行修正的整合采用关系、当前代码与复验见§10，不据本节认定旧符号仍是当前接口。


### 9.1 当前输入与实施边界

本轮任务为评估并实施修正。实际输入为本地分支 `docs/publish-local-doc-updates-20261029`、HEAD `2448b42e8976ffb75e2bfcd45b27c86eb1e3965d` 的工作树；不是原评审 HEAD。先检查目录、分支、HEAD、差异和适用 AGENTS.md，未同步远端、切换/恢复/合并分支。AGENTS.md 明确允许后续用户授权的代码工作，本轮按用户授权修改设计及未接入骨架。

原报告 §1～8 保持原文及其历史限制。两份主文档在原职责、协议、映射、分支和图中修订；代码改动限定在 `services/multisim/`，没有改生产 `services/sim/`、Core、公开 API/ABI/IDL/schema、GN、SA 或进程配置，没有转移写权或启用第二执行者。以本轮保存的 1276 个范围外跟踪文件字节摘要复核用户工作区，原有内容保持不变。未暂存、提交或推送。

以下裁决基于当前工作树的声明/实现及定向重读的生产函数：`MultiSimController::InitIccId/UpdateDataByIccId/SimDataBuilder/GetListFromDataBase/GetAllListFromDataBase/UpdateSubState/SetActiveCommonSim/SetPrimarySlotId/SetPrimarySlotIdWithoutModemReboot`、`CoreService::Init`、`EsimManager::OnInit/SwitchToProfile`、`EsimFile::SwitchToProfile`，并核对相关 Core/CMI、接口、Adapter 与构造关系。未将原报告的外仓版本或设备结论晋升为本轮新测结果。

### 9.2 逐项裁决与处理

表中源码路径以 `services/multisim/` 为根。“设计已修、骨架已修、局部通过”均不表示生产风险已消除。两份文档分别指主设计与流程对照。

| 问题编号 | 裁决及依据 | 修改文件与符号 | 已完成验证 | 剩余条件 |
| --- | --- | --- | --- | --- |
| P1-1 | **采纳。** 当前端口和模型确实压缩双投影。生产两次Query独立替换、失败与重试，且原bool返回不等于缓存替换成功。 | `topology/api/account_lifecycle.h`；`AccountRegistry::BeginReload/AcceptReload`及字段接纳实现；`AccountStore::LoadAccounts`、Adapter；主设计§8.2.1/§6.4、流程§10.3。 | 真实双投影状态机测试current成功/all失败及反向、独立重试/过期尝试、空成功/失败、current已改而historical失败、旧读不覆盖新字段/删除新成员。 | 设计/骨架/局部验证已修。实际DataShare错误映射、旧稀疏simId可见性和通知需G3a差分，未运行生产重载。 |
| P1-2 | **部分采纳，采用字段计划与分别接纳。** 新simId前置依赖成立；不把本地预检查当provider CAS，不承诺一次调用具有跨进程原子性。生产单卡旧卡恢复也写四角色MAIN，多卡恢复保留角色。 | `AccountRowPlan/Result`、`MatchAccountByIccid/ApplyAccountRow`；T的`DiscoverCard`协议；A的`AccountRoleProtocol`及角色准备/独立重载入口；主设计§13.2.1、流程§10.3。 | 新行身份为空仍能规划；单卡新建/旧卡恢复、多卡恢复、存前角色变化、存后已写版本失效、current/all角色独立失败、重复/无身份行拒绝。 | 纯角色资格已实现；真实匹配/Insert/Update/Unbind与完整发现编排仍未接provider，未填成功桩。匹配schema、实际返回身份及外部写者只阻塞相应存储接管。 |
| P1-3 | **采纳。** R加载成员校验不能覆盖T锁外存储窗口，发现入口确实缺资格。空位置也需延续版本。 | `CardDiscoverySink`；R的`FenceTopologyBeforeSessionPublication/PublishCardDiscovery`；`DiscoveryAdmission::Fence/Begin/CompleteStored`；`BindingIndex`；主设计§10.2.1、流程§10.3。 | A发现→B替换→A已写返回，实际`CompleteStored`拒绝绑定并要求核对；Unknown/缺身份/已知无效果/有效结果/重复结果，未知槽、无关槽、rev1→空2→再插3；同观察已知无效果显式重试、旧失败不影响新票据，Unknown不重发。 | 纯接纳及R路由逻辑已实现；生产事件桥须在新会话暴露前fence T，真正提交域/调用结束与字段核对仍待G3b。已写DB不因拒绝内存发布而消失。 |
| P1-4 | **采纳。** 生产搜网按slot读取，历史账户bool无法表达旧设备请求。 | 从`AccountRecord`移除进度；T应用`ActivationProgress::Prepare/Begin/Clear`按request/slot/sentSession/admission；激活流程改Hook后带资格清理并注入完成端口；主设计§10.2.2、流程§8。 | 旧清理/新请求、错误session及slot、重复清理、clear后旧Begin复活、账户移槽等实质状态测试通过。 | 槽进度与active、设备租约已分离；原搜网桥、force/卫星全部错误出口和Hook时序仍需生产差分。新资格增强未启用。 |
| P1-5 | **采纳。** 模式缺口仍在；生产第三卡优先、免重启先设备、M0无三阶段，不能统一预写。 | `PrimaryExecutionObservation/PrimaryMode`；`primary_switch_plan.*`；`RadioExecutionPort`；参数端口统一为`WritePrimaryParameters`，同步实现声明/注入；主设计§9.3、流程§9.3/9.4。 | 四模式成功/失败偏序、第三卡优先、非用户设置不走专用分支、controller缺失仍普通预写后失败、恢复/Done/重试/Data分支通过。 | 可执行计划已修；真实RIL/厂商/T换路及方法级旧返回差分未运行。第三卡只完成旧换路/登记节点，不承诺后续Primary收敛。 |
| P1-6 | **采纳。** 原Primary公开/CMI同步int不能被内部RequestId替代。 | `CompletionPlan::kind`、`CompletionPort`、`CompletionHostPort`、`CompletionCell`；`LegacySimFacade`同步Open→Start→Await桥；T激活/A注入完成出口；主设计§16.1.1、流程§8/9。 | 完成先于等待、等待时交付、超时/死亡/重复/晚到的一次完成规则通过；同步façade及全部构造语法通过。号码仍受理+callback，内部回执独立。 | 纯状态机无新增同步原语；真实Host FFRT等待、原预算/错误、CMI/IPC/通知适配尚未接入，不称生产同步调用已可运行。 |
| P1-7 | **采纳，限定独立SO切片。** 借用可用于同SO迁移，缺陷是将其视为最终创建边界。 | `AttachExistingSimModule`与`CreateOwnedSimModule/OwnedSimModule`分开；新增`ManagerPublicationSequence`；H构造/sink注入同步；主设计§7.1/§14.2.1、流程§6/§12.2。 | eSIM先发布再SIM、相同地址不同控制块拒绝、同一原控制块多消费者、创建侧deleter最后一次销毁、原OnInit false保留及eSIM关闭分支通过。 | 最终生产工厂/pimpl/deleter仍是明确契约，未创建生产对象。真实失败排空、共享RIL身份、私有导出/NEEDED/Feature/CFI/PAC/LTO待G2b/c，未宣称可安全卸载。 |
| P1-8 | **采纳参数及协作缺口，限定force影响。** 原force目前只被File保存，不能说本轮修复了设备编码；有效File返回后即写提示，与enum无关。非法槽还保留enum out不赋值。 | `SwitchProfileRequest`保留原port/force与独立verifiedRoute；`LocalCardRequests`真实短编排→A的`LocalProfilePreferencePort`→双结果；C只在optional enum有值时赋原out；R期望生命周期；主设计§7.2.1、流程§12.4。 | 原port/force透传、非法槽无期望/提示且out不变、卡失败仍提示、APDU效果Unknown、卡OK而关闭未知均保留原双结果；期望与后续观察分开，可信旧会话登记、期限清理、结束等待后仅核对、已发布新观察一次接续及旧EF仍无权均通过。 | 不复制66250任务；真实APDU/关闭、期望因果证据及T绑定观察仍待相应生产切片。外层成功不证明Profile/Radio成功，参数失败不覆盖旧返回。 |
| P2-1 | **采纳。** 整账户版本会因无关字段变化误拒绝。 | `NumberExecutionTarget`改为绑定/session和current/historical号码字段版本；`AccountView`携字段版本；`AccountRegistry`同值重载不递增无关版本；两份文档号码段同步。 | 写卡期间改名、仅名称变化的重载仍保留号码资格；真实号码冲突拒绝、缺投影部分失败及换绑定拒绝通过。 | 实际MSISDN和后台回写接管、同字段I/O预留及旧解析时点仍待G3b差分；未将执行点固定前移至IPC。 |
| P2-2 | **采纳。** R直接认识三个具体应用用例确实违背端口方向。 | `MsisdnResultSink/ActivationResultSink/RadioResultSink/CardDiscoverySink`，用例实现窄sink，H的`BindResultSinks`；主设计§5.2/§6.4、流程§6。 | 完整头/源语法、传递依赖扫描，以及用独立sink消费观察并拒绝旧观察的测试通过；R无具体应用服务include。 | 结果桥替换资格已表达；真实回包/投递失败、weak owner/epoch/排空仍待接入，不新增总线。 |
| P2-3 | **采纳。** 正常完成、未发撤销和Checking出口确实缺失。 | `ResourceBarrier`实现`TrackResource/AcceptInitialEvidence/Acquire/MarkSent/CancelNotSent/Complete/Quarantine/AcceptRecoveryEvidence`，租约保存资源集合/request/step/attempt/fence及query/mutation性质；主设计§12.3、流程§11.5。 | 正常释放、NotSent撤销、已发不能撤销、关闭未知隔离、旧/部分/伪造租约证据拒绝、query无旧payload可释放而write须核对、无关域可继续，通过。 | 纯状态机由R已有短域串行保护；没有设备静止证据生产者时不启用对应新屏障，不能用测试布尔证据证明硬件已结束。 |
| P2-4 | **采纳。** 卡→库→双缓存是兼容用例步骤，纯C++不使其成为Domain规则。 | `show_number_plan.*`从`topology/domain/`移到`topology/application/`，使用方/导航同步；Primary步骤计划同在Application。 | 实际计划偏序场景及全部引用/传递依赖检查通过，无两套计划实现。 | 结构修正完成；未借此改变生产步骤或新建聚合。 |
| P2-5 | **采纳模型修正；不存在已创建两个Manager的生产事实。** Core只有一个Manager，内部每槽两套File。 | `ExistingSimBindings::euicc`、`CardRuntime::euicc_`、构造注入及最终拥有边界；流程§6/§11.5/§12。 | 定向重读Core/EsimManager装配；单Manager构造一致、普通/低优先级共享资源身份规则及发布身份检查通过。 | 未改变真实eSIM初始化或通知；双File实际共享域、Feature及设备并发仍需产品证据，不能由对象数量推断独立eUICC。 |

### 9.3 验证、发现后的修正与剩余范围

本轮本地工作树当时实际执行以下检查，测试源和入口位于`services/multisim/tests/`；整合后入口迁入既有`test/multisim_architecture_rules/`，当前命令见§10：

```sh
python3 telephony_core_service/services/multisim/tests/run_host_checks.py
```

该入口用 Clang C++17、`-Wall -Wextra -Werror` 编译运行账户/发现/激活、Primary/完成/Profile、资源/工厂三套场景，并对全部72个目标头/源（含测试源）逐个执行独立语法检查，最终全部通过。测试直接调用新增纯状态转换与编排逻辑；Profile执行替身仅供注入结果、捕获原参数及观察副作用节点，不替生产能力返回成功。发布身份测试使用隔离对象验证控制块和匹配销毁，不冒充真实Manager初始化；生产工厂不补成功桩。另对拓扑组实际运行GCC及Clang严格构建/测试。

测试与复核发现的问题已继续修正并复验：无变化重载递增号码版本、旧激活Begin复活、清理未核对session.slot、单卡旧卡恢复漏四角色、非法Profile改写原out、Clang无用成员报错，以及资源只读查询无载荷不能正常释放、发现失败重试不应强求新R观察。新增同步设施改为已有宿主短域/FFRT接入契约，骨架没有引入std锁或伪FFRT。

两份设计中的18幅Mermaid图实际解析通过；PlantUML原图未改语法结构，本轮未渲染。代码本地include、Domain传递依赖、R不反依赖具体应用用例、端口声明/实现/构造及文档链接检查通过。指定范围`git diff --check`通过，范围外1276个跟踪文件摘要不变；未混入生产行为修改、已有行尾转换或无关清理。历史评审正文前缀保持字节一致。

**生产验证尚未执行。** 当前环境没有`hb`及完整OHOS产品构建入口；未运行OHOS链接、业务GoogleTest/fuzzer、真实初始化、IPC/ABI实验、设备、厂商、provider或66250联调。局部测试不证明匹配provider事务/外部唯一写者、设备结束/通道静止、Port/MEP/N槽能力。外部缺口沿§3.5限定：只阻塞真正依赖这些能力的存储接管、新资源屏障/安全重试、高层服务或新增产品能力；不阻塞同SO依赖收口和其他纯协议切片。

下一批适合推进G2a：在原SO中以真实HostPorts和原Manager身份连接一个无卡/查询最小闭包，完成实际FFRT同步出口与eSIM/SIM原发布时点核对；保留原writer/执行者/重试者。之后按已闭合协议选择完整WU切片。G3切换前仍需对应旧入口全覆盖、真实写点准入、方法级兼容差分与局部外部证据，不能把本轮骨架表达闭合写成生产风险消除。


## 10. 阶段一：main整合与复验（2026-10-30）

本次提交从远端`origin`（`https://github.com/zjuluowu/multisim.git`）的`main@6697ff7a487dcbd45b7eda0b2f6e75f4a671ae69`开始，在隔离worktree整理。main已包含原骨架基础及并行修正，旧本地HEAD为其祖先；未合入额外历史分支，也未整目录覆盖main。原工作区保持原分支/HEAD/已有修改，提交只含本轮协议修正、对应设计与测试；不含G2a生产接入、生产GN或原生产行为变化。

§9保留整合前的本地实施记录，不将其中旧符号复制成第二套模型。当前采用关系如下：

| 原问题 | main已有内容及本轮整合 | 当前代码/验证落点 |
| --- | --- | --- |
| P1-1、P2-1 | 保留main双投影/字段模型，补独立view尝试、成员删除接纳、同值字段版本不变和删除重建水位；原bool返回与替换资格分开 | `AccountRegistry::BeginReload/AcceptReload/AcceptReloadProjection`、`AccountLoadResult::legacyReturn`；topology规则含部分失败、旧读、无关名称变化及ABA |
| P1-2 | 保留窄`AccountRolePlanningPort`，联合新行不预设simId；单卡新建/旧卡恢复四角色MAIN、多卡恢复不代写角色；显式整组角色重载 | `AccountRoleProtocol`、`AccountDiscoveryWritePlan`、`AcceptReloadedRoleProjectionSet`；角色预留/存后核对/独立view测试，不承诺provider CAS |
| P1-3 | 使用main的`DiscoveryObservationLedger`，补`BeginStore/CompleteStored/EndPreparationFailure/RetryAfterKnownNoEffect`；确认调用结束且未生效才允许同观察显式重试 | 保留原epoch/Owner及持续空槽revision；旧发现、已写/Unknown核对、重复/重试场景。R在任何撤销当前会话前先`MayPublishVerifiedSession`拒绝旧候选 |
| P1-4 | 保留main完整epoch/Owner/step/session资格，`ActivationProgress::Prepare`签发admission，`Begin`消费并保留最后开始水位 | 旧清理、新请求、clear后旧Begin、账户移槽测试；兼容进度与A冲突登记、R租约仍分别维护 |
| P1-5、P2-4 | 保留main动态能力/Radio事实与模式，兼容步骤计划统一移Application；参数端口改`WritePrimaryParameters`覆盖前写与后写 | `assignment/application/primary_switch_plan.*`及原号码Application计划；四模式、失败偏序、第三卡优先测试 |
| P1-6 | 保留main的`CompletionRoute`三类出口及`CallbackHostPort`，补实际同步façade和纯`CompletionCell`一次完成 | 完成先于等待、超时/死亡/重复/晚到；真实FFRT等待依然未接入 |
| P1-7、P2-5 | 保留main单Manager/sharedRil、`AttachSimModuleToExisting/CreateOwnedSimServices`，补发布顺序/控制块身份纯逻辑 | `ManagerPublicationSequence`；eSIM先发布再SIM、不同控制块拒绝及匹配销毁测试。真实生产工厂仍声明 |
| P1-8 | 保留原Profile参数及窄A偏好端口，补实际短编排/enum未赋值表达和C出参投影；补期望生命周期 | `LocalCardRequests`、`legacy_profile_result.h`、`SessionRegistry`；非法槽out保持、失败仍提示、Unknown/关闭未知及一次有因果接续 |
| P2-2、P2-3 | 保留main窄sink与更完整的epoch/Owner/全资源集合屏障；补窄sink实际辅助及恢复资源可复用证据 | `RuntimeResultSinks`、`RecoveryEvidence::resourceReusable`；正常释放、NotSent、query/write、旧租约/错Owner负控 |

当前可复跑入口为：

```sh
python3 telephony_core_service/test/multisim_architecture_rules/run.py
```

整合后四套纯协议检查（topology、resource、assignment、runtime_protocol）实际编译运行通过；全部73个骨架头/源以Clang C++17 `-Wall -Wextra -Werror -fsyntax-only`独立检查通过。两份主文档17幅Mermaid实际解析通过，本地链接及`git diff --check`通过。测试使用真实纯规则与注入的结果载荷，不补生产设备/工厂成功桩。原§9的72单元/三套结果继续保留历史含义，本次以当前文件集合重跑。

阶段一未运行生产链接、Manager初始化、真实RIL/provider/设备或IPC；阶段二必须在远端main包含提交后另建工作区实施，不追加到本阶段提交。原工作区1352个已存在文件的字节检查点用于结束时复核；范围外用户修改不暂存、不清理、不重置。最终提交SHA及远端包含证据在后续阶段记录，不改写历史记录为当时已经推送。
