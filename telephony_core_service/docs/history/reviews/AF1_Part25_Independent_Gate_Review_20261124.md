# AF1 Part 25 独立 Gate 复核 · 2026-11-24

## 1. 执行摘要

**Gate 结论为 `B1_REVIEW_BLOCKED`。B24-01 在 Part 25 的设计层已消除。** 对同一条 Y，旧票在 w=2 签发，新原票发布 mC=3，旧空读交付时失去删除资格。本轮重新编译原探针，当前源码得到 Current/Historical/BindingRevision = **0/1/1**，真实核心类配合提案的成员判定得到 **1/1/1**。

新的阻塞项 **P25-R1** 在 Reload 的读源契约。即使查询确实在新票签发之后执行，它所读取的数据库视图仍须覆盖先前已确认的写。两组独立探针给 T 相同的最新 attempt、w=3、完整空集和未变化的身份/字段。第一组的 Y 后来确实被移出 Current，删 Y 正确。第二组只有读取端还看不见刚确认的写，主数据中 Y 仍在，探针同样删了 Y。另一个有行反例把已确认的名称改回旧值，并把 nameRevision 从 1 升至 2。决定错误的是对读结果的授权。把本地时钟再增加一次不能区分这两组输入。

这证明了提案在可表达的输入上允许错误结果。滞后读源是本轮明确注入的条件；实际产品 Provider 是否提供足以排除它的一致性保证，仍属外部证据。重新提交 Gate 的最小条件是规定 T/I 如何认定来源具有所需可见性，以及能力未知时如何保留原事实。§15 给出正反验收。

## 2. 当前 Git 与证据基线

