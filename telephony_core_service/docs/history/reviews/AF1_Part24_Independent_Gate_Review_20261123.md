# AF-1 Part 24 三项 P0 独立 Gate 复核

**结论：`B1_REVIEW_BLOCKED`。** 阻塞点在 P0-2。新票证明了一次新的外部写，并据此再次确认 Y 属于 Current；若内存中的 Y 原本就具有相同字段和 Binding，Part 24 允许成员版本不动。此前已经读到“缺 Y”的旧 Current reload 仍满足删除条件，能在新见证发布后清掉 Y 的 Current 投影。此时 FindBinding 仍有 Y，ReadPrimaryTarget 却已读不到目标。这个结果不需要伪造 Provider 回执、内存分配失败或 Stop 误判。

P0-1 对仅有匹配行的安全出口成立。P0-3 的双遍判定在根输入已封、活动版本不可回绕、交接不断档等前提下成立。它只能证明责任排空，不能识别已完成的错误业务更新。当前建议是退回 Part 24 的成员冲突判据；原有 AF-1、H3 及 PROPOSED 状态仍以检查点为准。

## 1. 审查基线及证据范围

|输入|实际审查版本|
|---|---|
|仓库|main，HEAD 与只读 ls-remote 所见 origin/main 均为 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`。工作区无已跟踪 diff，暂存区为空。开始时有四个未跟踪文件：Part 24、上轮独立报告、default.profraw、opencode.json。|
|目标源码|实际读 HEAD 的 services/multisim，与 `632f6a2a888cfbbc3c9387edc89732364e12a10e` 的整个子树均为 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。另读同一 HEAD 的 Legacy SimRdbHelper。|
|设计输入|检查点 Part 18–23，SHA-256 `0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2`。Part 22/23 的工作区审查基线为 `7a414c4e3c536ee67361095b5076614fa9444685` 加当时本地设计，首次入库为当前 HEAD；两提交间只改检查点。A/B/C/D 均读当前工作树的对应章节。|
|本地提案及独立输入|Part 24 全文 SHA-256 `a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164`；上轮独立报告 SHA-256 `147d25d703419225f31937007c01c73d788f37f71a34a45897cb6612b3b7adf4`。两份均无独立提交 SHA。上轮 R1/R2/R3 在本轮分别对应 P0-2/P0-1/P0-3。|
|本轮实证|用实际头文件及三个 .cpp 运行 C++17 最小探针；另做删除谓词检查和三 Owner 有界交错枚举。结果与可重放输入见 §3、§5。仓库业务 Host 测试套件、sanitizer、产品构建和设备运行均为 **NOT_RUN**。一次本地探针不构成整图 Host PASS。|
|历史证据|T1 报告输入 be63ae124、归档 5796fce68；T2 输入 5796fce68、归档 632f6a2a8。C0 集成用例中既有 `T2ReloadDoesNotRemovePostAttemptNewAccount`（:529）覆盖签票后新增成员；:363 的恢复例只检查 Reconcile 返回。两者均未验证下文的“既存成员被新写再次证实”。|

输入文档：[Part 24](../../AF1_Part24_B1_P0_Design_Revision_20261123.md) · [上轮独立审查](AF1_Gate_B1_Independent_Review_20261123.md) · [AF-1 检查点](../../AF1_Architecture_Design_Convergence_20261123.md)。权威约束取自 [A §6.5.2、§12.3、§14.3](../../SIM_Target_Architecture_Design_v1.2.md)、[B §6、§7、§11](../../SIM_Multisim_Source_Architecture_Detailed_Design.md)、[C §4.1、§4.3、§5.3](../../SIM重构代码目录结构设计方案.md)、[D §7.4、SC-01 与跨实例验收](../../migration/SIM_Architecture_Refactor_Design.md)。下列源码定位默认相对 `telephony_core_service/services/multisim/`。

## 2. 三个 P0 的独立判读

|审查对象|判读|决定性证据|
|---|---|---|
|P0-1 原操作因果归属|设计可判定，产品能力 OPEN|§4 将原 call、端口来源、原计划、字段覆盖、实际身份和调用退出分开。只读同一行不能提升效果；支持不了的输入能安全停在原票。|
|P0-2 同域发布及后续版本有效性|无抛发布机制可实施；发布后的旧读仍有实质反例|§5 的准备/撤销和无抛尾段解决写到一半的问题。§5 第 2 步、删除谓词与 §10 未区分“重放原判断”和“新写证据重新确认同一个成员”。旧读能在无资格变化时覆盖新的正向事实。|
|P0-3 稳定关闭|在已定义的执行边界内通过反向检验；产品生产者覆盖、跨实例恢复仍 OPEN|§6 要求同轮同参与集、所有入口封口、接收先登记、执行退出和双遍不变。禁止的三个窗口是已获执行权无在途记录、父已删而子未接、未注销独立入口仍可入图。任何一个存在都不能发终局收据。|

## 3. T 统一发布的失败点矩阵与独立反例

**C++ 审查。** AccountRecord 含 std::string 与两个 optional 投影，投影含 u16string；Registry 有账户 map、两层成员版本 map、retired 字段版和共享时钟。BindingIndex 的槽节点在构造时存在。当前 AcceptDiscoveredAccount 先复制/插入账户，再用 operator[] 写成员代次，直接搬进提交尾段不满足要求。Part 24 提出在锁内 try_emplace 私有预留、固定容量撤销记录、失败以迭代器逆序擦除；该方案对单票有界增量可行。向多个账户发布 reload 时须为整次受影响行预备撤销记录，不能拿一张票的固定容量覆盖任意长度的集合。比较器、析构、分配器及缓存引用都在证明边界内。[AccountRegistry](../../../services/multisim/account/account_registry.cpp)（:75–92、:190–260、:262–335）及 [BindingIndex](../../../services/multisim/account/binding_index.cpp)（:23–39、:59–94）。

|注入点 / 对象|失败或中断后的可见 Account / 两视图 / Binding / 见证|原票与判读|
|---|---|---|
|分配 t、计划、角色准备|未发 Store 时无本次业务值；若 A 已领权要按 t 找回|先登记父项；不能因 A 答复未返另签 token|
|复制账户、字符串、retired 版本，校验身份和装配|全部旧值，W 无|准备失败，T 留 t、原外部证据及 a|
|账户节点预留后失败；Current 节点后失败；Historical 节点后失败|三处分别撤本次新增节点，已有值/正式时钟不动；外层 map 预建|前提是每次插入成功后无抛登记迭代器，锁内禁止外调与递归读取|
|生成来源、Binding 候选、W 或返回描述失败|全部旧值；不允许已有正式新 Binding|原 t 恢复到待准备，重新检查原观察和 r；其它账户占槽/版本耗尽不重绕|
|正式赋 Account 后；赋 Current、Historical 或时钟后；赋来源/Binding 后；标记 W 前|真正尾段只有已验证的无抛交换、已有存储上的标量赋值和终局标记。线程暂停仍持同一短域，读者只能在解锁后取完整组|这些边界可暂停，不能容许返回失败。若真实候选在任一边界能分配、回调、抛出或认领失效，属于不符合提案的实现。禁止线程被强制取消后继续使用半更新图；进程死亡无进程内恢复承诺|
|W 已成，Finalization 或其返回失败|完整新组，W 仍在；真实可信移除可在后来改变现行关系|只恢复原释放，重复 t 不升 BindingRevision，不重发 Store|
|同一来源新观察在提交前/后|可信替换先登记则不可发布旧对象；发布在先则保留当时 W，再按来源正常撤旧|普通序号推进要有同一来源的连续性证据；暂时无法确认时保留原票|
|两个 reload、版本耗尽|同 view 用最新 issued/未 consumed 的 attempt；异 view 分别检查 m，共享 clock 在 T 域串行预检容量|跨两视图需两个新刻度而仅余一个时整组拒绝；任一身份/成员/字段/r 达边界都不靠回绕恢复。其它 view 的成功不能授权本 view 删除|

正常读口 FindBinding、ReadAccount、ReadPrimaryTarget 经过 T 锁；源码 AcceptSlotAssembly 直接写槽状态，AcceptStoredDiscovery 另有自签票入口。Part 24 §5 已要求约束两处。正式发布必须覆盖正式成员代次与票内见证，不能仅以函数返回 true 为据。源码位置为 `account/topology_service.cpp:31–32,46–56,253–265,317–340`。在本机 GCC 13.3 / C++17、默认分配器下，实际头文件编译通过 AccountRecord、optional<AccountProjection>、optional<BindingRef>、SessionRef 的 `is_nothrow_swappable_v` 静态断言。未来见证类型、生产工具链、全写面和异常策略未由此验证。

### 阻塞反例 B24-01 · 新的有效证据只确认既存关系，旧快照仍可删除 Current

**触发条件。** T 缓存已经有 Current Y、Historical Y 和同来源 Binding(Y,0,1)。Provider 行仍是 s=41、ICCID=Y，但旧 Current 查询取样时行的 SLOT_INDEX=-1；稍后原发现写把它设为 0。这样的旧读可以成功且缺 Y：Host 的 LoadLegacyAccounts 对 Current 调 QueryAllValidData，对 Historical 走 QueryAllData；Legacy 查询谓词为 SLOT_INDEX > INVALID_VALUE。T 缓存可以比查询结果更早。卡来源保持连续，给本次**新 admission** 合格的原写凭据；不重放上一张票，不发生可信移除。更高观察序号可进入发现入口，同一来源持续性与具体新票分别验证。[Provider 桥](../../../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)（:41–63、:66–104）、[Legacy 查询](../../../services/sim/src/sim_rdb_helper.cpp)（:492–511）、`account/discovery/discovery_observation_ledger.cpp:31–42,78–91`。

|时刻|T 的 Current / Historical / Binding|reload 的输入及比较值|原票、A 与责任|
|---|---|---|---|
|0 · 签 Current 票 L 并实际读空，延迟返回|Y / Y / (41,0,1)|expected 有 Y，取 mC=1、w=2、身份 i、字段向量 F|T 持 L；已知槽水位 1|
|1 · Y 新票写成 slot=0，先 Unknown，随后给足原 call 证据|T 的内存位置、active 等已经等于目标，i、F 不变|L 的空结果在写之前已确定|t 的写已证实，a 固定 s=41|
|2 · T 在无失败尾段确认既存关系|Y / Y / (41,0,1)，记 W|按 §5 第 2 步的既存确认分支，允许 mC 仍为 1；r 不动。W 和新的原写身份不在 L 的冲突谓词内|T 持 W，随后可取得精确 A 释放确认、按票结束|
|3 · 最初的 L 返回|**空 / Y / (41,0,1)**|最新未消费 attempt；Y 缺行、原在 expected、mC 相等且 ≤w、i/F 相等，无未核实前缀，§5 所列删除门全部满足|L 已消费，W 证明的是刚才的有效接纳；两份收据均可依约结清。旧快照清除了更新证据支持的 Current|

**为什么这是设计反例。** 物理顺序已固定为“旧读取样 → 原写恢复该行有效位置 → 新业务见证 → 旧读接纳”，没有后续业务事件要求撤销。删除 Current 后 `FindBinding` 仍能返回 Y，而 `ReadPrimaryTarget` 在缺少 current 时返回 nullopt（topology_service.cpp:46–56）。即使 W 永远保存正确、T/A 释放完全幂等、H 得到真实稳定快照，错误仍在。单独的晚于发布的权威新读可以合法改变投影；这里被接纳的恰是早于原写的取样。若认为“重新接纳成员”理应含此次操作，§5 的“只确认既存关系时不增版”及其删除条件仍允许本例，须写出区分这两类确认的可检验资格。

**独立 C++ 探针。** 在仓外临时目录链接实际 `account_registry.cpp`、`binding_index.cpp`、`infrastructure/assembly/slot_catalog.cpp`。输入为已装配槽 0、订阅 41、两个相同活跃投影。依次执行 AcceptDiscoveredAccount、AcceptBinding(0,41,0)、BeginReload(Current)、AcceptBinding(0,41,1)、AcceptReload(L,true,空行集)，然后读回两视图和绑定。本轮实测：

```text
expected=1 w=2 accepted=1 current=0 historical=1 bindingRev=1
```

探针验证的是实际容器、旧读删除与幂等 Binding 的后果；未模拟真实 I/O，亦未执行尚不存在的 Part 24 发布方法。另对提案的新增删除谓词作独立求值，在身份、F、attempt 全匹配时，`expected.contains(Y) && m签票==m现在 && m现在<=w` 对“新成员”是假，“撤后重建”是假，对本例 (true,1,1,2) 为真；负控将本次新证据反映为 m现在=3，结果为假。此谓词差异使源码现象足以构成对设计的反证。
## 4. Store 原操作归因判定表

原写以 (epoch,admission,WriteDiscoveredAccount) 标识，核对 claim 只区分执行者。原计划冻结字段、值、唯一目标、A token 和角色列范围；后端证据由实际执行端产生，由 I 沿原调用栈或预登记的完成通道交给 T。T 是效果认知与推进的 Owner。仅有连接世代或自行回显 t 不足以建立归属。当前桥的成功判断是 result>0 加字段匹配；错误返回和匹配行仍可为 Unknown。产品尚缺可验证的延迟完成及其语义。[AccountStore](../../../services/multisim/account/account_store.h)（:32–77、:94–95）、[适配器](../../../services/multisim/account/account_store_adapter.cpp)（:38–46）、[桥实现](../../../services/multisim/test/framework/internal/host/primary/legacy_account_store_provider.cpp)（:58–104）。

|触发条件|外部证据判定|T 原票、A 资格及业务动作|错误判据|
|---|---|---|---|
|1 原调用成功，有可信原完成|验证本 call、身份、字段覆盖和后端确认语义。同一唯一目标的合格字段可升 Applied|t 从效果待证转业务待接纳，a 仍持有；另验当前来源才可发布|只因 call 结束就释放|
|2 原调用实际失败，其他作者写出相同行|如果仅见失败码及等值行，仍 Unknown；只有对原调用全未生效有权威证明才可 NotApplied|未知时留 t/a；明确无效果只走未写终局，不能取得 Binding|按匹配行反证原失败|
|3 原写 Unknown，仅有相同 ICCID|Match 只证明数据库当前可见记录，同 s 甚至所有值相同也一样|T 持 t/未知列，A 持原 token；Stop 返回业务未了|核对成功或 Drained|
|4 原写成功，后写改同字段|若原回执可靠，可证“当时”效果；后写不使原回执失真。单凭变化后行则不明|记录旧效果，现行字段受其自身读源与版本约束，不恢复旧值|把原计划当作今天的字段值|
|5 s 正确，回执属于别的调用|拒绝按本 t 入账，错 claim 不能接管；可定位时交真正 Owner|原 t/a 不变；已知混合或不全继续分项保留|同 s 就授权原票|
|6 重复证据或票已回收|同一可信证据幂等，认领已经变化的真实原完成由现持有人处理；票回收后陈旧|不重写、不升 r、不重复释放。活票内相互矛盾的证据隔离|因当前位置相同而更新后继票|
|7 Stop 先封，原调用证据后返|原票受理在先仍可收原完成；执行体和投递退出单独登记|未有充分证据继续留项；充分则仅沿原步骤，不能签新 Store 权|封口丢弃原回执或未交还即回收|

依 §4 和 §7，无法归因时业务与关闭都能安全挂起。即使 provider 始终缺证，也没有合法的 Unknown→Applied 路径。当前 `TopologyService::ReconcileDiscovery`（topology_service.cpp:188–240）读同一行后仍可结束资格，这是 IMPLEMENTATION_GAP；不能用它否定提案明确要求的拒绝分支。

## 5. 双遍收据的反向证明

### 5.1 假设、模型与结果

对协议本身，参与集合是 H 在 q 封口时冻结的全体 Owner、执行器及来源，非固定两个。每个 Owner 的本地操作、快照与活动版本更新在同一短域排序。版本对每一次影响排空的变化推进，即使数量恢复原值也不能复用。封口后新子项必须有已登记的父项，复制/领取可执行回调先登记，交付实际返回才可清，接手者须确认精确主体。重复与迟到输入只沿旧票和受寿命保护的入口处理。外部已排队但不可见的工作不满足这些假设。正向全图结论要求确认其受管，缺证时保图。

**本轮有界模型。** 取 R、T、A 三个串行 Owner、一个 H 采样者，允许不同 Owner 执行体重叠。一个固定关闭轮，已封所有根输入。抽象两次投递（R→T、T→A），队列中每个已投递工作从入队前至出队执行、返回一直由相应记录覆盖；模型用登记/退出边表示两条通道，未建通用 FIFO。T 的原票跨 Finalize 全程保留。初态按 (R,T,A) 的责任数为 (1,0,0)，三个活动版均为 0。按以下事件枚举：

|事件|Owner 的本地原子更新|
|---|---|
|a / b|a 在 T 登记 t，T 数量加一；b 在 R 确认交接后退出，R 减一|
|c / d|c 在 A 登记原资格/子项，加一；d 是 A 释放并保留未交还的确认，数量不变、活动版递增|
|e / f / g|e 在 T 消费精确确认，父票仍在；f 在 A 清确认，减一；g 在 T 清 t，减一。每一步都推进所在 Owner 的活动版|

工作序列 a,c,d,e,f,g 固定，b 可在 a 至 g 后共六个间隙插入。H 两遍都各读 R/T/A 一次，每遍取全部六种顺序，两遍顺序可不同。将六次快照与七次工作交错，样本为 `6 × C(13,6) × 6 × 6 = 370656` 条**带扫描次序的轨迹**。以“决策时仍有本图责任”为假空。可按以下伪代码重放：

```text
for b 的六个插入点:
  预演七步，保存每个前缀的三元责任数与活动版
  for 十三个位置中给快照选六个位置:
    for 第一遍次序, 第二遍次序 in permutations(R,T,A) 的笛卡尔积:
      每次采样读取该前缀对应 Owner 的 (数量,版本)
      单遍：若首三数均零，检查第 3 次采样时真实总数
      双遍：若六数均零且逐 Owner 两次版本相等，检查第 6 次采样时真实总数
