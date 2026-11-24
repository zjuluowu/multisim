# AF-1 Part 25 · 成员资格重新确认与旧 Reload 的删除资格

**设计提案状态为 `P0_2_MEMBERSHIP_REVISION_PROPOSED`。** T 在同一发布点为新原票实际确认的每个读取视图签发一次新的成员版本。缺席行的删除要比较该成员版本与原 Reload 的签票水位。有行时则区分成员的存续期间和各个字段的写资格。同一有效 Binding 可以贯穿多次成员确认。

## 1. 本轮基线与证据范围

|输入|核验结果|
|---|---|
|Git|本地 main、origin/main 跟踪引用及远端 main 的 HEAD 均为 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`。目标源码最后变更于 `632f6a2a888cfbbc3c9387edc89732364e12a10e`，两提交的 services/multisim 树均为 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。复核以当前工作区源码为准。|
|设计输入|[检查点 Part 23 §§23.1–23.5](AF1_Architecture_Design_Convergence_20261123.md)，SHA-256 `0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2`。Part 24 在本地[独立提案 §§4–6、10–12、16](AF1_Part24_B1_P0_Design_Revision_20261123.md)，SHA-256 `a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164`。|
|独立审查|[Part 24 Gate §§3、10–11](history/reviews/AF1_Part24_Independent_Gate_Review_20261123.md)，SHA-256 `f55cb963c6118dbeeafda037900c260746d570f0c2e7ad44687714f0c78ec313`。B24-01 的 C++17 二进制仍可运行，本轮另用相同源码树重建对照。|
|工作区|开始时已跟踪文件与暂存区无差异。既有未跟踪项为 default.profraw、opencode.json、Part 24 和两份 B1/Part 24 独立报告。本文为唯一新增仓内交付。|
|证据口径|SOURCE 为 testonly 目标源码行为。PROBE 为仓外程序链接指定真实实现的实际结果。MODEL 为同一程序中显式增加的提案判定。DESIGN 为待实施契约。业务 Host、sanitizer、产品构建、Provider 实机均为 NOT_RUN。|

## 2. B24-01 复现与源码根因

以下行号相对 `telephony_core_service/services/multisim/`。Y 为订阅 41，槽 0，来源持续有效。旧查询可读空的具体前提是采样时数据库行的 SLOT_INDEX 为 -1，随后新的写才将它设回 0。Current 经 QueryAllValidData 按有效位置筛选，Historical 用另一读源。这里赋予新票充分原调用证据，以隔离 P0-2。探针的输入是已采好的空集合，未执行数据库写。[Provider 桥 :41–104](../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)；[Legacy 查询 :492–511](../services/sim/src/sim_rdb_helper.cpp)。

|时刻|动作与源码对应|旧规则可见状态|
|---|---|---|
|T0|AcceptDiscoveredAccount 导入一次，已装配槽首次 AcceptBinding 成功|C=有、H=有，i=1，mC=1、mH=2，w=2，r=1|
|T1|BeginReload(Current) 冻结 attempt、w 与 expected[Y] 中的身份及六个字段版本|expected 有 Y，F 为初值|
|T2|Current 完整取样缺 Y，延迟交付|签票、结果、读源均属本次读取|
|T3|同一来源产生新 admission t2，Store 实际写入成功|持久化效果与 T 内存是两步|
|T4|给 t2 原调用的可信完成及订阅证据|旧读取仍在途|
|T5|发现同一账户和同一来源绑定，位置/active 等 T 缓存已是目标值|DiscoverAccount 的已有账户分支跳过首次导入，无字段 setter，i/F 全同|
|T6|在 Part 24 §5 第 2 步允许的“只确认既存关系”分支发布见证|W2 已成，mC 仍可为 1。源码对照仅执行已有 Binding 确认，实际没有统一发布/W2 接口|
|T7|AcceptReload 对缺席 Y 比 expected、i、F 后 reset Current、记 retired 字段并推进删除版本|消费 attempt，mC 此时才推进，H 和 Binding 均未被写|
|T8|ReadProjection / Find 分别读回|原探针实测 `expected=1 w=2 accepted=1 current=0 historical=1 bindingRev=1`。ReadPrimaryTarget 还要求 current 存在，按源码此时返回 nullopt|

- [AccountRegistry 声明 :30–64、89–97](../services/multisim/account/account_registry.h) 和 [实现 :75–92、262–335](../services/multisim/account/account_registry.cpp)。首次导入才签成员版本，已有订阅直接 ChangedInputs。BeginReload 的 expected 只有 identityRevision 和字段向量。加载行的创建/补投影分支 :291–314 已比较部分成员水位，缺席分支 :316–333 的 unchanged **未读 membershipVersions_**。仅在发布端加计数仍会走到 reset。反过来，仅加水位比较而保留 t2 不升版，也仍会误删。
- [TopologyService :46–56、118–168、188–240、274–315](../services/multisim/account/topology_service.cpp)。直接成功先固定 A 片段，已有 Account 跳过导入，再 CompleteStored。现有 ReconcileDiscovery 按位置取项、查行、可能导入、调用 ReconcileStored、Finalize。AcceptAccountReload 在 T 域调用 Registry。Part 23/24 的原票、归因、统一发布均须作为拟议路径单独比较。
- [DiscoveryObservationLedger :61–91、124–164](../services/multisim/account/discovery/discovery_observation_ledger.cpp) 的 CompleteStored 可以委托 BindingIndex，而 ReconcileStored 仅更新原台账效果。[BindingIndex :59–94](../services/multisim/account/binding_index.cpp) 在原 r 匹配且同订阅已占本槽时直接成功，首次建绑和真实撤销才推进。缺席删除不读槽水位，增加 r 既挡不住当前删除分支，又会使合法 BindingRef 和在途请求误失效。
- 已有 [C0/T1/T2 集成测试 :175、348、461–551、602–634](../services/multisim/test/cases/target/primary/c0_topology_integration_test.cpp) 分别覆盖发现、部分核对、分视图、旧字段、后加入账户、解绑再入。T2ReloadDoesNotRemovePostAttemptNewAccount 中后加入者不在 expected。B24-01 的 Y 在 expected 且字段未变，这两项须各有断言。

## 3. 原票、成员、字段与 Binding 的版本语义

|资格与 Owner|表示的事实及作用域|何时变化|
|---|---|---|
|原票 t 与 W，T|t=(RuntimeEpoch,admission)，一笔已登记操作。来源、原 call、写计划、实际 s/ICCID、A 的 plan/token/解析回执与 t 关联。W 是该票当时的 T 处置见证|新准入签新 t，异步认领有自己的 claim。重复交付只读原处置|
|账户身份 i，T|s、ICCID 与身份代次限定目标账户，两个视图可分别存在|同一已验证身份保持 i。身份不一致拒绝，不能把 reload 行中构造的常量 1 当成身份续接证明|
|成员版本 m[v,s]，T/AccountRegistry|该视图、该成员最近一次被认可的成员事件。共用现有 membershipClock_ 签发严格递增刻度，比较使用自己的键；移除保留墓碑|新票在合格视图首次加入、恢复、重新确认或合法移除时各推进一次。纯字段合并不消耗成员刻度|
|本次加入刻度 j[v,s]，T|投影**这一段连续存在期间**的首个成员刻度，记录在同一成员条目。用于判断有行的旧读取可否沿用原字段资格|加入/恢复取当次 m，持续存在时不动，删除后标缺席。用它区分“同一投影重新确认”和“撤后重建”。m 本身不足以区分二者|
|字段版本 F[v,s]，T|name、number、active、label、imsPreference、position 各自有写资格。A 的角色版本另属 A|新权威字段事实被接纳时，令该字段版本推进一次，值相同但本票明确重新取得该字段写依据也须隔离更早冲突读。仅证明成员存在不构成六个字段的新确认|
|BindingRef 与 r，T/BindingIndex|s、slot、槽水位与经 T 认可的来源一起约束关系。r 按槽维持|同源、同槽、同订阅、原水位仍匹配时原样确认。首次建立、可信撤销或依既有规程换绑才升 r，投影增删本身无此权|

## 4. 新票与重放的判定

1. T 只在新发现准入或经明确准许的已知无效果重试处分配 admission。认领、直接返回、查证、重复通知都必须带回既存 t/call。epoch、admission 与 claim 不回绕。无活票的迟到完成只能返回陈旧。不能按 slot/ICCID 推断新操作。
2. 发出写前将原目标和当时视图资格固定于 t。s 已知时记各相关视图的存在性、m、加入刻度、i 和待写字段基线；当时未知 s 时至少冻结时钟 w0、观察及原写计划。返回实际 s 后，需证明身份映射、该视图从 w0 起无相冲突的创建/删除/确认。已有、身份原先未知且缺少原字段基线时不能用旧票覆盖其既有字段。缺元数据、并发同键变化或来源连续性待证时保留本票，不能把读取最新版本当作旧票的新写资格。
3. 续行先查原票阶段。有相同 t、计划、结果身份和处置的 W 即返回已提交的原结果，只重取原 A Finalize 确认。不同输入声称同 t 则拒绝并保留争议证据。尚无 W 才在真实发布点检查原始资格及本次 claim。相同键、不同 t 也必须分别通过检查；有序再次确认同一存续投影时，新的 t 才成为独立的成员事件。
4. 原票只提交一次固定的视图集合与逐视图字段集合。新资格必须来自能证明该视图成员条件的原证据。若计划要求 C 与 H 而其一待证，整组待证。若计划只证明 C 且只要求 C，W 的覆盖范围就是 C，后到另一读源不能扩写 W。可信终止仅记原操作历史效果，暂时不确定留在待证。两者都不能借旧成功刷新 C。
5. W 保存在 t 内，至少含 t/call、来源/顺序、实际 s/i、覆盖视图及各自前后 m/j、已接纳字段与版本、原 r 与结果 BindingRef、A 片段、处置和下一步。T 获同 t/token/处置的释放回执、原调用和所有已认领交付均已返回后才精确回收。A 保留可重读释放回执至双方交还。回收后迟到走安全寿命入口并按 epoch/活票表拒绝。保留成员水位和墓碑即可拒绝旧读，不要求永久留 W 或新增持久日志。

## 5. AccountRegistry / BindingIndex 接口契约修订（PROPOSED）

|调用面|拟议输入与判定|产生的效果及失败处理|
|---|---|---|
|DiscoverAccount / ReconcileDiscovery → T 同一原票续行|原 t/call/claim、原效果与实际身份、A 原预留、来源次序、读源覆盖、各视图和槽的原资格|只由 T 选当前接纳、待证或有据仅结历史。匹配数据库行不能触发发布。直接成功与核对后成功共用相同的发布尾段|
|AccountRegistry 内部准备一条账户差量|在 T 域用 find 取账户、(v,s) 成员/墓碑、retired 字段版；计算合格视图、字段和刻度需求|给调用者私有候选 AccountRecord、成员差量及所需节点。首次导入的 AcceptDiscoveredAccount 当前会改 map/clock，不可直接复用于正式尾段|
|BindingIndex 内部准备关系|原 r、装配、订阅跨槽唯一性、T 来源见证|返回具体三类结果“新建 r+1 / 确认原 r / 拒绝”。只确认时保持原 BindingRef。正式状态及 W 待联合发布|
|BeginReload / PrepareAccountReload|T 域内签发 epoch、view、最新 attempt、w 及 expected[s]={身份与字段资格}。签票先于该次真实取样|没有完整票据则不交付读取。Store 请求/结果与该票、读源和完整性对应。A 的视图/角色版本继续独立|
|AcceptAccountReload → AcceptReload / AcceptReloadProjection|校验原请求/结果及最新未消费 attempt。缺行用 §7 删除谓词，有行依存续期间与逐字段版本合并|整次消费资格与每个成员的结果分开。跳过冲突行/字段可消费本次尝试，不能把 acceptedAttempt=true 当成 Y 已删除。账号域对 A 只交原读源/版本|
|同一 T 域的读取、收据和释放|ReadAccount、FindBinding、ReadPrimaryTarget 及逐项关闭收据只观察已发布组。需要组合一致性时一次在域内取齐|已提交则见 W 并转待精确释放，未提交仍见 t/待成员接纳。H 不写业务版本|

## 6. 两个视图独立签发成员版本

沿用 `membershipVersions_[v][s]` 和 `membershipClock_` 的职责。拟把内层数值扩成一条小记录，含最近成员版本 m、加入刻度 j（可由投影存在性识别墓碑）。两个外层 view 在 Registry 发布前建好。j 是现有同一 T 事实的存续信息，可等价用“最近一次结构变更刻度”实现，不需要另一份 Registry。签票与发布在同一 T 提交域内排序。

|通过资格的事件|C、H 状态及刻度|字段 / Binding|
|---|---|---|
|只确认已有 C|取下一个刻度给 mC，jC 不变；H 完全保留|只处理有证据的 C 字段，既有关系 r 不变|
|同时确认 C/H|固定顺序 C 后 H，各取一个独有刻度；需要恢复哪个投影就为其设 j=m|两视图分别证明、各自检查字段基线|
|只确认 H 或 H 的独立合格 reload|只允许 H 的相应成员/投影效果|无当前来源资格不能建/恢复绑定；“只结历史 Store 效果”也不会自动确认 H|
|合法删除 v|只对 v 存 retired 字段版、发新 m、清投影和 j；另一个视图保留|成员被删除不自行解除物理绑定。两视图都空可去 Account 节点，墓碑和防 ABA 信息留存|
|新原票确证 v 已缺席又重新取得成员资格|用新 m 设 j，重新创建投影；所有字段版高于上次退役值|同一来源关系仍有效则 r 不动。曾经解绑再插必须用新绑定水位|
|普通完整 Reload 中已存在且有行|按原票做字段合并；即使全值相同也不隐式签发一次发现确认。同 view 的旧尝试已受最新 attempt 限制|如以后显式把读取作为新的成员确认，须证明独立读源、同成员资格、签票后无更新并受同一发布/重放规则约束|

视图覆盖由 T 从原计划与证据导出。以本反例为例，原更新的实际 s/ICCID、有效 slot 写入和仍有效的卡来源，能证明该账户满足 Current 读源的成员条件。若还要确认 Historical，需要证明同一身份确实满足独立 all-data 读源的成员条件；已有历史缓存或一次 Current 查询成功均不够。同一份逐项可归因回执只有在能分别证明两个谓词时才可覆盖二者。用于判断位置有效的输入，与是否接纳 legacyPosition 字段，是两个明确写集：如 T 同时确认该位置字段，即使值相同也在 W 中列出并推进其 positionRevision。

本文选择**全局签票水位 + 按键存最新刻度**。expected 已冻结签票时存在的成员、i/F。若该成员之后被确认/删除/恢复，则 m 必大于 w。若期间只有别的成员或另一个视图活动，本键原 m 仍 ≤w。比较全局时钟“现在值等于 w”会误伤无关更新。另一可行方案是把每个在场成员的 m 附在 expected 并要求 m等于签票值；非回绕且签票一致时，对这些成员与水位判法等价，失败时诊断信息更多，每行多一个刻度。仅冻结在场成员无法防御原先缺席者又建又删后被旧有行结果复活，仍需墓碑与缺席基线/水位。本方案保留 w、免加 expected.m，比 Part 24 的双条件少一个冗余快照。

## 7. 缺席删除谓词与有行合并

`valid(L)` 要求同一 T 寿命、view/签发 attempt/结果读源匹配，仍是该视图最新且未消费的票，读取成功、完整、可替换、行身份无二义，无重复或错误视图。失败、不完整、错误路由均无负向证据。旧票签发后才执行的读取也保守使用原 w，不重贴新水位。合法删除在 T 的发布域内按**每个 s**计算。

```text
mayDelete(L,s) =
    valid(L) AND absentInCompleteResult(s) AND s in L.expected
    AND projectionExists(L.view,s) AND trustedMetadata(L.view,s)
    AND currentIdentity(s) == L.expected[s].identity
    AND currentFields(L.view,s) == L.expected[s].fields
    AND 0 < m[L.view,s] <= L.membershipWatermark
    AND noUnresolvedPublishedPrefix(L.view,s)
