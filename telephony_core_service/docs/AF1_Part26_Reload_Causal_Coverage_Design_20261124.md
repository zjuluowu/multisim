# AF-1 Part 26 · Reload 读源因果覆盖与受保护事实接纳契约

**状态：`RELOAD_CAUSAL_COVERAGE_DESIGN_PROPOSED`。** 建议 T/A 在各自已接纳的事实旁保留“以后哪种读取可以替换它”的依据。I 交付本次查询的读源与可见性证据；T 对成员和字段分别判定，A 对整组角色投影独立判定。当前 Legacy 桥不能证明后续查询已看见原写，缺证时保留受保护值。一笔有身份、可排序、确已生效的后续业务操作仍可更新或移除该事实。这条安全路径可在现有 Owner 内实现。仅能通过无覆盖证明的 Reload 得知外部变化的产品切片，仍有活性和兼容性门禁。

## 1. 当前审查基线

|输入|实际版本|
|---|---|
|仓库|main、HEAD、远端 origin/main 均为 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`。目标 services/multisim 最后变更于 `632f6a2a888cfbbc3c9387edc89732364e12a10e`，两提交的该目录树均为 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。Legacy 查询实现读取当前 HEAD。|
|[检查点 Part 23 §§23.2–23.3](AF1_Architecture_Design_Convergence_20261123.md)|SHA-256 `0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2`。原票、业务见证和精确释放仍分阶段结清。|
|[Part 24 §§4–6](AF1_Part24_B1_P0_Design_Revision_20261123.md)|`a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164`。继承 Store 原写归因、统一发布、关闭轮次和稳定收据。|
|[Part 25 §§4–10](AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)|`4ff376e7adfae7ef1f475ebc8cb411030293c8c1a47bedc6780ea1289f753418`。沿用按原票重放、分视图成员 m、加入刻度 j、字段 F 与槽水位 r。|
|[最新独立 Gate §§8–9、14–15](history/reviews/AF1_Part25_Independent_Gate_Review_20261124.md)|`58940ed346b660d36d58c6519a2008fdc93036c569514aeb182d13aa9eb9218e`。P25-R1 同时影响缺席删除和有行替换。|
|工作区与证据|开始时已跟踪文件、暂存区无差异。既有未跟踪项为 default.profraw、opencode.json、Part 24、Part 25 及三份独立审查报告。本文新建。下文“源码”指当前实现，“提案”指待裁决契约。§14 的仓外 C++17 顺序探针为 E2/E3；业务 Host、并发、sanitizer、设备均为 NOT_RUN。|

## 2. P25-R1 的根因及跨 Owner 后果

在外部图像 11，原写已令 Y 属于 Current，T 已发布 W、mC=3。新票 L 带 w=3、expected[Y]。历史甲在图像 12 真正移出 Y；历史乙仍停在 11，查询端却看见更早的图像 10。两次查询均在签票之后取样，均成功返回完整空集。T 的本地谓词在两次执行中相同。甲可删，乙须留。对名称同理：L 的 F.name=1 与当前相等只能证明签票后没有本地新写。以旧名称覆盖 New，会把错误发布记作第 2 版。独立 Gate 已用真实 Registry、BindingIndex 配合外部图像模型给出了这两项结果。[独立反例与分级](history/reviews/AF1_Part25_Independent_Gate_Review_20261124.md)

**同批角色数据有第二个出口。** AccountRegistry::AcceptReload 在整次输入检查后先置 acceptedAttempt=true，才逐字段计 acceptedFields/conflictedFields。某字段拒绝时仍可消费 attempt。TopologyService::AcceptAccountReload 只验 view/attempt、两组行数，再以 loaded.complete 和 acceptedAttempt 决定向 A 交付全部 storedRoles，且未接收其处置结果。AssignmentService 在 A 锁内调用 AccountRoleProtocol::AcceptReloadSet。原 A 版本和 attempt 合格时，后者以新 map 整体替换该视图，空集清空，随后推进角色版本。T 本行名称冲突不一定涉及角色，但查询成功和 T 消费 attempt 都不能证明角色集可靠。§14 实测 T 保留 New 并接纳号码的同次输入，现有 A 接纳条件可让 voice 从 true 变为 false。[T 调用点 :275–315](../services/multisim/account/topology_service.cpp)；[Registry :272–335](../services/multisim/account/account_registry.cpp)；[A 委托 :687–703](../services/multisim/role_assignment/assignment_service.cpp)；[A 整组替换 :67–96](../services/multisim/role_assignment/account_role_protocol.h)

## 3. 本地资格、写入归因与读取覆盖

|命题|证据与 Owner|可授权的动作|
|---|---|---|
|L 是本实例、这个 view 最新未消费的读取|T 的 epoch/view/attempt、水位 w、expected 中的 s/i/F；A 另有本 view 版本|进入本地冲突检查。L 不代表 Provider 的事务或快照。|
|原操作 t 对哪些键写成|原 call、实际订阅/ICCID、原计划列、已归因的外部效果；T/A 各自接纳自己的事实|提交对应本地事实与见证 W。单有 writeConfirmed 或同值读回不证明另一查询已看见它。|
|本次读图像已经包含某项效果或其因果后继|I 提供来源、取样及覆盖依据。T/A 在各自写点核验该依据与本键的保护要求一致|结合身份、局部版本和业务读源，授权具体删除或字段替换。包含先前写允许现在读到合法的**不同值**或缺席。|
|真实来源已终止或绑定被撤|R 提供原来源的可信事件，T 检查 epoch、顺序、BindingRef 与槽水位|按原协议处理会话或绑定。已解绑本身不证明 Historical 或 Current 的数据库谓词已改变。|

## 4. Provider 现有能力取证

|能力|源码证据|判定|
|---|---|---|
|Current / Historical 逻辑读源|Current 用 QueryAllValidData，谓词 SLOT_INDEX > INVALID_VALUE；Historical 用 QueryAllData，全量查询。Legacy 成功后分别整体替换 localCacheInfo_ / allLocalCacheInfo_，失败有各自重试。目标桥把同一结果的账户字段及角色列并行序列化。[Legacy 查询 :465–523](../services/sim/src/sim_rdb_helper.cpp)；[旧缓存 :743–792](../services/sim/src/multi_sim_controller.cpp)；[桥 :41–55](../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)|两种读源及本地缓存独立，底层 URI 可相同。后端是否存在副本/服务器缓存、是否采用相同隔离级别均未获证明。|
|写入会话与读取会话|Insert、Update、Query 使用 SIM_RDB_SELECTION；InsertData、UpdateDataByIccId 及两种查询分别创建并释放 helper。TelephonyDataHelper 通过 SA 与 Creator 创建 helper，Current 建连还可回退重试。[SimRdbHelper :34–50、97–131、350–365、492–523、541–554](../services/sim/src/sim_rdb_helper.cpp)；[Creator 与 CreateSimHelper :26–70](../services/sim/src/telephony_data_helper.cpp)|同 URI、同进程或先后同步返回不能证明使用同一 RYW 会话，也不能证明重连前后的顺序连续。|
|事务位置、读图像与对象范围|AccountLoadRequest 只有 view/loadAttempt；AccountLoadResult 含 request、legacyResult、complete、legacyReturn、rows、storedRoles。AccountDiscoveryStoreResult 含返回身份、确认/结束位、effect。Adapter 委托 Legacy 桥。[AccountStore :34–84](../services/multisim/account/account_store.h)；[Adapter :27–45](../services/multisim/account/account_store_adapter.cpp)|本仓真实契约没有外部 commit token、snapshot prefix、数据库世系或可比较的跨 helper 顺序。源中的行 ID/受影响行数不具备这些语义。|
|结果完整性与列有效性|桥以 result==TELEPHONY_ERR_SUCCESS 设置 complete。Legacy 非空游标上 GoToFirstRow/GoToNextRow 返回非零即结束循环并报成功。SaveDataToBean 多处未校验列提取返回值。[桥 :41–54](../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)；[游标/解码 :367–419、480–489、514–523](../services/sim/src/sim_rdb_helper.cpp)|可证“旧查询报告成功”。权威缺席还需完整遍历、有效筛选/身份列与可识别的正常结束。列默认值不能证明已读。legacyReturn 另行沿旧值返回。|
|原写与后续业务入口|发现写可能写 SLOT_INDEX、active、label、eSIM 和 A 计划列；桥再按 ICCID 读回。Legacy SetShowName 写库后依次更新两套缓存；ForgetAllData(slot) 是槽谓词的外部更新。目标字段写及 WriteRemoval 适配器当前为未实现返回。AcceptCardRemoval 只调 RemoveBinding。[发现写 :66–104](../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)；[SetShowName :1836–1875](../services/sim/src/multi_sim_controller.cpp)；[位置更新 :600–629](../services/sim/src/sim_rdb_helper.cpp)；[T 移除 :242–251](../services/multisim/account/topology_service.cpp)|有业务写/撤销路径可供以后取证；这些入口的存在不证明同一行、每个列、每个 view 均已成功。尤其槽写必须排除换绑导致命中其他行。|
|匹配产品保证|权威设计将外仓 Provider 版本、外部写者和事务能力列为 TGT-OPEN-007 / §13 条件。本仓可追到 DataShare 客户端与 Host 桥。[A §4.3、§13 及 F21/F37](SIM_Target_Architecture_Design_v1.2.md)|没有把上游不同版本实现、Host 假读源、RDB 的一般性质列为本产品证明。A/B 两种覆盖能力默认未认证。|

## 5. 三种候选方案

|方案|产生者与可执行检验|代价、适用性及失败行为|
|---|---|---|
|A · 已证实的有序读路径|Provider/产品为精确路由定义会话、写完成可见点、查询开始取样、完整读取与重连边界。I 在每次调用核验实际写/读均属认证范围并保留原操作到本查询的有序执行链。若只能保证本会话 RYW，仅覆盖同会话已归因的写；还要证明随后已接纳的读取不倒退，或以等价有序路径保护其后继。|若已具备，可不改 schema。跨 helper、另一个用户/view、故障转移或会话更换均不能猜测连续。当前证据不足。|
|B · 外部提交位置与快照前缀|同一权威数据源为写签发提交位置，为读取签发已实际应用且可见的**完整前缀**，二者使用可比较的同一世系、分区与范围。分区分别比较，空集也须有集合谓词范围的证据。高编号中间有缺口、每行最后修改号或一次查询结束时间均不够。|适合读写跨会话。若端口尚无此能力，需要 Provider/产品接口支持及匹配认证；不要求为本提案先改生产库。缺 token 或不可比即 Unknown。|
|C · Owner 保存受保护事实，按证据接纳|T 保存成员/字段所需的依据，A 保存角色事实的依据。没有覆盖能力时，T 保留受保护键，有独立合法读源且无冲突的其他字段可前进。有可信后续业务事实时，按该原操作推进。|现在即可定义安全行为，无需新增 Owner。C 自身不能辨别两组相同的空结果；只有 Reload 通知外部变化且缺 A/B 或业务输入时，不能保证何时可删或刷新。|

## 6. 推荐的最小架构方案

采用 **C 为默认规则，A 或 B 为可选的证据来源**。I 输出本次读的证据和有效范围，不决定 Account/Binding/Role。仅获旧成功码的路径在覆盖维度输出 Unknown。A/B 是两种可替换的实现能力，产品无需同时提供。允许用独立、已授权、可排序的后续业务写完成更新，不必强制读回。

没有受保护事实的键仍按既有可信业务读源、身份、列有效性和 Part 25 版本规则接纳。这里的“没有”必须可证：从本实例可信基线建立起，所有相关本地接纳都经 Owner 登记，没有遗失的前缀。重建后元数据缺失或某写者尚未完成切换，不能标作“从未受保护”。读源只证明一条号码时，不能借它放行名称或整组角色。这样保留了 WU 的正常读路和逐字段并发能力。

**最小接口变化（均为 PROPOSED）。** 复用 AccountLoadRequest/Result 和 T 的 ReloadRequest，带出原查询身份与有效来源上下文，带回可验证的证据、终止原因和列/范围有效性；沿 T 私有 AccountReloadTicket 保留本地基线。扩充 Registry 的本次接纳结果，返回逐键处置及角色**集合**是否有可用的共同身份/成员依据。复用 AccountRolePlanningPort 原整组接口，传不可变批次证据与一次性交付资格，A 返回本批处置。保留既有 legacyResult/legacyReturn 的含义。不需暴露新公共 SIM API、跨库事务或新管理器。

## 7. 受保护写入事实的数据模型与寿命

下表为必须能恢复的语义，允许采用紧凑表示。业务值只留在原 Owner。

|存放处|键与最小内容|建立与更新|
|---|---|---|
|T 的成员条目（含缺席墓碑）|用户/库逻辑命名空间、view、s/ICCID/身份代次、成员存在/缺席、m/j，以及该成员以后替换所需的原操作或已认证读图像依据|合格新票随 Part 25 的 W 原子发布；同票重放读取 W。合法移除发新 m 并存**负向**保护，旧有行读不得恢复。|
|T 的投影/退役条目|同域、同身份、view、字段 k 与 F[k]，保护该事实所需的原效果引用；值取正式投影|仅为实际被接纳的字段设置。一次 SetShowName 的 C 成功与 H 失败分别记录。投影删除会消灭其所有字段，删除资格必须覆盖这些被消灭的保护或有独立的成员终止依据。|
|A 的原角色投影|view、角色集合成员/身份范围及相应列、A 版本、原角色写及已接纳读图像的依据；删除角色项需留负向范围|A 随本 Owner 的 setter/受权接纳发布。T 的 W 可引用合写的原 call，不能据此推断 A 已提交角色。|
|各 Owner 的来源依据|原 t/call 或 setter 的原操作身份、实际行/列/谓词、已确认效果、发布版本/见证摘要；有外部位置才附其世系/已应用范围。纯本地已见事实可记“仅独立业务处置可替代”|与业务事实一次发布，资源分配提前。原票结清后保留比对所需摘要，不要求永久保留整票、A 预留或敏感行副本。未知写仍由原票核对，影响键标在途，不借 Reload 提前处置。|

**终止不能造成回退。** 对同键保留尚须覆盖的一组因果要求。新证据支配旧要求时可压缩为一个后继；不可比要求保留并逐个检验。以图像 12 正式接纳 Later 后，保护要求至少推进到 12；后来只覆盖最初写 11 的图像不能覆盖 Later。对已知同值但无覆盖证据的读只作无写观察，不消除保护、不升版。有证据证明后继移除可终止该成员的正向保护，留下该次移除的防复活依据；重建必须有真正较新的资格。一次收益仅限本键/本 view。路线 A 没有数值位置时保存认证会话与操作/读取的顺序依据，按其契约判断后继；不把本地 ordinal 当成外部 commit。

已无外部工作时，这些保护是正式事实的元数据，可独立于 t 寿命存在。可在已证明往后所有合法读取必经不倒退的新基线，或旧命名空间最终退役且旧入口已失效时回收。能力降级、库恢复、用户切换、进程/模块重建须撤销原路线资格；新图用经确认移交的最低要求或可信权威基线启动。不能恢复时标为“基线未知”，在依赖键上停止替换和生产切换，直到重新取证。没有批准的持久日志，也不以重启清除保护。

## 8. Reload 的证据与接纳结果契约

|维度|I 在本次输入上可据实交付的内容|Owner 检查|
|---|---|---|
|身份与路线|原 L、模块寿命、库/用户、实际 Provider 契约版本、会话或分区/复制世系、连接代次、查询谓词/view、取样关联。运行时由适配器绑定实际调用，结果不可重贴票号。|T 核对本地签票与配置；连接途中切换、结果混自不同图像或能力已撤销时原凭据不转用。A 使用同一不可变批次。|
|物理读取|调用结束/失败、正常游标终止/中断/未知、是否覆盖全谓词、唯一身份集合、每行身份与每列解码有效性、角色列与账户行按 s 的对应|失败、截断或完整性未知的整次读按失败消费/保留重试，不作集合替换，本方案保守地也不从截断前缀写字段。遍历已完整而个别非身份列无效时，只拒这些列及其依赖，身份或过滤成员判据不明不得声称完整成员集。|
|因果覆盖|A 路径的已认证执行与实际会话链，或 B 路径的写凭据/可见前缀，作用域含事实键、列或完整集合。可对某键给出 Covers / 已证落后 / Unknown。|以本地**提交时**仍有效的保护要求计算。证据绑定正确身份/范围且支配所有所需前驱才为 Covers。证据不能跨分支比较；P0-1 的“已写”只提供要被覆盖的前驱。|
|逐项结果|T 记录 L 的消费状态，每键记录接纳、新事实值相同、局部版冲突、覆盖不足、陈旧、读取失败或在途。返回成功字段掩码、保留项和原因，必要时给出下一次取证责任。|相同 L 重送只读原处置或返回陈旧；不重做已完成子项、重写 Store 或发布通知。本次读取是否可用于整组角色交付另列。|

**可执行的覆盖检查。** 先在 Owner 域内取本键真实要求，验证样本身份和完整性，再从已认证适配器种类中选验证器。方案 A 检查批准范围、写/读同一可排序路线、原写已达可见点、该实际采样在其之后且看见包含先前已接纳读取的前缀；缺任一边返回 Unknown。方案 B 对每个受保护键检查原写凭据与快照证明位于同一比较域，且读取对该范围的**可见前缀**支配该键的每个前驱。两个版本只在这个已验证的域内比较。回显相同值、时间戳、attempt、sleep、轮询次数、消息抵达顺序均不能充当这一步。I 可传原始认证材料或不可变结果句柄，不能让业务调用者自报 covers=true。

**判定返回值及异常证据。** 无可信世系、无认证路线或缺少要求的任何前驱时返回 Unknown 并附缺失范围；只有可比较且确实落后才返回 Behind。二者都不能取得相应写资格。遇到同一不可变提交位置却返回相互矛盾的状态，或整批数据不可能来自其声称的同一图像，隔离这份证据并登记来源异常，不能用数值比较消解矛盾。已公布更新后的保护下界也必须在调用者可见前与业务值一并就位。已知整条路由错误、身份不可认证时，所有依赖该路由的列均失权；仅名称覆盖不足时才适用号码独立处理。

## 9. 缺席删除与合法业务移除

```text
ReloadMayErase(L,v,s) =
    CompletePredicateImage(L,v) AND Missing(L,s) AND s in L.expected
    AND LocalAbsenceChecks25(L,v,s) AND KnownLineage(v,s)
    AND NoUnresolvedWriteTouching(the facts to be erased)
    AND CoversAll(L, requirements for identity, membership and discarded fields)
