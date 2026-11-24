# SIM 多卡 AF-1 核心架构签收包 · 2026-11-24

## 1. 签收对象与裁决入口

请架构负责人裁决下述 **PROPOSED** 模块私有契约、迁移次序与生产门禁。最小裁决单位是能独立关掉全部旧写口的一组业务方法。本包给出设计层的行为和失败出口。实际产品上的写权、可见性、卸载与跨实例复用各有单独的证据门。

|材料|用途|
|---|---|
|[唯一历史检查点](AF1_Architecture_Design_Convergence_20261123.md)、[Part 24](AF1_Part24_B1_P0_Design_Revision_20261123.md)、[Part 25](AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)、[Part 26](AF1_Part26_Reload_Causal_Coverage_Design_20261124.md)|保留每轮证据。解释顺序为本包定向裁定 → P26 读覆盖 → P25 资格 → P24 发布与关闭 → P23 原票。后件仅替代明确指出的旧规则。|
|[独立最终 Gate 记录](history/reviews/AF1_Final_Independent_Gate_20261124.md)|独立输入、反例、复核结果和最终可提交性。历史 Gate 的 SHA 和结论保留。|
|[权威文档候选差异](AF1_Authoritative_Docs_Candidate_20261124.patch)、[AF-2 交接](AF1_AF2_Implementation_Handoff_20261124.md)|前者供逐篇审阅，后者给出获准后可实施的切片、用例和否决条件。|

## 2. 固定基线与证据范围

