# AF-1 向 AF-2、AF-3 的实施交接（2026-11-24）

本文给出 **Target Host 基础框架**的施工顺序、可观察断言及后续生产切片的入口条件。源码核对基线为 `main@d9c2d23dce7148df1fc474ce598fb1cff55b839d`。根 GN 使用旧 SIM，目标逻辑 target 为 testonly。下表的方法名分为“现有落点”和“拟增加的契约”，测试均为待执行验收。架构负责人签收相关契约、实施范围获授权后，实施者才可启用相应切片。当前结论和门禁以[唯一执行状态](SIM_Refactor_Execution_State.md#current-status)为准。

本交接采用检查点 §13 的 AF2-01～08 编号与 §15 的职责，按 §18～23 的反例收紧。B1 另依次应用 Part 24 原操作归因／关闭轮、Part 25 成员重申／重放、Part 26 读取覆盖／跨 Owner 交付。后文“AF-3”表示以 AF-2 接口承接随后获批的业务验证和迁移切片；具体生产切换仍逐 WU 按既有阶段签收。[架构检查点](AF1_Architecture_Design_Convergence_20261123.md) · [Part 24](AF1_Part24_B1_P0_Design_Revision_20261123.md) · [Part 25](AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md) · [Part 26](AF1_Part26_Reload_Causal_Coverage_Design_20261124.md)

## 1. AF-2 工作顺序与验收点

路径在未注明时均相对于 `services/multisim/`。T 是账户与绑定 Owner，A 是角色与参数 Owner，R 分为卡会话和设备执行，I 提供实际存储／平台返回，O 提供原请求及步骤机制，H 负责装配与寿命。每项预期都须从表列真实入口触发。装配缺依赖时返回缺失阶段；不通过成功桩或未实现能力补全整图。

|顺序／接口及 Owner|现有落点与拟实施改动|并发、失败与 Stop|正向、反向与可观测出口|
|---|---|---|---|
|① AF2-07(a)(b)，A/O、R 的 M06/07/10/11|在 `role_assignment/assignment_service.cpp` 的 StartPrimarySwitch、AcceptRadioStep、AcceptC1RetryDue、CompleteOnce 中拆分登记与调用；`device/resource/resource_radio_runtime.cpp` 的 OwnerCallbacksDrained 等先取本地快照，再查询外部投递端口。|A 在提交域记录发令或一次领取完成的身份，解锁后执行平台调用、通知和原回调，回来仅按原请求登记归还。同步回包可早于提交返回，先挂原关联。查询端口后重验局部状态／版本，防止锁外检查得到过期的空表。|一次 B 成功回调；同步早回包、回调内再入 Start 或 Stop、通知重入与重复完成。核验每步平台次数、回调进入/退出、已领取未返项和最终原返回次数。每个小步回归 C0/B/C1。|
|② AF2-01，R/H 的 M08|在 `device/resource/resource_barrier.{h,cpp}` 的 Acquire、MarkSent、CancelConfirmedNotSent、Complete、AcceptRecoveryEvidence 建立短域原子操作；H 创建并注入，`card/runtime/card_runtime.h` 与 `device/resource/resource_radio_runtime.h` 移除各自的独立屏障所有权。|执行适配 → 屏障；锁外调用平台。一次获取原步骤全部物理键，匹配实例、原持有者、全键集和 fence；未发确证才能走未发撤销，交接不明保留隔离。正常结束验证该命令需要的效果、结束和可复用，恢复另验当前状态。|两种命令族争同物理键，后到者发令数零；不同键可并行。业务域不同仍争同键；业务域名相同不能据此把不同设备键视为一个。错 fence、缺一键、仅用户完成均不能释放。第二命令族使用 Host 测试执行适配。|
|③ AF2-07(d) 接 AF2-02 第一步，R·card/T/A/H 的 M01/03/04/14/15/18|从 `card/runtime/card_runtime.cpp` 的 PublishAcceptedCardDiscovery、InvalidateVerifiedCardSession 以及 `card/state/session_registry.cpp` 分出已实现的会话观察；T 在 `account/topology_service.cpp` 的 DiscoverAccount、ReconcileDiscovery、AcceptCardSessionInvalidation 自己的执行域登记、受理和恢复。H 在 `infrastructure/assembly/sim_module_factory.cpp`、`sim_module.h`、`host_ports.h`（由 `exports/sim_host_ports.h` 引出）构造 T、共享持有 A、唯一屏障、Radio 执行适配及 PrimaryHostLifecycle。|先绑观察、角色规划、投递、完成端口与初始资源证据，成功才发布。卡侧来源短登记排在耗时 Store 之前，I/O 锁外。所有失败点留原票／路由，来源撤销先于公布时禁止建新绑定。AF2-02 第二步再接原 int 完成与 Primary 兼容入口。|经装配图跑一次 C0；从真实卡观察入口发现到 T 绑定。阻塞写再失效；观察不同阶段失败时读取 T/A 原票与收据。缺绑定或必需初始证据时报告建图失败阶段。释放最后强引用后才能按匹配 Deleter 销毁。|
|④ AF2-03，A/R/O 的 M06/07/09/10/17；依赖②|扩充 `role_assignment/primary/radio_execution_port.h` 的规划与逐次交接结果；`device/resource/resource_radio_runtime.cpp` 的 SwitchPrimaryRadio 关联每个 token；`infrastructure/request/runtime_operation_store.{h,cpp}` 增 Radio 专用 RecordRadioSent。A 经 PlanPrimaryRound 冻结真实成员及路由版本，消除由 bindingRevision 合成 SessionRef 和遍历目录生成成员。|A 写集、原 StepRef、localRound/phase/member、transport 与 lease 分别登记。逐成员返回 ConfirmedNotSent / HandoffAccepted / HandoffUnknown。部分交接保留已发成员；路由版本改变或 Unknown 进入原轮核对。|四端点两栈每阶段两条命令；缺必需路由时参数预写为零；内联回包、错轮、ACK 冒充 UPDATE NOTIFY、提交后丢回复、重放旧 epoch。断言原成员各一次、无新增发送、隔离及原资格仍可查。M2 阶段条件见 §3。|
|⑤ AF2-07(c) 接 AF2-04，A/I 的 M03/12/13|在 AssignmentService 的 StartPrimarySwitch、SetDefaultRole、AcceptEntryPrimaryPreference、AcceptLocalProfilePreference，及 `role_assignment/assignment_store_adapter.cpp` 的参数写／恢复实施逐键登记。父 StepRef 复用原资格，独立来源独立准入。|A 域内登记原键和值／快照，锁外写，回域重验后记原键效果。完成调用而效果未知时仍有核对项。Data 与 Primary 取实际写集交集；Voice/SMS 保持独立。冲突提示留 A，沿原返回点回应。|无冲突一次写一次释放；C1 持有偏好键时独立提示不直接落写；首键写成后次键未知，读出实际前缀、具体待证键与通知次序。成功 setter 与两视图 Reload 交错按 §3 N1 验。AF1-13 决定前保留原生产入口。|
|⑥ AF2-05，T/A/R·card/O 的 M19；依赖①⑤及本票登记|在 TopologyService、AssignmentService、卡会话发布组件和原完成出口增加只读 ReadCloseReceipt；以现有票、预留、执行和交付明细构造 CloseObligation / OwnerCloseReceipt。现有 AssignmentService::HasPendingDeviceEffects 只读 pendingPrimary_。|逐项记录 q、owner、kind、原请求／票及 subject（键、成员、token、交付），封口和活动版本一同取样；已领取回调到真正退出才归还。另列“已写未确认”和“未写未确认”；读收据无补发和业务写入。|C0 两种未知前态、B 设备已成功而参数未知、T 双票 Unknown 均可分别查账。结清 X 不消 Y，A 的释放请求、释放完成和 T 收到原确认可分别暂停。|
|⑦ AF2-06，H 的 M14/16/19；依赖③⑥|在 `infrastructure/assembly/sim_module.h` 对应的实现定义 StopAcceptingAndDrain，复用 `primary_host_lifecycle.cpp` 的 Primary 子图封口／排空；汇入 Facade、T、卡、Scheduler、R 和原完成。模块私有结果表达业务责任未了。|同 q 冻结参与者、一次根封口；可回收的原票续行保持可达。交接以子项登记、确认及本端执行真正退出为界；判定按 §4 的双遍快照，保留旧图直至可销毁。|执行 §4 全部截面与 AF1-18 跨实例负控。观察每参与者活动版本、原调用和平台次数、原出口次数、屏障状态、图的实际持有与销毁时点。|
|随项 AF2-08，测试／构建维护者|在逻辑 target 的 BUILD.gn、`test/cases/architecture/source_migration.json` 及架构规则中同步源表、include、allowed_edge。|只接已实现能力，独立记录固定文档摘要等既有失败的输入、报错及归属，不通过更新期望值消除失败。|运行 `test/scripts/run_architecture.py` 和带 `--with-gn --migration --output <仓外目录>` 的版本；验证实际 Host 可链接、可运行，独立记录每项结果。|

### 资源与交接的两项故障回归

|入口与确定性交错|实施动作与应可读回的结果|
|---|---|
|AF2-01 · 两个已证 Available 的物理键共同调用 ResourceBarrier::Acquire。在独立探针的分配排程中令第 4 次分配抛出，再逐一覆盖其他可失败的准备点。现有实现边遍历边把含 lease 的记录复制进正式键表。|先完成整组租约、各键记录及返回值的可失败准备，再用无抛尾段登记完整键集，或证明异常时全部回到原状态。异常返回后逐键读原状态及原持有信息，两个键均应仍可按原资格使用，不能遗留任一 InFlight 或无主 fence，平台发令为零。关闭收据不得漏报真实占用；成功对照一次得到覆盖两键的同一 lease。第 4 次针对本次探针的复现位置，后续实现的注入点随实际分配点更新。|
|AF2-03 · 对同一原 step/轮/阶段与共享 K 先提交成员 P，在平台 Submit 栈内暂停；并发提交成员 Q，Q 返回 ConfirmedNotSent；此时再尝试重复 P 及另一个争 K 的请求，最后让原 P 返回 HandoffAccepted。现有 SwitchPrimaryRadio 按 token 留在途，成员集合在平台返回后才插入，Q 的撤销分支只检查 group.accepted 等局部条件。|R 在进入平台前、同一短域内按原 step/轮/阶段/member 登记 in-submit、token、lease 与执行体。已占的 P 不能再次提交。Q 的结果只撤 Q 的该次尝试；原 P 未交还时，禁止取消整组租约，K 上其他请求发令为零。P 的交接受理回到原 token/fence，即使 Stop 并发也保留至原效果、结束/复用与执行归还均获证。观测 P/Q 各一次平台提交、P 回执可关联、K 不提前 Available、M19 的原成员与交付项。在所有成员均确认未发且所有 Submit 已返回的对照中，方可按未发撤销整组。|

## 2. 发现、视图与角色交付的实施顺序

以下切片在 AF2-07(d)／02 中建立业务事实，在 AF2-05／06 中接入收据。原票 t 为 (epoch, admission)，L 为一次指定视图的 Reload 票，W 是 T 对 t 的业务处置见证。C/H 分别为 Current/Historical。m 为每视图成员最近事件，j 为该投影连续存在的起始刻度，F 为逐字段版本，r 为槽绑定水位。

|依赖与落实点|保存的事实、线性化点与失败出口|Host 正反断言|
|---|---|---|
|1．T 签 t、A 预留、I 执行一次原写。改 `account/discovery/discovery_observation_ledger.cpp` 的 BeginStore/CompleteStored、`account/topology_service.{h,cpp}` 的 DiscoverAccount/ReconcileDiscovery、`account/account_store.h` 和 `account/account_role_planning_port.h`。|准入先挂 t、来源、slot/application、m/j/i/F 或未知身份时原水位、r 和原计划；锁外申请并按 t 关联 A token。T 域授予唯一发送权、登记 call/执行体，再锁外调用 WriteDiscoveredAccount。I 按原 call、实际返回身份和原字段的肯定／否定／未知效果交还。读到同值、ICCID 相同、affected rows 或调用结束单项均不能补出字段因果。|同位置 X/Y 各一次原写，独立控制两个写返回顺序及两次核对顺序（2×2）。注入错票、同值由先前值／后写产生、混合字段、Stop 抢在发送前或后；断言零或一次原写、每票的具体效果、A token 及 M19。同票不得重新 Insert。|
|2．T/A 完成原票的业务处置。改 `account/account_registry.cpp` 的 AcceptDiscoveredAccount、`account/binding_index.cpp` 的 AcceptBinding 及 TopologyService 原票续行。A 在 `role_assignment/account_role_protocol.h` 及其规划端口解析、接纳并精确释放原预留。|各自短域先准备候选与所有可失败分配；T 同域重验来源、位置、身份及两视图，预备节点后无抛发布 Account、成员/字段保护、Binding、W 与下一步。A 持 token 与实际身份的解析见证，T 发布 W 后按原票/原 token 和处置向 A 结清。失效区分待证和有据仅结历史；未取得当前资格不建立当前关系。原 A 回执可重读，待确认与原执行俱归还才回收。|直接成功及恢复后成功走同一写点；逐一打断准备／身份解析／发布前后／实际释放后丢回执。失败读者只见原组，成功读者见完整 W。新票同身份重申使相应 m 推进，j 不动，只升有明确写依据的 F，原绑定确认 r 不变。同票重放不升版、不再发布／释放。|
|3．T/I 接 Reload。改 `account/topology_service.cpp` 的 PrepareAccountReload/AcceptAccountReload、`account/account_registry.cpp` 的 BeginReload/AcceptReload/AcceptReloadProjection 及 `account/account_store_adapter.cpp`。|L 对应实际取样，冻结 epoch、view、attempt、水位 w 和 expected。I 交真实查询的世系/范围、谓词、遍历终止、身份列及各字段有效性；默认覆盖 Unknown。T 在写点按最新保护下界检验认证有序读或可见完整前缀。未知原写及不完整遍历不可破坏现值。删项比较 expected、i/F、成员 m≤w、完整缺席证据及被删保护；留负向墓碑。有行按原连续段 j、身份、局部 F 与该字段的证据合并。Reload 不写 Binding。|新签 L 返回同形空集，分别以滞后／已覆盖后真实缺席／无覆盖证据驱动，得到保留／删除／保留。旧名称不得覆盖 New，独立合法号码可变。验 C/H 各自 m/j/F、r 不动；先收较新图像再收只覆盖初始写的读仍保留后继。游标中断、重连、错域、旧行重放与撤后重建分别观测。|
|4．T 向 A 交付可用的整组。改 TopologyService::AcceptAccountReload、AssignmentService::AcceptReloadedRoleProjectionSet、AccountRoleProtocol::AcceptReloadSet。|同一 L 的 account/role 键集合一一对应，身份、成员、遗漏行都可判定才登记一次性交付。T 在域内撤回资格，A 在自己锁内复核整组角色列、负向保护、expectedRoleRevision、attempt 并争取提交。A 先准备候选，取得资格后无抛替换并记原回执；丢答复重取本次处置。精确记录 T 的逐字段结果与 A 的整组结果。|对照无关 T 字段冲突与成员冲突，前者在具备独立角色证据时允许整组，后者拒绝整组。两批行数相同但 s 错配、重复、setter 插入、错误 view、撤回／领取两种先后、角色回执阻塞；分别查两套角色 map/版本、L 处置和 M19。禁止把过滤后的子集作为完整 map。|

以下两项回归分别检查查询样本和释放回执能否越过原来源的寿命边界。

- **重建后同 s 的旧号码。** 令身份号与 ICCID 相同的成员退出后重新加入，由 T 确立新存续段 j，再签发新 L，使本地 expected 与当前 j/F 均匹配。I 故意返回来自上一次存续段的旧行和可解码号码。即使号码从未单独保护，T 也必须验证**该外部行样本**覆盖新加入／重建的来源依据，或有同等证据把它绑定到本次存续段。仅本地 j 相等、同 s/ICCID、号码列解码成功均不足。拒绝时号码及 F 原样、r 不变；以已证覆盖新加入且读到不同号码的样本作正控，此时才可合并该列。
- **A 合写后释放预留。** 让发现原 call 对某角色列的写确已归因，T 获 W，A 按本票正式接纳相应角色事实。在同一 A 提交域先把该列、身份/范围及原效果的因果下界登记到正式角色状态，再确认资格已释放；若仍待业务处置，A 必须保有可追踪项。精确释放、重复 Finalize、T 回收原票均不能擦掉仍需约束读取的下界。随后签最新 L，以成员集合合格、本地 A 版本匹配、角色列图像早于合写的输入尝试整组替换，必须拒绝并保留已证角色；同源覆盖该前驱的完整后继可依通常资格接纳。核对 A 两视图的角色值、保护摘要、原释放次数及 M19；历史效果结清而无当前角色写权时，不得凭释放制造当前角色。

真实 Host Provider 桥位于 `test/framework/internal/host/primary/legacy_account_store_provider.cpp`。当前跨 helper 的查询没有已证实的 read-your-writes 路由或可比较提交前缀。Host 应在 I 独立控制原写、可见图像、读取取样、游标终止和回执交还；旧返回码保留。静态保护本身无需永久占用关闭票，实际待补证、持有的旧执行、跨 Owner 投递须列入 M19。[Legacy 查询实现](../services/sim/src/sim_rdb_helper.cpp) · [旧缓存更新](../services/sim/src/multi_sim_controller.cpp) · [目标桥](../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)

## 3. M1、M2、N1 在真实写点对接

|契约与方法|排序与状态责任|实施验收与观察量|
|---|---|---|
|M1 · R·card → T → A。SessionRegistry::InvalidateForVerifiedReplacement、CardRuntime::InvalidateVerifiedCardSession、TopologyService::AcceptCardSessionInvalidation/ReadPrimaryTarget、BindingIndex::RemoveBinding；扩充 M01 失效原因/作用域与 M04 短读。|R 按每个可信来源递增序号；在异步入队前持投递凭证，同一受理协议内先在 T 短域登记撤销／连续性未知。T 保存来源、原 BindingRef、连续性与 active 资格；不持 R 业务锁等待 T。A 提交域内借 T 受保护短读登记下一原目标意图；释放 T 后才外调。锁序只允许 A→T 读，T 不持提交锁调 A。|真实 R 入口依次送无关应用刷新、唯一来源暂失、可信拔卡、重复拔卡及同 ICCID 新代次。无关关系可用，暂失暂停相关新决定，可信移除 r+1、再建 r+2；迟到旧撤销不删除新绑定。一次 Profile 请求返回不足以证明原载体已移除。观察 r、Session、A 实际首写次数和历史可查性。|
|M2 · A 的 StartPrimarySwitch/AcceptRadioStep/AcceptC1RetryDue 联合 R 的 AcceptDeviceResult/Recover、I 的逐键写和 O 的步骤消费。先落实 M1 与 §1 的①②④⑤。|C0 每个参数／通知只用本次已登记资格；B 成功回包须另验原目标/active 后才登记 Primary/Data；C1 分别冻结预写、CHECK、UPDATE、COMPLETE、最终提交、恢复与 retry 的原轮/成员/路由。旧设备事实、补原协议、当前角色发布、用户出口各有独立条件；Stop 撤未开始的重试，保留原证据路由。|在 C0 两键间撤权，仅首键允许在途。B 发出后 active 中断，设备成功仍记原请求，Data 不写。C1 预写并部分交接后撤 X、发现 Y，送 ACK／错轮／原 NOTIFY，旧 pending 只收原证；Y 不继承角色或完成。逐个验原参数快照/写前缀、成员结果、lease、原完成一次和已排 retry 的零新增发令。没有换绑或 Stop 期间 COMPLETE 的原路由/命令证据时留原轮与隔离。|
|N1 · A 的 SetDefaultRole → AssignmentAggregate::AcceptRoleSelection、AccountRoleProtocol::NoteLocalRoleMutation；T Prepare/AcceptAccountReload 经 A::AcceptReloadedRoleProjectionSet 交付。|成功 Store 后仍须在 A 域重验原写集、目标，确认本地选择提交，原子更新规划版本及 Current 角色版本各一次，同时记录该次已证角色事实及替换下界；Historical 版本独立。调用返回但效果未知仍有 M19。现 setter 没有调用 NoteLocalRoleMutation，后者本身只增版本。|签 C/H 两票，走真实 setter（Host 在 I 层控制存储），读回 A 的真实选择、C/H 版本和角色处置；旧 C 返回被拒，有独立合格依据的 H 可推进。确认未写／重复/未知分别断言零虚增、原回执与核对项。对已知事实还要注入 setter **之后**签发但读图像落后的新 L，不能只靠版本相等替换。|

## 4. 最终签收包 Flow-4 全图关闭及跨实例隔离（历史检查点 Flow-3）

现有 PrimaryHostLifecycle::StopAcceptingAndDrain 封 A/Scheduler/R，等待 A 串行 seal 和 R 投递，检查 Scheduler 及 pendingPrimary_；SimModule::StopAcceptingAndDrain 只有声明。AF2-06 在模块层以 q=(epoch, closeGeneration) 冻结参与者，收齐 sealAck 后先收完**全部第一遍**收据，再收完**全部第二遍**；同 q、同参与者、各自 activityVersion/明细不变且满足终局才有共同空截面。入队前先登记，接收端建子项并确认后，发送端仍等待本地执行体退出。任何输入能绕过封口、未能证实退订/寿命、版本变化或耗尽均回未完成；跨 Owner 转交造成版本变化时重取整组。[当前 Primary 子图](../services/multisim/infrastructure/assembly/primary_host_lifecycle.cpp) · [模块接口](../services/multisim/infrastructure/assembly/sim_module.h) · [完整协议](AF1_Part24_B1_P0_Design_Revision_20261123.md)

|确定性暂停截面|必须从哪个 Owner 读到什么|释放闸点后的判断|
|---|---|---|
|C0 原完成已领取，正在原 callback 中请求 Stop|A/O 原 request、completion route、已领取未归还；入口原调用若仍在执行也列入。Host 返回在途，不 join 自己。|回调实际退出后只消该交付；原完成恰一次，存储或其他投递在途仍保图。|
|Radio 空闲，Store 正在执行、返回未知或仅部分字段已归因|T 的 t/call/具体列、A 的 token；C0/B 的参数效果按 A 键列出。H 能区分执行未返与业务责任未了。|只吸收原证据，不重发。W 已发布但精确 Finalize 回执滞留时 T 仍列待确认；释放 X 不消 Y。|
|Stop 后 B/C1 的晚证据、已投递的卡撤销或 Scheduler 任务到达|R 原 token/lease/隔离键、Card 投递号、原子项或原接收者任务；H 检查执行体退出。|正确证据只归原请求。错轮/旧来源不写后继；仅效果已明但设备结束/可复用缺失仍非设备排空。|
|两次收据采样间从 A 转交 T 或两个调用者同时 Stop|相同 q/固定 Owner 集合，接方已登记才确认，转移使相应版本改变；一次封口执行者。|重读完整两组；重复 Stop 不重复取消、Store、发令或完成。注销/投递失败保留未了项及原接收能力。|
|驻留接手者拒绝或仅认可 Y，X 仍欠原完成／业务资格|拒绝和接受均按 q、原 subject、接手者及覆盖内容追溯。AF-2 只需测试拒绝接管，接受分支为协议条件。|X 继续保留原图；跨实例接管也不能替代原图执行体退出。所有收据稳定且已结、设备条件满足，才返回 Drained 并释放最后的 A 持有者。|
|AF1-18 · 旧实例已交接命令 K，仍 Quarantined，尝试新建相同物理域的核心图|新屏障同键 Checking。宿主留存旧图/原证据接收者，缺独立证据不得发 InitialRecoveryEvidence。初始证据为发布必需时，新候选停在未发布/不可用阶段。|旧实例未排空期间新平台新增发令为零；旧实例收到正确结束/复用证据、实际 Drained，宿主另有“旧执行权均退出、物理域可用”依据，才批准新实例初始证据。错 epoch、其他键或仅旧用户完成不通行。|

已有 HostLifecycleOldAndNewInstancesRemainIsolated 在旧实例 **已返回 Drained 后**才创建新 fixture。真实 ResourceBarrier::AcceptInitialRecoveryEvidence 只能检查本实例及传入证明。故 AF1-18 需让两生命周期实际重叠，控制宿主真实签发节点，并记录新旧两侧发令计数。跨实例 Host 观测不解决厂商旁路、其他消费者与重建时的真实 RIL 命令归属。[现有 Host 用例](../services/multisim/test/cases/target/primary/b_async_primary_test.cpp) · [资源屏障](../services/multisim/device/resource/resource_barrier.cpp)

## 5. 里程碑、外部证据与 AF-3 入口

|交付界面|交付证据／责任人|缺证时的范围|
|---|---|---|
|AF-2 可启动的最小 Host 图|H/A/R 完成 §1 的装配、锁外调用和单屏障；能从正常 Host 入口跑 C0/B/C1 与卡发现，并产生明确的装配失败阶段。结构规则、GN、实际链接、受控执行及回归分别出具。|架构负责人签收相应 PROPOSED 契约；目录或单测可编译不能替代可构造可销毁。|
|代表性发现、恢复与关闭|T/A/I/H 完成 §2～4，从卡观察、原 setter、平台回包及真实 Stop 进入；记录输入 SHA、注入点与顺序、身份/键/视图、原输出、实际收据、失败与 sanitizer 范围。|业务接口仍有内部设计反例时，依赖该接口的实施/签收按独立 Gate 处置；无关项按各自依赖推进。真实常驻接手者、SO 部署和生产写权归后续。|
|AF-3 账户／角色 WU|T/A/Provider 对指定版本、库/用户、两种读源与重连验证原写逐列效果、读图像覆盖或独立合法业务后继、角色集合来源，以及旧“查询成功整体替换”的可见时点。核对初始化、重载、恢复、Carrier Loader、CardFileDetectManager、厂商入口、查询回写和实际旧 writer 退出。|若外部变化只靠缺覆盖的 Reload，安全保留会失去刷新活性；对应 WU 保留旧路径。AF1-13 偏好冲突须由 SIM/eSIM/参数服务确认原返回与时序；AF1-10 Port/Profile 基数及动态端点归 G4。|
|AF-3 Radio／卡执行及热重建|R、宿主、协议方、厂商列出真实成员/路由、全部共享 K 的执行者与域边界，出具原轮收尾、命令结束、状态核实及复用证据；含旧 SIM、其他 RIL 消费者、eSIM 普通/低优先级和 ARA-M 确有共享的通道。|M2 在换绑／Stop 后是否可发送 COMPLETE 待产品协议证明；AF1-18 保留 G2c。外部接手者/跨实例驻留、Core 共享 RIL 连续性、设备静止和 SO 卸载逐项验证。|
|生产工程与行为验收|按批准的 WU 搭接 Core/CMI、原 Manager 控制块及销毁，核对根 GN、Feature 与旧初始化发布顺序；同基线 Oracle/Legacy/Target 差分、匹配 OHOS 树产品构建、SA/IPC/Provider 与真实设备证据齐备。|AF2 保持 testonly。AttachSimModuleToExisting 与 CreateOwnedSimServices、号码/激活/PIN/EF/Profile 生产迁移及旧 writer/executor/retry 退出各自另行授权。|

## 6. 可复用的历史基线

|历史输入与固化报告|本次可沿用的夹具／观测口|
|---|---|
|T1 输入 `be63ae1249db5927a216602aceb0346db6d53a09`，报告记录 Target T1 14/14、Provider 桥 3/3。[T1 报告](../services/multisim/test/docs/T1_Account_Discovery_Host_Report_20261123.md)|扩展 c0_topology_integration_test 的发现、身份不匹配、Unknown 与换卡交错。从正常观察入口并读回实际 Binding，避免仅检查 Reconcile 返回 true。|
|T2 输入 `5796fce68dfcc2ea152106b7ab65c217c63557ad`，历史 Target T2 11/11、桥 6/6；报告分别记录专项 sanitizer 和独立架构摘要阻塞。[T2 报告](../services/multisim/test/docs/T2_Account_Recovery_Reload_Host_Report_20261123.md)|扩展真实 Registry／BindingIndex 的 C/H 读回、成员代次、字段以及角色版本。专测旧失效用例“可读 Current”的行为差分与 Provider 来源。|
|L3.2 输入 `befd76d4eba73ade9552248c303e3d6f8427c8ce`，报告记录 Primary 子图 TSan/ASan 各 16/16，真实 Legacy graph 的 sanitizer 有 FFRT stub 限制。[Host L3 报告](../services/multisim/test/docs/Host_Lifecycle_L3_Report_20261120.md)|复用 b_async_primary_test 的封口执行者、早回包、错关联、重入、Unknown、受控调度及共享消费者。AF2 须另建整图两实例和逐项 M19 场景。|

实施时按[测试入口](../services/multisim/test/README.md)构建并定点执行，使用仓外输出目录。保留每步 C0/B/C1、正常发现与恢复、架构/GN、确定性交错及适用差分/ASan/TSan 的本次日志。本文只完成源码与设计交接；本次业务 Host、sanitizer、产品和设备测试均未运行。