|对象|本轮读取的版本|
|---|---|
|工作区与远端|本地 main 的 HEAD、origin/main 跟踪引用、只读查询的远端 main 均为 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`。开始时已跟踪文件和暂存区无差异。六项既有未跟踪内容为 default.profraw、opencode.json、Part 24、Part 25、两份独立报告。|
|C++ 与测试|实际读取 HEAD。services/multisim 最后变更提交 `632f6a2a888cfbbc3c9387edc89732364e12a10e`，目标目录树 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。Legacy SimRdbHelper 另取 HEAD 对应文件。Target 仍属 testonly 切片。|
|[检查点 Part 23 §§23.1–23.3](../../AF1_Architecture_Design_Convergence_20261123.md)|SHA-256 `0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2`。T 持原票，A 持原预留，处置与释放分别有据。|
|[Part 23 独立审查 §§5、8、11](AF1_Gate_B1_Independent_Review_20261123.md)|`147d25d703419225f31937007c01c73d788f37f71a34a45897cb6612b3b7adf4`。记录了本地部分发布、外部效果归因、关闭排序三项退回理由。|
|[Part 24 §§4–6、10、16](../../AF1_Part24_B1_P0_Design_Revision_20261123.md)|`a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164`。采用原调用取证、T 准备后统一发布、稳定关闭收据。|
|[Part 24 独立审查 §§3、10–11](AF1_Part24_Independent_Gate_Review_20261123.md)|`f55cb963c6118dbeeafda037900c260746d570f0c2e7ad44687714f0c78ec313`。B24-01 的原反例与再次签票的正控。|
|[Part 25 正文及 C++ 附录](../../AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)|`4ff376e7adfae7ef1f475ebc8cb411030293c8c1a47bedc6780ea1289f753418`。附录实际提取源码 `c65fdcbcbfe8bf72c087bfe3f08990572d0feaa8b152de6375bb47d598a1d0ca`。以上未跟踪输入均按本地内容固定。|

## 3. Part 25 的设计要点与实际调用链

T 用 t=(RuntimeEpoch,admission) 识别原操作。一次合格发布同时产生 W、各受影响视图的最近成员版本 m、必要字段版 F 与实际 Binding 结果。j 记录当前投影这一段连续存在期间的加入刻度。BeginReload 固定 view、attempt、水位 w、expected 中的身份 i 和 F。缺席要检查本键 m≤w；有行则用 j≤w 验证存续，再按字段各自的 F 合并。A 的处置凭原 t/token/W 精确结清。下文 C/H 在视图对中分别指 Current/Historical，关闭协调者另写 SimModule。

**发现路径。** TopologyService::DiscoverAccount 调用观察台账、Store 匹配、A 准备及 Store 写，返回后才接纳账户、Binding 和角色片段。Ledger::BeginStore 限定同次观察的 admission。CompleteStored 可调用 BindingIndex，ReconcileStored 仅更新效果。ReconcileDiscovery 当前仍按位置取未决项，可能由匹配行导入账户再结束 A 预留。当前路径没有 Part 25 的 W/m/j 联合发布。以上属于实现差距，判读目标设计时另给足原票、原效果和 W。[TopologyService :64–240](../../../services/multisim/account/topology_service.cpp)；[DiscoveryObservationLedger :61–91、124–164](../../../services/multisim/account/discovery/discovery_observation_ledger.cpp)；[AccountRoleProtocol :28–65](../../../services/multisim/role_assignment/account_role_protocol.h)。

**重载路径。** PrepareAccountReload 在 T 域取 Registry 的票。AccountStoreAdapter::LoadAccounts 委托 LoadLegacyAccounts，按视图调用 QueryAllValidData 或 QueryAllData。桥用查询返回码填 complete，并回填原 request。AcceptAccountReload 检查 view、attempt 和角色/账户行数，用 loaded.complete 调 Registry::AcceptReload，随后可能把同次角色行交 A。AccountLoadResult 含原请求、结果码、标志及行集，没有读端所见提交的位置或跨重连来源代次。按次同步调用能确定调用顺序，无法自行证明读端可见先前写入。[请求/结果 :44–51](../../../services/multisim/account/account_store.h)；[适配器 :32–33、55–58](../../../services/multisim/account/account_store_adapter.cpp)；[Host 桥 :41–55](../../../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)；[TopologyService :275–315](../../../services/multisim/account/topology_service.cpp)。

## 4. G1～G5 独立判读

|Gate|设计层判断|支持证据与限制|
|---|---|---|
|G1 同票与新票|本地契约成立|原 W 与推进 m 共一个发布点，同票只重取原结果。新票只确认成员时 F 不动。明确再接纳一个同值字段是另一个有写依据的动作，应单列，见 §6。模型的单个布尔 W 只能验证同输入重复。|
|G2 视图独立|成员计算成立|同一时钟签发不同键的刻度；删除比较本键 m。两个视图都缺席时 AccountRecord 可清理，已接纳的 Binding 仍有独立寿命。§7 分别验证。|
|G3 缺席删除|原反例关闭；新票对滞后快照的删除仍阻塞|m>w 必挡旧票。m=w 仅证明自本地签票以来没有本地成员事件。P25-R1 的两种执行在原读口不可区分，见 §8。|
|G4 有行字段合并|本地并发与 ABA 判定成立；读源缺口也影响字段覆盖|旧签票能保名称、合并号码。新签票携旧内容时，F 相等反而允许覆盖，见 §9。|
|G5 统一发布与 Binding 幂等|限定于同一 T 寿命及提交域，方案可实施|无失败尾段、先备后交换、带 t 的 W、精确 A 释放构成设计依据。当前类型探针只验证部分载荷，未执行全路径故障注入。见 §10。|

## 5. 原始 B24-01 的重新验证

初态为同来源、订阅 41、槽 0、i=1、r=1。下表的 m、j、W 属于提案，实际源码本轮也独立执行了对应的读取和删除。探针以已采好的空集合代替数据库 I/O。

|时刻|实际顺序|T / A、读取票与预期结果|
|---|---|---|
|T0|装配槽，首次导入 Y 并建绑|C/H 均在，mC=1、mH=2、jC=1、F=0|
|T1–T2|签 L.C 并采到缺 Y 的旧结果，暂缓返回|w=2，expected 有 Y，i/F 冻结|
|T3–T4|新 t2 获本次原 Store 确证及原 A 资格|原写已确认，尚欠 T 提交；无须重发 Store|
|T5–T6|T 在真实写点确认 t2 只覆盖 C 并发布|mC=3、jC=1、mH=2，W2 对应原关系 r=1；A 仅剩原处置的结清|
|T7–T8|L 的空读返回，实际尝试删除|原源码对 expected/i/F 成立即删除；提案因 3>2 记本行冲突。W2 不回退，H 与 r 不动|

**本轮实测**为 `source C=0 H=1 r=1`、`proposal C=1 H=1 r=1`。源码的 AccountReloadTicket 已有 membershipWatermark，AcceptReload 的有行创建分支部分使用它，缺席分支只检查 i/F。提案在真正删除点增加的逐键检查是修正成立的必要条件。探针为该行撤销 expected 中的删除资格来模拟这一检查。[Registry 声明 :58–65、89–97](../../../services/multisim/account/account_registry.h)；[Registry 实现 :262–335](../../../services/multisim/account/account_registry.cpp)。

## 6. 原票、重放与版本矩阵

|输入，初始 r=1|mC/mH；F；r|W 与外部责任|
|---|---|---|
|新 t2 有原写确证，仅确认原 C 成员|1/2→3/2；全部 F 不变；1|产生一次 W2。重载的旧缺席结果失去删除资格|
|重复原成功回调或 Reconcile(t2)，包括两个竞争认领|3/2；F 不变；1|同 t/call/计划/身份/处置才读 W2。T 域内一个 claim 可发布；另一执行者读取已发布处置。Store 写、逻辑绑定、实际释放均至多一次|
|W2 已发布、A 已释放但答复迟到|3/2；F 不变；1|允许重取相同释放回执。再次调用查询或交付确认与再次执行 A 释放是两件事。T 在确认及在途交还前保留 t|
|同 t 携另一身份或计划；t 已回收后迟到|所有业务版本原样|前者拒绝并保留争议；后者由 epoch/活票入口丢弃陈旧投递。不能按 slot 或 s 新建 t|
|后来独立的 t3 冻结在 mC=3，再次有效确认 C|3/2→4/2；成员专属输入不动 F；1|W3 与 W2 各属原票。反之，一个仍持 mC=1 原资格的并发旧票不能借新 m 自动续签|
|t2 有独立依据再次发布 name，即使值相同|mC 依其成员覆盖；只升 F.name；1|Part 25 §§3、6–7 要求把被明确重新声明的字段列在 W。G1 的值不变且 F 不变适用于仅确认成员的输入；实现不能从整行 Store 成功猜出六个字段均有新写资格|

A 现有 Finalize 成功即 erase token，不能重读成功回执。生效一次与可恢复重取由 Part 23 §23.3 的协议提供，E3 的 witness 布尔值没有模拟这张跨 Owner 对照表。[AccountRoleProtocol::AcceptDiscovered / Finalize :50–65](../../../services/multisim/role_assignment/account_role_protocol.h)。

## 7. Current / Historical 交错矩阵

下表先假定读源有资格替换相应视图，以隔离 T 本地逻辑。C/H 的读源分别要求有效位置记录和 all-data 成员依据。单一原写回执只有分别证明两个谓词时才能覆盖两者。首次导入源码常同时生成两份投影，不足以授予双视图资格。[Part 25 §§4–6](../../AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)；[首次导入 :75–92、清理 :316–335](../../../services/multisim/account/account_registry.cpp)。

|签票时 C/H|原票确认|初始旧读分别缺 Y 的交付结果|
|---|---|---|
|有/有|仅 C|C 保留，H 可删。H 的旧 m 未越过其 w|
|有/有|仅 H|H 保留，C 可删。独立 E3 实测旧 C/H 交付后为 0/1，AccountRecord 可读，r=1|
|有/有|C、H|mC、mH 各取一个新刻度，两旧读都不能删。任一必要资格或两刻度余量不足则整组不发布|
|有/无|仅 C；另一个对照为 C+H|仅 C 时 H 仍缺席；有独立 H 证据才可创建 H 并设 jH=mH。签票时 H 缺席的旧读没有删除新成员的资格|
|无/有|仅 H；另一个对照为 C+H|仅 H 不生成 C 或赋予 Primary 资格。具备原来源、C 读源及槽资格时才可补 C 并记录新 jC|
|有/有，C 的读很早|期间仅 H 重新确认|即使全局 clock 已升，C 自己的 m 依然 ≤w；其他条件满足仍可删 C。相反顺序对 H 同理|

身份存在、该 view 的可读投影和真实 Binding 的存在分别查询。仅删一个 view 时另一投影使 AccountRecord 继续存在。当最后一个合法视图也被删，当前实现移除 AccountRecord；独立探针仍读到 r=1 的 Binding。ReadPrimaryTarget 还要求该订阅有 Current。破坏性读一旦误获资格，即使绑定完全幂等也会使 Primary 读面丢失。[TopologyService::ReadPrimaryTarget :46–56](../../../services/multisim/account/topology_service.cpp)。

## 8. 旧票、新票与外部快照的删除资格

|读取输入|本地版本检查|应有处置 / 实际判断|
|---|---|---|
|先签且采空，后发布 C|mC=3>w=2|保留 C；原 B24-01 已验证|
|后签；可证明本次完整取样已看见原写，之后真实移除 Y|mC=3=w=3、i/F 同，缺行|允许删除。新确认不会永久屏蔽合法读取|
|后签；正确逻辑读源按次新建游标，但其可见数据仍来自原写前|所有本地条件同上一行|应保留 C；提案可执行判定却允许删除。此为 P25-R1|
|同视图非最新/已消费、错误视图、返回失败、行重复或身份冲突|attempt/完整性等门未满足|拒绝相应替换，不能从失败导出缺席。另一 view 独立|
|签票时无 Y，或中间删除重建、元数据未知|缺 expected，或新 m>w，或缺可信元数据|保留当前投影或墓碑；不能借旧读创建破坏性事实|
|签票后只改另一键、另一视图或本票重放|本键 m 未越过 w|仍需读源、i/F 等其余门。单看全局时钟会误拒合法删除|

**反例 P25-R1 的两种外部历史。** 下表的 10、11、12 是独立探针的真值标记，仅供评审者辨别历史，未作为 AccountLoadResult 的字段给 T。两边均假定 P0-1 已正确归因、T/A 正常、来源未变。后续数据库移出 Current 指该视图的存储条件改变，尚无另一 T 事件修改本地 m。

|时刻|A · 新空读可删|B · 新查询读到滞后数据，应保留|T 实际可见|
|---|---|---|---|
|0|图像 10 缺 Y|图像 10 缺 Y|T 已缓存 C/H，i=1、r=1|
|1|原写 11 确认 Y，t2 发布|相同|mC=3，W2 已发布，F 无变化|
|2|后续数据库图像 12 移出 Y|数据库仍为 11，Y 仍在|本地无后续成员事件|
|3|签 L'，读端取样时可见 12|签 L'，读端取样时只可见 10|同一逻辑读源、正确 request、attempt=1、w=3、expected[Y] 相同|
|4|完整结果为空|完整结果也为空，读取调用发生在签票之后|success、complete=true、rows/roles 为空；mC≤w、i/F 均相同|
|5|删除符合图像 12|删除覆盖了图像 11 确认的 Y|两侧 E3 都得到 C=0、H=1、r=1，W2 仍在；只有 B 错|

Part 25 §5 明确要求签票先于真实取样，§7 要求结果“完整、可替换”。本反例允许读端**真的在签票后取样**，取样所在读视图还未包含写 11；完整性只说明该图像的集合完整。如果把“可替换”解释为包含看见写 11 的保证，该解释能排除 B，但 §5、§7 未给出谁保证、T/I 如何区分两种输入、跨重连时如何失效及缺证时如何处理。S7 探针直接给 replaceable=true。以现有成功查询作证的可执行映射在 B 中确定给出错误结果。产品实际采用缓存、滞后副本或强一致读的情况属于 EXTERNAL_EVIDENCE；设计可以在能力未知时安全拒绝破坏性接纳，不必等待产品先实现新接口。

**另一条完整性边界。** SimRdbHelper 的两个 QueryAll 方法在结果集非空指针时循环取行，首行或后续行返回非零都退出并返回 TELEPHONY_SUCCESS。桥用其结果设置 complete。Host 游标模型对关闭状态和正常越过末行均可返回 E_ERROR，已有测试还专门区分合法空游标与查询失败。这能证明现有桥没有把每种遍历结束原因交给 T；若产品遍历错误也走该出口，只读到的前缀会被标作完整。这个条件性实现风险须与“完整但过旧”分别核验，本轮没有注入产品游标故障。[SimRdbHelper :465–489、492–523](../../../services/sim/src/sim_rdb_helper.cpp)；[Host 游标 :166–175](../../../services/multisim/test/framework/internal/host/stubs/include/linux_platform_types.h)；[既有空游标测试 :7–21](../../../services/multisim/test/cases/internal/platform/external_services_test.cpp)。

## 9. 有行时的字段版本与成员版本

|输入|成员资格|字段资格与结果|
|---|---|---|
|L 在先，t2 确认 C 并接纳新 name，L 返回旧 name 和未冲突 number|同一 i、同一连续投影，j=1≤旧 w=2。m=3>w 只挡删除|name 原 F=0、现 F=1，拒绝旧名。number 原/现 F=0，可接纳。实测六字段中 5 接纳、1 冲突，name 保留、number 为 42|
|t2 明确承认同值 name，随后交旧 L|j 同上|实际 AcceptDisplayName 即使同值也推进 nameRevision；旧名仍被拒。只有成员证据时不得调用该字段接纳。两个独立试次均有断言|
|L 希望更新旧 Y，其后该投影被移除再建|即使同一 s/i，j新>w旧，拒绝旧投影；移除时保留退役版本|有行分支不得用偶然相同的字段版创建或覆盖新存续期间。原探针的重建对照同时触发旧 attempt 已消费，未单独实测“仅 j 不符”的生产分支|
|t2 已发布新名后签 L'，返回来源图像里的旧名|j≤新 w，m≤w，无删除重建|L' 冻结的 F.name=1 等于现版；实测旧值覆盖新名，F.name 升到 2、0 个冲突。这是 P25-R1 在有行时的表现|

成员水位保护缺席删除，加入刻度保护存续身份，字段版本约束同一存续期间的本地写入顺序。三者都不能给外部取到的值签发新鲜性证明。重过 Gate 时既要挡住最后一行，也要保留第一行合法 number 的合并能力。[AccountRegistry::AcceptDisplayName :133–149、AcceptReloadProjection :190–260](../../../services/multisim/account/account_registry.cpp)；[Part 25 §§6–7、10](../../AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)。

## 10. 统一发布与 BindingRevision

|审查点|可实施契约 / 反向检查|
|---|---|
|槽与账户准备|先验原 t/claim、有效来源、装配、槽预期 r 与跨槽唯一性；再备 AccountRecord、字符串、投影、各自 m/j/F、槽来源、W、后续交付描述。私有新增 map 节点在同域记录迭代器，准备失败逆序撤销。多行 reload 的撤销容量须覆盖整批。|
|尾段与中断|已存在的节点上做无抛交换、标量赋值，最后发布“W 已成、待 Finalize”的票进度。持锁暂停时读者等待；执行恢复后读整组。析构、分配器、通知载荷、返回值均要避免在此引入失败。探针编译了 AccountRecord、optional<BindingRef> 的 nothrow-swappable 断言。W 的实际类型和目标工具链仍待实现取证。进程崩溃后的持久恢复超出该进程内承诺。|
|所有读写面|FindBinding、ReadAccount、ReadPrimaryTarget、AcceptAccountReload 的主要现有读写使用 commitLock_。组合快照需一次入域。当前 AcceptSlotAssembly 直接操作 BindingIndex，AcceptStoredDiscovery 又有独立播种入口。Part 24 §5 和 Part 25 §§8、13 已要求约束这两条路径，且准备期禁止递归读、外调和引用泄漏。|
|幂等与跨槽|源码 AcceptBinding 要求已装配且期望 r 相等。同槽同订阅已有关系直接成功；空槽签 r+1，拒绝占用其他槽的订阅，RemoveBinding 递增槽水位后清关系。源码无法仅凭该方法辨别同源重放与换源；设计另要求 T 在同域验来源。成员刻度位于 (view,s)，不写任何其他槽 r。|
|部分失败与关闭|准备失败时正式组与 W 均旧，T 持 t/待本地接纳，A 持原 token。W 已成直到释放和执行交还仍以 t 可查。缺失释放回执只重取原确认。只要所有 M19 读取同域就无“先见完成、后补版本”的窗口。|

本轮没有发现 C++17 对这类有界准备、无抛发布的根本限制，也未观察到部分发布。真实现目前的 AcceptDiscoveredAccount 仍先插 Account 再用可分配的 map 写成员版，不能直接作为尾段。W、保护读面和无抛约束尚需落实，属于 IMPLEMENTATION_GAP。[Registry :75–92](../../../services/multisim/account/account_registry.cpp)；[BindingIndex :30–46、59–94](../../../services/multisim/account/binding_index.cpp)；[TopologyService :31–32、46–56、253–265、317–340](../../../services/multisim/account/topology_service.cpp)。

## 11. 源码、模型与 Host 证据分级

C++ 实验使用 GCC 13.3.0、C++17、断言启用，实际链接 account_registry.cpp、binding_index.cpp、slot_catalog.cpp。附录可从本报告和固定的 Part 25 提取全部输入重新执行。

|等级 / 实验|实际输入与运行代码|实测输出或推导，以及边界|
|---|---|---|
|E1 · 源码取证与设计推导|核读 Part 23–25、两份历史报告、Target T/A、Host 桥和 Legacy 查询|§3、§6、§10 的所有者、析构及失败点为静态推导。此项不代表任何可执行测试成功。|
|E2 · 独立视图模型|附录 Python；4 种初始投影集合 × 3 种确认范围。枚举签 C/H、一次提交、一次同票交付、两次旧/新空读，共六个事件。要求重放晚于提交，每个删除晚于本视图签票及本地提交。读源一律合格|792 个合法用例，936 次删除、384 次保留在场成员、其余键本就缺席，0 违反预期。断言只发布一次。未枚举不同票竞争、锁、I/O、Stop 或签票前已取样的外部图像。|
|E3 · 原附录，B24-01 和新签正控|原程序不改，种实际 s=41/slot0。缺席旧读、再次签票、同票重复、C/C+H 两视图、缺证/失效、容量不足、删除重建与两组逐字段冲突|输出 `source C=0 H=1 r=1`、`proposal C=1 H=1 r=1` 及全部附录断言 OK。原票/W/时钟/加入刻度由 fixture 模拟；只对受保护缺行删去票副本中的 expected，再让真实 Registry 执行。Store、A、M19 和无抛联合尾段均未参与。|
|E3 · 独立空读对照|附录 C++ 用相同 Fixture 与实际 AccountLoadResult 形状，给两次新查询完全相同的空行和成功标志。源图像独立注入，检查真实 Registry、BindingIndex 的结果|实测 A 为 `db=12 read=12 attempt=1 w=3 C=0 H=1 r=1 expectedC=0`；B 为 `db=11 read=10 attempt=1 w=3 C=0 H=1 r=1 expectedC=1`。模型授予原写和 W；未运行 DataShare 或 TopologyService。db/read 仅为外部真值，不交给 T。|
|E3 · 独立有行对照|在同一存续期间以实际 AcceptDisplayName 写新 name，再签新票，交含旧 name 的完整行，实际 AcceptReload 合并|实测 `nameRev=2 conflicts=0 keptNew=0 r=1`。基线字段的真实比较得到错误覆盖。外部取值与新鲜性为受控输入。|
|E3 · H-only 与账户寿命|仅确认 H，交两个旧空读，再签一个 H 的新空读。值的存储与删除均由真实核心类执行|旧读之后 C/H=0/1；最后 H 也合法缺席时 AccountRecord 不存在、r=1。此处合格替换作为已给条件。|
|E4 · Target Host|本轮 NOT_RUN。源码已有 T2 分视图、失败保留、旧字段、乱序尝试、读后新增账户及 Binding ABA 场景|这些测试使用受控读结果。历史报告反映各自输入，不能证明本轮的 W 发布或新票排除滞后数据。[集成测试 :461–551、602–634](../../../services/multisim/test/cases/target/primary/c0_topology_integration_test.cpp)。|
|E5 · 并发压力、ASan、TSan|本轮均 NOT_RUN|未获得运行时的内存安全、竞争或交还时序证据。Gate 的否决依据来自 §8–9 的区分试验。|

## 12. P0-1 / P0-3 最小一致性检查

|交错|结论与收据|
|---|---|
|Store Unknown 后只找到同一行|T 持 t/call 的效果核对项，A 持原资格。Part 25 禁止从该读构造当前 W 或推进 m。附录仅用 proven=false 检查了拒绝分支，未证明产品归因能力。|
|Store 已归因、来源撤销已登记，或 Stop 时本地发布还未完成|按同一 T 提交序排定。可信终止可仅结原历史效果，暂时无法证明连续性则留 t；若当前资格仍足且只是未发布，M19 列 t/待成员接纳。|
|W 已成，A 的释放或返回在途|T 仍留 W 和待确认义务，A 按实际占用或在途回执出据。H 须取同轮封口与稳定收据，不可因 Registry 已有值就 Drained。|
|新读不能证明可替换，恰逢 Stop|该读取的拒绝/待证/合法移交必须有明确处置，已领取执行仍须交还。不得重新打开已结清的 t2 或重写 Store 来修补误删；也不能用不可信的空行集驱动 A 全视图替换。|

P25-R1 发生时可让 W2 和 A 释放都正确完成，错误仍成立，故本轮退回范围限于 Reload 的读源可见性和替换资格。原写归因与 H 稳定收据的现有门禁继续适用。[Part 24 §§4、6](../../AF1_Part24_B1_P0_Design_Revision_20261123.md)；[TopologyService::AcceptAccountReload :305–314](../../../services/multisim/account/topology_service.cpp)。

## 13. 留给实施验收的未执行场景

|阶段与责任方|最小验证及可见断言|
|---|---|
|E4，T/I/Provider|从正常发现入口写 Y，实际控制原回执、读端可见进度和返回时刻。对同一新票跑旧缓存、滞后读源、已追上且后来移出三例。消费 attempt 与“可删 Y / 可更新字段 / 可将角色行交 A”分别观测。|
|E4，T/A/H|阻塞准备分配和 Finalize 答复，校验两视图、m/j/F、Binding、W、t/token 及两遍收据。对同票异载荷、两票同基线竞争、回收后迟到计数实际写入和实际释放。|
|E4/E5，T 执行域|多行 Y/Z、双视图与提交交错；区分旧 attempt 守卫与仅 j 失配的拒绝。对每次准备分配注入失败，尾段逐点暂停并并发读组合快照。观察全旧或全新，运行 ASan、TSan、重复 Stop。|
|产品外部证据，I/产品读源负责人|说明哪些路由、库/用户、实例或读事务具备所需可见性，跨重连/切换何时作废。核实权威空读、游标截断与读取错误各自如何报告。|

## 14. 最终 Gate 结论

**`B1_REVIEW_BLOCKED`。**

1. **是否消除了 B24-01？是，限设计层的原交错。** 旧签票的 w 小于新成员 m，缺席删除被阻断，真实类配合模型得到了 1/1/1。当前未修改的源码仍得到 0/1/1。
2. **是否引入新的版本缺陷？** m/j 的逐视图区分、旧票逐字段合并及相同 Binding 的 r 幂等未发现新的本地计数错误。尚有一个关键的版本适用边界：**新票可给旧源数据带上最新本地资格**，既可误删成员，也可把旧名称当新事实升版。两种错误同属 P25-R1。
3. **是否可结束内部收敛、提交负责人？否。** Part 25 中的本地发布机制可以保留。先补齐新读替换已确认事实的证据和缺证处置，再提交独立 Gate。裁决针对提案，未对产品 Provider 的实际一致性作肯定或否定判定。

## 15. 必须修改的问题及重新过 Gate 条件

|具体位置、责任方|要求|正反验收|
|---|---|---|
|Part 25 §§5、7 中 valid(L) 及可替换读源；T/I|给出可实施的可见性依据。可采用已证实的指定路由“原写已可见、此调用取得同域最新一致读”契约，或由读源证明其图像覆盖需要保护的写。写明证明作用的库/用户、view、顺序和失效范围；本地 w、时间戳、attempt、成功标志本身均不能作证。无需预设 Provider 已有事务版本。|重放 §8 A/B。A 有可信新缺席证据可删；B 或能力未知时 C/H、r 和 W 原样。证据缺失的路由可继续作为外部 OPEN，只要内部默认结果确实安全。|
|§7 有行分支、§10 S7 与字段控制；T|同一读源资格也约束会替换已知较新值的有行数据。保留已有字段版/j 判定、各 view 独立资格。明确不具资格时能否消费本次尝试、哪项待重新取证、如何避免传递失真的整组角色行。|新签票取旧名时 name/版均不回退或误升；合法来源上“旧名冲突、号码无冲突”仍仅合并号码。新的合格空读仍可删，禁止永久冻结。|
|§§10、13–14，I/T 验证输入|把查询成功、集合完整、所见写入进度分开观测。正控加入真实后来移出；负控加入完整旧图像、游标中断只返前缀、来源重连使既有保证失效。举证可依赖一条已有可信路由，不要求新建全局服务。|相同本地签票可得到“可替换 / 缺证保留 / 读取失败”可区分处置，逐项报告证据来源。Stop/重试不产生额外 Store 写或第二次 A 释放。|

## 16. 权威文档的同步条件

本轮退回。本节仅定位下一次独立 Gate 通过、负责人作出决定后应同步的条款。

|文档|应同步的契约|
|---|---|
|[A 目标设计 §6.5.2、§9.2、§10.1](../../SIM_Target_Architecture_Design_v1.2.md)|区分 T 签票顺序、成员确认、字段接纳、外部读源可见性及 Binding 水位。|
|[B 源码详细设计 §§4.2、6、11](../../SIM_Multisim_Source_Architecture_Detailed_Design.md)|列出读请求/结果可用的证明与失效条件，映射到 T/I 判定和 M19 原票收据；同时保留一次无抛发布尾段。|
|[C 目录设计 §4.1 Account](../../SIM重构代码目录结构设计方案.md)|定位 Registry 的 m/j/F、BindingIndex 的 r、T 中的原票 W 与按读源接纳的责任；保持既有目录边界。|
|[D 迁移设计 §7.4、AF-1 补充验收](../../migration/SIM_Architecture_Refactor_Design.md#af1-acceptance)|补强合法新缺席与滞后完整空集两条负载相同、预期不同的差分，以及部分遍历和 Stop 的验收。|

### 附录 · 独立实验的复现输入

从仓库根目录执行。源程序和两份模型均可从文档本身恢复。以下仅生成临时文件。

```sh
proposal=telephony_core_service/docs/AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md
review=telephony_core_service/docs/history/reviews/AF1_Part25_Independent_Gate_Review_20261124.md
work=$(mktemp -d)
src=telephony_core_service/services/multisim
extract() { awk -v tag="$1" '$0==tag {p=1;next} p && $0=="```" {exit} p' "$2"; }
extract '```cpp' "$proposal" > "$work/part25.cpp"
sed '/^int main()/,$d' "$work/part25.cpp" > "$work/fixture.h"
extract '```cpp' "$review" > "$work/independent.cpp"
extract '```python' "$review" > "$work/views.py"
for name in part25 independent; do
  g++ -std=c++17 -Wall -Wextra -pedantic -I"$src" "$work/$name.cpp" \
    "$src/account/account_registry.cpp" "$src/account/binding_index.cpp" \
    "$src/infrastructure/assembly/slot_catalog.cpp" -o "$work/$name"
  "$work/$name"