```

LocalAbsenceChecks25 包括同 epoch/view、最新未消费 attempt、仍在场、身份与全部字段基线相等、0<m[v,s]≤w、无未核实前缀。需要可发布删除刻度和墓碑。对已证明从未有保护的键，空要求不妨碍旧业务的可信全读，但绝不跳过完整性和本地条件。不满足时只在受影响键记冲突并保留投影；Reload 永不直接写 BindingIndex。若对成员给出完整权威的**后继成员终止事件**，可以按另一条业务路径退出。

例如，已归因的原位置更新 t3 在 t2 之后按同一行/原 s 与有效来源受理，实际将该行置为不符合 Current 条件，且资格覆盖这段成员寿命（含此前已发生的相关写，在途后继已排序）。T 在短域重验原 i、m/j、相关字段及事件来源。一致则正式退役该段投影、保留字段退役依据、mC 升一次、jC 清空，H 独立。处置只终止 Current 的投影，不声称外库删除历史字段。按槽批量执行 ForgetAllData 的返回码不能独自证明命中了 Y；须有匹配行/范围和部分效果证据。真实拔卡可另按原来源及 BindingRef 调 RemoveBinding，使原 r→r+1；若只有数据库移出证据，r 原样。反之仅有拔卡、Session 刷新或 Stop 也无 Current 读源删除授权。[T 移除入口](../services/multisim/account/topology_service.cpp)；[BindingIndex :59–94](../services/multisim/account/binding_index.cpp)

## 10. 有行字段覆盖与部分接纳

```text
ReloadMayReplaceField(L,v,s,k) =
    EligibleRead(L,v) AND ValidRowIdentityAndColumn(L,s,k)
    AND SameAccountAndContinuousProjection(L,s,i,j) AND j <= L.w
    AND expected.F[k] == current.F[k] AND NoUnresolvedWriteTouching(s,v,k)
    AND KnownLineage(s,v,k) AND CoversAll(L, requirements for this field)