当前工作树 main，HEAD 与查询所得 origin/main 为 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`。仓库根为 multisim，目标目录 services/multisim 最近源码提交 `632f6a2a888cfbbc3c9387edc89732364e12a10e`，树对象 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。Legacy 与文档以 HEAD 加实际本地文件为准。已核对 worktree、remote、两区 diff、部件 AGENTS、执行状态及 README。签收工作开始时暂存区和跟踪文件无修改。八个既有未跟踪文件包括 default.profraw、opencode.json、P24～26 和三份独立 Gate 历史报告。本文路径如无前缀，源码相对 services/multisim。

|实际审查输入|SHA-256 或 Git 锚点|进入本轮时的结论|
|---|---|---|
|检查点 §§15.3、16～23|0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2；Part 22/23 在 d9c2d23dc|H1/H2 PARTIAL；旧 H3 Gate 阻止进入当时的 H4|
|[Part 23 独审](history/reviews/AF1_Gate_B1_Independent_Review_20261123.md)|147d25d703419225f31937007c01c73d788f37f71a34a45897cb6612b3b7adf4|NEEDS_REVISION|
|P24 / [独审](history/reviews/AF1_Part24_Independent_Gate_Review_20261123.md)|a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164 / f55cb963c6118dbeeafda037900c260746d570f0c2e7ad44687714f0c78ec313|BLOCKED，旧缺席快照可删新确认|
|P25 / [独审](history/reviews/AF1_Part25_Independent_Gate_Review_20261124.md)|4ff376e7adfae7ef1f475ebc8cb411030293c8c1a47bedc6780ea1289f753418 / 58940ed346b660d36d58c6519a2008fdc93036c569514aeb182d13aa9eb9218e|BLOCKED，本地新票可读到旧图像|
|P26|946d7f954351b59607d3e540ef42c776a706ff46e1bf5cf918e900b49c781ed8|读源覆盖设计 PROPOSED。进入本轮时没有其独立放行记录|

## 3. 架构与依赖

兼容入口 C 按旧调用节点投给 T 或 A。T 接收 R·card 的观察及 I 的持久效果；A 在 T 的只读目标资格下作决策，向 R·device 请求真实计划和执行。O 在两域各自上下文登记步骤和完成，不持第二套业务投影。H 装配窄端口。运行调用可用双向的请求/证据回传，编译时实现依赖消费方所需的契约，不能借反调取得对方的容器或写指针。旧读投影允许缓存，冲突时按原视图、源、代次和 Owner 的接纳见证裁决。五组一级目录分别为 account、role_assignment、card、device、infrastructure。[检查点 §§2、8、16～17](AF1_Architecture_Design_Convergence_20261123.md)

## 4. 责任和写权限

|对象|可修改的事实及写点|提供的能力与关闭责任|
|---|---|---|
|T TopologyService|账户身份、两读取视图和逐字段版本、端点绑定/来源资格、原发现票和本地接纳见证|短域提供目标资格；收回 Store 调用、来源事件、Reload 和送 A 的回执|
|A AssignmentService|角色语义/写集、计划与 Reservation、角色投影及版本、逐键业务效果接纳|裁决 Primary/Data/Voice/SMS，冻结 R 成员计划，负责参数、协议结果的业务接纳及原完成|
|O RuntimeOperationStore/Scheduler|原 Request、Step、派发/领取/返回、一次完成与重试代次|按 epoch/请求投给原 Owner；已领取未返继续列项|
|R·card / R·device|可信 Session、观察次序；原生 Radio 身份/成员/路由、租约及设备效果|给 T 来源证据、给 A 原步证据；保留未投递/未交还、隔离与复用义务|
|C 兼容边界|方法级参数转换、旧同步返回/out/通知映射|保持既有方法的完成时点与权限；业务变更经相应 Owner|
|I Store、平台、参数适配|本次调用的实际返回、已证实的行列/采样来源|只附可核验的原操作或读图像证据；在途体交还，不能替 T/A 裁决|
|H SimModule / assembly|实例装配、公开入口、关闭轮与汇合|只绑定端口与汇合各 Owner 的逐项确认；不能清票、改角色、宣布资源可复用或停共享 RIL|

## 5. 业务对象与身份

|业务关系|身份与基数|事实边界|
|---|---|---|
|Account 关联 Subscription，AccountRecord 持 Current/Historical 投影|已核实订阅 s、ICCID 与身份代次 i；可能仅一个视图存在；订阅使用既有不透明标识|移出视图只能改变该视图。最后投影消失仍按 tombstone/活绑定保存判断所需事实。数据库中的位置不能证明实体仍在槽上。|
|Binding 引用订阅与逻辑端点|实例、slot、s、位置版本 r 和已验证原来源；同一有效关系幂等；目录兼容 slot 可重用|物理卡、Port、eUICC、Profile 与 vSIM 端点分别可观察；同卡跨设备观察不自动成为第二当前绑定。产品未认定的多绑定基数留在能力门。|
|Role → Assignment → 目标|指向合格订阅；slot 输出由当前关系投影。AssignmentView 有自己的来源和版本|Primary 与 Data 可合用偏好/参数键；按真实写集冲突。不同 RadioDomainId 仍可能共享一个物理资源。|
|CardApplication、LoadSession、ProtocolSession|R·card 私有应用代次/加载期；Radio 原生轮、真实成员、传输标识另属 R·device|卡侧通道生命周期不等于协议轮或账户寿命；不得把 SlotId 构造为 SessionRef。|
|RuntimeOperation → Step → 传输与 ResourceLease|O 组合持有请求步骤，StepRef 引用原目标；R 把平台 token 与原步映射；租约标出完整 ResourceKey 集合|完成回调、设备效果、资源可复用分别确认；一次派发成功不终结任何尚未证明的设备效果。|

## 6. 创建、持有、失效和替换

H 构造一套 Owner 和屏障，通过就绪的依赖口发布组件。运行期新票按 epoch 签发，不向外泄露 registry 引用。R 持会话与源事件的执行寿命，T 持关系见证，A 持预留，O 持请求机制，R 持物理租约；读取和借用均不转移责任。所有可能绕回旧图的入口有独立寿命门。新的装配要么使用已证明同物理域同一权威，要么先证旧图完全排空。在无法证明覆盖**所有**同域新入口时，H 禁止发布该能力。通信共享失联、Profile/vSIM 启停按来源与作用域单独核验，只有可信替换才撤原关系。未知时保留关系与历史义务，暂停依赖它的操作。

## 7. 核心私有接口与排序点

|现行调用点|候选契约、失败与回收|
|---|---|
|TopologyService::DiscoverAccount / ReconcileDiscovery；DiscoveryObservationLedger::CompleteStored / ReconcileStored|在第一次外部发送前，T 建独立 t=(epoch,admission)，占用按 ICCID/身份核冲突，冻结 slot/app/来源序号、期望 r、原计划、列、A 原 token。位置索引只管最新观察。call=(t,原写) 只发一次。领取/重入按 claim；旧证据按原 call 收回，不能清其他票。|
|AccountRolePlanningPort::Prepare / AcceptDiscoveredRoleProjection / FinalizeInitialRoleReservation|A 以 t/token/原 plan 固定解析身份；T 以真实 W 或据证历史终止传处置。释放、派发确认和对端收到分别登记。原计划不符、部分效果或未知留原项；不存在 token 不能充当释放确认。精确确认可重取至执行体全部交还并双方确认清理。|
|AccountRegistry::BeginReload / AcceptReload / AcceptReloadProjection；BindingIndex::AcceptBinding|Begin 锁内签 L=(epoch,v,attempt,w,expected[s/i/F])；实际采样在签票后。原发现的 T 发布点和 reload 逐键判定见 §9。暂存失败只回到原票；副作用未知不得重放。|
|TopologyService::PrepareAccountReload / AcceptAccountReload → AccountRoleProtocol::AcceptReloadSet|I 返回原 L、路由世系、完整遍历/正常终止、有效身份和列及覆盖；T 返回逐项处置与独立批次交付资格，A 用原批次作整组裁决并回执。|
|PrimaryTopologyPort、StartPrimarySwitch、AcceptRadioStep、AcceptC1RetryDue|A 短域内取得 T 的词法只读资格，原目标 active 修订、Binding/source、域写集均合格才**登记一个不能重领的效果意图或提交本地角色**。此点与 T 撤权有序；不可把可缓存快照当授权。外调前释放短锁，已经登记的一次原意图按历史效果追踪。R 仍在真实发送处重验冻结路由和租约；不把预登记算作实际发令。|
|StopAcceptingAndDrain、OwnerCloseReceipt|按同一个 q 与参加者集合读取所欠具体项/持有方、活动版本、输入门状态、在途和接手确认；反复 Stop 只推进原轮核对，实际调用退出另计。|

## 8. 四条端到端信息流及其失败出口

|流|实际对象与候选阶段|正例|反例、恢复与关闭|
|---|---|---|---|
|1 发现 → 绑定|CardRuntime/SessionRegistry → 来源短登记 → T 独立 t → A 原预留 → I Store → 原效果 → A 固定身份 → T 本地发布 W → A 精确释放 → T 收回|Y 源持续有效、完整原写已证，FindBinding 返回正确订阅；同一 t 只发布一次|X/Y 同位置双 Unknown，各有 t/a/call；可信替换后 X 只结历史，Y 仍用自己的证据。T 已建但 A 答复丢失，只重取回执；Stop 枚举两票和仍未返执行。|
|2 Reload → T → A|L 冻结 v/w/i/F，AccountStoreAdapter 绑定原查询，T 在发布域逐项消费，A 按原 L 的一次性资格在自己域替换整组|号码无冲突可更新，名称保留；具备独立成员与角色依据时仍可更新角色组|缺完整性不替换，旧图像不能清空成员；A 可拒旧角色而 T 已成功更新号码。失答复取原 L 处置，T/A 在途项留至交还。|
|3 角色 → Radio → 原结果|A 的写集、T 目标、O StepRef、R 真实成员/资源计划。C0 逐键完成偏好、Primary、Data、通知与完成。B 一次专用异步发令，确认后依原资格写参数。C1 预写，CHECK→UPDATE 的成员 NOTIFY→COMPLETE→最终参数、输出|只有原步骤、轮、成员与允许的证据种类都匹配才推进；同一真实键组同一租约，独立键可并发|换绑/失活后已证设备仍属 X；错轮 ACK 不代替 NOTIFY；不产生 Y 的新角色结果。可恢复参数只在原键未被后来写覆盖且仍有资格时执行；不能证明外部写者顺序则留项。换绑/Stop 后 COMPLETE 的实际许可由协议证据决定，缺证保留会话隔离。|
|4 Stop → 稳定收据|H 一次 Running→Closing(q) → C/R/adapter/Scheduler 根入口封口 → T/A/O/R/投递器逐项收据 → 完整两遍比较 → 排空或保图|新实例复用同物理域须获旧 Lease 终结/有效移交和唯一权威证据。回调内 Stop 可报告未排空，不能等自身|R 已接纳撤销未交 T、Store 未返、A 确认未返、Completion 已领取未返、Radio Unknown 均有原父项。新 Owner 确认登记后且旧执行退出才清父；一次瞬时全空不够。|

Flow-4 的充分性依赖每个可派生工作的生产者都已注册并在同一 q 封口。所有第一遍采样完成后才开始第二遍，两遍逐 Owner 的版本/封口/明细相同，版本不回绕；共同截面上必须既无本地执行，又无未结或未确认移交项。此后不能无父项生成内部工作。发现遗漏生产者、计数耗尽、路由失效、无驻留接手者即保图或拒绝启用。运行中的原 C++ 栈不能用纸面移交跳过。证据见 P24 §6、检查点 §21.4。

## 9. Part 23～26 的合并裁定

**写入归因。** 原调用已返回、某行存在、原写逐列已生效是三件事。仅原执行端认证完成或能排除其他写者的等效证据，才提升相应列的认知；实际订阅身份也须核实。另一 writer 写出完全相同的行与本次成功在 Match 下不可区分。混合/矛盾/暂不可归因保持原 t、a 及物理键责任，不重发。只有原写未发生可证、历史资格终止可证或当前 W 已存在时才能按对应处置收尾。没有要求全局日志。[P24 §§4、7](AF1_Part24_B1_P0_Design_Revision_20261123.md)

**统一发布。** T 域内预备 AccountRecord、两个视图的投影与保护、版本/墓碑、Binding/来源、原票 W 及下一步。检查所有版本容量；预先构造字符串、返回值、稳定节点和有界撤销清单。任一准备失败，只撤本次私有节点，保留 t 与 a。执行尾段只用现成迭代器、已证 noexcept 的赋值/交换或预构造指针交换，不分配、不外调、无可失败出口；读者、M19、装配及撤权共用 T 域。W 和票阶段随事实同次可见。中止进程不提供此内存协议的持久保证，新实例须重建可信基线。用 noexcept 检查和逐位置故障注入否决无法保证该性质的具体实现。[P24 §5](AF1_Part24_B1_P0_Design_Revision_20261123.md)

**同票、新票与四种刻度。** W 绑定 t/call、s/i、原源/轮、视图与被接纳字段、m/j/F、原和结果 r、A plan 及处置。重放读取原 W；新 t 仅能在合格 v 对同一成员重新确认一次。m[v,s] 标记成员事件，j[v,s] 标记本段加入，F[v,s,k] 标记各字段，r[slot] 标记真实关系。共用签发时钟仅给每个键独立取值。值未改变且只确认成员时字段不升版。恢复同 ICCID 的新绑定须获新关系资格。见证回收须全部交还且保证 epoch/票号不复用；墓碑、字段退役版本和防回退依据独立存续。[P25 §§3～9](AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md)

**Reload 两项谓词。** 对可替换完整谓词的缺席 s，须属于 L.expected、当前同 s/i 和投影、原字段资格匹配、m[v,s] ≤ L.w，且 I 对该谓词/视图的实际图像证明覆盖成员及将被删除的受保护字段。对 L 中存在的行则按同身份和连续投影段 j ≤ w，逐 k 验 F 和本键覆盖；无冲突号码可前进。新增行另验身份、负向墓碑、来源与准入。只对已证无须保护的事实允许使用原合格读源。真实后继的受权操作可独立证明成员终止或字段变更，需同域排序，留下新的下界。删除 DB 视图成员只推进该视图 m；撤销真实 Binding 另验原来源与 r。读取不能借 tombstone 或合法角色恢复旧资格。[P26 §§7～10](AF1_Part26_Reload_Causal_Coverage_Design_20261124.md)

**I 的证明边界。** I 把用户/库、实际查询/会话世系、期间重连、view、行列范围、遍历终止及实际可见前缀绑定到原 L。可接受的实现是认证路线的写可见点+后续读取单调性，或同一数据源可比的提交与采样可见前缀；两者都要覆盖每项前驱。仅 RYW 不能证明后读包含已经接纳的外部读。错误、截断、混世系或不明游标不替换；同一认证位置内容矛盾隔离。无证保留，重新签票、两次同值、时间和 complete 都不能升格。可吸收后继要求、保留不可比要求；缺可信重建基线时不把空表解释为没有保护。源见 P26 §4 的 AccountStoreAdapter → LegacyAccountStoreProvider → SimRdbHelper/DataShare 的两个查询，未见能签发上述凭证的产品契约。

**同批角色接纳的两个额外约束。**

1. T 在发布域比较 L 的预期键、当前键、原样返回行键、角色行键，及其并集内的正负成员与身份依据。对每个差异须证明是本次允许的合法新增/移除；当 Z 在签票后加入而两组返回都只有 Y，Z 的 m 越过 w，整组交付失败，即使两个向量逐键相等。A 再独立核对本视图全部会被替换或隐式清除的角色键与受保护负项、角色列、原版本、在途写和原 attempt。T 的无关字段冲突不构成组拒绝的理由，也不能使组通过。T 在发布尾段/来源失效登记点撤旧资格；A 在本域备妥后用共享的一次性资格作条件交换，领取成功后只执行无抛发布；A 的读者与 setter 同域。双向外调不在锁内。
2. **发现合写与 A 释放**：A.Prepare 在实际外部发送前将计划中 includeInStorage 的物理角色键、所影响的集合/视图标在途；s 未分配则用 plan 的未解析范围（不能证明不相交时挡住全相关组）。原 call 的已证列随精确处置输入 A；A 按其真实影响，把操作/行列/集合及正负因果下界安装进 A 自己的接纳元数据，再消除在途占用，连同释放回执在同一无抛尾段发布。仅为覆盖下界发生变化也须撤销相冲突的旧批次资格，让 A 提交点再次检查。该动作不发布业务角色值、不通知、不从 T 值生成角色。确证未写的列可不建前驱；混合/未知未合法处置则保留占用，历史处置也保留确已写列的下界。后来的 Reload 只有能覆盖它们时才可发布原角色值。释放答复遗失仍按原 token 取同一确认；相同票不第二次升版。

## 10. SC-01～03 一致性

|决定|设计验收点|直接源码反证或边界|
|---|---|---|
|SC-01 完整效果与关闭|每个参数键、Store、设备、计划恢复、调度、通知、Completion 各自登记认领与返回。H 的两遍收据还须有来源封口与旧图所有权。|PrimaryHostLifecycle::StopAcceptingAndDrain 只汇合 Primary 子图；SimModule 声明不证明全图可关闭。检查点 §§15.5、21.4。|
|SC-02 原生身份、参与者与资源|R 提供成员/路由版本，A 冻结，O 以专用发送登记保持原 RequestId/StepRef，R 保持真实 session/token。ConfirmedNotSent 只在确证未发时使用；HandoffAccepted 继续等结果；HandoffUnknown 隔离，不重发。|assignment_service.cpp:132、339、563 与 device/resource/resource_barrier.cpp:39–100。按整键集预备节点并无抛统一占用，分配失败不得留下部分租约；资源独立性缺证则保守共用域。|
|SC-03 完整写权|偏好、Primary、Data、Voice、SMS 和独立 Profile 提示按实际物理键和作用域入 A 写集。父资格可证明包含该键才复用。每一外部效果先记账；确认未写才释放对应未生效义务。|assignment_service.cpp:324、659、662 的路径需改。独立提示遇冲突先不发；产品不能保持同步旧语义则整组留旧写者。|

## 11. H1～H4 的处置

|阶段|继承的结果|本包给架构负责人的依据|
|---|---|---|
|H1 事实所有权与依赖|历史 §16 及 §15.3 的决定/提案分界|§§3～4、10 明确唯一写点，允许有来源的读投影。|
|H2 对象完整性|§17～19 识别 B1/M1/M2/N1。当时状态 AF1_HARDENING_H1_H2_PARTIAL|§§5～7、9、12 区分身份存在、资格、已有效果和未了义务。|
|H3 契约|Part 22 的 H3_REVIEW_BLOCKED 及 Part 23 的原反例均保留|§§7～10 构成替代提交给本次独立 Gate 的契约。既往 H3 没有追记 PASS。|
|H4 数据流|旧 Gate 当时阻止开展 H4|本轮 §8 按最终任务重建四条数据流；§§12～13 覆盖 C0、B/C1 与跨流交错，证据分级。早期 H5/H6 未单独出具历史结论；本轮独立 Gate 含并发、寿命与文档统一。|

## 12. M1、M2、N1 的最终提请裁定

|项|正向资格、撤销与线性化|失效后仍由谁负责|
|---|---|---|
|M1|R 按原 epoch/端点/应用/代际/连续序号登记受影响范围。在事件可进入队列前 T 短登记来源待判；监视失去连续性不能伪造关系已终止。仅应用刷新保留关系；唯一依据暂失则暂停依赖它的操作。目标资格含**连续性代次**，中断即使同 r、同 active 值后来恢复也不能授权中断前的下一步骤。可信载体移除/替换按原绑定撤销；同 ICCID 再插签新 r，旧 seq 不撤新卡。|R 保留投递与卡会话，T 保留待判/原绑定来源，A 追踪已授权效果。读新关系用新资格；来源中断与协议终止分别取证。见 card/runtime/card_runtime.cpp:34–54、topology_service.cpp:46、242。|
|M2|C0 每键及每个出口、B 发单与角色提交、C1 预写/每轮每成员 CHECK/UPDATE/COMPLETE、最终 Data 及 retry **分别**验原目标/active 版本、来源连续、A 写集与本步资格。用 §7 的 A→T 短读登记，T 不持锁投给 A；R 发令另验资源与真实路由。排队时尚未获得不可撤销发送权的命令须在授予时复验，已授的原目标发送权最多一次，列在途。重试需旧轮可结束、预算合格、新原生计划和原目标连续。|A 保留预写/恢复/通知/完成收据，R 保留逐成员 NOTIFY/原会话和租约，O 留原请求。换绑或 Stop 后 COMPLETE 仅能据真实协议决定；两成员一交接不明不能据另一成员 ACK 解隔离。目标失活但 r 未变仍不提交当前成功。|
|N1|SetDefaultRole 真正成功接纳在 A 同一域提交 aggregate、roleRevision、Current 投影版本及写入依据。Historical 按自己读源/版本。原写失败但效果未知时登记原键，用同票核对，旧 Reload 不能覆盖。|A 保留原 setter、写集和未归因列；T 仅传原批次。源码 assignment_service.cpp:324–336、687–697 与 account_role_protocol.h:68–97；现具体适配器失败路径不能替成功分支作验收。|

## 13. 可复核反例与证据等级

E1 为源码/设计推导，E2 为独立有限模型，E3 为链接真实核心类的仓外探针。E4 全链 Target Host、E5 并发压力/ASan/TSan/匹配设备，本轮均 NOT_RUN。复现入口为 [Gate25 附录](history/reviews/AF1_Part25_Independent_Gate_Review_20261124.md) 和 [P26 附录](AF1_Part26_Reload_Causal_Coverage_Design_20261124.md) 中的 sh 块，从本仓根目录执行；它们在仓外重新编译。以下保留当前实际负控，设计推演没有充作实现通过。

|#|输入截面|应观察及证据|
|---|---|---|
|1|同位置 X/Y 两票交错，两写 Unknown|逐票效果、两份预留、关闭逐项；有归因只推进本票。E1 P23 §23.4，P24 §11。|
|2|原写已证，Binding 未接纳|保留 t/待业务接纳，不能 finalize 为当前成功；同源仍可续。E1 P23 §23.3，topology_service.cpp:188 的早结反例。|
|3|投影/成员节点/W 准备中失败|所有对外读者见旧组，原票可续；尾段只允许暂停，A 释放失败留下已发布 W。E1 P25 §9，实际故障断言留 AF-2。|
|4|同 t 重放/新 t 只确认 Current|mC 仅新票前进一次，mH/F/r 不随动。P25 顺序 E3，资格/W 为仓外模型。|
|5|旧缺席读取后新票确认 Y|本轮 E3：source C=0 H=1 r=1；proposal C=1 H=1 r=1。独立 E2 两视图 792 cases，deletes=936、retained=384、violations=0。|
|6|新 L 读到旧空集|Gate25 E3 未加覆盖：db=11/read=10 C=0，期望 1。P26 E3 read=-1/10 均 C=1 H=1 r=1。|
|7|新票返回旧名称、无冲突号码 42|Gate25 E3 旧规则 nameRev=2、keptNew=0。P26 E3 read=10 时 nameRev=1、number=42；权威后继 read=12 时 nameRev=2、number=42。|
|8|T 局部拒绝，A 收到同时陈旧的角色组|P26 E3 baseline conflicts=1 number=42 voice=0；候选门控 conflicts=1 number=42 voice=1。原样批次全键域、A 已释放后仍须覆盖角色前驱另见 §9，E1。|
|9|同形空集，后来真正移出 Current|P26 E3 read=12 时 C=0 H=1 r=1；独立可信拔卡另验原关系才升 r。图像 10/11/12 由测试输入签发。|
|10|R 已接纳未投递、Store 正执行、A 已释放未确认、H 已采第一遍|E1 §8；[P24 独审 §5](history/reviews/AF1_Part24_Independent_Gate_Review_20261123.md) 既有三 Owner 有限枚举不含真实 C++ 内存/产品注销。双遍需全部生产者封口。|
|11|来源暂失→恢复；拔卡→同 ICCID 再插|r 不变但连续性中断拒旧步 / r 两次结构变化拒旧移除。E1 检查点 §21.2。|
|12|X C1 预写，拔 X、插 Y、X 旧结果，Y Store Unknown，Stop 后 Y 得凭据|旧 X 只能核对自身参数和设备；Y 仅在原观察仍受监视且全部资格成立时可提交 W，否则保留或据证结历史。H 保留两者原项。E1 检查点 §§20.4、21.3 与 P24 §6。|
|13|A 成功 setter 后旧 Current 与独立 Historical Reload 返回|前者版本落后，后者按自身资格可接纳；setter 结果 Unknown 不释放键。E1 检查点 §21.4。|

以上两份仓外脚本本次均 exit 0。E3 涉及 AccountRegistry、BindingIndex、SlotCatalog、P26 另含 AccountRoleProtocol；覆盖证明、路由和票据控制由 fixture 提供。实际产品 Provider 完整性、真实 CAS/关闭轮及全图未由这些输出证明。

**独立挑战引出的两项执行约束。** 即使 L 签在重建后，同 s/ICCID 也可能在滞后副本命中上一次存续期。逐字段许可须能把**实际采样行**绑定到获准的身份/当前投影段，或由认证路线的同域可见前缀证明已经跨过最后一次退役和重建；直接可归因的后继字段输入亦须匹配当前身份。j≤w 只约束本地签票到接纳期间。暂缺此共同身份证明则该行的所有依赖列均留存；只缺名称专属覆盖且身份有证时，号码可以独立通过。对不携稳定行世系的 Provider 这是明确的切换门。

R 在放开自己的锁调用平台**之前**，为冻结轮与每个成员/Step 换取唯一 in-submit 权利，预存回执位置、路由与整组租约；该步重复进入仅查原状态。同轮 Q 已确认未发时，若 P 已入 Submit 尚未返，R 不撤整组、不删 token 映射、不重发 P。整组确证从未交手必须同时证明全部成员已封发送、所有原调用退出、全部原交接均确证未发；任何已交接/未知者按其效果证据确认协议终止和可复用。可重入同步回包按原步先登记，不能以它替代执行体退出。资源预留全键与此逐成员登记均须有可恢复的准备期及无抛发布尾段。设备运行期负责，A 与 H 不能替 R 清项。源码 device/resource/resource_radio_runtime.cpp:90–131、resource_barrier.cpp:39–100。

## 14. 四篇文档的统一候选基线

|文档|唯一维护内容|候选落点与适用方式|
|---|---|---|
|A 主设计|事实、业务身份、成功边界、覆盖、条件决策|§§3～4、6、9、12～14；af1-final-owners/reload/discovery/target 和关闭节。C/Historical 术语分开。|
|B 源码详细设计|实现处、端口、发布/排序及寿命|§§5、7、10～11；af1-final-ports/topology/roles、af1-sim-module。按原票实施，不能以当前 bool 重命名充当证据。|
|C 目录|五组目录、窄依赖和唯一装配图|af1-final-account-dir、sc03-role-dir、sc01-assembly-dir。目录不另立业务规则。|
|D 迁移|切片、写权切换与验收|af1-final-cutover 与 §7.4；逐 WU 核实外部能力、关闭旧出口和产品证据。|

候选差异见 §1。README 的文档维护分工与 A §3 术语仍适用；执行状态继续区分 Host 与产品。签后才可按裁决更新索引/执行状态及相应权威节。历史摘要不回填；AF1-19 只能按获批工具规则新增比较输入。

## 15. 全量遗留事项台账

以下分类可重叠。DESIGN_CLOSED 表示本候选已经给出可实施的安全/失败出口；负责人批准、代码达到该契约、外部证明均单独登记。独立审查中发现的未解反例以 §1 链接的最终报告为准。

|遗留项|内部处置与边界|分类 / 后续责任|
|---|---|---|
|B1、P23～26、H1/H2 与 H3/H4|§§7～13 给出统一判定；过往 BLOCKED 作为原始反例。T/A/R 实现仍保留旧失败路径|候选 DESIGN_CLOSED；独立 Gate 裁断，负责人 ARCHITECT_DECISION_REQUIRED；AF-2 IMPLEMENTATION_GAP|
|M1/M2/N1、SC-01～03|来源和角色分阶段排序见 §§10、12，COMPLETE、恢复及参数真实效果缺证安全保留|候选 DESIGN_CLOSED；产品 EXTERNAL_CAPABILITY_OPEN；AF-2/3 IMPLEMENTATION_GAP、TEST_GAP|
|AF1-01、04、18 资源域与跨实例|物理键唯一准入、原子成组、无证拒绝新图；需宿主保证唯一或排他停旧|DESIGN_CLOSED；负责人择部署手段，R/H 实现与外部验证|
|AF1-02、07、14、16 装配与效果|发布前全端口就绪、能力切片、逐项关闭、未知保图；缺实现端口不构造能力|DESIGN_CLOSED；H/R/A IMPLEMENTATION_GAP|
|AF1-03、06、09 Radio/重试|平台原生身份，不新增公共 ID；限定 C1 重试原轮/代际|DESIGN_CLOSED；R/A 实现/真实路由验证|
|AF1-05、13 偏好|同键共准入；未决产品语义的整组留旧写口。候选默认暂缓切换，若后续决定延后写须证明原同步语义|DESIGN_CLOSED 的安全分支；产品与架构负责人裁决，禁止旁路|
|AF1-08 会话/加载容器|候选以 CardRuntime 为现阶段 R·card 的组合根，registry/load 只附于专属卡应用代次；未来内部拆分须保持同一寿命/唯一写口|ARCHITECT_DECISION_REQUIRED（维持现承载或等价私有组件）；不阻塞 Host 接口定义|
|AF1-10、11|Port/Profile 基数由匹配产品证据驱动；SimModulePublication 仅指发布能力|前者 EXTERNAL_CAPABILITY_OPEN；后者 DESIGN_CLOSED|
|AF1-12、15、17|A→T 短读以外避免业务跨域持锁；慢 I/O、平台提交与回调锁外；R 查询投递亦短登记后锁外|DESIGN_CLOSED；T/A/R IMPLEMENTATION_GAP|
|AF1-19|历史文档摘要绑定旧字节。保存失败与输入差异，测试负责人复核新增合法基线；无授权不放宽|TEST_GAP / 工具责任方；AF2-08|
|A §21.1 TGT-OPEN-001～006|001 跨 SA 任务、003 厂商 ABI、004 持久原子性、006 身份恢复均保既有协议并设产品门；002 窄协议、005 不上事件溯源|001/003/004/006 EXTERNAL_CAPABILITY_OPEN；002/005 DESIGN_CLOSED 决策建议|
|A §21.1 TGT-OPEN-007～013|007 T 并发见 §9；008 成功范围、009 通知按原出口；010 Linux 真闭包；011 排队目标时点按原方法；012 组合读分源；013 源码组件图已核|007/009/011/012 架构裁决与逐 WU 测试；008/009/011 外部增强另证；010 TEST_GAP；013 源码 DESIGN_CLOSED、设备 OPEN|
|A §21.2～及 B/C/D 产品余项，O1～O6|外部写者/组合读、通信共享失联、eSIM/vSIM 互斥与远端任务、卡侧域与 SO/SA 边界|外部责任见 §16；新能力默认关闭，原 WU 不切换。B/C/D 按 A 的业务约束做映射和验收，不创第二决策|

**DESIGN_OPEN 的判定口径。** 当前候选没有把产品“不知如何判断”交给实现者自行决定：若无法归因、证明新鲜、确定协议收尾或封闭来源，分别输出 Unknown、逐键保留、Quarantined、未排空。独立 Gate 若对这些内部契约给出可达反例，必须列为 DESIGN_OPEN，只有修订并由独立审查者重验后才能移出。

## 16. 外部产品门禁

|责任方与缺少的能力|缺证时的运行方式|阻止的里程碑 / 可执行的解除条件|
|---|---|---|
|Provider/I：逐原调用、行身份、合写列、否定/混合效果归因|留原票/预留和已知前缀，不推测或重发|阻止发现写权切换；同值其他 writer 与原写成功能凭原执行证据区分，且覆盖失败/迟到/混合。|
|Provider/数据平台：C/H 两条实际查询的可见点、完整遍历、行世系、复本/缓存/重连|保留受保护键和未决组；未证明源身份的行不更新，允许独立后继业务入口|阻止只靠 Reload 获知外部变化的 WU 切换；对真实后来移除与滞后空集、旧名/合法号、同号重建、重连降级做产品差分，证明活性。|
|宿主/平台：根生产者注销、已排队事件、回调退出、跨实例唯一屏障|未证保留旧图；不开放可能并行访问同物理键的新实例|阻止热重建、卸载；在旧调用悬挂时起新图实测拒绝，逐入口 fence、执行数与逐项接手均可核对。|
|协议/厂商/RIL：真实成员、session、路由、完成/重试/晚到及可复用条件|隔离原键，禁止猜测 COMPLETE 或后继发令；未证独立域保守串行|阻止 B/C1 及共享设备写权切换；匹配设备分别交付 ACK、NOTIFY、错轮、超时迟到和换绑/Stop 原协议收尾证据。|
|SIM/API/eSIM/参数服务及全部 writer|同键冲突停留在唯一旧写者；疑似被新写覆盖的快照不回滚|阻止偏好与跨入口写权切换；比对物理键、旧返回/通知、前后台顺序，确认旁路已关闭。|
|产品、卡厂与远端服务|未知 Profile/Port 数量、vSIM 账户等级、共享来源连续性均限制在已支持的能力|阻止新基数、跨 SA 可靠交付等增强；匹配产品采集启停、重连、重建、解绑、远端任务死亡与恢复轨迹。|
|构建/SA/模块/产品发布方|保留原部署/单写路径；Host 仅作合约验证|阻止产品接线、SO 卸载和正式切换；匹配版本构建/符号导出/实际加载，设备验证分支与失效、保留切回唯一 writer 的证明。|

## 17. 实现与运行差距

T 的按位置 pending、Reconcile 不真正接纳 Binding、Registry 的字段判定和 A 的整组替换均可沿实际入口观察。R 屏障与模块装配、A 持锁外调和 N1 各有具体落差。AF-2 必须以实际入口在 Host 验收，AF-3 做并发、故障、sanitizer、真实调度/关闭与外部门禁。现在能立即复现的是 §13 的 E2/E3；产品供证、跨线程取交付资格、无抛故障注入和实例重叠仍有 TEST_GAP。详情按 [实施交接](AF1_AF2_Implementation_Handoff_20261124.md) 逐项定位。

## 18. 架构负责人签收条件与选择

1. 查阅独立报告，确认审查的主包、候选差异、交接与当前来源相同。若有未解内部反例，退回其精确写点和验收断言；不能以材料齐全代替独立裁决。
2. 就历史检查点 §20.1 的 B1 方案 A/B（推荐独立发现票驻 T、Ledger 只作观察）和 §15.3 的待裁决契约，以及一次性本地发布、读覆盖、角色精确释放、资源权威和逐项 M19 作明确选择。推荐本包的 T 原票方案、Owner 私有保护和有证才替换。若不同意，应记录受影响的接口及重新复核范围。
3. 明确接受缺外部活性时**限制生产切换**、缺设备结束证据时保留隔离、无驻留接手时保留旧图的成本。主设计状态与签字、日期、接受的输入版本由负责人登记。本包不含该签字。

## 19. AF-2 入口

满足独立 Gate 和负责人书面批准后，按交接文档的有向依赖开展 Host 核心框架。先实现身份/收据和唯一资源准入，再建立私有 T/A 发布与投递，随后接入真实 Primary 步骤与完整 Stop。C 保持旧出口，I 的未知能力默认关闭。每切一组 writer 另须满足 §16 相应产品证据，未满足的切片保持原部署。发布候选补丁、更新执行状态或开始实现均须另按批准范围办理。
