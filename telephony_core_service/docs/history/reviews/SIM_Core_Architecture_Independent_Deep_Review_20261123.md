# SIM 多卡管理重构 · 四篇核心架构文档独立深度评审

2026-11-23　｜　评审输入 `main@befd76d4eba73ade9552248c303e3d6f8427c8ce`　｜　裁决建议 **附条件批准**

## 第一章　独立架构总体评价

**保留账户与绑定、角色分配、卡应用、设备执行四种责任，保留五组源码目录。最需要调整的是 Primary 从业务意图到设备协议的接口，以及关闭时对未完成业务的交代。** 在同一份提交上，75 个 Target Host 用例通过，但另一个可以重复的场景中，ICCID 参数写入结果未定，原调用没有完成，Host 却报告已排空。用例通过与关闭协议仍有缺口，两项观察可以同时成立。实验见第四章。

### 从业务事件推导边界

设同一位置先后插入甲、乙两张卡。甲的号码写请求可以在乙插入后才收到响应。需要分别查清写的是哪次卡应用会话，哪条账户资料可以改变，以及谁还在等这次调用。再设两个位置共享一个 Modem。业务即使能唯一识别两张卡，也无法凭账户身份决定上一条命令是否执行完。由此可以划出四个职责边界。

1. **账户服务**保存可跨会话存在的订阅资料，接纳发现、解绑及资料补丁。卡被拔出可以立即撤销本次访问资格。历史资料按原查询和保留政策处理。
2. **角色服务**选择默认 Voice、SMS、Data 及 Primary，列出此次会影响的参数、投影和通知。两个账户的变更若争用同一个默认 Data，须在首次相关写入前取得同一份资格。
3. **卡运行与设备执行**分别回答“哪次应用/文件加载有效”和“哪个物理资源仍在执行”。一次 Radio 切换可以只需要协议组和绑定证据。PIN 或 EF 可以在尚无账户时执行。两者都没有理由伪造另一种身份。
4. **原入口负责交付，宿主负责接线与关闭。** 用例所有者保存必要的步骤、重试和原出口。存储、RIL、通知返回各说明一种效果，各自在原可见节点接受核对。新增一张卡时，只应增加确有变化的路由、成员和策略输入。

对照四文档，A 已处理旧架构的两个根因，即同一个槽号承担过多语义，Controller 既改账户和角色又等待硬件。按事实划分有工程价值。C 的物理组织让维护者能按“号码、主卡、设备资源”找到实现。D 按写权单元迁入的方向也正确。生产根源表和 Core 装配仍指向 Legacy，以上收益目前只在部分 Host 路径可观察。

**没有发现需要推倒重建的业务边界。** 局部复杂度偏高。C0 同步分支分配整套操作记录，Primary 的行动序列在计划和执行中重复表达，所有 Radio 步骤又被要求提供卡应用会话。相反，设备租约、原请求完成资格及 current/historical 投影各解决不同的可观察问题，应当保留。建议局部修订并按受影响的 WU 放行。

### 范围和证据口径

本次通读 A §1～24、B §1～21、C §1～10、D §1～9，包括附表、历史基线及迁移反查。历史评审仅用于定位来源。提交中的实现、当前产品装配、受控 Host 和匹配设备分别判断。Git 根为 `multisim`，部件是普通子目录。核对时本地 HEAD、跟踪分支和远端 `origin/main` 相同，暂存区为空。初次核对时，工作区另有七个已修改文件，涉及 Core SIM、Primary Host 生命周期及测试，另有三个未跟踪项。本次源码取证和可执行验证固定在上述提交，未把在途修改纳入裁决。仓外解包仅供验证。

| 标记 | 本报告使用边界 |
|---|---|
| FACT | 指明版本的源码、构建输入或本轮观察。静态调用只能证明可达路径。 |
| CONFIRMED | 限于本任务明确的最终替代、已授权落地的五组目录，以及已发布接口或明确冻结的兼容边界。实现中观察到的旧缺陷保留 LEGACY 证据属性。 |
| PROPOSED | 本次方案、反例所导出的风险或尚待签收的设计。A 页首仍为 draft/pending。 |
| OPEN | 缺少产品选择、提供方契约、设备因果/静止证据或对应运行验证。 |
| HISTORICAL | 固定旧提交的运行、接入或表格覆盖，只按其原输入解释。 |