```

已有、连续存在的投影可接纳独立字段而不重新确认成员。读到同身份的一行只用作地址核对，不签发 m；行身份来源不清、成员已撤再建或读源已整体失效时不走这条分支。投影原本缺席的创建须另验 Part 25 的创建谓词、墓碑及成员覆盖，不能借“字段更新”复活旧对象。改变身份、位置筛选或投影存续含义的输入，要按其实际写入的依赖组验证，不能把一项成员动作伪装成无关字段。完整结果中各可解码列分开判断，不相干的字段版本不作为全行拒绝条件。

对已有 Y，New/nameRev=1 的原写未被覆盖，返回 Old 只记名称冲突。若号码是同一身份、同一存续期间、列来源有效、从可信初态以来无受保护号码，且原号码版仍相等，可以接纳读到的 42。若号码也有已确认写，必须单独覆盖它。对可接纳的有行读取沿既有规则“值变化才进字段版”，接纳有足够证据的同值读取可更新其可见性下界而不误升 F。Part 25 中**新原写**明确重申同值字段属于另一事件，按该票一次推进。普通 Reload 若承接更新过的保护下界而没有升 F，后继交付仍在真实写点比较最新下界；不能靠相同 F 越过它。[字段实际接纳 :190–260](../services/multisim/account/account_registry.cpp)

## 11. 两个视图与 A 的整组角色投影

**同批读缺乏支持角色集合的证据时，A 必须保留原角色集。** A 的当前角色版本匹配只表达其本地准入。Current 的过滤集合、Historical 的全量集合、各自的 m/F、A 的两套角色版本及覆盖范围分别计算。同一外部快照只有**分别证明**两个查询都在该读图像上实际执行且各自完整时才可作为两边的依据。用同一 URI 无法推出这点。

1. **T 决定是否可交付原批次。** 在处理该 view 的同一提交域，检验取样及遍历、行身份和 storedRoles 一一对应（键集合相等、无重复、无错配）、返回/遗漏的每个成员及已知墓碑。全组角色替换可能删除遗漏的角色行；因此任何相关成员/来源冲突或缺证都禁止按整组转发。只有 T 的无关名称/号码有局部冲突，而整个成员集合和身份依据仍合格时，可以交付**原样**角色载荷及证据。不得过滤冲突行后把较短的向量当完整角色集。
2. **A 在自己的写点决定是否替换。** 必须有同一 L/epoch/view 的原批次、经认证的完整角色列与成员域、A 自己所有会被替换/擦除的角色事实均已被覆盖、expectedRoleRevision 与当前相等且 attempt 合格。对缺证的整批保持原投影，不从 T 的值拼角色、不降为调用 AcceptSingleProjection 构造部分成功。现在的角色接口是整组替换，维持该粒度有真实依据。T 可以已经接纳无关字段，双方分别返回真实结果。
3. **交付间隙。** 每个 view 最多保留一个可执行的角色集交付资格，挂在原 L 上，持原不可变载荷。T 在出域前登记，影响该集合的成员/身份更改、路线失效或更新的同 view 尝试，使尚未被 A 使用的旧资格失效。一个可实施方式是 L 内的一次性状态：T 在相关无抛提交尾段把“可用”改为“撤回”，A 则在本域完成资格确认和候选 map 准备后，以条件交换把“可用”改为“已领取并提交”。仅一个转换成功。A 成功后尾段仅作无抛交换和版本/回执赋值；A 读者同锁。撤回先赢则 A 无写；A 先赢则该批线性化在后来 T 变更之前。A 的随后本地 setter 仍有独立排序。该资格属于一次投递，不能当作共享业务版本。涉及适配器路线的撤销须在受理点先进入该失效排序，否则该路线无权签发资格。无跨 Owner 阻塞锁。A 是否取用和执行是否已交还分别记入收据。
   在合写流程内，A 仍持原角色片段及 token；角色列效果未归因或原片段仍可能影响这次替换时，先把相应 A 键标为有原操作待处置。A 在自己的正式接纳点用同一原 call 的已证列安装本域保护；仅固定 resolvedIdentity 本身没有产生角色值。不能因为 T 已获得自己的 W 就丢弃 A 的角色前驱，或从 T 读出的角色值补齐 A 的证明。

4. **返回与幂等。** A 返回整组已接纳、版本冲突、覆盖不足、交付已撤回或仍待确认，并按 L 保存精确结果至 T 已消费且无在途返回。丢答复只重取原回执。T 不会把被 A 拒绝的批次标作角色已更新。T/A 投影反映各自已经接纳的观察时点；需要两者共同资格的调用者检查两份原处置，不能以本次 T 字段成功推断 A 已完成。

## 12. R1～R14 反例推演

各行独立。除特别说明外，槽 0 的 Y 为 s=41、i=1，当前/历史投影均存在，C 的 m=3、j=1，H 的 m=2、j=2，r=1。C.name=New/Fn=1，C.number=7/Fp=0，A 当前角色集 a1 含 Y 的 voice=true。L 为最新 C 票，w=3，冻结上述 i/F 及 A 版。M11/N11 表示成员/名称分别依赖已经确认的操作，数字是**受控 B 路线**的示例外部位置。U 为未证覆盖。各行写“保留”均包括其版本与保护依据。本表 A 状态指角色投影，不等于 A 预留寿命。

|编号、真实顺序与本地资格|Provider 输入及覆盖|T 允许/拒绝与保留事实|最终 Account、Binding、A；后续责任|
|---|---|---|---|
|R1 确认 M11 后签 L|完整空集，图像 10 或 U|m≤w 但不覆盖 M11/N11，拒删；记成员冲突|C/H 有/有，r1，a1。T 结本次尝试的缺证结果；若用例仍要求权威刷新，登记取证义务|
|R2 写 New 后签 L，Fn=1|有 Y.name=Old，缺 N11 覆盖|拒绝名称写，不升 Fn；N11 保留|New/1，r1；无独立整组依据则 a1。T 返回 name 冲突|
|R3 已确认号码 99/Fp=1，后签票|旧号码 7，能覆盖名称，未覆盖号码|保留 99/1 及号码前驱；合法其他列独立判断|两视图仍在，r1，a1（角色未证）；T 按号码键回报|
|R4 L 收到 Y 的部分合法字段，j=1|Old 和 42；行/号码有效，无号码保护，名称 U，角色列未证|拒 name，接纳 number 7→42/Fp=1。M11、N11 留存；不因号码成功确认成员|New/1、42/1，r1，a1。T 返回两个不同处置，不转整组角色|
|R5 L0 的 w=2/Fn=0，随后 W2 发布 m=3/Fn=1|完整旧空集，或有行携 Old；即使补给充分源覆盖|空读受 m>w 拒删；旧名受 Fn 冲突拒绝；不取消 W2|Y/New、r1；有成员冲突不发整组，a1。T 消费旧 L0，不重写 t2|
|R6 后续真实写 12 将 Y 移出 C，L 在原确认后签发，本地键未再变|可验证 12 的完整空集覆盖所退休事实；另一合法输入是带原身份/顺序的独立移除效果|退役 C、mC→4，建负向保护，H 保留。独立移除不复用旧 L|C/H 无/有；仅 DB 移出时 r1，另有匹配可信解绑时 r2 槽空。A 若整组及自身保护均获覆盖则去 Y，否则保留 a1 并单列拒绝|
|R7 合法外写 12 把名称改 Later，L 的 Fn=1|Y.name=Later，能覆盖 N11|接纳 Later/Fn=2，下界推进至 12；m 不随纯字段变更|C/H 仍在，r1；仅给名称证据时 a1。T 可结束旧 N11 保护记录，由后继约束接替|
|R8 只为 Historical 签 LH，另有 C 的能力凭证|同 URI，H 全量读未获同域保证，或单独有 H 的证据|C 的证明不得替代 H；只在单独合格时更新 H[mH,FH]|C/New、r1、A.current=a1 始终不动；A.historical 仅按 H 整组资格；H 读缺证由其所属 Owner T 处置|
|R9 L 返回失败或游标在 Z 后截断|即使 legacyResult 为成功，完整结束未证|本次不作集合替换或前缀字段写，保留 Y 所有事实|C/H、r1、a1 保留。T 记 read-failed/incomplete，按原预算另签票或明确结束|
|R10 已有 M11/N11，端口从不返回 A/B 证据|反复读到空集、Old，或恰好 New|不删、不覆盖、不因同值卸保护。合法独立后续写仍可执行|C/New、r1、a1。T 标记本次未刷新；仅靠该读源的外部变化无法保证最终反映，切换门禁未满足|
|R11 原 t2 成功回调重放，或 L 结果重复|原 call 不变；重复图像并非更强证据|有 W2 取原处置；L 已消费则只读结果。m/F/r、保护不重复推进|C/H 与 A 原值。T/A 只重取仍欠的原释放/交付回执|
|R12 T 发现 Y 缺席无依据，或 T 仅拒 name 却收到角色向量|同批空角色集/旧 voice=false，A 版本仍匹配；另一路只证明号码|成员冲突时 T 不交整组；纯字段冲突时若角色仍 U，A 也不可替换|Y 保留，r1、a1；号码可如 R4 独立成功。T/A 各回本批处置。有独立完备角色/成员证明且仅 name 冲突时，A 可以按证修改|
|R13 L 已领取回调，Stop 开始，覆盖/角色回执尚未返回|确认缺证或调用仍在途分开判断|已知拒绝可结本次结果；必须完成的核对、未返调用、A 交付继续列 M19。既成保护保留|至可信后继前 C/H、r1、a1 保留。T/A 分别持 L/子投递，H 按同轮双遍收据等待交还|
|R14 已接纳图像 12 的 Later/Fn=2，随后**新签** L2|图像 11 的 New，或新世系里的位置 99|即使覆盖初始写 11，也不能覆盖当前 12；不同世系不可比。来源重连先撤凭据|Later/2、r1、a1 保留；负向墓碑同理防旧行复活。T/I 待同域后继或可信重建基线|

## 13. 失败点、恢复与关闭责任

|失败点|谁持有什么；允许的下一步|
|---|---|
|原写未知或业务接纳尚未完成|T 持 Part 23/24 的 t/call，A 持相应 token。读到相同值不能解除未知。把受影响键上的破坏性 Reload 留在原尝试处置，互不代替。|
|创建保护节点、候选投影、墓碑、返回掩码或 W 失败|统一发布前 T 保留原票和原组。锁内可失败准备按迭代器撤销；尾段用既备存储无抛交换，含保护要求、m/j/F、W、下一步。不能出现有 W 无保护的窗口。A 同样在自己提交前准备其组。|
|查询未返回、连接变化、证明无法比对、单列解码失败|I/执行器保留执行至交回，T 持 L。路线变化使旧认证失效。已结束输入可按项报告拒绝或部分接纳，不重试已确认的原写。用例允许“保留旧缓存”时明确以 retained/partial 结本次刷新；要求权威结果时 T 持重新取证项并按原有预算/来源变更驱动。绝不把拒绝伪报为已刷新。|
|删除和更新同时准备，时钟/字段版耗尽、分配失败|依 Part 25 在发布前预检本批容量。可独立的逻辑冲突行可跳过；**资源失败**不得留下半发布、半消费或下界回退。按原票重验，版本不得回绕。|
|T 已发布部分字段，A 因资格撤回、更新 setter、证据不足或分配失败而拒绝|保留 T 的已发布事实。A 不修改原角色集；T 保留投递/回执至确知处置。允许重取原结果，不能给旧结果贴新 attempt 补送。后续独立查询按新签票进行。|
|Stop、重复 Stop、丢回执|根封口阻止新业务，已有 L 的返回/精确交付按 Part 24 保留通路。T 入队前登记父项，A 先登记后确认，父项等确认且执行体退出才清。业务要求的补证无合法接手者则保图；若该次刷新已明确以缺证结束，静态保护本身不虚构在途工作。重复 Stop 不驱动写入或版本变化。|
|未来实例接管仍须服务相同账户|正常销毁的执行体与后续服务所需事实分开。若需要续办事项或保存跨实例最低依据，要有相应逐项接手/可信重建；两者都无证不得把新图当成已同步的权威读面。无后续继承责任且所有在途已交还时，按原关闭条件处置。|

## 14. 可复现的 C++17 探针与后续断言

本轮实际从 Part 25 附录提取 Fixture，链接当前真实 AccountRegistry、BindingIndex、SlotCatalog，另使用真实 AccountRoleProtocol。第一对试次给同一旧读签票后更新 name，再交旧 name、号码 42 和旧角色列。baseline 按现行 T 转发条件调用 A 的真实方法；gated 使用合成覆盖判定。后两组则在写确认后**重新签票**，分别给缺证/旧/覆盖后的图像。输出如下。

```text
baseline conflicts=1 number=42 voice=0
gated conflicts=1 number=42 voice=1
read=-1 C=1 H=1 r=1
read=10 C=1 H=1 r=1
read=12 C=0 H=1 r=1
field read=10 nameRev=1 number=42
field read=12 nameRev=2 number=42
```

真实类执行了字段冲突、部分合并、整组角色替换、账户删除和槽关系读取。读源、W、部分本地刻度及覆盖比较由仓外模型提供；单成员空读用 Registry 的 replaceable 分支，逐字段新票试次调用真实字段 setter 演示可见结果。数字是认证路线 B 的**测试输入**，绝非从实际 DataShare 取得。程序还断言错路线、错代次、不完整、缺列范围、读取只覆盖旧下界时拒绝。E3 未运行真正的跨线程交付许可、故障注入或 Target 全图；E4/E5/PRODUCT/DEVICE 为 NOT_RUN。附录保留全部复现材料。

下一次独立复核应另加三个负控：同一空载荷在真实后来移除与滞后读端两种历史下必须走不同处置；遮蔽 Y 的缺席保护时不能误删独立 Z 或阻塞 Z 的合法更新；在 T 允许发送与 A 真正消费之间插入 T 的成员变更、A 的 setter、Stop，逐个验证票、回执、活动版本与旧角色集。另注入身份列错配、游标中断、回收 t 后的新读和图像 12→11→13，核对下界持续性。

## 15. 对 Part 24、Part 25 的精确修订建议

|历史位置|下一版需增加的内容|
|---|---|
|Part 24 §4、§7 原写归因|原调用成功证据的有效终点是“可判原效果”。保留下游替换所需的行/列、写来源与世系引用；没有 Provider 位置可记未知，不得为之后的 read 自动背书。|
|Part 24 §5、§6、§8、§10|T/A 的候选与无失败发布包含保护依据或墓碑。补“读已交付、业务缺证”“角色批次已发、A 尚未交还”两类原 subject。静态保护、在途工作、未来重建门禁分别记载。|
|Part 25 §§5–7 的 valid(L)、mayDelete、有行合并|将查询完成、游标完整、本地可接纳、外部覆盖拆成独立判据。删除需覆盖会退休的事实闭包；有行按字段；创建须覆盖墓碑；跨读保留单调下界。|
|Part 25 §§10、13–14 的 S7 与 Gate 正反控|同一张新票给滞后空、真实后续移除两种输入。给同一行旧 name 和合格 number，再分别控制角色范围。覆盖只及旧保护、不及已接纳新图像也必须拒绝。|

## 16. 权威设计后续同步位置

|文档|负责人裁决后同步的契约|
|---|---|
|[A 目标设计 §§6.5.2、9.2、13](SIM_Target_Architecture_Design_v1.2.md)|确认外部写、确认读取覆盖、业务事实发布三个不同的资格；逐视图/逐键接纳、后继处置与生产能力门禁。|
|[B 源码详细设计 §§4.2、6 T、7 A、10.1 Adapter、11 H](SIM_Multisim_Source_Architecture_Detailed_Design.md)|读请求/结果的证据与有效范围、Registry 的逐键处置、角色集一次性交付及精确回执、无证时的关闭收据。|
|[C 目录设计 §§4.1、4.3、5.3](SIM重构代码目录结构设计方案.md)|T 账目和投影保护归 account，A 的角色保护归 role_assignment；适配器只提供证据，H 汇合收据。沿用五组目录。|
|[D 迁移设计 §7.4 与 AF-1 验收](migration/SIM_Architecture_Refactor_Design.md#af1-acceptance)|在账户/角色读写 WU 与 G2c 列出同票空集两因、截断、独立字段、角色整组、退出旧写者与重建资格的切换断言。|

## 17. AF-2 最小实现切片（待授权）

1. **T/I，AF2-07(d)**：在 AccountStore 边界保留 legacyResult/legacyReturn，附实际读源描述、每列解码与遍历处置。先实现 Unknown 和受控 Host 有序路线，再为确有证据的产品路由启用 A 或 B。对所有当前/历史 setter、发现、移除和初始化写点核对保护范围，处理既有元数据缺失。
2. **T，账户发布与 AF2-02**：扩充 Registry 原成员/退役条目、投影和原票 W。BeginReload 签本地基线，AcceptReload 在同一提交域计算逐键差量与处置；AcceptReloadProjection 受约束的逐字段入口不能旁路。完整实体撤销继续经 BindingIndex。
3. **T/A，读源交付**：验证 role 与 account 的 s 对齐，区分整组许可与字段掩码；在原 AccountRolePlanningPort/AssignmentService 上增加带 L、不可变来源、一次性许可的接纳/回执。A 的原角色版本和 lastAttempt 仍在真实写点检查，撤回的批次不得覆盖已更新 setter。
4. **H，AF2-05/06**：保留 L 的在途、子投递和必要的补证工作，与 Part 24 的同轮稳定读取复用；对来源热切换、返回丢失、旧图保留及拒绝移交注入故障。验证不能靠“没有 Radio”判断排空。
5. **Host 与产品分开**：从正常发现/字段写入口产生真实事实；在 I 独立控制取样、读取可见进度、游标和返回顺序。读回两视图、局部版、A 角色 map/版本、r、m/j/F 与收据。产品取证还须覆盖匹配 Provider 版本、原写者和所有查询路由；不把合成前缀投入生产。

## 18. External OPEN 与内部边界

|类别、责任方|目前能够成立的结论|解锁条件|
|---|---|---|
|内部设计 / T、A、I|C 的缺证保留、无冲突字段、独立业务后继、门控整组交付均可通过现有 Owner 实施。P25-R1 的两条坏路径在该契约下被拒。|重过独立设计 Gate；产品能力不足时仍须执行安全分支。|
|IMPLEMENTATION_GAP / T、A、I、H|真实源码仍以 complete 传递整次可替换，缺逐键保护与完整角色交付回执。E3 仅验证接纳差异。|按 §17 获授权实现，有实际入口、故障及并发证据后再声明实现通过。|
|EXTERNAL_OPEN / 产品 Provider 与 I|A 路径的可见点、RYW/后续读取单调性、是否同一真实会话、C/H 及重连的缓存行为未证。B 的提交号/快照前缀也未在本仓发现。|指定版本和路由正式契约、匹配产品实测，或外部提供带世系/范围的可验证写读凭据；验证旧缓存与恢复分支。|
|EXTERNAL_OPEN / 业务 WU 接管负责人|若生产只靠此 Provider 的 Reload 接收他方变更，且无可信直接移除/更新输入，保留可阻止误写，但某次真正的后续变更可能永远无法反映。旧 GetList/GetAllList 的成功整组替换在这种情况下不能等价履行。|逐 WU 证明覆盖读或合法后继业务事件可达，并核实旧 writer 退出、行为差分和原错误/输出语义。此前保留原生产路径，不启用该写权切换。|
|EXTERNAL_OPEN / 宿主、Provider 与迁移方|未知来源连续性、无权威重建基线、来源失效无法有序送达时，不能从空保护表启动即视为同步。|安全移交或可信全量基线、可失效的数据源路由资格及匹配的关闭/重建证据。|

## 19. 重过独立 Gate 的条件

- 对完全相同的本地新票/结果形状分别提供滞后视图、覆盖已确认写且后来真实缺席的视图、来源能力未知。必须依次得到保留、合法删除、保留。C/H 只按各自读源处理，r 只随合法绑定操作改变。
- 新签票的旧名称不能让 New/1 变 Old/2。对独立合法号码能部分接纳；若号码也受保护，必须逐项取得其覆盖依据。新且可靠的 Later 可变第 2 版，后来仅见最初写的读不能退回。改变 j 后旧有行结果不得复活新存续段。
- 同一原票与同一 L 重放不再升版、再发 Store、再释放。库/用户/世系错、窗口内重连、游标非正常终止、身份或角色集合错配均不能取得对应写资格。独立业务移出可终止正向保护，负向依据能挡住随后旧行。
- 在 T 保留成员或角色证据不足时，A 不得整组清空或改回旧角色。只有无关 T 字段冲突时，给足成员和角色独立依据应允许 A 在本版资格下接纳。交付期间插入来源失效、成员变更或更新的角色 setter，须观察到正确的先后、原回执和在途交还。
- 从准备失败到 Stop 全程检查原事实、保护依据、业务见证、角色预留及逐项收据。静态保护不会仅因本次读取结束被删；没有实际待办时也不能永久制造关闭义务。无法证明生产活性/兼容性时保留 §18 外部门禁，不能以超时清保护交付“成功”。

**本轮结论为 `RELOAD_CAUSAL_COVERAGE_DESIGN_PROPOSED`。** 内部方案具备重新提交独立审查的条件。现有裁决仍为 `B1_REVIEW_BLOCKED`、`H3_REVIEW_BLOCKED`，AF-1 等待签收。

### 附录 · 本轮顺序探针的复现输入

从仓库根目录执行。所需 fixture 从本地 Part 25 的固定源提取，模型中的 10/11/12 只在合成域内有意义。

```sh
p25=telephony_core_service/docs/AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md
p26=telephony_core_service/docs/AF1_Part26_Reload_Causal_Coverage_Design_20261124.md
d=$(mktemp -d)
src=telephony_core_service/services/multisim
awk '/^```cpp$/{p=1;next} p && /^int main\(\)/{exit} p' "$p25" > "$d/fixture.h"
awk '/^```cpp$/{p=1;next} p && /^```$/{exit} p' "$p26" > "$d/probe.cpp"
g++ -std=c++17 -Wall -Wextra -pedantic -I"$src" "$d/probe.cpp" \
  "$src/account/account_registry.cpp" "$src/account/binding_index.cpp" \
  "$src/infrastructure/assembly/slot_catalog.cpp" -o "$d/probe"
