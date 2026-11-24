# AF-1 最终独立架构 Gate 记录（2026-11-24）

**独立结论为 `AF1_REVIEW_READY_FOR_ARCHITECT_DECISION`。** 在下面固定的设计材料中，我未找到满足其全部准入条件却能删除新成员、让旧角色覆盖新事实、提前解租、重发已交接命令或提前判定整图排空的反例。三个实测反例仍能在当前 testonly 源码上触发。架构负责人要决定是否采用这些契约，并接受缺证时保图、隔离或暂缓切换的代价。生产写权、协议收尾、SO 卸载和设备静止仍有各自的验收条件。

## 1. 审查人、范围与固定输入

本轮只有一位独立最终审查者。我没有编写或修改主包、候选差异和交接材料。冻结前我向协调者给出了失败输入和实际观察，冻结后重新核对被审文本。以前的 H3、Part 23、24、25 结论按其原版本保留。冻结前的挑战没有被签为一次单独的正式 Gate。

Git 根为 multisim；审查开始时本地 main、origin/main 和直接查询 origin 的 refs/heads/main 都是 `d9c2d23dce7148df1fc474ce598fb1cff55b839d`，工作区跟踪文件与暂存区无差异，工作树保留原有未跟踪输入。目标源码树对象为 `e7986b29585d50da05198e773bf6a2e3eb0f9828`。以下行号相对于各自固定的文件；源码路径相对于 services/multisim，Legacy 另标。

|关键输入|SHA-256 或固定位置|
|---|---|
|[本轮签收包](../../AF1_Architecture_Final_Signoff_Package_20261124.md) §§3–13、14–19|84047e5a882aaeaf116dc3a8112f784d53b8b5c401b1ea1f2085a128ffdc74c7|
|[A/B/C/D 候选差异](../../AF1_Authoritative_Docs_Candidate_20261124.patch)|e489418fcd807842e821a30e0c619db9620b4c713f9ab8c0dd6045a7fdae3cac|
|[AF-2/AF-3 交接](../../AF1_AF2_Implementation_Handoff_20261124.md) §§1–6|505e85442cb4a30c0d23024682add0ff2548c8644ba7ad00f9eb6885c0f0d66c|
|[前序主包](../../AF1_Architecture_Design_Convergence_20261123.md) 1336–1483、1484–1657、1658–1913、1914–2113|0e1dd3ebfe75e86becbcea13477902b93427973bbd7f31da2b8b33ec23fbecf2|
|[Part 24](../../AF1_Part24_B1_P0_Design_Revision_20261123.md) 33–123、210–256；[Part 25](../../AF1_Part25_P0_2_Membership_Reaffirmation_Design_20261123.md) 36–117、134–173|a4402511ee997d9c0a63f6527e9363cfcc176bdefc074f5cbafbd4a59ae05164；4ff376e7adfae7ef1f475ebc8cb411030293c8c1a47bedc6780ea1289f753418|
|[Part 26](../../AF1_Part26_Reload_Causal_Coverage_Design_20261124.md) 22–156、176–220|946d7f954351b59607d3e540ef42c776a706ff46e1bf5cf918e900b49c781ed8|
|[Part 23 独审](AF1_Gate_B1_Independent_Review_20261123.md) §§6–12；[Part 24 独审](AF1_Part24_Independent_Gate_Review_20261123.md) §§3–7；[Part 25 独审](AF1_Part25_Independent_Gate_Review_20261124.md) §§6–15|147d25d703419225f31937007c01c73d788f37f71a34a45897cb6612b3b7adf4；f55cb963c6118dbeeafda037900c260746d570f0c2e7ad44687714f0c78ec313；58940ed346b660d36d58c6519a2008fdc93036c569514aeb182d13aa9eb9218e|

我对照了 A 主设计 893、1048、1095、1161、1181 行，B 源码详细设计 418、459、495、550、625 行，C 目录设计 220、259、287、367、413 行及 D 迁移设计 521、777、799 行的原约束。对候选差异执行 `git apply --check` 返回 0。差异四部分依次始于第 1、105、216、248 行，分别核对了决策、接口、目录和旧写者退出，未将补丁应用到工作树。

## 2. 独立判据与反例