```

已知 i/ICCID/s 映射由 T 固定，实物更换时另验身份与来源。无成员节点的旧数据不是版本 0 的可信当前成员。旧前缀按 Part 24 §5 第 5 步留项待权威恢复，期间禁止破坏性删除。无历史记录的真正新键才可视作从未出现。删除所需的下一刻度/墓碑必须可发布，否则返回未完成。删除先赢时，较早冻结在旧成员上的发现票也须重验其原基线；新的合格发现可以正常恢复。确认先赢时 m>w，旧空读消费并记成员冲突。同票重放不产生第三种顺序。新读重新签票并在确认之后取样，若缺席且其余门满足则可删。

若旧空读先发布删除，正好破坏了一张已发 Store 票的原成员前置条件，应在 t 上保留“效果已确认、成员待核对”。T 可请求该视图按新签票和正确读源重新取证。新读有行时由新读按自己的资格恢复投影；t 只有具备可验证的顺序证据，证明唯一冲突是原写之前已取样的那笔负读、身份和来源在此期间持续有效、没有介入的有效撤销/替换/其他新票确认，且这次读后无人再改相关键，才可在同域以这份独立新证据完成自己的首次确认并记 W。新读缺席仅证明该次读取，不能抹去原外部效果；若已证原来源终止则按历史处置，否则保留争议。单独取当前 m、过期的原行、或者旧票重放都无法走这条路径。

**有行需要另一组条件。**

- 签票时在场且现在在场，要求同源请求、身份匹配且同一投影存续（0<j≤w）。m>w、j≤w 表示该存续期间有新确认，只撤销旧读的删除权；其余合法字段仍可合并。对字段 k 仅当原 expected.F[k] 等于现版且有合法输入时接纳。新票明确承接的新字段写入/重新声明也要在同一发布点推进 F[k]，包括“原写恢复值恰与 T 缓存相同”。单纯观察到相同值或仅确认成员，不扩大写集。这样旧 name 可被拒，独立 number 可成功。
- 签票时在场、现在缺席或 j>w，拒绝用该行重建旧投影/更新重建者，即使 i/F 数字偶同。签票时不在场，只有现仍缺席、m≤w、身份映射合格、无待处置前缀才可新建；有墓碑时字段采用 retired+1 的下界。新建时 m=j=新刻度。
- 这项区分细化 Part 24 的“有行先验成员代次”。若对一切 m>w 的有行结果一律拒绝，会屏蔽未冲突字段；若只比较 F 而允许旧票在新的存续期间创建/写入，则有 ABA 风险。字段有新的写依据但自己的原字段资格已变化时，禁止把旧字段写计划改成“以最新版本重试”，未决必要步骤留 t。元数据不足以判定存续时保守留冲突。

所有签发器在准备期检查余量，C/H 两次签发须同时满足 clock≤MAX−2，单视图须≤MAX−1；字段版、attempt、admission、claim、需要递增的槽版同理。耗尽挂原责任或拒绝未签发的输入，不能在同 epoch 重置。墓碑至少保留到该寿命所有可能引用其旧资格的读取、发现及投递退出。最小方案在整个 T 寿命保留；新实例使用不同 epoch 和已隔离的入口，不移植旧票。

## 8. 与 Part 24 统一发布的关系

T 的本地提交单位为一张 t 的 Account 身份与合格投影、m/j、字段版、成员时钟、Binding/来源、W 和票的下一步。Store 交付、A 身份固定与 A 精确释放仍各有自己的所有者。提交按以下顺序实现。

1. 在 commitLock_ 内按活票/claim 查 W，复核原效果、A 固定身份、来源及未决撤销、各原键基线、所有版本余量与槽关系。禁止仅凭同 s、同 r 的相等关系绕过来源检查。锁外得到的候选输入回锁后重验。先计算受影响视图与字段，与当前无关的较新字段原样保留。
2. 先构造候选 AccountRecord、字符串/投影、成员节点内容、槽来源、W 和后续交付描述。预留账号、各成员及必要墓碑节点。两层 map 外层预建，使用 find/try_emplace 和稳定迭代器，针对单账号用预备好的定长撤销记录逐项记录新插节点。准备期间不能有递归读、外调或泄露引用。任一失败在同域逆序 erase(iterator)，原时钟、正式值与票的“待提交”进度可继续使用。对于任意行数的 reload 单独预备相应容量，不能套用单账号的固定上限。
3. 保持同一短域，以已验证无抛的交换/标量存储写 Account 候选、各 m/j、clock、Binding/来源和 W，再写原票“已提交、待 Finalize”进度。该最后标记为线性化点，业务读及 M19 都必须在域内取值。尾段无插入、查找后补节点、分配、格式化、用户回调、跨 Owner 调用或失败返回。实际类型、分配器及析构必须满足 no-throw；不能证明某个大对象时，准备期构造完成、尾段交换独占指针。禁止异步取消持锁发布线程后继续使用该图。进程崩溃的持久恢复不在进程内 W 承诺内。
4. 解锁后向 A 提交原 W，延迟/失败只恢复原释放。通知或执行交还按原票单独登记。Stop 在上述临界区前见“原票待成员接纳”，之后见“W、待 A 精确释放或在途交还”。不得在两者之间得到空收据。

## 9. 失败点与恢复矩阵

初态 B 表示发布前完整组，N 表示含本票 W 的发布后完整组。对既有 C/H 的示例，B=(i=1,mC=1,mH=2,r=1,W空)。C-only 的 N=(1,3,2,1,W2)。已有 A 身份确认和 Store 写在两个状态中都记在原票。

|失败或暂停位置|已准备/读者所见|原票下一步及重放边界|
|---|---|---|
|Store 未返、Unknown、只收到相同行|无可归因证明，B|T 持 t/call，A 持 token。只接收原证据，不能备成员版本或再写 Store|
|原证据已足，来源待排定或 A 解析未交还|效果及原资格已登记，B|按 t/claim 恢复相应步骤。可信终止才转历史处置；当前来源不明继续挂起|
|身份、基线、槽占用、耗尽检查不通过|B，未耗正式刻度|原始键仍有效的临时故障可续行；过期授权不能靠重新取当前 m/r 自我授权。只有进一步证据足以说明原操作与现状的顺序和处置时才可结清，否则 M19 列原项|
|复制账户/字段，预留账号、C 节点、H 节点或墓碑分别失败|私有候选和成功预留的节点可撤，业务读者只见 B|用撤销记录逆序移除本次新节点，再按原 t 准备。旧节点、时钟、retired 版本不变|
|两个视图只有一个剩余刻度、字段或恢复的 retired 字段版溢出|可算出一侧候选，仍是 B|整组拒绝。不只发 C，不消耗最后一个刻度，不发 A Finalize|
|W、槽来源或通知描述构造失败|候选关系尚未发布，B|清私有准备，保留 t、A 回执和证据；不制造通知或成功返回|
|正式写入 Account 后、C 后、H 后、Binding 后或 W 后暂停|持锁执行体独占；其他读者尚无法取到新快照；按顺序无抛完成后全部见 N|这些点只可暂停，不允许业务失败出口。若工具链/容器可在此分配或失败，应否决实现，不允许以补写另一半处理。竞争续行只能在解锁后读 W|
|发布后收到重复 Reconcile、A Finalize 返回 Unknown|N；历史 W 不随随后真实解绑而消失|按原 t/token/W 重取得 A 回执。不得重发 Store、再递增 m/F/r、重发布角色或通知|
|Reload 自身准备删除差量/墓碑或分配输出失败|没有部分删除、半消费或孤立时钟变化|原 attempt 的失败处置与资格在 T 域可追踪；未发布的同票可按原资格续行，被更新 attempt 淘汰则按陈旧终结。一次完成的 attempt 不可再次应用|

## 10. 场景推演与探针结果

表中各行独立。起点为有效 Y、i=1、C/H 都有、(mC,mH)=(1,2)、(jC,jH)=(1,2)、六字段版本均为 0、同来源 Binding r=1。L 为签在 w=2 的相应旧读，L' 为新读。t2 是 epoch e 内新的 admission 2，t1/tX、t3 为各自独立原票。† 表示墓碑而非在场成员。W 表示本地完成、待释放，其后满足 §4 的全条件才能结票。列中“有/无”是投影存在性。除注明外，各行使用成员专属证据，不另授予字段写权。

|场景与输入 ticketId|i；字段版本 C/H|mC / mH；投影 C/H|BindingRevision|原票完成位置|Reload 删除权 / 可观察结果|
|---|---|---|---|---|---|
|S1 t2(C)→签 L'→同 t2 再提交|1；0/0|3/2；有/有|1|原 W2；相同输入读 W|L' 签于 w=3，重放不使其失权；若结果为空仍可删|
|S2 新 t2(C)，所有字段值相同|1；0/0|3/2；有/有|1|W2 仅列 C|旧 L.C 失去对 Y 的删除权，其他键另判|
|S3 L.C 已取空→t2(C)→交 L.C|1；0/0|3/2；有/有|1|W2 可继续释放|mC=3>w2。attempt 可消费，保留 Y，ReadPrimaryTarget 的 Current 前置仍在|
|S4 t2(C) 后分别交两种旧 L.H|1；0/0|L.H 为空时 3/4†；有/无；若 t2 也确认 H 则 3/4；有/有|1|分别 W2(C) 或 W2(C,H)|只确认 C 时 H 原 m=2 可删；也确认 H 时旧 L.H 缺行不能删|
|S5 L.C、L.H→t2(C,H)→两读均空|1；0/0|3/4；有/有|1|同一 W2 载两个后版本|两个视图各自拒绝旧缺行，交付顺序互换亦同|
|S6 L.H→t2 仅有 C 证据|1；0/0|3/2；有/有（交付前）|1|W2 无 H 确认|L.H 仍有旧资格，若权威空读完成才删；H 不能因 t2 自增|
|S7 t2(C)→签且取空 L'.C→同票回放→交 L'|1；C 退役 0/H 0|4†/2；无/有|1|W2 保留当时处置或已按票结清|mC=3≤新 w3，i/F 未变，准删，ReadPrimaryTarget 应反映后来事实|
|S8 已有 Y 与关系，t2 再次确认|1；0/0|3/2；有/有|1，原 BindingRef|W2 记既存关系|旧 L.C 不可删；账号键数量仍为 1。同槽同订阅但不同来源不适用本行|
|S9 t2 在途→可信移除原卡→原票晚到|1；0/0|1/2；有/有（本例移除 Binding，未执行读源刷新）|2，槽空|原票仅结原效果，等待原 token 回执|t2 不改变 C/H 删除权；不能创建 Binding、恢复 Current 或给已有投影再确认|
|S10 tX 先失去资格，Y 已在场，t2 已获 A 解析→Stop→继续 t2(C)|1；0/0|3/2；有/有|1（Y 原 r）|tX 效果 Unknown；t2 的 W2 等 A 回执，两项按独立票登记|L.C 对 Y 不得删；H 两遍收据受两票和在途交还约束，不能提前 Drained|
|S11 L.C、旧发现 t1 冻结→合法删除→同 Y 合格新输入重建 C→旧输入重放|1；C 六字段均 ≥1/H 0|3†→4 / 2；无→有/有|1（本例仅视图撤建）|t1 原基线过期；新输入可形成自己的处置|旧票 w=2，jC=4。旧缺行不能删、有行不能复活/覆写；新身份资格不能给 t1。若发生实体解绑则另验 r|
|S12 t2 要求 C/H，clock=MAX−1（其他键已耗用）|1；0/0|1/2；有/有|1|无 W2，保留待处置|无孤立升版或回绕。旧读的比较条件未变，实际删除也须有其所需版本余量|

**有行的两个对照。** 先签 L.C，再由新票确认 C 并在 Current 接纳 displayName（一次改为新值，一次明确确认已缓存的相同值）。两次独立试验的旧行都携不同 name 及合法 number。提案应保留新 name 并接纳 number，name/number 各为 1，其余字段不变，H 字段版不变、r=1。若本票只确认成员而未获得 name 写资格，合法旧行仍应按原字段版竞争。这里的字段测试复用真实 AcceptDisplayName/AcceptReload 的判定，不声称两调用在当前源码中已是一次联合提交。

**实际执行。** GCC 13.3.0，C++17，断言启用。先复跑 Gate 探针，输出见 §2。再将附录程序链接当前的 account_registry.cpp、binding_index.cpp、slot_catalog.cpp。真实对照输出如下。

```text
source C=0 H=1 r=1
proposal C=1 H=1 r=1
fresh-delete, replay, views, proof/source, capacity, recreate and per-field controls OK
```

第二行是**执行过的提案模型**。账户存在性、字段合并、槽装配、关系确认、缺席删除和返回值均取真实实现。程序在仓外为一个目标维护拟议刻度和一张原票的 W 标志，对本应冲突的唯一缺席行从本地票副本移除其删除资格，再让真实 AcceptReload 消费这次读取。允许删除时原样交付。此手段仅检验逐行判定的后果；正式实现必须在 Registry 内比较 m，不能让调用者删改 T 签出的 expected。模型的 w 独立于尚未实现重新确认的生产私有时钟。S1–S8 的相应版本/存在性对照、S9 的来源拒绝条件、S11 的真实删除与重建、S12 的容量拒绝及两组字段对照均有已执行断言。S9 完整历史处置、S10 整图 Stop、S11 的原票交还、分配失败和统一发布的并发可见性为 DESIGN，业务 Host 未执行。没有把顺序探针当成并发发布或 Provider 证明。

## 11. 对 Part 24 的精确修订建议

|历史位置|下一版应替换或补入的可验条款|
|---|---|
|§5 第 1–2 步、§8 T 接纳|移除“只确认既存关系时不增版”对**视图成员**的豁免。把同票重放、跨新票重新确认、纯字段输入和既有 Binding 确认分开。按原票写依据冻结视图/字段集合|
|§5 第 3–5 步、旧 reload 表|用本文 §6–7 的 m、水位和 j 区分缺行删除、有行逐字段合并及重新加入。将 m/j、必要字段版本及 W 一并纳入无失败尾段；将原票和旧读各自的准入检查写到实际提交点|
|§7–8、§10–12|待接纳与待 Finalize 是两种同票关闭义务。补 B24-01、只确认 C、双视图、同票重复、新签空读、同值字段确认、删除重建和容量不足的观测点|
|§14、§16|拆列源码实测、模型实测、正常 Topology 入口 Host 和产品证据。将 §14 的逐项反例作为重过 Gate 的否决项|

## 12. 四篇权威设计的后续同步位置

|文档|架构决定后应同步的内容|
|---|---|
|[A 目标架构 §6.5.2、§10.1](SIM_Target_Architecture_Design_v1.2.md)|在发现接纳补新票对视图的正向确认，身份表区分 t、视图成员、字段和 Binding 水位；仅对有资格的视图和字段签发|
|[B 源码详细设计 §4.2、§5、§6 T、§11 H](SIM_Multisim_Source_Architecture_Detailed_Design.md)|把 BeginReload/AcceptReload 的两个分支与 T 的无失败发布尾段、票内 W 映射到具体对象；§11 只引用待本地提交及待释放收据|
|[C 目录设计 §4.1 Account](SIM重构代码目录结构设计方案.md)|在 Registry、BindingIndex、T 编排的职责行标出视图刻度/墓碑、槽关系和原票各自的存放处。沿用现有目录与依赖方向|
|[D 迁移设计 §7.4、AF-1 补充验收](migration/SIM_Architecture_Refactor_Design.md#af1-acceptance)|在账户写者/重载切片与 SC-01 逐项收据验收中加入旧缺席读、较新合法读取、A 已解析而 T 未发布三类闸点。产品写权沿既有 WU 流程验收|

## 13. AF-2 最小实施与跨 P0 回归输入

1. **T / AF2-07(d)**。把直返和核对收敛到按原 t 的资格判定。扩充 Registry 的成员记录、准备差量与 W，统一槽来源、m/F 与绑定关系发布。查清 AcceptStoredDiscovery、装配、reload 及所有可绕过正常 T 域的写点。禁止通过夹具“先 seed 后宣布发现成功”替代真实入口。
2. **T 读取 / AF2-02**。由 PrepareAccountReload 受控签票，分别拦截“旧读已实际取空”和“旧读已含旧字段”，在真 DiscoverAccount 路径交付 t2 证据再释放旧读。在同快照读 i、C/H、m/j/F、FindBinding、ReadPrimaryTarget 与 W。再交同票、发新读、复测两个独立视图。多行时保护 Y、允许不相干 Z 的合法删除/字段更新。Inject 分配失败与限幅时钟，核对 §9 各边界。
3. **P0-1 回归**。用另一写者产生相同行，给错误 call 或不完整逐字段证据。T 留 t，A 留预留，断言所有 m/F/r 都不动、无当前 W。只有 P0-1 允许的原调用证据及当前资格齐备才到本提案入口。
4. **P0-3 回归 / AF2-05、06**。控制来源撤销、内部 claim、发布、A Finalize 返回及执行退出闸点。对同一关闭轮逐项读取 T/A 收据和活动版本。Store 已确认而 T 未发布时仍列 t/待成员接纳；已发布则列 t/W/待释放。一次有变化的双遍快照整组重新获取，重复 Stop 不触发成员推进。来源未确证、Provider 未补证或宿主不具备稳定封口时维持原 OPEN。

## 14. 重过独立 Gate 的通过与否决条件

|独立审查者施加的输入|必须观测到的行为；出现相反结果即否决|
|---|---|
|先签票并采空 → 新 t2 有原证据、同来源同绑定 → 交旧读|C=1、H=1、r=1，旧读对 Y 删除冲突，t2 的 mC 和 W 同一次发布。反过来先删除则旧基线不能复活 Y|
|保持同一原操作重复交付，再交真正晚于发布才签发并采空的新读|t2 不重签 m/F/r，不重发 Store/角色/通知。新读能消费并合法删 Y。其他键活动不能让其无条件失权|
|独立交错 C/H、只确认单视图、重建与同值字段确认|两视图分别对自己的 m 判定。旧有行只保留同一存续期间且未冲突的字段资格。单靠相同 Subscription、slot、字段值或 r 不能穿越 ABA|
|每次准备失败及尾段逐点暂停|读者只见 B 或 N，容量不足也无一个视图先升版。所有尾段操作有针对实际载荷和分配器的无抛依据；只证明锁存在不足以通过|
|错原写回执、可信失效、待排序撤销、Finalize Unknown 与 Stop|错误原写不推进版本。当前资格未证时保留 t，不能从已知 Store 效果推导可关闭。t/W 和 A 回执的 M19 项直至精确交还始终可追踪|

提案层结论为 **`P0_2_MEMBERSHIP_REVISION_PROPOSED`**。B24-01 在本轮模型中已被逐成员水位比较阻断，同票与新读取的两个负控均保持可用。现有独立裁决继续为 **`B1_REVIEW_BLOCKED`、`H3_REVIEW_BLOCKED`**。正式 Gate 由另一位独立审查者据上述条件复核。实际实现的无抛尾段、全写面与正常入口端到端观测仍需相应实施证据。

### 附录 · 仓外探针复现

以下为本轮实际编译的完整源文件。可从此文档提取，无需保留任何临时目录。模型不改仓库中的头文件、实现或测试。

```sh
p=telephony_core_service/docs/AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md
probe_dir=$(mktemp -d)
awk '/^```cpp$/{on=1; next} on && /^```$/{exit} on' "$p" > "$probe_dir/probe.cpp"
src=telephony_core_service/services/multisim
g++ -std=c++17 -Wall -Wextra -pedantic -I"$src" "$probe_dir/probe.cpp" \
  "$src/account/account_registry.cpp" "$src/account/binding_index.cpp" \
  "$src/infrastructure/assembly/slot_catalog.cpp" -o "$probe_dir/probe"