"$d/probe"
```

实际探针 SHA-256 为 `50b13f1ed3c1d2d8ae71f9c49043f2372f4d09343092ce66e9e198a3c15396c3`。

```cpp
#include "fixture.h"
#include "role_assignment/account_role_protocol.h"

// Synthetic B route: only comparable certified prefixes in this fixture.
struct Sample { int route, generation, seen; bool complete, name, member, roles; };
bool covers(const Sample &s, int minimum, bool scope) {
    return scope && s.complete && s.route==1 && s.generation==7 && s.seen>=minimum;
}
int main() {
    for (bool gated : {false,true}) {
        Fixture f;
        AccountRoleProtocol a;
        assert(a.AcceptSingleProjection({Y,true,false,false,false},0,false));
        auto l=f.begin(C);
        auto av=a.Revision(false);
        assert(f.accounts.AcceptDisplayName(Y,C,0,u"New")==AccountAcceptance::Accepted);
        auto row=f.p;
        row.displayName=u"Old";
        row.displayNumber=u"42";
        auto r=f.accounts.AcceptReload(l.actual,true,{{Y,"Y",1,row,{}}});
        assert(r.acceptedAttempt && r.conflictedFields==1);
        Sample s{1,7,10,true,false,true,true};
        if (r.acceptedAttempt && (!gated || covers(s,11,s.roles)))
            assert(a.AcceptReloadSet({{Y,false,false,false,false}},l.actual.attempt,av,false));
        const auto p=f.accounts.ReadProjection(Y,C);
        assert(p->displayName==u"New" && p->displayNumber==u"42" && f.r()==1);
        assert(a.Projection(false).at(Y).voice==gated);
        std::cout << (gated?"gated":"baseline") << " conflicts=" << r.conflictedFields
                  << " number=42 voice=" << a.Projection(false).at(Y).voice << '\n';
    }
    for (int seen : {-1,10,12}) {
        Fixture f;
        assert(f.reaffirm(true,false));
        assert(f.accounts.AcceptDisplayName(Y,C,0,u"New")==AccountAcceptance::Accepted);
        auto l=f.begin(C);
        Sample s{1,7,seen,true,true,true,true};
        const bool replace=covers(s,11,s.member) && covers(s,11,s.name);
        // Only Y in this fixture. Keep the original ticket and real destructive branch.
        auto r=f.accounts.AcceptReload(l.actual,replace,{});
        assert(r.acceptedAttempt==replace && f.exists(C)==!replace && f.exists(H) && f.r()==1);
        std::cout << "read=" << seen << " C=" << f.exists(C) << " H=1 r=1\n";
    }
    for (int seen : {10,12}) {
        Fixture f;
        assert(f.accounts.AcceptDisplayName(Y,C,0,u"New")==AccountAcceptance::Accepted);
        auto l=f.begin(C);
        Sample s{1,7,seen,true,true,true,false};
        // PROPOSED per-field decision; exercise the real single-field writers.
        const auto fetchedName = seen==10 ? u"Old" : u"Later";
        if (covers(s,11,s.name))
            assert(f.accounts.AcceptDisplayName(Y,C,l.actual.expected.at(Y).fields.name,fetchedName)
                ==AccountAcceptance::Accepted);
        assert(f.accounts.AcceptNumberVisibility(Y,C,0,u"42")==AccountAcceptance::Accepted);
        auto p=f.accounts.ReadProjection(Y,C);
        assert(p->displayName==(seen==10?u"New":u"Later") && p->displayNumber==u"42");
        std::cout << "field read=" << seen << " nameRev=" << p->revisions.name << " number=42\n";
    }
    assert(!covers({1,8,99,true,true,true,true},11,true));
    assert(!covers({2,7,99,true,true,true,true},11,true));
    assert(!covers({1,7,99,false,true,true,true},11,true));
    assert(!covers({1,7,99,true,false,true,true},11,false));
    assert(!covers({1,7,11,true,true,true,true},12,true)); // prior accepted read raised floor
}
```
