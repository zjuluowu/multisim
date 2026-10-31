# 多 SIM 源码目录组织架构决策：A / B+ / C 方案比较

## 1. 执行摘要

**推荐 B+，状态 Proposed。** 六组目录回答账户、选卡、卡能力、设备、集成和基础机制在哪里；Card 内保留 PIN/PUK、Records、STK、eSIM 等业务入口。源码支持卡协议与 Modem 执行/共享资源有不同变化原因，但不能按是否调用 RIL 划线，也不增加统一设备转发。相比 A 减少术语学习，相比 C 接受一层自然分组以明确整体地图。Owner 和 GN 边界不随目录合并；通道结束保证、Service 收窄及生产验证仍待独立验收。报告推送不等于批准或授权迁移。

## 2. 评审背景与代码基线

### 2.1 范围与 Git

| 项目 | 本轮核实结果 |
| --- | --- |
| 决策日期 | 2026-10-31 |
| 输入分支与 HEAD | `main@b86f9e4faa56c921d06113f7d2f99fb75ea021f7` |
| 上游跟踪 | `main -> origin/main`；origin 为 `https://github.com/zjuluowu/multisim.git` |
| 远端核对 | 开始时 ls-remote 与 fetch 后 origin/main 均为输入 HEAD；提交前、推送前再次检查，不覆盖远端新增历史 |
| 输入工作区/暂存区 | 均干净；本次提交仅允许本文件 |
| 生产装配 | 根 `tel_core_service` 仍使用 services/sim；本区域不是独立子系统或新增 SA |
| 原 SIM 实际范围 | services/sim/include 为 61 头，src 为 55 cpp；根 GN 51 基础源 + 3 条件源，共 54 个不同 SIM cpp |
| 未列入根产品源表 | sim_cache_sync_manager.cpp；不作为当前活动生产能力或随重组启用 |
| multisim 实际范围 | 88 文件：63 头、23 cpp、BUILD.gn、README.md；24 个 source_set 均 testonly |
| 其他实现 | G2a 检查点 cc787945792527eeb94bc20a5e955315d6269e83 在独立 work/g2a-readonly-checkpoint-20261030；其远端 tip 为 728ad77ed4c723efbb5cb1baf7b3f10bb9830240，main 不包含检查点 |
| 其他 worktree | 已核对列表，未切换、修改或混入其实现；不依赖本机临时路径作为证据 |

[根 GN][E01]将 SIM、RIL、Core、搜网编入原 SO。esim_manager.cpp 无条件列入；esim_controller.cpp、esim_file.cpp、start_osu_result_callback.cpp 是 core_service_support_esim 条件源。[Feature 配置][E02]从产品 parts 决定宏，不能用“关闭整个 card/esim 目录”替代现条件行为。源表存在不等于每个产品、运行分支都已经执行。

### 2.2 权威输入、复用与限制

读取[主设计 v1.2](SIM_Target_Architecture_Design_v1.2.md)的 Owner/依赖/异步/目录约束及[详细设计 0.4](SIM_Multisim_Source_Architecture_Detailed_Design.md)的当前/稳态、GN 和生产承接说明。主设计名义 version 为 1.2、draft/pending，YAML revision 仍为 2026-10-30-directory-adjudication-07，正文已有 10-31 路径调整；详细设计更新日期 2026-10-31。不能将元数据或本报告日期当作批准记录。

复用[SIM-001～062/WU-01～09](SIM_Architecture_Refactor_Design.md)、[流程对照](多SIM架构重构_设计说明与业务流程对照.md)、[执行状态](SIM_Refactor_Execution_State.md)、[接口契约](SIM管理对外接口说明_源码核实版.md)、[Opus](SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md)/[Gemini](SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md)评审。上一轮生产代码分析只保存在会话，未冒称本地存在独立报告；本轮复用定位线索并增量重读影响 A/B+/C 裁决的实际代码，不复制平行功能台账。

本轮已有检查：恢复工具验证检查点可达、main 未含 G2a；`python3 -B telephony_core_service/test/multisim_architecture_rules/check_structure.py` 返回 86 文件、156 include、24 target 的文本边界/环检查 PASS。没有运行 C++ 编译、GN gen/Ninja、单元/系统/设备或产品测试；历史 PASS 不继承。候选路径尚未实现，因此没有对候选构建或性能作 PASS 判断。

源码引用以输入 HEAD 为准，链接为相对当前报告的 Git 路径及 `#L行号`。未来重组后应按该 HEAD 查询历史证据，而非假设行号永久有效。

## 3. 设计原则与评价维度

| 维度 | 判断方法 | 不采用的替代指标 |
| --- | --- | --- |
| 架构不变量 | 唯一事实接纳、Session/Resource责任、依赖及兼容节点能否保持 | 目录名看起来像分层 |
| 整体源码可理解性 | 首次进入能否建立账户/选卡/卡/设备/集成/支持六类地图 | 单纯一级目录更少 |
| 局部业务可发现性 | T1～T8 首选入口是否符合常用术语，是否有多个合理落点 | 路径少一层即效率更高 |
| 长期变化隔离 | V1～V4 的必要修改 M 与额外传播 P 是否可分辨 | 修改目录数或零传播承诺 |
| 工程与迁移成本 | 现有类型、GN、消费者、测试和生命周期能否分批保护 | 每目录一个 target/SO，或统一 include/src |

优先级依次是：不变量、整体地图、局部定位、稳定变化原因、演进可预测、可执行依赖约束、维护成本、迁移成本。不设置主观总分，也不虚构开发耗时、Token 节省或生产效率。物理分组、逻辑层、namespace、target、SO/运行对象分别评价。

## 4. A / B+ / C 方案说明

### 4.1 A：当前架构职责目录

下树先列当前实际子目录；注释中的未来能力尚未创建，不能按计划统计现有文件。

```text
services/multisim/
├── topology/
│   ├── accounts/
│   ├── discovery/
│   ├── activation/
│   ├── account_profile/
│   ├── api/
│   └── adapters/
├── assignment/
│   ├── domain/
│   ├── primary/
│   ├── account_roles/
│   ├── api/
│   └── adapters/                 # 未来 roles 承接 Voice/SMS/Data 用例
├── runtime/
│   ├── state/
│   ├── records/
│   ├── euicc/
│   ├── session/
│   ├── resource/
│   └── api/                      # 未来 phonebook/sms_storage/stk/carrier/radio
├── foundation/
│   └── orchestration/
├── compatibility/
│   ├── api/
│   └── output/                   # 未来 monitor
├── assembly/
└── include/                      # infrastructure 当前无文件，不创建占位
```

A 的稳态设计允许 infrastructure 仅放真正共享技术客户端；当前磁盘是七个一级目录，不是八个都已存在。T/A/R 与父目录直观对应，已有 reviewer、GN、测试和历史导航迁移成本最低，这是 A 现阶段不可替代的实施基线价值。它并非因术语抽象就错误。