我从“哪一刻谁有资格写或删”倒推可观测结果，用拥有相同返回值的不同历史检验接口，再试着把同一工作移交、重复、跨代或在发布前打断。能够在满足全部拟议前提时破坏不变量才构成未解内部缺陷。若实现没有存放相应信息或产品没有能力签发证据，分别检查承接切片与缺证时的行为。

|挑战与证据|冻结契约的判读|
|---|---|
|**对象与写权。** 同一个数字可同时出现在 T 的 BindingRevision、R 卡应用代次、C1 原生协议轮次及 RIL serial。替换其中一个，能否更新别的域？现 A 在 assignment_service.cpp:199、239、392、600 合成 Session，253 由目录产生参与者；659–664 的两个物理偏好入口可直接写 Store。|签收包 27–64、94–100。T 持订阅/视图/Binding；A 持角色、参数和冲突写集；R·card 签卡 Session，R·device 维护真实协议路由、令牌和物理 lease。O 机制在原 Owner 使用，C/I 各自传递原出口/原效果，H 构造与持有寿命；同一个 Store 合行不扩大字段写权。SC-02 和 SC-03 有可执行反向断言。|
|**B1 与 Store 归因。** 同槽 X/Y 原票乱序，给其中一方“按 ICCID 读到同值”，另一历史中原写确实成功；再让 Y 原结果只证名称但角色未知。源码 topology_service.cpp:118–185、188–240 的按位置 pending/reconcile 不能证明逐票效果。|两种等值历史都保持原票未知；部分已证只推进其列，原身份、原 plan、call、执行体退出、A 原 token 均有单独收据。新票不复用旧票写权。所需列、当前来源、A 解析等齐备才可发布 Y；永久可信撤销才可仅结历史。两种去向都不能替另一张票清账。P24:33–54，最终包 59–60、70、79–83。|
|**T 统一发布及续行。** A 已固定身份，在第二视图节点或 W 的构造失败；另一次 T 已发布，A 精确释放返回丢失，期间发生真实解绑。参照 account_registry.cpp:83–91、binding_index.cpp:59–94。|前者撤销同锁内私有节点，旧事实完整，t/a 可枚举；后者按既成 W 重取原确认，无第二次 Store/Binding/通知。所有业务读口、装配/来源突变、M19 受同一 T 域约束；跨 Owner I/O 在域外。新 t 重申只对证实的视图/字段升 m/F；原有连续关系 j/r 稳定，同 t 重放均不再升。校验余量和所有分配在无抛发布前完成。最终包 79–85；P25:109–117。|
|**缺席、墓碑与读源。** 先签 L 再重申 Y；新签 L 却读到早于 Y 的空图；真实后继确已移出 Current；继而交付旧有行或新世系的更大序号。源码 account_registry.cpp:262–335、account_store.h:44–52，Legacy sim_rdb_helper.cpp:465–523。|依次为本地 m>w 拒删、源覆盖不足拒删、证实后继方可退役 Current、拒复活。注销 DB 成员不改变 r。移除要覆盖全部被消灭的正向事实，负向墓碑和已接纳较新快照的下界继续有效。C/H 读源、集合、字段和保护各自独立。查询成功、游标非空、同 URI、一次同步返回均不能颁发覆盖。最终包 83–87、141；P26:64–84、88–112。|
|**同身份旧号码。** 拔出并在同一 s/ICCID 上重建投影，重建后签 L。当前 j≤w、号码 F 不冲突，读源却返回前一段存续中的号码。|只验本地 j/F 的判据会污染新投影。最终包 141、交接 42 和候选差异 29 要求实际样本跨过退役/重建边界或有等价身份证据。行存续不可证时依赖该行的列一并失权；样本确属本段，仅名称覆盖未知时则允许独立合法号码前进。|
|**T→A 角色整组。** 令 account 行 41、等长角色行 42；另令两向量完全等于 {Y}，但 L 签后 T 新增 Z；再试 T 已交资格而 A 尚未占用时修改成员。参照 topology_service.cpp:289–314、account_role_protocol.h:68–89。|前两个反例均挡住整组。比较原期望、当前、返回、角色全键域，含漏项、墓碑及 A 所有被替换/擦除项。只对不影响完整成员/角色的姓名冲突放行。后一个交错由撤回与领取竞争决定；A 先准备、取得资格后同域无抛发布。如 A 先赢，T 后续变化按后继处理；相反无角色写。交付执行退出与认领分报。最终包 89–92；P26:114–123。|
|**A 释放后的因果前驱。** t 原写确证 Y 的角色列，A 的旧 map 尚无 Y。接纳固定身份不改角色值；在释放 token 后签新 L，其成员域可用，角色图像较旧。|原值尚未发布也不能丢掉该写的因果要求。A 从发送前冻结受影响的身份/集合，真实生效列在移除占用前安装保护，未知保留原票。保护可独立于释放回执存在；同票结清既不发表角色值也不发通知。仅有后继完整角色批次可替换。最终包 92；候选差异 159–161；交接 43。|
|**M1 / N1。** 先发生相关来源短暂失证，再恢复到同 r、active；随后真移除并插回同卡。另令 setter 已返回但效果未知，再交付旧 Current 与独立 Historical。|连续性资格中断废掉原步骤；无关应用刷新不影响有效目标；确证摘除再建得到 r+2，旧源不得撤新绑定。M04 允许 A→T 词法短读，T 不持锁投 A。A 成功 setter 在同域推进角色及 Current 版本/保护，Historical 另验；失败未写、效果未知及成功可分辨。未知占用原键。当前源码 card_runtime.cpp:34–54、account_role_protocol.h:92–96、assignment_service.cpp:324–336 仍需接线。最终包 115–117；交接 51–53。|
|**M2、共享资源与 SC-02。** C0 首键写后失权，B 已发后 active 中断，C1 两成员中 P 在 Submit，Q 返回确认未发。再试旧轮普通 ACK 充当 UPDATE NOTIFY，或在参数恢复后提交新轮。|仅已登记原目标意图可承担历史外部效果；其后参数、通知、角色更新、重试分别取资格。R 预登每个原轮/步/成员与整体 lease/fence，Q 只能说明 Q。P 未返时阻止撤组或重入发送。同组全部新提交已封、执行退出且每成员证实未发才可整体撤租。已交接或未知留原轮，收尾与复用另取证。全键获取须失败原子。无换绑/Stop 后 COMPLETE 的原路由及协议依据时保留隔离。最终包 72、99、116、143；交接 22–26、52。|
|**SC-01 与重建。** Stop 抢在 T 本域授发送权前后；R 已取撤销回调但 T 还未交消费；H 首遍读空而子项正从另一 Owner 交来；最后旧图 RIL 仍在运行时启动新图。|T seal 先赢不发送，已授权的原执行可继续一次。来源入队与 T 撤销资格同序；旧图保留执行寿命、投递、原出口以及未结列。所有根都封、子先登记、父确认且本地执行退出后才清、同参与者双遍版本/明细均稳定才可 Drained。缺实际接手方保图；新实例缺同域权威及旧资源接手/结束证据，停 Checking。Primary 子图不能替其他生产者答复，Core 所有共享 RIL。最终包 53、73–75、182–185；primary_host_lifecycle.cpp:15–74、resource_barrier.cpp:30–50。|