"$probe_dir/probe"
```

```cpp
#include "account/account_registry.h"
#include "account/binding_index.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>
using namespace OHOS::Telephony::MultiSim;
constexpr auto C = AccountReadView::Current;
constexpr auto H = AccountReadView::Historical;
constexpr SubscriptionId Y{41};
constexpr SlotId slot{0};
struct Read {
    AccountReloadTicket actual;
    uint64_t w;
};
struct Fixture {
    SlotCatalog catalog{{{slot, EndpointKind::Physical, {}, {}, VisibilityClass::Ordinary, false}}, {}};
    AccountRegistry accounts;
    BindingIndex bindings{catalog};
    AccountProjection p{};
    // Only these explicit model variables represent proposed metadata.
    uint64_t clock = 2;
    std::array<uint64_t,2> m{1,2}, joined{1,2};
    bool witness = false; // one fresh t2 in each isolated fixture
    Fixture() {
        p.active = true;
        p.legacyPosition = slot;
        assert(accounts.AcceptDiscoveredAccount({Y,"Y",1,p,p}) == AccountAcceptance::Accepted);
        assert(bindings.AcceptAssemblyState(slot,true) && bindings.AcceptBinding(slot,Y,0));
    }
    bool exists(AccountReadView v) const { return accounts.ReadProjection(Y,v).has_value(); }
    Read begin(AccountReadView v) { return {accounts.BeginReload(v),clock}; }
    bool reaffirm(bool c, bool h, bool proven=true, bool live=true) {
        if (witness) return true; // exact same input in duplicate case
        if (!proven || !live || clock > UINT64_MAX - unsigned(c) - unsigned(h)) return false;
        assert(bindings.AcceptBinding(slot,Y,1));
        if (c) m[0] = ++clock;
        if (h) m[1] = ++clock;
        witness = true;
        return true;
    }
    void empty(Read l, bool proposed) {
        const auto v = l.actual.view;
        const int k = v == C ? 0 : 1;
        // One absent row. Withhold only its delete permission, never the whole attempt.
        if (proposed && m[k] > l.w) l.actual.expected.erase(Y);
        const bool before = exists(v);
        assert(accounts.AcceptReload(l.actual,true,{}).acceptedAttempt);
        if (before && !exists(v)) { m[k] = ++clock; joined[k] = 0; }
    }
    unsigned r() const { return bindings.Find(slot)->bindingRevision; }
};
int main() {
    static_assert(std::is_nothrow_swappable_v<AccountRecord>);
    static_assert(std::is_nothrow_swappable_v<std::optional<BindingRef>>);
    for (bool revised : {false,true}) {
        Fixture f;
        auto old = f.begin(C);
        assert(old.actual.membershipWatermark==2 && old.actual.expected.size()==1);
        if (revised) {
            assert(f.reaffirm(true,false) && f.reaffirm(true,false));
            assert(f.m[0]==3 && f.m[1]==2 && f.clock==3);
        } else assert(f.bindings.AcceptBinding(slot,Y,1));
        f.empty(old,revised);
        assert(f.exists(C)==revised && f.exists(H) && f.r()==1);
        std::cout << (revised?"proposal":"source") << " C=" << f.exists(C)
                  << " H=" << f.exists(H) << " r=" << f.r() << '\n';
        if (revised) {
            auto fresh=f.begin(C);
            assert(f.reaffirm(true,false) && f.clock==fresh.w); // replay after new read
            f.empty(fresh,true);
            assert(!f.exists(C) && f.exists(H) && f.r()==1 && f.m[0]==4);
        }
    }
    for (bool both : {false,true}) {
        Fixture f;
        auto oldC=f.begin(C), oldH=f.begin(H);
        assert(f.reaffirm(true,both));
        f.empty(oldC,true);
        f.empty(oldH,true);
        assert(f.exists(C) && f.exists(H)==both && f.r()==1);
        assert(f.m[0]==3 && f.m[1]==4); // H=4 means a tombstone OR a confirmed member
    }
    {
        Fixture f;
        assert(!f.reaffirm(true,false,false,true) && !f.reaffirm(true,false,true,false));
        assert(!f.witness && f.m[0]==1 && f.m[1]==2);
        f.clock=UINT64_MAX-1;
        assert(!f.reaffirm(true,true) && !f.witness && f.m[0]==1 && f.m[1]==2 && f.r()==1);
    }
    {
        Fixture f;
        auto old=f.begin(C);
        f.empty(old,true);
        auto create=f.begin(C);
        assert(f.accounts.AcceptReload(create.actual,true,{{Y,"Y",1,f.p,{}}}).acceptedAttempt);
        f.joined[0]=f.m[0]=++f.clock;
        assert(f.exists(C) && f.joined[0]>old.w && f.m[0]>old.w);
        assert(!f.accounts.AcceptReload(old.actual,true,{}).acceptedAttempt && f.r()==1);
    }
    for (bool sameValue : {false,true}) {
        Fixture f;
        auto old=f.begin(C);
        assert(f.reaffirm(true,false));
        const auto name=sameValue ? u"" : u"confirmed";
        assert(f.accounts.AcceptDisplayName(Y,C,0,name)==AccountAcceptance::Accepted);
        auto row=f.p;
        row.displayName=u"stale";
        row.displayNumber=u"42";
        auto result=f.accounts.AcceptReload(old.actual,true,{{Y,"Y",1,row,{}}});
        auto actual=f.accounts.ReadProjection(Y,C);
        assert(result.acceptedAttempt && result.conflictedFields==1 && result.acceptedFields==5);
        assert(actual->displayName==name && actual->displayNumber==u"42");
        assert(actual->revisions.name==1 && actual->revisions.number==1 && f.r()==1);
    }
    std::cout << "fresh-delete, replay, views, proof/source, capacity, "
                 "recreate and per-field controls OK\n";
}
```