不足是 topology/assignment/runtime 需要先解释；Runtime 同时容纳卡自身协议与设备执行，静态能力另在 foundation。api/accounts/adapters/场景并列，容易对“号码规则放哪里”产生多种答案。A 完整生产能力仍须按原详细设计扩展，不以小骨架掩盖规模。

### 4.2 B+：稳定功能分组，Card 与 Device 分开

```text
services/multisim/
├── account/{discovery,activation,number,profile}/
├── card_selection/{default_roles,primary}/
├── card/
│   ├── state/
│   ├── security/
│   ├── records/{files,codec}/
│   ├── phonebook/
│   ├── sms_storage/
│   ├── stk/
│   ├── carrier/
│   └── esim/{profile,channel,provisioning}/
├── device/
│   ├── slot_catalog.*
│   ├── radio/
│   ├── session/
│   └── resource/
├── integration/{api,events,lifecycle,storage}/
└── support/request/
```

六组分别是 T、A、R 的卡能力部分、R/H 的设备部分、C/I/H 技术集成及 O/纯值支撑。不是六个 Context、六个服务或六个 target。专属规则、用例、契约和 Adapter 随功能；少量纯事实模型与对外窄契约平放所属组根部，用显式源集合保护逻辑层，不机械复建 api/domain/adapters。

主要收益是卡能力与设备执行形成显式父子地图，同时具体能力仍可在二级按业务名直接找到。主要风险是把 card/device误建成总服务、把integration/support变成公共业务池；§5/§8给出禁止项。完整稳态树见§12。

### 4.3 C：生产能力直接一级可见

```text
services/multisim/
├── account/{discovery,number,profile}/
├── activation/
├── card_selection/{default_roles,primary}/
├── state/
├── security/
├── records/{files,codec}/
├── phonebook/
├── sms_storage/
├── stk/
├── carrier/
├── esim/{profile,channel,provisioning}/
├── radio/
├── device/{slot_catalog.*,session,resource}/
├── support/request/
└── integration/{api,events,lifecycle,storage}/
```

C 有十五个一级功能/支撑入口，已知目标为 STK、PIN、Records 时最直接。逻辑 T 跨 account/activation，R 跨卡能力、radio/device；仍需与 B+ 相同的接口和 GN 约束。

C 并未天然增加依赖或破坏 Owner。它的取舍是整体阅读时需要把多个顶层能力重新归为“卡自身功能”，把 radio 与 device 组合为“硬件执行/资格”。这是分类解释成本的推断，不是已测量的开发效率损失。一级数量不是否决理由。

## 5. Card 与 Device 专题分析

### 5.1 边界成立，但不是硬隔离的调用流水线

Card 回答卡提供什么功能、卡协议如何处理、能力状态和结果如何接纳；Device 回答跨能力硬件如何执行、共享执行资格如何获得/结束以及设备会话何时有效。前者主要随卡规格、文件/安全/Toolkit/Profile演进，后者主要随Modem握手、平台接口、并发资源域和代次规则演进。

正证据是 RadioProtocolController 的跨槽 phase/session/Modem 观察与 PIN、EF、STK 的专属协议明显不同。[E10][E11][E12][E13] 反证据是 PIN/EF/STK/APDU 自己直接使用 RIL，当前卡态和 eSIM 又含业务副作用。[E11][E14][E17] 因而只支持按语义、状态和变化原因分组，**不支持“凡调用RIL就搬Device”或“改完目录已解耦”。**

### 5.2 真实场景与责任分配