done
python3 -B "$work/views.py"
```

独立 C++ 输入（SHA-256 为 `2c223db3c3a7acc64635be8bbdc9bcf1434dac023a128b3d1933a659e4ef38f5`）。

```cpp
#include "fixture.h"
#include "account/account_store.h"
#include <string_view>

struct Image {
    unsigned revision;
    bool current;
    std::u16string name;
};

void emptyRead(bool readerCaughtUp) {
    Fixture f;
    const Image before{10, false, u""};
    const Image committed{11, true, u"confirmed"};
    const Image laterRemoval{12,false,u"confirmed"};
    assert(f.reaffirm(true,false));
    const auto fresh = f.begin(C);
    const Image &database = readerCaughtUp ? laterRemoval : committed;
    const Image &reader = readerCaughtUp ? database : before;
    assert(!reader.current);
    AccountLoadResult response{{C, fresh.actual.attempt}, 0, true, true, {}, {}};
    assert(response.request.view==fresh.actual.view && response.rows.empty() &&
           response.storedRoles.empty() && response.complete);
    assert(f.m[0]==fresh.w && fresh.w==3 && fresh.actual.expected.count(Y)==1);
    f.empty(fresh,true);
    std::cout << "empty db=" << database.revision << " read=" << reader.revision
              << " attempt=" << response.request.loadAttempt << " w=" << fresh.w
              << " C=" << f.exists(C) << " H=" << f.exists(H) << " r=" << f.r()
              << " expectedC=" << database.current << '\n';
    assert(!f.exists(C) && f.exists(H) && f.r()==1 && f.witness);
}