## 3. 本轮实测与界限

用 g++ 13.3.0、C++17 将附录 A 链接到 Git 基线中的真实 T/A、R 核心类。三个对照均按断言正常退出。每行输出描述基线可达的错误行为。使用受控输入的 R 端口重入不证明生产 A 一定同时派发两个成员。

```text
row=41 role=42 current_T={41} current_A={42}
failure_index=4 first_inflight=1 second_inflight=0
platform_entries=2 K_available_before_P_returns=1 R_recorded=0 K_available_after=1
```

第一项走真实 PrepareAccountReload/AcceptAccountReload，A 角色端口只转交给真实 AccountRoleProtocol。第二项对两把已就绪的资源键执行 Acquire，从第一次堆分配开始逐个注入失败。此工具链在第四处分配失败后留下单边 InFlight，具体分配序号不构成 ABI。第三项从 R 的 SwitchPrimaryRadio 进入，在 P 尚未交还时让同轮 Q 确认未发，随后 P 报 HandoffAccepted。可读的 K 在 P 返回前已 Available，R 最终计数仍为零。附录 A 的断言固定的是**源反例**，将来实现修复后应建立相反的回归断言。

我另用附录 B 分别穷举两种责任移交与两遍采样的交错，各 35 条。接收先登记时，单遍误签 3 次，双遍误签 0 次。故意先清父再登记子时，单遍误签 18 次，双遍也能误签 4 次。这证明正确交接是该稳定截面论证的独立前提；两 Owner、一个派生工作的模型没有覆盖 C++ 调度、数据竞争、外部队列或注销实现。多 Owner 的一般化依赖同轮所有第一遍结束才开始第二遍、各自版本不回绕、封口后只能从仍持的父项派生三项约束。