| 场景/代码证据 | 留 Card 的内容 | Device 的内容 | 其他 Owner / 不应采取的做法 |
| --- | --- | --- | --- |
| PIN/PUK：[StateHandle::UnlockPin](../services/sim/src/sim_state_handle.cpp#L204) 创建专属事件后直接调用 ITelRilManager；StateManager::UnlockPin 等待、映射结果并恢复状态 [E11] | card/security 的命令语义、剩余次数、专属等待/结果；card/state 接纳公开卡态 | 仅必要的会话有效性、已证明需共享的硬件资格 | 不增加 PIN→DeviceManager→RIL 转发；不复制公开卡态 |
| Records/MSISDN：SimFile::UpdateMsisdnNumber 复用 DiallingNumbersHandler 写 EF_MSISDN [E12] | EF解析、卡缓存、加载/Refresh及号码结果；窄复用 phonebook Handler | 共享设备资格若确实需要，由具名契约提供 | account/number主编排账户号码，不把T资料和R缓存合并；不复制ADN协议 |
| STK：Controller订阅专属事件并直接发TerminalResponse [E13] | Toolkit session、主动命令、等待、业务重发、专属BMS/输出 | 必要设备有效性与底层共享执行约束 | STK session不是卡应用SessionRegistry；专属通知不中央化 |
| eSIM：EsimFile开关通道、同AID复用、APDU/BPP、Profile参数 [E15][E16] | card/esim/channel持AID、逻辑channelId、APDU分片、开关响应及专属关联；Profile/Port请求在profile能力 | device/resource持已证明真实共享物理域的资格；session持应用代次及可信转换 | T持已接受绑定；A持偏好。不得各建Profile/Port权威事实或重复分配同一物理租约 |
| Primary：Controller选择分支/保存参数/业务重试，RadioController跨槽握手 [E07][E10] | 无需把Radio握手塞进Card | device/radio执行模式所需命令、phase/session、原始反馈 | A在card_selection/primary做决策、参数恢复、重试；参数回滚不冒充设备回滚 |
| 激活：SetActiveCommonSim与SetActiveSimToRil [E05][E10] | Card消费设备观察并处理卡自身状态/后续加载 | device/radio只做激活设备执行及结果关联 | account/activation属于T，唯一接纳active/业务进度；A负责业务冲突准入 |

卡能力可以直接依赖受控的 ITelRilManager/消费方窄执行契约，不因此反向依赖 Device 具体实现。也不为目录整齐给每个方法创造新虚接口。Device 只在真实共享资格、跨槽执行或稳定平台差异需要时参与。

### 5.3 eSIM通道：专属协议状态与真实资源资格

**现状事实：** EsimManager 对同slot创建普通与低优先级两套 EsimFile，同借一个RIL；部分通知操作走低优先级数组。[E15] [currentChannelId](../services/sim/include/esim_file.h#L244)、AID、occupy/open/close锁和CV是EsimFile实例成员，不是两个对象/不同Port的共同仲裁器。[E16] LowPriority名称不能证明设备QoS或抢占。

SyncCloseChannel 最多两次等待后清本地channelId/AID；[ProcessEsimCloseChannelDone](../services/sim/src/esim_file.cpp#L437) 不解析回包错误就清本地值。迟到开通道回包尝试关闭spare channel，但不足以证明所有异常和厂商路径都已静止。[E16] 因此不能从本地清零生成“资源可重用”证据。

**目标分权：** card/esim/channel保有卡协议状态机和逻辑句柄；device/resource仅持真实共享域的租约、隔离和结束资格；device/session仅持卡应用会话代次/可信ExpectedTransition。channelId可以被关联到资格，但不能再在Device复制一套权威AID/Profile/协议状态。共享域究竟按eUICC、Modem还是其他范围划分，须产品/命令级证据确定，不能按软件对象数推定。[E23][E24]

这支持B+的物理归类，**不表示当前生产已有共享资源屏障**。原关闭、超时、重试语义先保护；新增隔离/释放/重发政策属于后续行为变更。

### 5.4 卡状态、设备状态、业务状态分开

| 状态/事实 | 唯一责任 | 禁止混同 |
| --- | --- | --- |
| 身份、绑定、active、账户加载与激活业务进度 | T：account受控入口与对应场景 | 不由Card/Device回包直接代写 |
| Voice/SMS/Data、Primary意图/参数、业务冲突准入/重试 | A：card_selection | 不等于真实设备租约或驻网/数据成功 |
| ABSENT/READY、锁因、公开卡态 | R：card/state具体状态接纳者 | 不等于T账户active、设备可复用资格 |
| PIN/PUK结果/剩余次数 | R：card/security请求责任者 | 不建立第二份公开卡态 |
| EF缓存、加载成员、Refresh/LOADED | R：card/records | LOADED不是Session/资源释放证明 |
| STK会话、eSIM逻辑通道 | 各Card协议能力 | 不因同名session/channel合成Device通用状态 |
| 卡应用SessionRef、epoch/代次、可信接续 | R：device/session | 等待结束不结束设备效果 |
| 真实资源租约、栅栏、隔离、结束/可重用资格 | R：device/resource | A业务准入不替代设备结束证明 |
| Radio握手phase/session及原始反馈 | R：device/radio | 协议session不抹掉RIL serial或业务RequestId |
| Request/Step/等待/完成机制 | O代码由各Owner实例化 | 不新增全局请求签发/业务调度/重试中心 |

### 5.5 对边界八个问题的结论

1. 两类变化原因相对独立，Radio跨槽握手与卡专属协议提供正证据；不是完全独立。
2. PIN/STK/EF与Modem握手容易分流；插拔恢复、Port/通道和Modem重启必须读契约，目录不能消除跨边界流程。
3. 状态边界可明确到上表，但当前旧类混合实现，尚未完成移交。
4. StateHandle、Controller、EsimFile等确需按事实/执行拆分，详见§9。
5. Modem替换可主要触及device和Card窄执行适配；前提是卡语义/错误/完成契约等值，不能承诺T/A绝对不变。
6. eSIM卡规格变化主要在card/esim，绑定/偏好/资源变化有明确协作者；不新增双通道/双Profile Owner。
7. CardRuntime/未来DeviceManager是实质风险，禁止以两个父目录为由新建两个总服务。
8. 相比C，B+显式表达卡能力集合与硬件执行集合，接受一层导航；整体地图收益有模拟定位与职责证据，尚无真人效率实测。

## 6. 真实业务任务定位对比

### 6.1 方法与独立冷启动结果

本轮使用未继承架构上下文的独立Agent，第一阶段只给T1～T8、三个完整候选树和相同篇幅的简短导航，禁止读仓库。A明确标出未来卡能力，三树均列静态slot_catalog，避免以漏项制造优劣。随后开放定点真实源码及Owner约束核对。主Agent的既有知识没有冒充盲测。

第一阶段：三者均能找到Phonebook/STK/SMS；A识别PIN需要“在state”的说明；B+/C的number、default_roles、security更明确。C已知能力少一层，B+给出Card父类含义；C的radio与device关系需重组理解。三树都无法仅凭树证明Owner、测试、真实依赖或Port绑定。

第二阶段结果与限制见本节末；以下为结合生产证据后的定位，不把定点读取当自由检索效率或真实用户研究。

| 任务 | 真实生产主入口/关键写点 | A首选入口 | B+首选入口 | C首选入口 | 主要Owner、歧义及必要协作 |
| --- | --- | --- | --- | --- | --- |
| T1 插入/初始化/发现/激活 | SimManager::OnInit；Monitor::InitData/RefreshData；Controller::InitData/SetActiveCommonSim [E03][E04][E05] | topology/discovery、activation；runtime/state/records；assembly | account/discovery、activation；card/state/records；integration/lifecycle | account/discovery、activation、state/records；integration/lifecycle | T账户与激活，R观察/加载，A后续选卡，H对象启动。激活不是所有初始化；复合任务无唯一目录 |
| T2 默认卡/Primary | Controller Voice/SMS/Data入口、Primary四类分支/重试 [E06][E07] | assignment/roles、primary；runtime/radio | card_selection/default_roles、primary；device/radio | card_selection/default_roles、primary；radio | A决策，T身份/active视图，R执行。默认Data参数路径与Voice/SMS DB不同，不强行统一 |
| T3 号码/Records | Controller::Get/SetShowNumber、SetShowNumberToDB；SimFile::UpdateMsisdnNumber/完成 [E08][E12] | topology/account_profile；runtime/records | account/number；card/records、phonebook | account/number；records、phonebook | T账户号码、R卡缓存/EF，Core提供IMPU，C完成。卡更新不等于账户全部成功；codec只在编码问题时阅读 |
| T4 PIN/PUK/恢复 | Core/Manager→StateManager::UnlockPin→Handle→RIL、结果/CV/卡态恢复 [E11] | runtime/state | card/security、state | security、state | 请求结果和公开卡态均R但不同责任；扩大到插拔/账户重载时才协作T/A；不默认先改device |
| T5 电话簿/STK/卡短信 | DiallingNumbersManager/Cache/Handler；StkController；SimSmsController [E12][E13][E18] | runtime/phonebook、stk、sms_storage | card/phonebook、stk、sms_storage | phonebook、stk、sms_storage | 主要R能力内，按操作读Records/安全/专属事件；无需经过T/A或统一Device门面 |
| T6 eSIM Profile/Port/APDU | EsimManager::SwitchToProfile；EsimFile开关通道/Profile及BPP [E15][E16][E17] | runtime/euicc；T绑定/A偏好 | card/esim/{profile,channel,provisioning}；device/session/resource | esim同子包；device/session/resource | R卡协议，T绑定/标签，A偏好；Port请求不是已接纳绑定；关闭响应不是通用释放证据 |
| T7 四卡扩展 | InitTelExtraModule特例、GetMax/GetRealSimCount、Primary slot3等 [E03][E07] | foundation/slot_catalog；T/A/R及assembly | device/slot_catalog；account、card_selection、device及lifecycle | 同B+，Card能力在一级 | H静态容量、T绑定、A策略、R硬件/C容量投影。catalog仅首查点，不是只改常量或配置 |
| T8 新Modem/状态/RIL迁移 | RadioProtocolController；StateHandle事件/副作用；TelRilSim消息 [E10][E14][E19] | runtime/radio/state/session/resource；compatibility | device/radio/session/resource；card/state及能力适配；integration | radio、device；state及其他能力；integration | R执行与卡协议分流；语义改变才联动T/A/C。共享消息不能反依整个Card/Device |

### 6.2 必要阅读和测试入口

测试路径均相对 telephony_core_service/；这里只确认入口，未执行或宣称覆盖完整Oracle。

| 任务 | 必须阅读的源码/契约范围 | 现有测试入口与后续缺口 |
| --- | --- | --- |
| T1 | SimManager装配、Monitor事件、T发现/激活、ActivationRuntime/AssignmentPort、R卡状态/加载 | test/unittest/sim_gtest/{sim_manager,multi_sim_controller}_gtest.cpp；test/unittest/sim_state_handle_gtest；新增真实插拔/force/卫星/Hook差分 |
| T2 | AssignmentService/聚合、PrimarySwitchPlan、PrimaryTopologyPort、RadioExecutionPort、原参数/广播 | test/unittest/sim_gtest/multi_sim_controller_gtest.cpp；test/multisim_architecture_rules/assignment_rules.cpp、resource_rules.cpp；补四模式、失败恢复/重试及Feature |
| T3 | 三个号码文件族、T账户/Store、R MSISDN/Handler、Core IMPU/完成 | test/unittest/sim_gtest/{multi_sim_controller,sim_file}_gtest.cpp；test/unittest/icc_dialling_numbers_handler_gtest；补卡→DB→两缓存→通知/callback及查询回写Oracle |
| T4 | StateManager等待/恢复、Handle结果、PIN请求契约、R状态/Session资格 | test/unittest/sim_state_handle_gtest；test/unittest/sim_gtest/sim_manager_gtest.cpp；认证fuzzer；补超时/剩余次数/恢复轨迹 |
| T5 | 对应Manager/Controller/Cache、专属事件及FileController | test/unittest/icc_dialling_numbers_handler_gtest；test/unittest/sim_gtest/{usim_dialling_numbers_service,sim_sms_controller}_gtest.cpp；test/unittest/core_service_gtest/tel_stk_controller_test.cpp；补PBR/重发/缓存失效 |
| T6 | 普通/低优先级EsimFile、channel/Profile双结果、A偏好/T提示、资源/Session契约 | test/unittest/esim_gtest、test/unittest/esim_parcel_gtest；test/multisim_architecture_rules/{assignment_rules,runtime_protocol_rules}.cpp；补跨实例竞争/关闭失败/迟到/MEP及设备结束证据 |
| T7 | 容量宏各TU、SlotCatalog、绑定、角色、硬件路由、Core过滤/装配 | test/multisim_architecture_rules/{topology_rules,assignment_rules,resource_rules}.cpp；test/linux_sim_guard容量设施；匹配产品Feature与真实并发矩阵仍缺 |
| T8 | 指定R能力/协议及RIL消息，错误/完成/取消/超时契约，必要的T/A/C消费点 | test/unittest/tel_ril_gtest/tel_ril_sim_test.cpp；相应能力测试；新旧设备轨迹和产品联调需独立运行 |

第二阶段实际读取5个源码文件及2个入口/状态文件，使用3次functions.exec（其中4次exec_command）；这是得到函数位置后的定点核对，不作方案间检索动作对照。定点核查确认：号码任务必须区分T用例与R Records；PIN请求不强制经过Device；eSIM通道和Device资源不等价，Port/初始化/恢复仍需接口证据。此结论不是所有分支或调用跳数的完备测量。目录深度、搜索动作、逻辑模块阅读范围不可互相代替。

## 7. 整体源码结构认知成本

| 初次打开的六个问题 | A | B+ | C |
| --- | --- | --- | --- |
| 账户在哪里 | 需把Topology解释为身份/绑定/资料/激活 | account | account + activation |
| 默认卡/Primary在哪里 | 需把Assignment解释为业务选卡 | card_selection | card_selection |
| 卡自身功能在哪里 | Runtime中筛出卡能力，区分设备执行 | card，各二级能力名直接可见 | 多个一级目录，由读者归成卡能力组 |
| Modem/Radio执行在哪里 | Runtime的radio/session/resource；静态能力在Foundation | device：radio/session/resource/catalog | radio + device |
| 系统集成在哪里 | compatibility、assembly、窄infrastructure及Owner适配 | integration；专属适配仍就近 | 同B+ |
| 基础值/请求机制在哪里 | Foundation含O但需解释不同target | support，静态硬件目录另在device | 同B+ |

局部任务已知“STK”时，C直接看到stk比B+的card/stk少一层，这是可观察的结构差异；两者都有完整业务名，**没有证据表明这点足以抵消或证明整体理解成本**。B+把这种多一层转化为“这是卡功能”的分类信息，Device则表达跨能力硬件执行和共享资格。

Card会随生产迁入成为代码规模最大的分组，但大文件集合不等于大依赖中心：不建总头、不建CardManager、不把能力都挂到单一CardRuntime，能力间只暴露实际需要的窄契约。新增卡型/协议一般进入既有二级能力；只有独立变化原因出现才增能力目录，不无限新增一级。Device不吸收所有RIL调用，因此也不应承接EF解析、STK重发、PIN结果或Profile协议。

C的平铺成本属于本轮独立模拟及源码语义支持的**架构判断**，不是已发生的维护故障或真人耗时结论。对于只维护少数单项能力的团队，C仍有合理优势；本任务要求理解完整生产模块，优先选择B+。A的原结构/历史认知与工程稳定性继续作为迁移对照，不能直接抹去。

## 8. T/A/O/R 架构边界与 GN 约束

### 8.1 物理分组不改变逻辑Writer

```text
外部入口 C -> T(account) / A(card_selection) / R(具名Card能力)
T激活用例 -> A业务准入 + R设备执行 -> T原节点接纳active
A选卡用例 -> T窄只读视图 + R设备执行 -> A原节点接纳角色/分配
R Card能力 <-> Device窄资格/结果契约（按实际需要，不是强制逐级转发）
O提供每Owner私有请求机制；H生命周期组合根注入同一套对象
```

这是运行协作说明，不是编译target图。Card与Device具体实现target不得互相包含；Device所需Card观察/结果通过独立窄契约注入，契约target仍须成DAG。T/A可双向协作，但具体实现target不能互相include。T账户/active/激活进度仍唯一；A角色、偏好、业务冲突与重试仍唯一；Card和Device不各建Session或Resource。静态SlotCatalog由H构造只读，T持动态绑定，A持分配意图/准入，R持设备观察/租约。[E20][E21][E23][E24][E25]

### 8.2 保留现有构建隔离，准确说明当前不足

现BUILD的sim_values、request_protocol、request_mechanism、T/A types/contracts/domain/cases/adapters及R机制/能力/装配不与目录一一对应。[E26] B+路径阶段可以保留24个target及deps，仅更新源路径和文件→逻辑target分类；不能把所有card合成一个target，或把device/slot_catalog通过总device依赖连到RIL。纯规则只取得纯值/自身types，不取得O等待、平台或设备实现。

当前GN存在public/check_includes/deps边界，但visibility通常为同BUILD的 `:*` 加架构测试，内部范围较宽；Service和聚合头也在选定target的public中。Owner禁止边主要由显式deps和check_structure.allowed_edge补充，检查器并未验证visibility取值。[E26][E27] 因此不能宣称现GN visibility已单独实现精确Owner访问控制。

后续可独立收窄visibility/public白名单；先以反例护栏证明越界会失败，再谈约束增强，不与纯移动混验。当前统一namespace仍未实现命名空间隔离；改目录不要求机械改符号、类布局或公共ABI。

### 8.3 RIL及公共消息

TelRilSim引用SIM的消息和工具，使用IccToRilMsg/IccFromRilMsg及controlHolder回传。[E19] 禁止 `RIL -> 整个Card/Device实现 -> RIL`。共享消息/可独立codec应精确导出，平台事件类型不冒充纯值；MccPool的FFRT初始化/搜网消费者、PlmnFile的Parcel、VoiceMailConstants的Core/EXT都要逐项审计，不能打包进纯support。

不同目录不强制不同SO；原tel_core_service、API/ABI/IDL/schema、Feature、SA发布、RIL共享控制块和销毁次序保持，部署变化按原G2b/G2c门禁验收。

### 8.4 三个集中式Service风险

| 类及证据 | 应保留 | 不应继续聚集 | 与本次路径工作的界限 |
| --- | --- | --- | --- |
| TopologyService [E20] | 唯一账户/绑定/资料/进度接纳，受控短提交域 | 所有发现/号码/激活/映射步骤塞同cpp | 独立场景类可原样移动；生产事实写点拆分另验收 |
| AssignmentService [E21] | 唯一角色/投影、Primary与激活共用业务冲突登记 | 总选卡流程、设备协议/等待和所有业务重试执行混在一起 | Primary用例收窄不复制ConflictRegistration或角色缓存 |
| CardRuntime [E22] | 过渡期必要绑定/窄结果接缝 | 号码/PIN/EF/Profile/STK/Phonebook总入口及全部能力状态 | 当前多方法仅声明/伪代码，少量发现/失效/结束委托已有实现；不能称已薄化。可先私有保留，再按能力分步替换 |

B+的Card父目录不授权CardService，Device父目录不授权DeviceManager。只有H装配允许集中连接对象，且不持业务分支。support/request中的RuntimeOperationStore按Owner实例化，不提供统一队列或业务发令器。[E28]

## 9. 原生产文件和职责归属

以下是既有功能/WU台账的目录投影，不是新的迁移功能清单；所有候选均保护原接口。A栏使用现有稳态设计，B+栏为本报告建议，C栏为上轮能力平铺建议。

| 原文件或类 | 实际主要责任 | A归属 | B+归属 | C归属 | 是否需拆分职责 |
| --- | --- | --- | --- | --- | --- |
| SimManager | 外部门面、槽对象装配、能力委托 | compatibility/assembly + runtime | integration/api/lifecycle + 具名Card/Device | integration + 各一级能力 | 是；不能只改成CardManager [E03] |
| MultiSimController | 账户、active、默认角色、Primary、号码、设备等待 | T/A/runtime多处 | account、card_selection、device/radio | account、activation、card_selection、radio | 是；按WU全写入口而非整类搬 [E04]～[E08] |
| MultiSimMonitor | 设备/系统订阅、账户加载重试、callback/death/Registry | T发现 + C monitor/output | account/discovery + integration/events及能力输入 | account/discovery + integration/events | 是；加载资格不能归纯技术事件层 [E04] |
| SimStateManager/Handle/IccState | 卡态、安全请求/结果，夹杂账户清理及Primary反应 | runtime/state + T/A接缝 | card/state、security + T/A受控观察 | state、security + T/A | 是；ABSENT/UNKNOWN副作用需保护原时点 [E11][E14] |
| SimFile/SimFileManager/各卡型File | 卡缓存、加载、Refresh、MSISDN、语音信箱/配置查询 | runtime/records | card/records/同名实现族 | records/同名实现族 | 可先搬路径；收窄FileManager和跨能力副作用另切片 |
| IccFileController/卡型Controller | EF读写、GetSimIO、响应解析 | runtime/records | card/records/files/同名文件 | records/files/同名文件 | 稳定协议可机械移；消费者/消息导出先审计 |
| IccDiallingNumbersManager/Cache/Handler、USIM服务 | ADN/FDN、PBR/ANR/IAP、缓存/请求 | runtime/phonebook | card/phonebook/同名文件 | phonebook/同名文件 | 保留Records窄复用、PIN2、static关联，不复制协议 [E12] |
| SimSmsManager/Controller | 卡短信RIL/EF、列表/等待/静态锁 | runtime/sms_storage | card/sms_storage/同名文件 | sms_storage/同名文件 | 多数可先移动，锁/等待语义不顺改 [E18] |
| StkManager/Controller | Toolkit协议、专属订阅/输出/重发 | runtime/stk | card/stk/同名文件 | stk/同名文件 | 专属系统交互随能力，不全塞integration [E13] |
| SimAccountManager、ConfigCache、PDP helper | 运营商配置/APN/权限装配 | runtime/carrier | card/carrier/config/同名文件 | carrier/config/同名文件 | 不因类名进account；配置/权限后续可分 [E09] |
| SimStateTracker、Loader/MatchingRule/Parser | Records就绪后规则匹配和配置更新 | runtime/carrier | card/carrier/matching/同名文件 | carrier/matching/同名文件 | 不按State名字归state；保留卡数据读写责任 |
| IccOperatorPrivilegeController/Rule | 卡规则/证书权限及通道 | runtime/carrier | card/carrier/privilege/同名文件 | carrier/privilege/同名文件 | 共享资源资格与专属规则分开 |
| EsimManager/File/Controller、OSU callback | 卡Profile/Port、通道、BPP/通知、验证，含T/A提示 | runtime/euicc + T/A接缝 | card/esim根及profile/channel/provisioning；T/A提示归对应入口 | esim同分包 + T/A | 是；卡协议不整体移Device，远端任务不复制 [E15]～[E17] |
| RadioProtocolController | 跨槽Radio协议、phase/session、激活设备命令 | runtime/radio | device/radio/radio_protocol_controller.* | radio/radio_protocol_controller.* | 文件主责稳定；Controller中专用Primary执行另抽 [E10] |
| CardFileDetectManager | 探测资料、独立账户Insert/Update、缓存刷新 | T adapters/discovery + R输入 | account/discovery中的资料接纳/专属Adapter + card/records输入 | account/discovery + records | 是；不能漏掉第二账户写入口 [E29] |
| MultiSimHelper/SimRdbHelper | 一次建行含T/A字段、专属CRUD/广播 | T/A adapters、C输出、窄I | account与card_selection各自字段计划，专属适配；仅技术连接入integration/storage | 同B+逻辑Owner | 是；A规划角色，T组织一次物理写，不各插一行 |
| SimUtils/SimDataType/SimConstant | 平台事件、协议消息、部分工具/类型 | 窄导出 + foundation/能力 | 精确消息契约、能力工具；真正纯值入support | 同B+ | 需消费者审计/拆取，不能总头池 [E19] |
| SimCharDecode/SimNumberDecode/TagService | 编码解码/TLV | runtime/records/codec | card/records/codec候选，窄target可跨消费者 | records/codec | 先证实独立依赖，保留一个实现 |
| MccPool/PlmnFile/VoiceMailConstants | MCC表初始化、Parcel、语音信箱配置 | 共享窄单元 / records | MCC表随carrier并精确导出；PLMN按消费者；VoiceMail在card/records | carrier / records | 非整体纯codec；实际共享/平台依赖先审计 |
| SimCacheSyncManager | 未装配用户切换缓存同步源 | 未启用候选 | 不迁入活动生产路径 | 同左 | 单独确认去留，不因本报告启用 [E01] |

“多数可先移动”仅指保持现有对象、target和行为的路径调整，不代表已关闭纯规则/平台耦合。对esim、carrier、records的大文件拆分按实证规模实施；未迁入能力不提前建空目录或成功桩。

## 10. 长期变化分析

M=必要检查/修改，P=组织方式可能额外引入的传播，S=输入/错误/完成契约等值时预计稳定，?=证据不足。单位是逻辑责任，不按目录数计成本。

| 场景 | M：各方案相同的必要影响 | A / B+ / C 的P及避免方式 | S | ? |
| --- | --- | --- | --- | --- |
| V1 三卡→四卡 | H静态能力/装配、T绑定、A候选/冲突、R路由/并发、C容量投影 | A可能各Owner重复槽解释；B+若device/catalog吞动态绑定/策略，会迫使T/A随硬件内部变化；C虽同有device，能力平铺仍可能复制容量常量。保持catalog只读/事实分权 | 设备语义等值时PIN/EF解码、Phonebook/STK协议 | 不同TU容量宏、EXT、slot2/slot3特例、产品并发能力 |
| V2 MEP/Profile/Port | Card eSIM协议，T已接纳身份/绑定，A必要偏好，Device共享资格/代次 | A大Runtime、B+ Card/Device双造channel/Profile状态、C平铺导致多处映射都可能增P。AID/APDU/逻辑channel留Card，物理资格唯一Device，绑定唯一T | 普通卡协议、未变角色规则 | Port与逻辑slot/物理资源映射、一槽多绑定、匹配provider/66250任务 |
| V3 新Modem/芯片 | Device Radio/Session/资源及Card必要执行适配/状态事件解释；契约变化才联动T/A/C | A/B+若通用Runtime/Card/Device总门面扩大，所有能力受牵连；C也可能靠共享device总头扩散。禁止中央转发、使用窄输入/结果、保留独立纯值target | 卡协议、T账户和A规则在输入/错误/完成语义不变时 | 真实结束/静止、厂商错误、READY/LOADED与恢复事件偏序；不能承诺仅Device修改 |
| V4 Telephony/RIL上移下沉 | 指定Card协议或Device执行，RIL消息契约/构建；旧可见错误/完成节点必要联动 | 三者若RIL依整Card/Device都会成环；B+也可能把所有协议下沉Device，C则可能复制codec。精确消息/codec target与单实现，不以目录决定上下移 | 等值输入/结果/取消/完成条件下T/A纯规则 | 平台ABI、Feature、RIL/厂商行为等值、异步结束证据 |

B+把V3的首查范围明确为Device和必要Card适配，是物理组织收益；**不会自动减少M，也不消除卡协议与硬件之间本来必要的协作**。Card/Device边界在规格与硬件变化中可保持，但必须允许具体能力按契约直接使用RIL，不构造表面零传播的策略工厂或总设备代理。

## 11. 正式架构决策

### 11.1 推荐 B+，保留两项最小修订

1. `device/slot_catalog.*`明确静态硬件能力归属，继续是纯只读白名单target，不把整个Device传递给T/A。
2. `account/number`独立承接现有六个号码文件；`account/profile`仅名称、标签、IMS等其他账户资料，eSIM Profile固定在`card/esim/profile`。契约和专属Adapter随功能，不复建模板目录。

推荐理由依优先级：

- **不变量可以原样保持。** 六组仅导航，不需要新Owner、运行对象、依赖边或SO；现有target可继续承载逻辑边界。
- **整体地图显式。** account/card_selection/card/device/integration/support分别回答六类问题；比A少一层术语解释，比C少一次由多个一级能力重组整体关系的工作。
- **局部定位接近C。** 同样有security/records/STK/eSIM等业务名，区别通常只是一个稳定父组；number/default_roles改进来自功能命名，不归功于目录数量。
- **Card/Device有真实变化原因。** Radio跨槽握手与EF/STK/Profile协议的差异支持分组，直接RIL调用的反证又约束它不能成为硬分层。

### 11.2 相对备选方案的取舍

A保有最低迁移成本、历史知识和明确Owner父目录，适合正在验收的切片暂时不动；但不能用“已落地”证明其为最终最易理解结构。C保有已知能力直接一级定位优势，技术上仍可安全；本轮不否定上一轮对局部定位的判断，而是在新增“整体结构理解”和Modem/Card分组优先级下重新选择。

B+接受一层业务父组、Card代码规模较大及Card/Device跨边界流程需契约说明的代价。不声称B+获真人研究或构建结果证明最优。若后续实际任务定位显示某二级能力持续错误归属，应针对证据局部修正，不再并列大量方案。

### 11.3 接受B+的必要约束与未解决问题

禁止新CardManager/DeviceManager、总card/device实现target、全局业务DTO池、双active/角色/Session/资源Writer、RIL反依整个能力实现。允许直接窄RIL调用、能力内专属事件/请求、按真实共享域注入资格。

仍未解决：普通/低优先级EsimFile的物理竞争域、关闭/结束保证、Port/slot映射、匹配provider字段/schema、原SO产品与Feature矩阵、三个Service生产收窄、精确visibility和外部头ABI消费者。这些不阻止本轮形成物理推荐，但阻止把目录决定当成运行安全/迁移验收。

## 12. 推荐的完整稳态目录树

下树承接完整生产能力，是供批准后实施的目标投影。`.h/.cpp`同场景；既有类名/符号先保留，文件归属不自动要求公共改名。只有carrier、Records、eSIM这类规模/变化原因充分的能力使用三级；不为每个逻辑层建目录。

```text
services/multisim/
├── BUILD.gn
├── README.md
├── account/                              # T：身份、绑定、资料与生命周期事实
│   ├── account_registry.*
│   ├── binding_index.*
│   ├── topology_service.*                # 保留原类型，受控接纳；非总流程实现
│   ├── account_store.h                   # 窄契约；必要专属Adapter就近
│   ├── discovery/                        # 发现/联合建行/重载/探测资料接纳
│   ├── activation/                       # 普通/force/卫星用例、进度、Hook契约
│   ├── number/                           # 设置、查询、独立回写及专属契约
│   └── profile/                          # 名称/标签/IMS；不含卡Profile协议
├── card_selection/                       # A：角色、选卡、业务冲突/重试
│   ├── assignment_service.*
│   ├── assignment_aggregate.*
│   ├── role_selection.h
│   ├── account_role_protocol.h           # 唯一联合建行角色资格
│   ├── default_roles/                    # Voice/SMS/Data保留差异
│   └── primary/                          # 模式计划、参数恢复、业务重试
├── card/                                 # R：卡自身能力，不建总CardService
│   ├── state/                            # 卡态、锁因、插拔/Radio观察接纳
│   ├── security/                         # PIN/PUK/PIN2/认证及专属请求结果
│   ├── records/                          # 卡缓存、加载、Refresh/LOADED
│   │   ├── load_session.*
│   │   ├── sim_file.*                    # 包含同族init/parse实现文件
│   │   ├── ruim_file.*
│   │   ├── isim_file.*
│   │   ├── voice_mail_constants.*        # 有配置依赖，不是纯support
│   │   ├── files/                        # IccFile及各卡型FileController
│   │   └── codec/                        # 经消费者审计的解码/TLV子集
│   ├── phonebook/                        # ADN/FDN、PBR/ANR/IAP
│   ├── sms_storage/                      # 卡侧短信，非完整短信服务
│   ├── stk/                              # Toolkit及专属订阅/输出/重发
│   ├── carrier/
│   │   ├── config/                       # 配置/APN与专属存储适配
│   │   ├── matching/                     # Tracker/OpKey/运营商匹配
│   │   └── privilege/                    # 卡规则/证书权限
│   └── esim/                             # 本地卡能力，非66250远端任务
│       ├── esim_manager.*                # Feature关闭分支仍保护
│       ├── esim_controller.*
│       ├── start_osu_result_callback.*
│       ├── profile/                      # Profile/Port请求、库存、通知/管理结果
│       ├── channel/                      # AID/APDU/逻辑通道及专属关联
│       └── provisioning/                 # 认证/BPP/卡侧安装原语
├── device/                               # R/H：执行与设备资格，非总调度
│   ├── slot_catalog.*                    # H构建后只读；不含动态绑定/策略
│   ├── radio/
│   │   └── radio_protocol_controller.*   # 跨槽握手/激活设备命令
│   ├── session/                          # 唯一卡应用代次/期望/可信接续
│   └── resource/                         # 真实共享域租约/隔离/结束资格
├── integration/                          # C/I/H：边界集成，不代写事实
│   ├── api/                              # 原签名/错误/完成语义适配，分能力cpp
│   ├── events/                           # 通用账户回调/death/Registry交付
│   ├── lifecycle/                        # 组合根、HostPorts、发布/匹配销毁
│   └── storage/                          # 仅真实共享技术连接/CRUD
└── support/                              # 受限共享代码，不建公共业务中心
    ├── sim_identity.h
    ├── request_ref.h
    └── request/                          # O协议/机制；各Owner自有实例
```

纯规则、types、contracts、应用步骤和平台适配按显式源集合分target，即便在同目录也不合并依赖。跨模块才导出窄契约，场景私有头默认不对其他Owner开放；公共API仍在原interfaces/innerkits/IDL。现有include两个仓内转发头需审计消费者后选择保留过渡或直接导出lifecycle契约，最终不建公共头池。

## 13. 当前目录到推荐目录的映射

MEC=可保持代码语义的机械路径调整；RESP=职责/接口/状态移交需独立验收；CONTRACT=涉及外部/仓内消费者须保护契约。这里只裁决，不执行。

| 当前位置 | B+推荐位置 | 调整类型 | 分类及约束 |
| --- | --- | --- | --- |
| topology/accounts | account根 | 平铺/迁移 | MEC；AccountRegistry/BindingIndex类型及纯规则target不变 |
| topology/discovery | account/discovery | 迁移 | MEC；生产Monitor/探测职责归入另属RESP |
| topology/activation | account/activation | 迁移 | MEC；T进度/active唯一，原Hook/完成偏序保护 |
| topology/account_profile | account/number | 改名/迁移 | MEC；当前set_show_number_use_case、show_number_plan、show_number_query各h/cpp，共6文件 |
| topology根Service | account根 | 迁移/后续收窄 | MEC路径；RESP生产流程，类符号不自动改 |
| topology/api、adapters | account根或discovery/number/activation的具名契约/专属适配 | 拆分/就近 | MEC仅移动完整头/类；需拆定义或改变可见性则RESP/CONTRACT单列 |
| assignment/domain、account_roles | card_selection根 | 合并物理层级 | MEC；保留唯一角色/冲突状态，纯规则不依O |
| assignment/primary、未来roles | card_selection/primary、default_roles | 迁移/业务化改名 | MEC已有计划；生产用例迁入为RESP |
| assignment/api、adapters、根Service | card_selection根或对应场景 | 就近/后续收窄 | MEC路径与RESP接口分开；不复制初始角色资格 |
| runtime/state、records、euicc | card/security、records、esim | 迁移/按能力归属 | PIN契约可MEC；成熟安全/状态及eSIM副作用拆分为RESP |
| runtime/session、resource | device/session、resource | 迁移 | MEC；同一实例/真实域资格，不随槽复制 |
| 未来runtime/radio | device/radio | 调整父组 | 生产RadioController多数可MEC；专用Primary/RIL等待从Controller抽出为RESP |
| runtime/api/local_card_requests、card_runtime、result_sinks | 对应Card能力、device执行桥与具名结果契约 | 拆分 | RESP；过渡期允许H私有薄接缝，不把CardRuntime机械变DeviceManager |
| foundation/slot_catalog | device/slot_catalog | 迁移 | MEC；仍属sim_values白名单，不能依device动态实现 |
| foundation纯身份/关联值、orchestration | support、support/request | 迁移/改名 | MEC；纯值/协议/可变机制分别target，Owner实例不变 |
| compatibility/api、门面/output | integration/api/events及能力专属输出 | 拆分/合并 | CONTRACT；兼容错误/回调/通知顺序，STK/eSIM专属内容不全部中央化 |
| assembly | integration/lifecycle | 合并父组 | MEC路径；H-only组合根依赖/原发布与匹配销毁不变 |
| include | 审计后精确导出lifecycle工厂/HostPorts契约 | 收口/最终删除冗余视图 | CONTRACT；不复制类型、不创建新公共接口 |
| 未来infrastructure | Owner专属适配；仅共享技术连接入integration/storage | 拆分/不单建一级 | RESP/CONTRACT；不造shared_store业务中心 |

## 14. 后续实施建议与报告检查

### 14.1 分阶段执行，批准前不迁移

| 阶段 | 操作范围 | 必须保护/验收 |
| --- | --- | --- |
| 1 最终设计确认 | 架构师审批B+、Card/Device排除项、slot_catalog与契约归属 | 本ADR保持Proposed直至明确批准记录；报告推送不等于批准 |
| 2 文档一致性 | 批准后同步两篇权威设计及实际/计划导航 | 不改T/A/O/R契约，不新建功能编号；修正下述历史表述 |
| 3 当前骨架安全移动 | 精确全文件映射、逐依赖切片，优先纯值/catalog与已内聚场景 | 保留类型/成员/实现token及对象，git mv，不启用生产路径 |
| 4 GN/include/测试门禁 | 更新路径和checker分类、显式源表、仓内导出 | 保留逻辑DAG；严格语法、四套规则、反例护栏、GN check/对象构建；visibility增强另验收 |
| 5 原SIM稳定能力迁入 | FileController、Phonebook、卡短信、STK等成熟文件族 | 不双写/双发/双重试；Feature on/off，平台头消费者与实际行为差分 |
| 6 大型类拆职责 | Controller/Monitor/State/eSIM/探测/存储/CardRuntime | 沿既有WU覆盖初始化/后台/恢复/重载所有写入口；保留一次物理建行与旧部分效果 |
| 7 系统验证/最终替代 | 原SO/产品、SA/IPC、初始化发布、设备与生命周期验收 | 匹配OHOS/厂商/provider/Feature；退出旧生产路径后由multisim完整替代，不长期双执行 |

G2a真实只读闭包仍按执行状态在独立工作分支适配/复验，不能因本报告推送就合入生产。新固定目标、迟到拒绝、共享资源隔离/释放、安全重试、MEP/四卡扩展和公开契约变更属于独立行为/能力切片。

### 14.2 仅记录的勘误建议

详细设计§4历史裁决仍写“当前无multisim BUILD”，§5仍写model/sim_identity计划迁foundation，需标为历史基线；“拟议target边界”与已实施骨架应分清。主设计revision元数据滞后于局部路径修订。AGENTS的“A角色/资源”宜明确业务分配/准入，真实执行租约仍R。源码头部“未接构建”应区分testonly与产品。这里只提出修订项，不修改这些文件。

### 14.3 本报告质量验证与可复核范围

本轮已检查15个必需章节、六组稳态树与映射、10处围栏、13张表格、40个Git相对链接（其中32处源码行引用）及23个关键函数/语句定位；自动检查通过。独立复核Card/Device、GN和冷启动描述无阻断，已修正测试路径、补充实现依赖禁环和精确证据。提交前还将核实修改范围与暂存区仅本文件。当前骨架文本依赖检查结果见§2，不代替候选实现、编译、运行Owner或设备验证。

提交固定消息为 `docs(multisim): add A-Bplus-C source directory decision`。只普通推送明确跟踪的origin/main；远端若新增提交导致不能快进，保留本地报告提交并报告，不自动merge/rebase或强推。报告自身不嵌最终提交SHA，实际提交/远端tip由Git核实并在交付回复提供，避免自引用循环。

## 15. ADR 决策状态

| 字段 | 内容 |
| --- | --- |
| 状态 | **Proposed** |
| 决策日期 | 2026-10-31 |
| 输入代码 | main@b86f9e4faa56c921d06113f7d2f99fb75ea021f7 |
| 依据 | 两篇目标设计、既有功能/WU/流程/接口、当前生产源/骨架/GN、独立冷启动模拟与增量源码核对 |
| 推荐 | **B+：六个稳定功能组；Card专属协议与Device执行/共享资格分开** |
| 备选 | A保留现结构；C具体能力一级平铺 |
| 主要取舍 | 用一层稳定父分组换显式整体地图；不承诺比C更少真实阅读或更低耗时 |
| 架构师待批准 | B+物理树；Card/Device边界及直接窄RIL调用；static catalog纯target；Owner/契约/可见性约束；分阶段迁移与差分门禁 |
| 未关闭风险 | eSIM物理资源域/结束保证、Port映射、匹配provider与产品Feature、公共消息/ABI消费者、三个Service收窄及GN内部visibility |
| 执行授权 | 本轮只生成、核查、提交、推送本报告；没有目录迁移、行为修改或批准生产切换 |

**提交远端只保存决策建议，不将Proposed升级为Approved，不替代两篇权威设计或已批准契约。**

[E01]: ../BUILD.gn#L67
[E02]: ../telephony_core_service.gni#L21
[E03]: ../services/sim/src/sim_manager.cpp#L41
[E04]: ../services/sim/src/multi_sim_monitor.cpp#L281
[E05]: ../services/sim/src/multi_sim_controller.cpp#L976
[E06]: ../services/sim/src/multi_sim_controller.cpp#L1280
[E07]: ../services/sim/src/multi_sim_controller.cpp#L1483
[E08]: ../services/sim/src/multi_sim_controller.cpp#L1720
[E09]: ../services/sim/src/sim_account_manager.cpp#L42
[E10]: ../services/sim/src/radio_protocol_controller.cpp#L103
[E11]: ../services/sim/src/sim_state_manager.cpp#L197
[E12]: ../services/sim/src/sim_file.cpp#L150
[E13]: ../services/sim/src/stk_controller.cpp#L231
[E14]: ../services/sim/src/sim_state_handle.cpp#L448
[E15]: ../services/sim/src/esim_manager.cpp#L36
[E16]: ../services/sim/src/esim_file.cpp#L133
[E17]: ../services/sim/src/esim_manager.cpp#L318
[E18]: ../services/sim/src/sim_sms_controller.cpp#L134
[E19]: ../services/tel_ril/src/tel_ril_sim.cpp#L19
[E20]: ../services/multisim/topology/topology_service.h#L48
[E21]: ../services/multisim/assignment/assignment_service.h#L35
[E22]: ../services/multisim/runtime/card_runtime.h#L70
[E23]: ../services/multisim/runtime/resource/resource_barrier.h#L31
[E24]: ../services/multisim/runtime/session/session_registry.h#L28
[E25]: ../services/multisim/foundation/slot_catalog.h#L29
[E26]: ../services/multisim/BUILD.gn#L1
[E27]: ../test/multisim_architecture_rules/check_structure.py#L32
[E28]: ../services/multisim/foundation/orchestration/runtime_operation_store.h#L27
[E29]: ../services/sim/src/card_file_detect_manager.cpp#L98