void staleField() {
    Fixture f;
    assert(f.reaffirm(true,false));
    assert(f.accounts.AcceptDisplayName(Y,C,0,u"confirmed")==AccountAcceptance::Accepted);
    const auto fresh=f.begin(C);
    auto cached=f.p;
    cached.displayName=u"before";
    assert(f.joined[0]<=fresh.w && fresh.actual.expected.at(Y).fields.name==1);
    auto result=f.accounts.AcceptReload(fresh.actual,true,{{Y,"Y",1,cached,{}}});
    auto a=f.accounts.ReadProjection(Y,C);
    assert(result.acceptedAttempt && a->displayName==u"before");
    std::cout << "late-source field nameRev=" << a->revisions.name << " conflicts="
              << result.conflictedFields << " keptNew=" << (a->displayName==u"confirmed")
              << " r=" << f.r() << '\n';
}

void independentViews() {
    Fixture f;
    auto c=f.begin(C), h=f.begin(H);
    assert(f.reaffirm(false,true));
    assert(f.m[0]==1 && f.m[1]==3);
    f.empty(c,true);
    f.empty(h,true);
    assert(!f.exists(C) && f.exists(H) && f.accounts.Read(Y) && f.r()==1);
    auto newH=f.begin(H);
    f.empty(newH,true);
    assert(!f.accounts.Read(Y) && f.r()==1);
    std::cout << "H-only: after old C/H=0/1; last legitimate delete account=0 r=1\n";
}