本轮没有执行全套 Target/Legacy、产品构建、设备、SO、ASan/TSan，也没有实现 W、逐成员 in-submit、Provider 可见前缀或跨 Owner 一次性许可。前序报告中的通过数没有计作本轮结果。

## 4. 处置、签收门槛及剩余问题

- **设计修订轨迹。** 冻结前提出了遗漏新成员 Z、A 合写确认的寿命、同 s 跨存续旧字段、资源多键准备失败和组内平台调用未返五类可区分输入。冻结包第 91–92、99、141、143 行分别给出本地写点或拒绝依据；补丁对 A/B 明确身份/读覆盖、角色保护和资源规则，C 维持目录职责，D 给出交接与切换反例。交接 §§1–4 列出实际方法、受控失败和可检查读口。对上述输入重推后没有残余的内部授权歧义。
- **外部 OPEN。** Provider 须证明原 call 和实际列，以及完整查询和可比较世系，或提供可达的合法业务后继。否则涉及外部变化的 WU 可能长期保留旧视图。协议方须给原生参与者、路由、停止期收尾和 commandEnded/resourceReusable 证据；宿主须证明全部相关入口已封、跨实例唯一屏障或驻留接手。产品另验 Core/CMI、旁路 writer、原旧行为与 SO/设备。对应能力缺证时保留原 WU 或旧图，不能宣称新路径已能收敛。
- **实施门槛。** AF-2 可在架构负责人签收相应契约、另有实施授权后，通过真实对象图和受控时序验收。除负例外，必须能用真正可归因的输入完成仍有效的 Y、能删除真正移出的 Current、能接纳完整且较新的角色组、能在全部成员证实结束后释放完整资源。只有保守拒绝、没有正向出口或错误修改原返回/通知均不过实施门禁。AF-3/生产须在完整 WU 退出旧写者/发令者/重试者后，按匹配产品和设备分验。
- **冻结后导航修订。** 第一份候选签收包摘要 a5f486a5a672c3462cae5a89e60727aae3b71ab8010057294486ab703b985b43、交接摘要 031db5fd78b46c562afbe5cde6a2012ba26b47f063b0b8de8a4ddd82befe564f。审查指出第 197 行应指历史检查点 §20.1、交接第 55 行应区分旧 Flow-3 与新包 Flow-4。已按表中最终摘要重查两处和不变的补丁；两项导航勘误均已修正。

**最终原始三选一判定：`AF1_REVIEW_READY_FOR_ARCHITECT_DECISION`。** 该判定覆盖表中冻结输入的架构可提交性。架构负责人的最终取舍、实施复验、外部供证各自待办。

## 附录 A　当前源码上的三个可重复反例

在仓库根从本报告提取唯一的 cpp 区块。输出在新建临时目录，所有所需逻辑和输入都保存在本报告与上述基线中。

```sh
set -eu
d=$(mktemp -d)
f=telephony_core_service/docs/history/reviews/AF1_Final_Independent_Gate_20261124.md
s=telephony_core_service/services/multisim
awk '/^```cpp$/{on=1;next} on && /^```$/{exit} on' "$f" > "$d/gate.cpp"
g++ -std=c++17 -O0 -pthread -I"$s" "$d/gate.cpp" \
  "$s/account/topology_service.cpp" "$s/account/account_registry.cpp" \
  "$s/account/binding_index.cpp" "$s/account/discovery/discovery_observation_ledger.cpp" \
  "$s/account/activation/activation_progress.cpp" "$s/infrastructure/assembly/slot_catalog.cpp" \
  "$s/device/resource/resource_barrier.cpp" "$s/device/resource/resource_radio_runtime.cpp" -o "$d/gate"
"$d/gate"
```