```

|实际结果|计数|
|---|---:|
|首遍会假空的完整轨迹|6,018|
|满足双遍签空条件的轨迹|648|
|双遍签空时仍有责任的轨迹|0|

这是独立枚举，未沿用 Part 24 的 35 条轨迹。未穷举多个票、第三方 provider、丢弃/重复消息、跨进程、退订失败或两个模块的物理设备；这些靠下表的负例和额外前置证据检查。计数不表示实际错误概率。

### 5.2 逐个尝试破坏五条不变量

|不变量|反向检验及免于反例的条件|
|---|---|
|一 · 责任覆盖|若在执行权已授予之后才登记，则 Stop 可看不到原调用。§6 将授予和记录置于 T 同一提交点。准备在途、未返 Store、未归因字段、a 和原出口均有命名主体。登记失败不得授予。|
|二 · 交接无空窗|把父删除放在接收者登记之前，两个采样都可落入同一真实空窗，版本校验也无济于事。Part 24 §6 的“转交不断档”条款要求接收方先登记，再向发送方确认。Post 返回或任务弹出时仍应区分接收登记与执行交还。|
|三 · 关闭轮次一致|q、epoch、参与集合或 sealAck 有一项不一致，就没有同一图上的截面，整组作废。并发 Stop 共享首个关闭者及 q。|
|四 · 活动版本有效|假设 T、A 数量周期性 0→1→0，而 H 只比较相同计数，则两次均会漏看移动工作。两遍同一 Owner 之间有任一变化必须异版；溢出、非原子快照、漏记独立生产者均不能签空。|
|五 · Drained 安全|设每个 Owner 两次采样时刻为 uᵢ、vᵢ。所有 u 都先于所有 v，逐 Owner 版本相等且无复用，故区间 [max u,min v] 内各自一直满足终态。假设此后还能产生旧图工作，只能来自新根、未登记的在途或仍可生成子项的父项。三者分别与封口、责任覆盖、稳定终态矛盾。已移交项必须由驻留者接管后续入口，本图任何返回执行仍算在途。|

### 5.3 十个指定交错的状态表

|输入 / 触发时刻|原工作、Owner 与收据的状态变化|错误结果及判读|
|---|---|---|
|1 R 已收到失效，尚未投递 T|R 有来源/seq 在途；对可异步入队的实现，T 短域先登记撤销或 pending，再承认语义受理、入队。若仅 R 收到、尚未短登记，仍属输入交接窗口|R 单独收到不等于 T 已经消费；R 若早已承认权威失效却没封 T 来源，不得启用该路由的当前资格。无论哪种窗口，R 凭证阻止 Drained|
|2 T 已获发送权，尚无执行中记录|§6 只允许“准备未领”或“授予且已记 t/call/在途”。Stop 先赢则写零次，后赢则唯一执行者可继续|题设中间状态违反线性化点，不能用将来进入 I 的线程来补证明|
|3 T→A 时 T 已删、A 未登记|合法状态顺序为 T 持父 → T/A 均有项 → A 确认；本发现 t 还要留到真实释放确认|若制造空窗，两遍都会错。必须拒绝该交接实现|
|4 A 已 Finalize，T 尚未收到|A 的持有占用转为可追索确认，T 仍有 t/W/待确认；回调与清理另计|尚不能 Drained；T 不可把缺 token 当释放证明|
|5 第一遍后派生新子项|父项或发送在途仍可见，接收者在入队前登记；一遍可先看空接收方再看已清发送方|误报被非空项或两次版本差挡住，须重新取整组|
|6 二遍间数量 ABA|T/A 互转后各自可回原数量；每次增删改变版本，重读能识别。若允许取模回绕，可把工作从 A 搬到 T、搬回、再搬到 T，令每遍先读空 T 后读空 A|回绕/漏报则反例成立，Part 24 明定耗尽时只回未完成|
|7 R→T→A 三方交接|R 输入在途 → T 父票 → A 子项及确认 → T 消费 → 双方退出；生产者不能靠对方的队列长度清本项|三方枚举无提前签空；从 roster 漏掉 R 时不能沿用该结论|
|8 重复 Stop，首轮收据尚不稳|H 已有 q、部分 ack，第二调用读 q 的当前未完成原因；Owner 继续原交付|不另开 q，不重复授写/Finalize/通知|
|9 已进 Store，执行体未返回|T 有 t/call，A 持 a；即使原完成证据先到，T 仍列在途，投递侧仍持执行凭证|效果已知、队列空或 radio 命令零都不能结束旧图|
|10 旧实例关闭中，新实例申请相同物理键|旧 H/R 的未结资源和原图并存，新 epoch 的 ResourceBarrier 从 Checking 起步；缺合法 InitialRecoveryEvidence 时 Acquire 不放行|本 q 的稳定性不授予另一个实例键资格。当前 AcceptInitialRecoveryEvidence 只查本 epoch 及两个布尔，无法自证旧命令已停。按 AF1-18 保留产品门禁|

源码交叉点为 `card/runtime/card_runtime.cpp:34–54`、`infrastructure/assembly/primary_host_lifecycle.cpp:15–74`、`infrastructure/assembly/sim_module.h:53`、`infrastructure/request/runtime_operation_store.h:49–57`、`device/resource/resource_barrier.cpp:30–69`。目标模块级 M19 与产品源封口仍须独立实现取证。
## 6. 原七组反例复核

表中“终结”均以原调用与投递退出、精确释放已确认为前置；A 的角色字段由原计划负责，解析身份本身不发布角色缓存。

|原场景与输入|原票 / 观察 / 效果的时序|Account、Binding、A 与结果|
|---|---|---|
|1 X 迟到，Y 始终有效|tX Unknown → 可信换 Y → tY 独立发写 → X 的证据只使 tX 历史待确认|A 分别持 aX/aY，前提是两张已匹配的不同订阅。仅给 X 的证据不能绑定 Y；再给 tY 自有证据，空槽 r→r+1 一次。该原例通过。若本来已有 Y，还要接受 §3 的额外旧读挑战|
|2 Y 核对中失效|tY Unknown → R/T 排序可信移除 → 原写后来证实|无新 Current、Binding、角色资格，T/A 按历史处置；若只是停监，只能待证，aY 仍在|
|3 Account 成功、Binding 失败|新路径中 Account 节点只在准备态，Binding 条件拒绝 → T 逆序撤预留 → tY 待内部续行|新读者看旧组，A 不释放；旧路径已公开的未核实前缀须隔离破坏性 reload。正常失败已闭合；§3 的新反例发生在**完整成功之后**|
|4 Binding 成功、Finalize 回复未知|T 有 W 和 Y₁ → A 可能已释放 → T 等/重取同一确认|A 原资格或原确认均可定位，Binding 不重建；Stop 保图。后来真实解绑不改变 W 的历史意义|
|5 同票并发 Reconcile|执行者一锁内取得阶段 claim，执行者二看到进行中；原写回执可交现任领取者|只有一个内部提交与实际释放；旧执行体未退出不能被超时抢占。两次重复调用原 t 与 §3 的另一张新票必须区分|
|6 Y 已当前绑定，X 才核对|tY 已有 W；tX 只补原写效果 → 凭 X 的可信终止仅结历史|Y 的位置版本、角色计划和 tY 均不受影响。只见同槽或版本差、无终止证据时保留 tX|
|7 Stop 与续行|封口前领权则原写可返回；封口先于授予则写数零。A 解析、撤销和 Finalize 在 Closing 仍按旧票归账|来源未证时不发布；H 等 T/A 在途和 R 执行退出。重复 Stop 不重复效果。§5 的根封口与交接前提缺一即不能给终局|

## 7. 联合反例与顺序变换

**Part 24 §12 的 Z 对照。** Current/Historical 分别签 L 时都无 Z → 另一合法发现新增 Z，mC、mH 均晚于各自 w → Y 的 t 返回 Unknown、读回仅匹配 → 两个旧 L 交还 → Y 撤销入队 → Stop。Z 属于真正新增成员，两个旧 L 均不能删 Z；R 持 seq、T 持 t、A 持 a。没有补证，Finalize 请求没有合法前置，释放确认不可能正常出现。若另给同 call 权威成功证据且撤销已排序，则只结 Y 的历史，T 在 A 答复迟到时仍有项。该原例的 Z 不覆盖 §3 的既存成员。

**本轮指定输入。** 先跑不给有效原回执的负控。设 L 是旧 Current 票、t 对应 Y、a 是其原预留，Q 是本关闭轮，B₀ 是该票前的 Binding。无独立确认前“目标 Account 写数零”专指本 t；合法 reload 可按自己的视图读源更新 Account。

|时刻、输入|原票/Store|卡来源与当前事实|A / H 收据、是否可前进|
|---|---|---|---|
|T0 L 签票|无 t；T 持 L 和 w|L 的 expected 已冻结，保存其结果采样顺序|无 a|
|T1 写交出|t/call 已授并登记在途|来源最初有效，B₀|a 已持有|
|T2 Unknown|t/未知列，待证明原效果|B₀，本 t 未发布|a 保留|
|T3 R 收到 Y 失效，尚未把正文投递给 T|t 仍未知|若已语义受理，T 必已短登记来源 pending；若仅 R 已接收，R 持输入凭证，T 受理顺序仍未确定|R 的未交还项可查；不能将 R 的物理收件当作 T 已完成撤销|
|T4 返回匹配行；T5 T 收到|t 仅有行证据，认知仍 Unknown|本 t 不改 Account/Binding|a 继续持有，无可释放处置|
|T6 L 返回|T 按 L 的 view/attempt/m/w/字段处理；不归因给 t|真正新成员受 m 保护。合法 Current 视图变更不会生成 t 的 W|L 可结束，t 仍在|
|T7 Stop 封根；T8 原票请求续行|H 发 Q；T 可核对旧 t，不能授新 Store 权|即使撤销正文已消费，仍须先知原效果才可仅结历史|T/A/R 的未完项依当时事实出据|
|T9 第一遍取据|t 未知项必在，或采样变更使整组重取|B₀ 由合格的其他事件维护|a 仍在，不能签空|
|T10 所谓“a 的释放确认延迟”|原序列从未送出有效 Finalize；此输入不可达。主动投错票/旧确认只得陈旧或冲突|不得补造 W 或历史终局|原 a 不释放|
|T11 第二遍|即使 T/A 的版本两遍**相同**，也有尚待核对的非空项|无本 t 发布|H 不得 Drained|

|改变 T3–T11 的顺序或单一输入|关键对象状态与判定|
|---|---|
|把匹配行提前至 T3 前，或把 T8 放在 T6/T7 前|仅凭匹配行，t 始终 Unknown。失效、旧读和内部领取的排列不能将观察变成原写证据|
|另给 t 权威回执，T 撤销短登记已先发生，而正文延迟到 T9 后|t 效果已知，来源 pending，不能发布。正文若证可信终止，T 产生历史处置后 T10 才可成为真正延迟确认；a 释放和执行交还前 H 返回未完成|
|权威回执与 T 发布均先于撤销的语义受理|当时 W 合法；随后撤销必须按原来源撤该关系。若先有 T 短登记而只延迟 R 的正文，则此顺序不成立|
|让 T6 落到新证据的统一发布之后，并用 §3 的既存 Y 取代新增 Z|即使尚未 Stop、没有任何新失效，旧 L 也可凭相同 m/i/F 清掉经新写确认的 Current。后续封口和真回执均不能修复它。这一变体先把 T3 的可信受理推迟，以隔离旧读错误与合法撤销|
|把真正的 T10 放在两遍之间，或在 T9 后由 R/T 派生最后一项|有变化须重新收整组。最终可以在业务状态已错误时真正排空；这是 B24-01 的后果，不是“双遍读到了虚假的空”|

## 8. 源码事实、设计契约和差距

|来源及定位|Part 24 约束|分类与处置|
|---|---|---|
|TopologyService::ReconcileDiscovery，`account/topology_service.cpp:188–240`，读匹配行并可删票；ledger 的 CompleteStored/ReconcileStored，`account/discovery/discovery_observation_ledger.cpp:124–164`|按原票寻址，验证归因和当前处置，各项未完持续出据|IMPLEMENTATION_GAP。产品能否给权威证据另列 §9|
|AcceptDiscoveredAccount、BeginReload/AcceptReload，`account/account_registry.cpp:75–92,262–335`；AcceptBinding，`account/binding_index.cpp:59–80`|私有预留和无抛尾段可避免半发布；成员代次参与旧读删除|IMPLEMENTATION_GAP 是现行写点无联合保证。**DESIGN_DEFECT** 是在完成拟议保证后，§5 的既存确认分支仍允许旧读抹掉新证据。关联的 **CONTRACT_GAP** 是新票的正向成员确认与同票幂等回放未规定不同的冲突资格|
|AccountRoleProtocol::AcceptDiscovered / Finalize，`role_assignment/account_role_protocol.h:28–65`；AssignmentService，`role_assignment/assignment_service.cpp:666–685`|在 A 域固定身份，按原处置给可重取的精确确认|IMPLEMENTATION_GAP。实际 bool 及删除 token 不具备该返回语义；与 B24-01 相互独立|
|CardRuntime、SimModule/PrimaryHostLifecycle，见 §5.3；B §11 描述 OwnerCloseReceipt|同轮封口与源交接、稳定读取|IMPLEMENTATION_GAP。当前 Primary 切片的排空条件不能代替稳定性前提|
|C0 集成 `test/cases/target/primary/c0_topology_integration_test.cpp:348–372,461,499,529`；桥 `test/cases/internal/shutdown_test.cpp:135–157`|旧读要与新证据、正常重复、两个独立视图区分|TEST_GAP。既有“签票后新增账户”及字段改变的断言未涵盖“身份/字段均未变但被新写证实”；保留它们作不同对照|

## 9. 外部 OPEN 与责任方

|外部证据|责任方及安全出口|
|---|---|
|Provider 的原完成、唯一实际行、逐列提交或原操作的确定否定语义|I/Provider 产品负责人；无匹配能力则 T 保持 Unknown、A 持资格、H 保图。向本例**假定给足**证据仍触发 B24-01，不能把内部反例归为 EXTERNAL_EVIDENCE|
|卡源最早可排序受理点、上游队列、退订和已领取回调的实际退出|R·card/H/平台执行器；无证明的输入不能提供当前发布资格或关闭终局|
|旧实例、共享 RIL、其它写者的退出与真实物理资源复用|H、R、RIL/厂商；AF1-18、G2c/O2 单列，未证时新实例保持 Checking|
|内存图寿命以外的进程崩溃、驻留接手能力|H/产品宿主；本方案限原图存活或逐项确认的合法移交，普通本地探针不能证明崩溃恢复|

## 10. 最终 Gate 结论

**`B1_REVIEW_BLOCKED`**。选择阻塞结论的依据是 B24-01 已给出可执行的旧读覆盖顺序、全部删除前置条件和错误读面。P0-1 的降级策略与 P0-3 的稳定性证明无需因该反例重做。Part 24 保持 PROPOSED；本次独立结论不修改 Part 22、Part 23 或原 Gate 记录。

## 11. 退回要求与重新过 Gate 的条件

|必须修订的位置|要求与通过判据|
|---|---|
|Part 24 §5 第 2 步、旧 reload 表及 §8 的 T 接纳契约|定义**跨新原票**重新确认既存成员时哪些旧读失去删除/覆盖资格，明确比较依据由 T 持有，且与该票真实正向证据一起发布。保留同票重放的幂等和既存 Binding 不升 r 的约束。架构复核只要求排除反例，成员代次、读屏障或其它等价机制由设计者裁决。|
|§10–12、§16 的正反验收|新增 §3 同一输入。先证旧读确实在新写前取样且缺 Y，再交新票权威原完成，随后交旧读。要求 Current Y、Historical 原值、Binding r、ReadPrimaryTarget 与新见证一致；旧读可以消费并报告冲突。对**新写后才签发和采样**的独立有效读仍允许按其读源更新。|
|逐视图与重复输入对照|分别测首次加入、撤后重入、只确认既存关系、同票重放。何种权威证据覆盖 Current/Historical 须分开判断；一次 Current 位置写不自动证明 Historical 的所有字段。真实重放不得造成第二次 Store、逻辑绑定、角色发布或无意义的重复冲突版本推进。|
|独立复验|至少重复 §3 的删除谓词模型，说明修订改变了哪一个有效条件；检查同票重放和真正较新的 reload 两个负控。实现阶段再从正常 DiscoverAccount → 确证 → 旧 reload → ReadAccount/FindBinding/ReadPrimaryTarget 取得 Host 证据。现阶段不能用“计划以后加测试”代替明确的设计后置条件。|

## 12. 签收前置条件的适用性

本轮未达到提交架构负责人决策的条件。解除本次阻塞需要第 11 节的内部设计证据；Provider、产品来源和跨实例资源能力继续独立登记。重新过 Gate 时，应分别报告设计反例是否消除、实现是否验证和产品门禁是否仍 OPEN。