四篇设计的职责分别为业务与不变量、类与构建、物理目录、功能与写权迁移。本报告的条件和裁决仅是独立评审建议。[A 目标架构](../../SIM_Target_Architecture_Design_v1.2.md) · [B 源码详细设计](../../SIM_Multisim_Source_Architecture_Detailed_Design.md) · [C 目录方案](../../SIM重构代码目录结构设计方案.md) · [D 功能与迁移](../../migration/SIM_Architecture_Refactor_Design.md) · [唯一执行状态](../../SIM_Refactor_Execution_State.md#current-status)

## 第二章　架构设计决策逐项审查

下文“文档 A～D”对应四篇设计。模块代号 T 表示账户与绑定，A 表示角色分配，O 表示请求关联与完成机制，R 表示卡运行与设备执行，C 表示兼容交付，I 表示平台适配，H 表示宿主装配与生命周期。

### 2.1 对象、基数与身份

| 对象与关系 | 现有方案及优点 | 局限、备选及建议 |
|---|---|---|
| Subscription / Account | Subscription 是本机业务身份。AccountRecord 是 T 保存的该身份资料，可各有一份 current、historical 投影。内部 SubscriptionId 包装旧 simId。当前 BindingIndex 支持每个槽和每个订阅各有 0 或 1 条有效绑定。 | 双向唯一适合已证实的路由。保留旧身份分配和过滤。另建永久 Subscription 实体、跨机主键或拿 ICCID 作表主键会改变删除、重装、占位记录和重复识别语义。多位置同时承载同一订阅须由产品给出基数，再改 T 的绑定键。依据 A §3.1～3.2、B §5～6。 |
| Card / CardApplication | 实体卡是载体，一个载体或启用的 Profile 可提供多个应用。访问会话随替换失效。Card 负责原卡态、安全和 Records。槽可为空，锁卡也可有安全操作。 | 不必为每张卡再造长期 Card 实体表。应用键限于原载体/来源，跨等待用 R 签发的 SessionRef。同卡局部 Refresh 只使涉及的加载成员失效。T 的 bindingRevision 与 R 的 bindingGeneration 能独立变化。依据 A §3、§10，B §9，第四章实验与源码。 |
| eUICC / Port / Profile | 一个 eUICC 可有多个 Port 和多个库存 Profile。禁用 Profile 可以没有应用会话或本地账户。Port 是访问路由，Profile 的库存身份属于 eUICC 作用域。 | 保存请求的 portIndex、可信路由、观察到的启用实例三者各有用途。单 Port 同时可用的 Profile 数、重启后绑定复用均 OPEN。先传已有值和有效性，需要同时暴露多绑定时才增加关系对象。依据 A §3.2.1、§17，B §8～9，D §6.5。 |
| 角色与设备 | Voice/SMS/Data 角色语义关联订阅，兼容入口、参数和部分查询仍按槽。Primary 涉及位置与 Radio 分配。多个角色可选中同一订阅，Data 可独立变化。意图、有效选择、硬件结果允许暂时分离。 | 保留每个角色的旧读源和每分支的联动节点。把四种角色统一成“主卡槽”会改变独立设置 Data 后的查询，按 Modem 各维护一个默认 Voice 又会产生多个全局默认。依据 A §4.3、§8.2、B §7、D §7.1.1。 |
| 对外 ID 与内部关联 | 旧 API 的 slotId/simId、IDL 的 slotId/portIndex、下载结果的 cardId 继续原样交付。内部把它们限制在特定入口、对象引用或传输步骤。RIL serial 定位原传输，不能单独证明订阅、会话或资源已结束。 | GetSimId(slotId) 是显式兼容查询。禁止业务代码把其结果当容器下标。NAPI 的 cardId 来自结果载荷，生产者的跨设备身份语义 OPEN。不要从 cardId 反推本机 simId。无需全局映射服务。依据 A §3、§16、D §2。[InnerKit](../../../interfaces/innerkits/include/i_sim_manager.h) · [结果序列化](../../../frameworks/native/src/download_profile_result_parcel.cpp) · [JS 输出](../../../frameworks/js/esim/src/napi_esim.cpp) |

### 2.2 分责与执行机制

| 决策 | 可保留的现有方案 | 问题、替代选择与评审建议 |
|---|---|---|
| T 唯一接纳账户资料和绑定 | R 提供带来源的观察。T 在同一边界维护索引，分别处理当前/历史投影、绑定 ABA 和字段重载。改号码无需让另一槽重新发现。 | 以完整快照、一个全局版本或两个独立可写容器替代都会扩大冲突。Host 的 AcceptStoredDiscovery 接受已确认写入，不能直接充当真实 Provider 的发起协议。生产应在原调用前登记 ticket，返回时只凭原 ticket 接纳。见 B §6、§16 和第四章。 |
| A 唯一接纳角色 | 在一个写集中预留 preference、Primary、联动 Data 和对应输出。A 可观察设备映射。Device 持执行资格。跨域 C1/C0 仍可能争用同一 Data。 | 同属 A 的方法也能绕过资格，Profile 提示已在 Host 组合中观察到。每个实际写者在原节点经过同一准入，同一父请求可沿用已取得的写资格。Voice/SMS 无交集时保持独立。见 A §4.1、§13.5、B §7、第四章。 |
| O 为请求机制 | 发起者持本次 epoch、请求、step/attempt 及原完成出口。T 主编排普通激活，A 主编排 Primary。R 的 PIN/EF 保留短请求。纯读可同步返回。 | 用统一 Coordinator 容易形成 T→O→A→O 的隐式循环。同步 C0 成功/前置拒绝也 Begin，成功路径完成后未调用 Retire。重复调用会在持久存活的服务里留下表项。优先保留方法局部完成与前缀记录，或在确定终态精确回收。异步路径仍使用步骤门控。见 A §11.2、B §8、StartPrimarySwitch/CompleteOnce。 |
| R 的卡运行与设备资格 | 卡协议复用原解析、电话簿、STK、安全。Device 验证一次命令的完整资源集、fence 和完成证据。 | CardRuntime 同时借原 Manager、Radio 与 RIL，含 sessions、loads 和 ResourceBarrier。另有 ResourceRadioRuntime 持自己的屏障。目前是不同 Host 接缝。将来若装入同一物理域，两套屏障各自都能“独占”。随真实命令族迁移，先交唯一屏障和事件接收者，再退旧成员。不另设全卡总接口。见 B §3.2、§9.1～9.2。 |
| C 兼容、I 适配、H 装配 | C 保留原权限、错误、回调、同步等待、通知与查询源。I 持参数/DataShare/订阅等技术接缝，业务 Adapter 随 Owner。H 负责建图、静态 SlotCatalog、引用和宿主级封口。 | C 的 getter 可能触发后台写，必须进具名用例。按查询名称直接读 T 缓存会丢失回写。H 不持角色策略。关闭时应取 A、R、调度器各自收据。无需全局 Repository、对象注册中心或事件总线。依据 A §5.4、§8～9、§14～15，B §10～13。 |
| 2→3→4 个以上端点 | T 的 map 和按键版本可支持稀疏槽，角色选择与静态枚举可随输入扩展。旧产品预留槽 2、受限实体槽 3，槽数、已装配数、可见数和 Radio 栈数不同。 | C1 现在令全部目录槽参加同一协议。应让协议适配给出本轮可达参与集合，A 固定目标、结果与核对依据。产品不能给集合就保留既有执行路径。四端点两栈与四栈分别验。改变硬件协议确需改执行和测试，无须改变账户身份模型。依据 A §3.3、§17.1，B §7、§17.4，实验一。 |
| vSIM、通信共享及能力变化 | H 给出允许的端点，R 接纳本次来源/可达性，T 接纳或撤销本机绑定，A 按产品规则判角色资格。实体能力、当前可达、对调用者可见、可充当默认角色可分别变化。 | 域外多卡开关主流程、66250 的 Profile 任务、Core 本地卡原语各管一次发令/取消。eSIM→vSIM 级联和互斥位写者须查实际服务。远端实体归助理机，本机代理重连要能辨别新来源。先用有版本的来源观察/失效理由。仅当独立 Port 配额或多连接不变量成立，再增加对象。静默断链检测窗口、远端角色资格均 OPEN。依据 A §7.2.1、§10.4、§14.5、§16.4、§21.3。 |

### 2.3 物理目录与逻辑依赖分别裁决

| 物理位置 | 开发者应从这里查什么 | 工程边界与取舍 |
|---|---|---|
| account | 发现、身份匹配、绑定、激活、资料和号码回写 | 生命周期一起变化，留在 T。输入是 R 的观察，建行时用 A 的角色片段。 |
| card | 卡应用、安全、EF、Records、电话簿、短信、STK、eUICC | 成熟协议按原依赖复用。低层 EF I/O → 供 ADN I/O 使用。高层 Records 和电话簿用例可分别使用两者。避免两个总 target 互依。 |
| role_assignment | 默认角色与 Primary 的选择、逐步输出、失败恢复 | 分支与兼容动作留在具名用例。物理域租约和设备是否结束由 device 给出。 |
| device | Radio/Modem 观察、协议参与者、传输与资源复用 | 同一物理资源只建一个准入者。逻辑通道的 APDU/卡语义在 card，物理冲突域的独占在 device。 |
| infrastructure | compatibility 的旧交付、assembly 的建图、request 的通用关联、身份值 | 三部分分别建逻辑目标。通用请求机制头已出现业务回调 AcceptScheduledC1Retry，宜把业务投递接口收回 role_assignment，或明确仅用于 C1。避免引入第二个业务用例前就泛化。 |

五组目录便于定位，保留。逻辑上 T/A 是业务写边界，R 跨 card/device，O 在具体用例中使用。没有理由按目录数建立五个 SO。当前 `topology_cases → assignment_contracts`、`assignment_cases → topology_contracts`，并且 `topology_contracts → assignment_contracts`，反向运行协作依靠窄端口和 H 注入。当前逻辑图无环。以后若向 assignment_contracts 引入具体 T 类型，这一精确位置会形成回边。`public_deps` 暴露公有头所需类型，`visibility` 允许同 BUILD 文件内 `:*` 及指定架构测试，内部边仍靠 `allowed_edge` 和 include 检查约束。目录整齐无法代替这些检查。当前没有足以支持整体缩减 target 数的构建成本证据。[GN 源表](../../../services/multisim/BUILD.gn) · [依赖检查](../../../services/multisim/test/cases/architecture/check_structure.py) · [CardRuntime](../../../services/multisim/card/runtime/card_runtime.h) · [请求机制头](../../../services/multisim/infrastructure/request/runtime_operation_store.h)

## 第三章　四篇核心文档一致性矩阵

表内章节均属列首文档。“引用”表示该决定只需指向主责章节。目录规范没有规定协议细节，本身不构成缺失。末列先列主维护者，再列需要同步的位置。

| 决策 | A 业务/约束 | B 源码/接口 | C 物理组织 | D 功能/迁移 | 差异、维护及同步 |
|---|---|---|---|---|---|
| 契约与事实等级 | §1.3 整体 pending | §1 定义实现深度 | §8 分阶段 | §1.1 区分 F/C/P/H/Q | 一致。A 维护批准口径。B/D 的“当前”应写提交。 |
| Slot、订阅、应用身份 | §3 分类目标 | §5 三类 target | §4.1～2、§5.5 值类型 | §2.1 旧参数 | 一致。A 管语义。B 注明旧 ID 的使用边界。 |
| 绑定与基数 | §3.1 当前双方 0..1 | §6 BindingIndex、版本 | §4.1 账户管绑定 | §5.2 WU-01 | 同意现状基数，MEP 的多绑定 OPEN。A 主责，B/D 随实际政策改。 |
| 静态目录、装配、动态清单 | §3.2.1 H 静态、R 来源、T 接纳 | §5～6 SlotCatalog/BindingIndex | §4.4、§5.3 H 建只读目录 | §5.2 首行写“T 管配置、R 管实例清单” | 运行对象持有可归 R，但“配置”与已接纳槽清单写反。A 管事实边界，D 改行，B/C 引用。 |
| current/historical、active | §6.5.1、§8、§9 分视图/字段 | §6、§14 分别接纳 | §4.1 概述激活状态 | §5.2.1 WU-01/02 | 主方向一致。D 按 T 投影、DB 与 R 设备效果分别验。C §4.1 加限定词。 |
| 发现建行、重载、Provider | §6.5.2、§13 按字段只写一次 | §6、§16 ticket/字段版本 | §6.1 Adapter 随 Owner | §5.2.1、§7.2 WU-01/03/05 | B 的接受已确认结果入口只支持 Host，生产发起与回包要共用原 ticket。B 主责，D 加退出证据。 |
| 激活状态、设备执行与主卡冲突 | §4.2、§6.5.3 T 编排、A 准入 | §14 含普通/force/卫星 | §4.1、§4.4 | §6.3 图将 O 标为激活用例 | D §4 已正确划分。D §6.3 以 T 为泳道，O 留在步骤旁，B 注明尚未接线的准入接口。 |
| 默认角色和 Primary | §4.1、§8.2 区分意图和生效 | §7、§15 A 持写集 | §4.3 对两类目标有区分 | §5.1、§7.1.1 保留旧读源 | 同意。A 管不变量，D 按入口说明 Primary 联动 Data 的条件。 |
| 偏好键与 Profile 关闭键 | §6.3、§13.2 列 WU-04/05 | §7、§17.2 识别 Core/eSIM | §5.2 不授 C 写权 | §7.1.1 有两例，§5.2.1 主表未汇齐 | D 是写点账本，须把 Core 两入口、Profile 切换与关闭提示落在相应行。B 补“同 Owner 多入口”准入。 |
| O 的范围与生命周期 | §4.1～2、§11.2 按用例 | §8 每 Owner 一份机制 | §5.4 简单只读直返 | §4.1 正确，§5.1.1、§5.4、§6.3、§7.5 有 O 协调/发出写法 | 去除会导出独立执行者的箭头。A 主责，D 改图表。B 允许无设备的短路径局部化。 |
| 会话、绑定、步骤及传输 | §10.1、§11.1 分别签发 | §5、§8 有不同值类型，RecordSent 要求 SessionRef | §5.4～5 提供基础类型 | §4.6、§7.3 概述关联组合 | 当前角色服务的 C++ 实现用 T 版本构造 R 会话。B 改接口。A/D 明确 Radio 与卡步骤各需要哪些证据。 |
| 超时、重复、迟到证据 | §11～12 分离原出口和设备效果 | §8、§15 有原 step、pending、retire | §5.4 机制不决定重试 | §4.7、§6.2～4、WU-07 | 大体一致。B 需补 R 核对后由 A 收尾的独立条件。D 验原完成次数和后续请求。 |
| 多资源与协议成员 | §3.3、§12.3、§17.1 按真实域/参与者 | §7 当前按目录成员发 C1 | §4.3～4 角色与 Device 分开 | §6.4、§8.1 SIM-028/G4 | 两槽样本不能定出稀疏拓扑。B 维护成员接口。A 给选择依据，D 加四端点两栈负例。 |
| 锁、I/O、通知重入 | §9.2、§14.1/4 锁外 I/O、锁外通知 | §7/15 假设 A 串行 | §7.1 限制依赖 | §7.1～3 保留中间可见性 | 当前 Host 方法体锁覆盖 I/O 和输出。B 区分执行前提与目标分段，D 增跨线程验收。 |
| 卡、Records、电话簿与设备 | §6.6 指出低层复用 | §9.1～2 给能力和 DAG | §4.2/4.4、§6.2 从属不同事实 | §5.1 SIM-012～019、036～044、WU-06 | 一致。C 细化逻辑通道分工，B 管物理接缝退场，D 保留算法和通知顺序。 |
| 五组、Adapter 和 GN | §5.2、§6.6 只定原则 | §3～4 唯一文件树、target | §3～7 主责物理职责 | §4.4 指向 B/C。§5.1.1 仍列 application/topology 等路径 | C 管一级布局。B 管文件/target。D 用 Owner+定位链接替换旧“建议目录”。 |
| CardRuntime / 装配 | §6.1、§6.6 R 保存原能力 | §3.2、§9 混合接缝有退出条件 | §3、§5.3 识别过渡责任 | §3、§5.4 原能力迁向 R | 保留过渡，B 注明每类命令唯一活跃屏障/结果出口，C 无需另设一级。 |
| Core、CMI、共享 RIL、SO | §7.1、§14.2～3 共享和驻留 | §10.2、§11 原控制块与工厂 | §5.3 assembly | §7.4、§8.1 G2a/b/c | 方向一致。H 只能封本组件。B 链接最新 Host 实现，D 分开组件、产品关闭、单库卸载。 |
| 停止、部分失败与收据 | §14.3 原出口和每实例 Closing | §11 窄 Host 协议 | §5.3 管组合寿命 | §7.4 还列 Starting/Draining/Destroyed 等阶段 | 这些可作为内部阶段。A 定对外承诺。B 增未结束存储/交付的收据。D 指明接收者交接早于销毁。 |
| eUICC/Profile/Port、66250 | §3、§7、§12.6 卡原语与域外任务 | §8～10 本地双结果/偏好 | §4.2 卡侧操作 | §5.1 SIM-046～054、§6.5、WU-08 | 一致。A 管边界。D 应把“查询也开通道”和 ARA-M 竞争写入该 WU 的切换集合。 |
| vSIM、通信共享与端点失效 | §10.4、§14.5、§21.3/22.4 | §5、§17.4 有值与扩展限制 | §4 可承载观察/绑定/选择 | §6.1、§8.1 以原能力和 G4 为主 | D 缺少对静默死亡、级联和隐藏端点切片的明确反查。D 引用 A 矩阵即可。保持外部决定 OPEN。 |
| API、参数、DB、通知 | §8～9、§13、§15～16 | §10、§12～16 方法级路径 | §5.2 旧交付 | §7.1～3 方法轨迹 | 同意。D 管差分。确认旧 getter 后台回写、空槽缺省、广播失败和调用者权限序。 |
| 切写时点与旧实现退出 | §6.3、§22.1 拆 WU-04、09 子项 | §17.2～3 摘要 | §8 是物理迁移 | §5.2.1 与 §7.5/8.1 的“WU-01～05”粒度不同 | 易误读为 G3a 全切参数，或等 G3d 才迁 loaded。D 以字段补首接管时点，A/B 保留引用。 |
| 当前产品与 G2a | §6.4、§22 查执行状态 | 页首/§3 已写回退，§17.3.1 仍称实际产品编译 | §8 的旧目录调整为实施步骤 | §1.2、§8.3 有历史基线 | 以根 GN/Core 裁决。B 主责标记历史。C §8 日期化。执行状态仍为进度依据。 |
| 验证入口与覆盖 | §1.2/§22.4 用 TEST_F，§19.1 仍指旧 systemtest | §20～21 指现行检查器 | §8～9 管目录护栏 | §8.1/G1 仍推荐旧 systemtest，§9.4 是 B1/W1 | A/D 改活动入口，保留旧 SHA 与编号。D 更新现行接口反查。不能仅按表行数宣告通过。 |

## 第四章　源码落地一致性

### 4.1 在固定提交上核实的链路

| 检查问题 | 本轮源码证据与判断（FACT） |
|---|---|
| 产品执行谁 | 根 tel_core_service 源表含 TELEPHONY_SIM_ROOT 下的 SimManager、MultiSimController 和 RadioProtocolController。EsimManager 始终列入，另外三个源由 eSIM Feature 加入。CoreService::Init 创建 TelRilManager，发布共享引用，依次建可选 eSIM、SIM，再建 NetworkSearchManager。GetRealSimCount 在 CoreServiceSim 读原宏或委托原 Manager。LegacySimQueryBinding 参考文件在测试树。B §17.3.1 的“实际产品源表直接编译”只适用于旧接入提交。[根 GN](../../../BUILD.gn) · [Core 初始化与停止](../../../services/core/src/core_service.cpp) · [Core SIM 方法](../../../services/core/src/core_service_sim.cpp) · [G2a 参考](../../../services/multisim/test/framework/internal/host/g2a_reference/) |
| T 落实到什么深度 | BindingIndex::RemoveBinding 递增空槽版本。AcceptBinding 拒绝同一订阅暗中移到另一槽。AccountRegistry::AcceptReload 独立处理 view、attempt、字段版本与成员水位。TopologyService::ReadPrimaryTarget 从绑定和 current 资料取 ICCID、active。AcceptStoredDiscovery 收到外部提供的已确认 store result 后才调 BeginStore。此入口是 Host 建立前态的接缝，不能证明真实 I/O 发出前已有 ticket。BeginRouteMapping 仍显式抛出未支持。[绑定](../../../services/multisim/account/binding_index.cpp) · [账户重载](../../../services/multisim/account/account_registry.cpp) · [T 入口](../../../services/multisim/account/topology_service.cpp) · [发现票据](../../../services/multisim/account/discovery/discovery_observation_ledger.cpp) |
| A/O 能处理什么 | StartPrimarySwitch 在读取 Radio 前后复核 T 的目标信息，并使用 AdmitTransitionWithRoleWriteSet。C0 无设备发令，B 在匹配结果后写参数，C1 预写后按阶段及成员推进。RuntimeOperationStore 有 step 消费和原出口领取。C1 首次失败后可保留旧重试预算。A 在 B、C1 首步、续步和重试处以字面量 “USIM” 和 T 的 bindingRevision 构造 SessionRef。真正的 Radio 关联另有 protocolSession、phase、成员与 transport。RecordSent 接口使两个不同代次看起来可互换。SetDefaultRole 有准入逻辑，但 AssignmentStoreAdapter::WriteDefaultRole 未接入真实角色存储。[A 入口和结果](../../../services/multisim/role_assignment/assignment_service.cpp) · [A 写集](../../../services/multisim/role_assignment/assignment_aggregate.cpp) · [参数适配](../../../services/multisim/role_assignment/assignment_store_adapter.cpp) · [R 会话](../../../services/multisim/card/state/session_registry.cpp) |
| 设备执行与回包 | ResourceRadioRuntime 在平台 Submit 前登记 transport，释放 R 锁后交接，再区分未发、已交接和交接未知。回包匹配 token、serial、原步、成员与绑定。ResourceBarrier::Acquire 一次核对完整键集，Complete 要求原持有者、已发、结束、复用与适用的效果证据。旧 RadioProtocolController 将栈数限于双栈范围。CHECK/COMPLETE 响应和 UPDATE 阶段的 NOTIFY 有各自协议含义。产品如何从这些消息证明物理静止仍 OPEN。[R 执行](../../../services/multisim/device/resource/resource_radio_runtime.cpp) · [屏障](../../../services/multisim/device/resource/resource_barrier.cpp) · [原 Radio 协议](../../../services/sim/src/radio_protocol_controller.cpp) |
| 谁写旧数据 | CoreServiceSim::SetPrimarySlotId、SetActiveSim 在不同校验序之后写同一偏好键。EsimManager::SwitchToProfile 在有效 File 调用后也写该键，同时保留外层返回和内层枚举。OperatorConfigLoader::InsertOpkeyToSimDb 直接走 DataShare。CardFileDetectManager 自行 Insert/Update。GetShowNumber 读卡缓存后会通过 Lifecycle 提交 SetShowNumberToDB。仅拦公开 setter 会遗漏这几类写者。[Core 入站](../../../services/core/src/core_service_sim.cpp) · [eSIM](../../../services/sim/src/esim_manager.cpp) · [Carrier](../../../services/sim/src/operator_config_loader.cpp) · [Detect](../../../services/sim/src/card_file_detect_manager.cpp) · [原 Controller](../../../services/sim/src/multi_sim_controller.cpp) |
| L3/L4/L5 差多远 | PrimaryHostLifecycle 先冻结 A/Scheduler/R，投递 Owner 封口任务，查收据、pending/running 及设备。CoreService::OnStop 实际执行 IMS UnInit、搜网 DeInit、RIL DeInit 等，未调用这套协议或 Legacy SimManager::Stop。RIL 此处注销 HDF 服务状态监听，实际解绑全部消费者、所有在途回调/执行帧退出及跨 SO deleter 返回无产品证据。[Host 关闭](../../../services/multisim/infrastructure/assembly/primary_host_lifecycle.cpp) · [Core](../../../services/core/src/core_service.cpp) · [RIL](../../../services/tel_ril/src/tel_ril_manager.cpp) · [产品证据盘点](../../../services/multisim/test/docs/M10_3_Product_Device_Evidence_Readiness.md) |

**覆盖反查。** 将 B §17.1.1 的文件 stem 集合与本提交的 sim/include/*.h、sim/src/*.cpp 逐一比对，61 头、55 源均有归属，根 BUILD 列出 54 个 SIM 源。差集只有 sim_cache_sync_manager.cpp，D 的 SIM-062 已记录未装配。D §5.1 的 SIM-001～062 连续，含 8 个子项。§5.2.1 有 9 个 WU。另从两份 InnerKit 头逐个抽取 virtual 方法名，ISimManager 得 123 个不同名称，IEsimManager 得 33 个。与 D §9.4 各方法行对照，前者漏列 UpdateDBActiveByIccId，后者未发现方法名遗漏。前者在 D 的 WU-02 已有业务归属，补一行反查即可。文件/方法名覆盖的结论不延伸至重载签名、条件编译、全部外部写者或实际行为。[SIM 接口](../../../interfaces/innerkits/include/i_sim_manager.h) · [eSIM 接口](../../../interfaces/innerkits/include/i_esim_manager.h) · [D 迁移表](../../migration/SIM_Architecture_Refactor_Design.md#writer-cutover)

### 4.2 实测与反例复核

在仓外解包上述提交的部件，按仓内 dependency_lock 校验 GoogleTest 和所用依赖后重新编译。run_architecture.py --with-gn --migration 通过，检查了 94 个 C++ 单元、合并头编译、25 个 source_set，拓扑、资源、分配和运行协议四组纯规则及 GN 对象构建均通过。test.py run --backend target 逐例独立进程运行，**75/75 通过**（本轮二进制 SHA-256 前缀 532154f8d541e9e8）。Target 链接自己的生产候选源。平台、参数与调度仍有 Host 边界替身。当前能力表只有六个可做双后端差分的 Primary 场景，另有一个纯 Target 计划场景。[架构脚本](../../../services/multisim/test/scripts/run_architecture.py) · [Target 链接源表](../../../services/multisim/test/framework/internal/host/BUILD.gn) · [双后端能力表](../../../services/multisim/test/config/backend_capabilities.json)

六个 SIM-028 场景在本轮 **6/6 通过**。以 `dual-active-primary0` 建立双卡前态，分别运行 Legacy 和 Target，共 12 次。逐场景对照既定行为和两个后端的观察，差异清单均为空。覆盖 C0、B 成功、B 错误、B 超时、C1 检查失败后重试、C1 成功并收到过期回包。可从部件的 `services/multisim/test/` 目录运行以下命令复核。

```sh
python3 -B scripts/test.py run --backend both --filter SIM028-LEGACY- \
  --init-model dual-active-primary0 \
  --target sim_primary_slot_real_test sim_test_target_host
```

这批差分固定了双卡分支的原有可见行为。下述四端点、存储效果未定和关闭交错另行取证。本轮未重跑全量 Legacy、ASan 或 TSan，也未执行产品镜像构建、设备侧 SA/IPC、RIL 或动态卸载验收。

下面四项为**本轮新增的 Host 观察**。复用了同一提交内的 Fixture、ResourceFixture、参数适配、A、R、H 与 Owner 调度器。观察驱动在仓外链接同一次构建的 sim_target_production_host。表中给出重建前态与调用顺序，不依赖临时目录或私有字段。实验一、二使用记录命令的 Radio 端口，三、四使用 ResourceRadioRuntime。它们各自检查一个边界，不能据此归因真机故障。[可复用夹具与 Seed/StartC0/ResultAt](../../../services/multisim/test/cases/target/primary/b_async_primary_test.cpp) · [原有未确认参数用例](../../../services/multisim/test/cases/target/primary/c0_store_adapter_integration_test.cpp)

| 实验 | 输入与复核方法 | 实际结果 | 证明范围 |
|---|---|---|---|
| 一　四端点、两栈 C1 | Fixture::Seed 建立 0、1 两个绑定。给 A 的目录追加 2=Virtual/Internal、3=RestrictedPhysical/CallerCapabilityRequired，二者 rilInstance 和 radioDomain 均空。容量取 {4,2,2,2,2}。关闭专用 Radio 支持，以 userSet=false 切到槽 1。 | 首个 CHECK 的 memberSlot 依次为 **0、1、2、3**。 | A 从 catalog.Slots() 生成成员，包含两个无已证实 Radio 路由的端点。产品是否允许任一额外成员发令 OPEN。 |
| 二　Profile 提示与 C1 重叠 | 延续实验一，在首个 CHECK 尚待回包时，用同 epoch、Owner=LocalCard 的独立 StepRef 调 A::AcceptLocalProfilePreference(step, slot1)。 | 偏好值 **false→true**，参数写次数增加 **1**。C1 尚持 Primary 写集。 | 同一个 A 对象里有绕过写集的入口。这里直接驱动偏好接缝，未运行完整 Profile 任务。该写入本身有旧语义，处置冲突需批准。 |
| 三　C0 已有参数效果时 Stop | 用 ResourceFixture 的真实 A/R/H 图，参数端口按原夹具写值，唯独 ICCID 写后返回 {0,false}。令 A 使用该参数适配并重绑 R 结果口。执行 StartC0，调 StopAcceptingAndDrain，等 Owner 空闲，再调一次。 | **原完成数 0，角色进行中 1，Radio 命令 0，最终 Drained 1。** | A 的 HasPendingDeviceEffects 只看 pendingPrimary_，C0 没有该设备条目。写入未定、原完成责任和角色资格都不能由此核销。这是实际 Host 反例。 |
| 四　B 的迟到否定证据 | ResourceFixture::Start(false)，以 45000ms 调 AcceptPrimarySwitchTimeout，再用 ResultAt(0, ConfirmedNotApplied, 83886087, false, true, true, true) 交原命令结束、可复用及已核对现态，等 Owner 空闲。 | 超时已接纳，迟到回包已接纳，**R=Available，A.HasPendingDeviceEffects()=true，原完成数 1**。 | 一次完成和 R 解隔离有效。A 在这一节点仍持待核对项。产品需要具名的业务核对/放行或驻留移交。不能收到迟到回包就自动改角色或重试。 |

关键写点可从本提交直接跳转。实验一、二分别定位到成员构造和未参与写集的偏好入口。实验三沿 C0 未定分支回到 H 的排空判定，实验四沿迟到证据回到 A 的待核对项。[C1 成员](https://github.com/zjuluowu/multisim/blob/befd76d4eba73ade9552248c303e3d6f8427c8ce/telephony_core_service/services/multisim/role_assignment/assignment_service.cpp#L253) · [Profile 写点](https://github.com/zjuluowu/multisim/blob/befd76d4eba73ade9552248c303e3d6f8427c8ce/telephony_core_service/services/multisim/role_assignment/assignment_service.cpp#L662) · [H 排空判定](https://github.com/zjuluowu/multisim/blob/befd76d4eba73ade9552248c303e3d6f8427c8ce/telephony_core_service/services/multisim/infrastructure/assembly/primary_host_lifecycle.cpp#L52) · [A 迟到接纳](https://github.com/zjuluowu/multisim/blob/befd76d4eba73ade9552248c303e3d6f8427c8ce/telephony_core_service/services/multisim/role_assignment/assignment_service.cpp#L539)

### 4.3 WU 能否支持完整替代

作为迁移分组，九个 WU 足以容纳本次抽到的写点。它们管理的是字段、执行和交付责任。短信存储、STK、运营商规则、厂商 ABI 等能力仍按 SIM 功能编号逐族退出。**不足在于从某一行 WU 单独出发，还不能找到所有必须同时关闭的入口。** 下列是逐单元判断。全产品消费者与条件分支覆盖仍 OPEN。

| WU | 首次接管和不能遗留的旧入口 | 结论与核销依据 |
|---|---|---|
| 01 账户视图/绑定 | G3a，InitIccId、两类 DB 重读、排序、Monitor 恢复、卡探测 | current 与 historical 分开导入。角色列只作来自 A 的投影。须采稀疏 simId、空槽、重载失败的兼容读轨迹。 |
| 02 active | G3a 先委托所有账户字段写，再迁激活用例。包括初始化、普通/卫星、拔卡 ForgetAllData、UpdateDBActiveByIccId、恢复 | 区分缓存、DB、设备。force 保留原分支。D §9.4 补漏项，核到 InnerKit/CMI 直接入口。 |
| 03 Voice/SMS | G3a，默认 setter、建行初值、重载、缓存更新及通知去重 | 一次建行接受 A 的字段。读取与选择跨槽，不能按设备域建多个默认值。 |
| 04 Primary/Data/偏好 | G3a 转建行/重载片段，G3c 同转参数、last、偏好和通知去重。初始化、恢复、Core 两个 setter、eSIM 切换均入表 | D 主表欠两类跨模块来源。一次准入的集合须覆盖独立 Data 与 Profile 提示。实验二说明同属 A 仍可能旁路。 |
| 05 资料/号码/标签/IMS/探测 | G3a，显式写、初始化、getter 后台、Carrier Loader、CardFileDetectManager 和 eSIM 关闭提示 | EsimFile::DisableProfile 按位置写带 slot0/slot1 后缀的恢复提示，EsimManager::DisableProfile 另写无后缀键。按旧节点分别列出，前两者有标签恢复读者。无后缀输出的域外读者 OPEN。 |
| 06 卡态/Records | G3b，硬件与软件状态输入、File 回写、Refresh、ClearData、LOADED 及电话簿依赖 | 人工 ABSENT 附原因。局部刷新后仍可接纳不相关 EF 的有效响应。旧解析器、File 扩展和写回边一起迁。 |
| 07 进行中/发令/重试 | G3c，主动请求、Controller 定时/自调用、失败恢复、设备回包、停止时已入队重试 | T 持激活进度，A 持切换/重试，R 持传输。切换时先停旧自主发起、核对已发命令和旧出口，才启新重试者。 |
| 08 eUICC | G3d，普通/低优先级 File、开通道的查询、关闭/重试，另追 ARA-M 特权及厂商入口是否共域 | 对已证实共享的资源一次接管全部使用者。保留 Profile 内外结果、库存读源与 66250 的任务归属，资源未知时保留该旧路径。 |
| 09 订阅、loaded、平台 token | T 的账户 loaded 随 01，R 的卡 loaded 随 06，外围登记/平台订阅按 G3d 迁入 | 死亡清理、首帧、重连、退订前已入队回调分别核销。不能等外围对象搬迁才交出前两项写权。 |

物理替换结束须满足三个条件。根产品 GN 与 Feature 源表切至新实现，旧公开入口及直接 C++ 消费者仍可用。旧路径找不到独立 writer、executor、retry 或会再生任务的登记。受影响产品能从持久前缀和在途效果安全恢复或切回。同一构建中只读比对可先行，影子路径不得写参数、发设备命令或重复发布通知。回退前先封口、核实已发生效果和旧构建可读性。单纯切回 Git 提交不会撤销一次已经执行的 Profile/APDU 操作。依据 A §6.3、§22，B §17，D §5.2.1、§7.5。

## 第五章　架构风险与反例

| 场景与触发顺序 | 会观察到什么 | 已有保护 | 证据缺口／验收断言 |
|---|---|---|---|
| **关闭时仅存储未定。** C0 已写主槽，再写 ICCID 未获完成确认 → Stop → 再次 Stop。 | 实验三已观察到 Drained 与未完成的原调用并存。宿主若据此销毁，会失去接收结果或按原契约交付的对象。 | A 保留冲突资格，R 正确显示无设备命令，H 有重复 Stop 的封口收据。 | A 还要交代“尚有业务效果/交付责任”。比较参数已写与未写两种前态。没有完成或驻留接收者的交接收据时，H 不能报告完整排空。 |
| **晚证据只解决了一半。** B 超时后旧命令有可信未生效、已结束/可复用证据。另一个 Primary 随后请求相同写集。 | 实验四确认 R 已可用，A 仍留 pending。长期无接续将阻塞后继。盲目放行又可能吃掉未核对偏好。 | 原完成一次，旧结果不能复活旧请求。A::RetryPrimaryAfterReconciliation 仍是未实现接缝。 | 以同一原请求关联分别核对设备与参数/角色前缀，允许 A 在业务闭合后退资格。后继不能读到混合前缀。是否自动重试另行签收。 |
| **四端点两栈。** 两个真实协议成员，另有一个隐藏端点与一个受限端点 → 普通 C1 → 等所有成员。 | 实验一已见预期外的 CHECK。若无相应响应，可能卡住阶段或走错误恢复。 | R 能拒错 serial、错步或重复成员。角色写集阻止另一请求改 Data。 | 不可路由的成员根本不能入组。按产品协议给出数量和确切成员，两栈、独立域、多栈分别检查命令集合与不相关卡零影响。 |
| **跨业务同键。** C1(userSet=false) 持偏好写权 → Profile 返回后提示 true。另一路独立 Data/Voice/SMS 进入。 | 实验二已见偏好旁路。不同写入的相对次序可能改变重启后的用户偏好。 | SetDefaultRole 对 Data 使用相交写集，Voice/SMS 可不相交。同线程嵌套请求已有用例。 | 把偏好入口也纳入同一序列，冻结原先的写入节点和失败返回。分别测可并行的 Voice/SMS 与应互斥的 Data。未证实外部 writer 仍 OPEN。 |
| **回调重入。** A 在 primaryCommitLock_ 内发布通知。监听者同步等待另一线程执行下一次 Primary 或查询。 | 另一线程需要同一锁时可能相互等待。未串行的查询/SetDefaultRole 则需证明线程归属。此为可构造交错，未复现产品死锁。 | 同线程递归及同一写集冲突已覆盖。R 的平台 Submit 会释放 R 锁。 | 在“准备输出→出锁调用→凭原资格接纳”的边界注入同步重入和异步 Stop，逐条比对可见性、完成次数与线程等待。 |
| **同槽换卡、刷新与重载。** 甲发号码查询回写或 EF 读 → 同槽换乙，或只刷新甲的一个 EF → 老结果和 DB reload 晚到。 | 用槽号重查可能写到乙。使用全局代次又会误丢未刷新 EF。Provider 已写而 T 拒绝绑定时，持久效果仍需处理。 | BindingIndex 防同进程 ABA，SessionRegistry、LoadSession 按应用/范围，AccountRegistry 按字段水位。 | 初始发现、丢卡、重载、未建账户 PIN、独立历史读取一起测。只让受影响成员失效。固定号码回写目标会改变原后台解析时点，G3b 另行裁决。 |
| **部分效果与恢复。** C1 预写、设备失败、恢复旧参数一键成功。或 B 成功后主卡广播已发、Data 写未确认，再遇崩溃。 | 参数两键、投影、通知与设备可呈不同前缀。整用例重放会重复发令/广播。只记录一个成功布尔值无法解释恢复。 | A 区分 B 后写和 C1 前写，原轨迹允许已完成的参数失败不改变外层成功。 | 记录各必要键、操作节点、原目标和仍未知的效果。重启通过实际读源核对。无已批准 Outbox 时不承诺旧通知必达。 |
| **事后开通道及跨服务互斥。** 低优先级读库存与特权 ARA-M 开通道交错 → 关闭超时，本地 channelId 清零 → 新任务重入。 | 如两者共享配额，会在旧通道实际未关闭时重用。EsimFile::SyncCloseChannel 等待/重试后确实清本地字段。 | A §12.3 和 B §9.2 已识别查询型使用者。ResourceBarrier 能隔离**已纳入同一域**的命令。 | 产品提供逐命令共享键、可在隔离态执行的安全核对与物理释放证据。关闭响应/本地清零/服务死亡均不足以替代。 |
| **宿主异常关闭。** 已发布 SIM Manager 后搜网初始化失败。或 Stop 时 CMI 旧句柄、RIL 回包、延迟任务/插件调用仍活着。 | 提早销毁使原完成出口失联。若卸载函数所在 SO，保留对象引用也不足以保护代码。 | Core 共享同一 RIL。Host 封口及调度器排空覆盖特定 Primary 图。 | 按已成功创建的子集清理，封存旧句柄仍接纳原完成，实际回调返回后再释放。另验搜网、呼叫等 RIL 消费者连续性、库驻留及匹配销毁。 |
| **远端静默死亡或级联。** 通信共享代理消失无拆除事件，同槽迅速重连。eSIM 关闭同时触发域外 vSIM 变化。 | 旧来源的 READY/结果可能被新代理接收，错误地重建账户或设默认 Data。 | A §10.4 要求来源/会话分代，§7.2.1 将父意图、远端任务、本地原语分开。 | 产品确认死亡检测窗口、关联证据、受影响范围、互斥与单重试责任。只撤本端受影响的资格。域外服务不在场时不得伪造原始卡观察。 |

## 第六章　架构优化方案

P0 表示**阻止所列生产切片**。一个产品的 MEP 证据不会成为所有只读迁移的先决条件。前三章已经有充分依据支持的责任边界可以继续用于设计与 Host 验证。

| 优先级／性质 | 影响范围 | 最小修改与验收 | 收益／潜在兼容风险 |
|---|---|---|---|
| **P0　关闭遗漏未定存储效果**（实测） | Primary Host → Core 封口、热重建、提前销毁 | 让 A 的关闭收据说明已接受但未交付的出口、待核对存储前缀及驻留移交。H 独立查询调度、A、R。用实验三同时跑已写未确认和未写未确认。 | 避免假排空。停止等待可能变长。超时返回未完成并保留接收者，不能换一个成功码或丢旧回调。 |
| **P0　真实资源与消费者闭包未证**（OPEN） | 启用 B/C1、eUICC 新屏障/重试，Core 生命周期或 SO 缩短 | 原命令族列真实资源集合、发令/响应的关联、何时已结束、何时可复用、核对预算与失败出口。用产品 RIL/HDI 与另一真实消费者验证。需要卸载时另验执行帧、符号、deleter。 | 避免双发、跨消费者误停。新增等待、拒绝、隔离均影响错误或时延。证据不足的命令族维持旧实现。 |
| **P0　完整写集尚未收口**（实测及清单缺项） | WU-04/05 切写，跨 Core/激活/Profile 提示、关闭恢复键 | D 汇总所有入口并追到键/物理写点。各旧入口在原写入节点委托唯一入口。以实验二、失败返回和同键跨业务交错验收，核对 Provider/厂商直接写者。 | 消除双写或“同 Owner 旁路”。不能随意丢弃一项原提示、挪到卡成功后，或同一入口先旧写再新写。 |
| **P1　Radio 身份与成员**（静态及实测） | B/C1、第三卡、隐藏端点及设备能力变化 | Radio 步骤按原 StepRef、目标绑定、协议会话和传输关联登记。只有真正访问应用才携 R 的 SessionRef。由协议计划固定实际成员，按原产品保留数量和特殊槽规则。实验一应被限在两成员。 | 消除身份混用，新增端点不扩大命令扇出。不得把新成员选择默认为现网政策。 |
| **P1　A 收尾、锁与外部输出**（实测加推演） | Late Evidence、未定写、通知重入及 Stop | A 维护仍有业务责任的最小条目。取得原前缀证据后决定释放资格。I/O、通知、完成出锁。同一 Owner 短提交域覆盖准入、消费与状态接纳。用实验三/四及跨线程重入证明。 | 同时保障一次完成和重新准入。先固定现有参数/通知偏序，避免缩锁后让不同请求交错覆盖。 |
| **P1　迁移证据与实现时态**（文档差异） | G2a、G3a/c、WU-09、入口反查 | B 划清已回退 G2a。D 逐字段写首接管阶段，补 InnerKit 入口、旧执行者/重试者及实际删除条件，关联匹配 Feature。 | 防止从旧表直接启动错误切换。保留旧 SHA 与编号，不更改产品行为。 |
| **P2　分支计划与执行的重复表示**（维护成本） | SIM-028 模式计划、长期运行的 C0 | PlanPrimarySwitch 的动作向量与执行方法各列一次顺序，当前执行按 mode 分支。建议只保留有消费者的模式/条件，顺序留在具名方法和差分 Oracle。同步结束时退请求记录。保留确需核对的前缀。 | 比开发通用动作解释器改动小，能减少两份规则漂移。删除向量前将其承担的顺序断言移到真实执行输出，不能丢失负控。 |
| **P2　收窄接缝**（有条件的工程取舍） | CardRuntime、基础请求头、GN 导出 | 随每个真实调用迁出相应设备成员及唯一屏障。将 C1 专属重试处理留在 A。公共头仅暴露所需契约。保持依赖检查与成熟 EF/ADN DAG。 | 限制跨域编译与运行影响。提前拆空壳反而增加维护成本，首个真实调用随迁随验。 |
| **P3　术语与入口去重**（表达问题） | C §4/§8、A §19、B §3/§18、D §5/§8 | 标明“角色/资源分配”是 A 的业务选择，“设备执行/复用”是 R。为历史步骤、旧测试入口加日期和现行跳转。 | 降低代码定位误差。保留历史锚点、WUs 和外部接口名称。 |

## 第七章　四篇文档的具体修订建议

| 主文档 | 可直接实施的修改 | 删除、转为历史或引用的内容 |
|---|---|---|
| **A 目标架构** | §3.1、§10.1 在既有身份表补两行实例“换 Profile 而账户保留”“Radio 切换无需卡应用会话”。§11.1～2 按卡步骤/Radio 步骤分别列必需证据。§14.3 的 L3 收据加“原调用、存储未知前缀、未释放角色资格的完成或可证明的移交”，另保留 L4、L5。§22.2 增实验一至四及无关角色不受影响的断言。 | §19.1～2 的旧 systemtest 位置/“下一切片”移入该轮历史，现行入口统一引用 test.py 与能力矩阵。§6.3 保留不变量和首接管例子，将逐入口清单交 D，防止两个清单再度分歧。 |
| **B 源码详细设计** | §3.1 加入 infrastructure/assembly/primary_host_lifecycle.h/.cpp，标明 befd76d4e 输入下 94 个 C++ 单元，另有 BUILD/README。§7～9、§15 写明当前 A 锁跨度、偏好两入站、Radio 合成 SessionRef 和全目录参与者。分别给出下一接口签发方、使用方及返回节点。§11 列 A/调度/R 的 L3 收据及实验三。§16/§19 明确业务核对如何结束。§4.2、§9.1 说明 CardRuntime 和独立 R 执行器在同一物理域的排他装配。 | §17.3/§17.3.1 将“本分支产品已直接编译”“当前只读闭包”改为原接入 SHA 下的 HISTORICAL。首段写明现行根 GN/Core 及 g2a_reference 实际落点。§18 把已运行的 C0/B/C1 入口和仍为伪代码的路径逐项区分。 |
| **C 代码目录结构** | §4.1 增“仅账户 active 投影及激活用例”。§4.2/4.4 用“APDU/通道协议在 card、物理独占与复用在 device”作共同例子。§5.3 写 H 借 Core 共享 RIL、收各 Owner 排空责任。§5.4 以 C1 为例区别通用任务投递和业务重试决定。§7.1 增 T/A 契约依赖与反向运行回调的小例子。 | §3、§8 的 card_selection、integration/support 和搬迁步骤注明为已完成的目录历史，链接既有迁移记录。删去会让人再建旧目录的操作口吻。维持五组、现有 public/visibility 及“无实际代码无需空目录”的决定。 |
| **D 架构与功能迁移** | §5.2 首行改为 H 静态配置、R 原始实例、T 已接纳运行清单。§5.2.1 在 WU-04/05 写入第四章列出的跨入口及真实键，在 WU-08 加 ARA-M/厂商共域待证行，在 WU-09 加分阶段 loaded 移交。§7.5/8.1 将 G3a 的片段与 G3c 的参数/重试写权分开，标委托安装、首次切写、旧执行/重试撤除节点。§7.4 加原完成与存储核对收据。§9.4 在 B1 表后加本 HEAD 的增量行 UpdateDBActiveByIccId → SIM-024/WU-02 及直接调用链。 | §5.1.1 的 application/topology 等旧建议路径替换为 B 的位置引用。§5.4/§6.3/§7.5 的“O 激活/O 发令”改为 T 激活或 A 切换驱动 R，O 提供关联。§8.1 的活动 systemtest 入口改为 TEST_F/test.py。保留 §9.3 的 B1/W1 摘要、SIM-062 未装配说明和既有稳定编号。 |

## 第八章　最终架构裁决建议

**附条件批准。**

作为设计基线，我会保留分类目标、T/A 单写者、卡与设备分责、每类用例唯一的主编排者、原接口逐步可见性以及五组目录。依据是原功能和文件已有归属，T 的绑定/重载、A 的 Primary 分支以及 R 的租约可以独立取证，已有 Host 差分可以识别真实行为变化。为解决本次缺口，不需要新增一个全局协调器、持久请求系统或公共 Subscription API。

**正式签收这份基线之前**，应由架构、宿主和迁移负责人共同完成三项可核销的修订。

1. A/B 明确“设备已空闲、业务仍在核对、原调用尚未交付”分别由谁持有，给出销毁或向常驻者移交的条件。把实验三的假 Drained 和实验四的持续 pending 写成验收断言。
2. B 将 Radio 关联从卡应用会话中解开，指定协议成员来源和不具备路由的处理。A/D 明确新增拒绝、隔离、固定目标、迟到抑制的兼容裁决。两槽的既有轨迹与四端点两栈的非目标不受影响都要可测。
3. D 补写者、字段、首接管和旧执行者退出点，B 消除 G2a 的现状矛盾。用本提交复核源表、直接调用、Feature 和 K/V 副作用。批准记录应给出责任人、适用产品/分支和证据，不能由本评审代签。

**具体生产放行**另按切片取证。WU-04/07 和 Core 生命周期接入须关闭相应 P0，保持同一构建中一套实际写者/发令/重试者。匹配产品的 Provider、RIL/HDI、SA/IPC、Feature、镜像及必要设备证据仍 OPEN。动态卸载才要求指定 SO 的 L5，新四卡、MEP、vSIM 与通信共享政策由相关产品独立签收。可复核的只读、源表治理和纯规则工作可以继续。历史 G2a 检查点 cc787945 可在工作分支定位，不是本次 main 的祖先。既有合入后回退的只读实现不能作为今天的生产验收。