```cpp
#include "account/topology_service.h"
#include "role_assignment/account_role_protocol.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "device/resource/resource_barrier.h"
#include <cstdio>
#include <new>
#include "device/resource/resource_radio_runtime.h"
using namespace OHOS::Telephony::MultiSim;

struct Store : AccountStore {
AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) override { std::abort(); }
AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) override { std::abort(); }
AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) override { std::abort(); }
AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) override { std::abort(); }
AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &patch) override { std::abort(); }
AccountLoadResult LoadAccounts(const AccountLoadRequest &request) override { std::abort(); }
AccountIdentityMatch MatchAccountByIccid(const std::string &iccid) override { std::abort(); }
AccountDiscoveryStoreResult WriteDiscoveredAccount(const AccountDiscoveryWritePlan &plan) override { std::abort(); }
AccountStoreWriteResult WriteRemoval(const AccountRemovalPatch &patch) override { std::abort(); }
int32_t QueryImsPreferenceByIccid(const std::string &iccid, int32_t &value) override { std::abort(); }
};
struct Roles : AccountRolePlanningPort {
 AccountRoleProtocol a;
uint64_t ReadRoleProjectionRevision(bool historicalView) const override { return a.Revision(historicalView); }
bool AcceptReloadedRoleProjectionSet(const std::vector<StoredAccountRoleFragment> &loaded,
        uint64_t loadAttempt, uint64_t expectedAssignmentRevision, bool historicalView) override { return a.AcceptReloadSet(loaded,loadAttempt,expectedAssignmentRevision,historicalView); }
bool AcceptReloadedRoleProjection(const StoredAccountRoleFragment &loaded, uint64_t loadAttempt,
        uint64_t expectedAssignmentRevision, bool historicalView) override { std::abort(); }
std::optional<AccountRoleFragment> PrepareAccountRoleFragment(const AccountRolePlanningInput &input) override { std::abort(); }
bool AcceptDiscoveredRoleProjection(const AccountRoleFragment &planned, SubscriptionId assigned) override { std::abort(); }
bool StillOwnsInitialRoleReservation(const AccountRoleFragment &planned) const override { std::abort(); }
bool FinalizeInitialRoleReservation(const AccountRoleFragment &planned,
        const InitialRoleFinalizationEvidence &evidence) override { std::abort(); }
};
int RoleRepro() {
 SlotCatalog catalog{{}, {}};
 Store s;
 Roles r;
 TopologyService t{catalog,s,{3,5}};
 t.BindRolePlanner(r);
 auto l=t.PrepareAccountReload(AccountReadView::Current,0);
 SubscriptionId y{41}, z{42};
 AccountStorageRow row{y,"Y",SlotId{0},u"Y",{},true,{},{}};
 t.AcceptAccountReload(l,{l.storeRequest,0,true,true,{row},{{z,true,false,false,false}}});
 assert(t.ReadAccount(y) && !t.ReadAccount(z));
 assert(r.a.Projection(false).count(z)==1 && !r.a.Projection(false).count(y));
 std::cout << "row=41 role=42 current_T={41} current_A={42}\n";
 return 0;
}

static long fail_at = -1, count = 0;
void *operator new(std::size_t n) {
 if (fail_at >= 0 && ++count == fail_at) throw std::bad_alloc();
 if (auto p = std::malloc(n)) return p;
 throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p, std::size_t) noexcept {std::free(p);}
int MultiKeyRepro() {
 for(long n=1;n<=10;++n) {
  RuntimeEpoch e{5,9}; ResourceBarrier b(e); ResourceKey a{"k0"},c{"k1"};
  for(const auto &k:{a,c}) { assert(b.RegisterForInitialCheck(k)); assert(b.AcceptInitialRecoveryEvidence(k,{e,true,true})); }
  const std::vector<ResourceKey> keys{a,c}; StepRef step{{1,e,RequestOwner::Assignment},1,1};
  count=0; fail_at=n;
  try { (void)b.Acquire(keys,step); }
  catch (const std::bad_alloc&) {
   fail_at=-1;
   bool ia=b.ReadState(a)==ResourceState::InFlight, ic=b.ReadState(c)==ResourceState::InFlight;
   if(ia!=ic) { std::printf("failure_index=%ld first_inflight=%d second_inflight=%d\n",n,ia,ic); return 0; }
  }
  fail_at=-1;
 }
 return 1;
}

struct Sink : RadioResultSink, RadioOwnerDispatchPort {
 void AcceptRadioStep(const RadioSwitchResult&) override { std::abort(); }
 void AcceptLateRadioEvidence(const RadioSwitchResult&) override { std::abort(); }
 bool PostRadioOwnerTask(std::function<void()>) override { std::abort(); }
 bool RadioOwnerCallbacksDrained() const override { return true; }
};
struct Platform : RadioPlatformPort {
 ResourceRadioRuntime* r{}; int calls=0; bool reusableInside=false;
 PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId) const override { std::abort(); }
 void EndLegacySenseSwitch(const StepRef&,const RadioDomainId&) override { std::abort(); }
 RadioSubmitResult SubmitPrimaryRadio(const RadioSwitchRequest &p,uint64_t token) override {
  ++calls;
  if (p.target.legacySlotId==1) return {RadioSubmitDisposition::ConfirmedNotSent,{}};
  auto q=p; q.target=SlotId{1}; r->SwitchPrimaryRadio(q);
  reusableInside=r->ReadResourceState({"K"})==ResourceState::Available;
  return {RadioSubmitDisposition::HandoffAccepted,static_cast<int>(token)};
 }
};
int OverlapRepro(){
 RuntimeEpoch e{5,7}; Platform p; ResourceRadioRuntime r(e,p,{{"K"}}); p.r=&r;
 auto s=std::make_shared<Sink>(); r.BindResultSink(s); r.BindOwnerDispatch(s);
 assert(r.Initialize({"K"},{e,true,true}));
 RadioSwitchRequest req{{{1,e,RequestOwner::Assignment},1,1},{0},{"G"},PrimarySwitchMode::ThreePhaseProtocol,{},4,1,2};
 r.SwitchPrimaryRadio(req);
 std::printf("platform_entries=%d K_available_before_P_returns=%d R_recorded=%zu K_available_after=%d\n",
  p.calls,p.reusableInside,r.SubmittedCommands(),int(r.ReadResourceState({"K"})==ResourceState::Available));
 assert(p.calls==2 && p.reusableInside && r.SubmittedCommands()==0 && r.ReadResourceState({"K"})==ResourceState::Available);
 return 0;
}

int main() { return RoleRepro() || MultiKeyRepro() || OverlapRepro(); }
```