int main() {
    emptyRead(true);
    emptyRead(false);
    staleField();
    independentViews();
}
```

独立 E2 模型（SHA-256 为 `f205426850381d2b706416724ec4ff4dafacd60eb2b9390a32cf412bb9640bfe`）。

```python
from itertools import permutations
runs = removals = retained = 0
for initial in range(4):
    for mask in (1, 2, 3):
        for order in permutations(('Bc', 'Bh', 'W', 'dup', 'Dc', 'Dh')):
            at = order.index
            if at('dup') < at('W') or any(at('D'+v) < max(at('B'+v),at('W')) for v in 'ch'):
                continue
            runs += 1
            clock = 0
            live = [bool(initial & bit) for bit in (1,2)]
            m = [0,0]
            for k in range(2):
                if live[k]:
                    clock += 1
                    m[k] = clock
            reads = {}
            witness = None
            publications = 0
            for event in order:
                if event in ('W','dup'):
                    if witness is None:
                        for k,bit in enumerate((1,2)):
                            if mask & bit:
                                clock += 1
                                m[k], live[k] = clock, True
                        witness = tuple(m)
                        publications += 1
                    continue
                k = 'ch'.index(event[1])
                if event[0]=='B':
                    reads[k] = live[k], clock
                    continue
                present, w = reads[k]
                expectKeep = live[k] and bool(mask & (1<<k)) and at('B'+event[1])<at('W')
                delete = present and live[k] and 0<m[k]<=w
                if delete:
                    clock += 1
                    m[k],live[k] = clock,False
                    removals += 1
                assert live[k]==expectKeep
                retained += int(live[k])
            assert publications==1
print(f'cases={runs} deletes={removals} retained={retained} violations=0')
```