## 附录 B　责任移交的有限交错

以下 Python 3 输入可单独执行。1、2、3、4 表示依次读取 A/B/A/B 的本地责任数及活动版本。真实工作在 x 才完成。每种内部顺序与四次采样所有保序排列均纳入，输出为顺序、排列数、单遍误签、双遍误签和末个双遍负例。

```python
import itertools

# A starts empty, B holds the parent. b registers the child at A,
# r clears the parent at B, x completes the child at A.
for path in ("brx", "rbx"):
    schedules = single = double = 0
    witness = None
    for order in itertools.permutations(path + "1234"):
        if "".join(a for a in order if a in path) != path:
            continue
        if "".join(a for a in order if a in "1234") != "1234":
            continue
        schedules += 1
        held, version, snapshots = [0, 1], [0, 0], []
        completed = False
        for action in order:
            if action in path:
                i = 1 if action == "r" else 0
                held[i] = int(action == "b")
                version[i] += 1
                completed = action == "x"
                continue
            i = 0 if action in "13" else 1
            snapshots.append((held[i], version[i]))
            all_empty = all(n == 0 for n, _ in snapshots)
            if action == "2" and not completed and all_empty:
                single += 1
            if action == "4" and not completed and all_empty and snapshots[:2] == snapshots[2:]:
                double += 1
                witness = "".join(order)
    print(path, schedules, single, double, witness)
```
