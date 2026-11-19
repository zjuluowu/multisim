# 角色并发与异步请求协议专项审查 · 2026-11-19

## 决策摘要

建议 A 在本用例首次需要保护的外部写入之前，一次登记实际共写的角色单元。在 A 的短提交域内检查资格、接纳投影并决定下一动作；系统参数、设备、通知与完成端口均在锁外调用。C0 需要保护 Primary 参数对及联动 Data，但不需要取得 Radio 执行租约。无关的 Voice、SMS、账户资料和设备域可以继续工作。现有证据只足以推荐技术边界，冲突的公开返回与等待政策仍为 PROPOSED。

建议请求机制只保留原请求、当前步骤/尝试、发送关联与一次性完成资格。逐步消费与外部效果认知独立，进入终态后由原效果核对者接收晚证据。同步 C0 可保留可查询请求与部分写入前缀，省略没有异步响应的空步骤。每个用例继续自己编排。目标约束由[架构主文档](../../SIM_Target_Architecture_Design_v1.2.md)维护，本文是固定输入的论证与实验记录。

**同一反例的前后对照。** 标准单 M0 双卡上，旧 Target 的 C1 为槽 0 预写 Primary，C0 成功选槽 1，C1 随后也成功。持久 Primary=1、Data=0、内存 lastPrimary=lastData=0，与两种完整串行顺序均不符。在 `bcc1614fd` 运行同一调度，C0 在写入前返回 -1，原 C1 成功，最终 Primary、Data、两个投影均为 0。该 Host 切片已覆盖最初反例。产品上能否并发进入、公开 Busy 语义及设备资源能否复用仍需签收。[单 M0 见证](#单-m0-的-c1-与-c0)

## 固定输入与证据口径

开始时本地/远端 main 均为 `eea678d6746abc48e799afc9560dc8c55b82ba9e`。执行期间其他批次发布 B 异步与 C1 三阶段，复核提交为 `ee32ed917`、`9c4c82fef`（状态提交 `ba95cf2c0`），随后发布 C1 错误/重试 `3c5a974c0` 和共享写集/完成保护 `bcc1614fd`。“单 M0 的 C1 与 C0”记录旧反例；“阶段九复验”记录同一输入在新保护下的可达性。最初输入与[上一轮审查](SIM_Architecture_Independent_Review_20261119.md)的 `0e4004bef` 相比，差异仅在文档。产品仍从原 Core 装配 services/sim，Target 为 testonly。对照了[功能与 WU](../../migration/SIM_Architecture_Refactor_Design.md#writer-cutover)、[业务流程](../../business/多SIM架构重构_设计说明与业务流程对照.md)、[五组目录](../../SIM重构代码目录结构设计方案.md)、[测试入口](../../../services/multisim/test/README.md)和[执行状态](../../SIM_Refactor_Execution_State.md#current-status)。

事实类型与可达性分别记录。FACT 为源码或本轮运行事实，CONFIRMED 约束只继承有依据的既有契约，INFERENCE 是从证据推导，PROPOSED 为建议，OPEN 为待签收/缺证，HISTORICAL 保留原批次。下表中的可达性 CONFIRMED 特指在**所列环境**同时有代码和运行见证，REACHABLE 为已知条件下的静态路径，CONDITIONAL 依赖外部条件，HYPOTHETICAL 尚无调用证据，NOT_REACHABLE 仅限明确的配置和路径。冻结 Legacy 的外部观察与内部新协议分别验收。

### C0 的真实读写顺序（FACT）

1. 旧 SA 入口经 CoreServiceSim 核系统调用者、权限、能力、在卡和 active，先写偏好再调用 SimManager。CMI 和内部自动选卡另有入口。原普通分支在 GetRadioProtocolModemId 命中 M0 时走 SavePrimarySlotIdInfo，经主卡保存、广播、默认 Data 保存与广播，成功返回后 Manager 可通知账户。设备分支的写入时点不同。[Core 入口](../../../services/core/src/core_service_sim.cpp) · [Manager](../../../services/sim/src/sim_manager.cpp) · [旧 Controller](../../../services/sim/src/multi_sim_controller.cpp)
2. Target Host 调用 StartPrimarySwitch（具名兼容入口 StartLegacyPrimarySwitch 可委托）；Begin 固定原目标与完成出口。可选父请求须同 epoch，随后 AdvanceStep 从 0 到 1。SlotCatalog::Find 取静态描述；T 经 BindingIndex/AccountRegistry 读取已接纳绑定、ICCID、current.active。要求原 BindingRef 的订阅、位置、版本匹配，ICCID 非空、active 为真。[A 入口](../../../services/multisim/role_assignment/assignment_service.cpp) · [T 读取](../../../services/multisim/account/topology_service.cpp)
3. 读 Radio 观察后再读 T，对比以上字段。观察槽匹配、版本非零，存在当前 modem 与配置 rilInstance。计划优先选厂商映射，其次是 userSet 且支持专用 RIL，再次是当前 modem 与 rilInstance 均为 0，最后才是普通三阶段。eea678d67 只执行 AlreadyPrimary；随后 B 与 C1 已接入受控 Radio。以下步骤 4、5 描述 C0。配置的 radioDomain 用于登记 transition，Completed/Failed 之外的同域登记会挡住重入。[计划](../../../services/multisim/role_assignment/primary/primary_switch_plan.cpp) · [聚合](../../../services/multisim/role_assignment/assignment_aggregate.cpp)
4. A 依次写偏好 → 写主槽 → 写原 ICCID 的 SHA256 大写十六进制 → 接纳 lastPrimary/阶段 → 调 PublishMainCardChanged → 写默认数据槽 → 接纳 lastData → 调 PublishDefaultDataChanged（携原订阅）→ 阶段 Completed → CompleteLegacyInt → MarkTerminal(Succeeded)。实际 C0 未调用设备切换；投影接纳不等于系统参数写成功。写阶段部分 AcceptTransitionPhase 返回值未被消费。早期失败先交付 -1，再标 Failed。[参数适配](../../../services/multisim/role_assignment/assignment_store_adapter.cpp) · [完成适配](../../../services/multisim/infrastructure/compatibility/events/legacy_completion.cpp)
5. 参数端口仅返回 parameterResult、callFinished。已完成非零错误按旧分支可能继续，未完成则转 Reconciling/NeedsReconciliation，该路径不交付成功。主槽/ICCID 是两个外部调用，返回组合结果损失逐键信息。已完成的写入、已提交的通知无法撤回。输出端口为 void，C0 无法观察订阅者收到或投递失败。LegacyCompletion 在配置需要且整体成功时先通知账户，再按原 route 交付整数；Target 夹具使用记录端口。[输出契约](../../../services/multisim/role_assignment/assignment_output_adapter.h) · [拓扑夹具](../../../services/multisim/test/cases/target/primary/c0_topology_integration_test.cpp)

### 操作影响矩阵

表内 P 代表 `persist.telephony.MainSlotId` 和 `persist.telephony.MainCard.Iccid`，D 代表 `persist.telephony.MainCellularDataSlotId`，U 代表 `persist.telephony.userPreferPrimarySlot`。这些缩写只用于本文的写集分析。P 要成对解释；U 记录意图，last 为兼容内存投影，写调用有自己的持久化结果，Radio 当前分配由设备观察提供。

| 操作及已实现范围 | 读取 | 修改的业务事实/投影 | 参数/存储 | Radio/设备 | 通知、资源与执行域 |
| --- | --- | --- | --- | --- | --- |
| SetPrimarySlotId，Legacy 各分支 / Target C0 及受控 B/C1 | T 当前绑定、active、ICCID，配置与动态 Radio | A 的 transition、lastPrimary、lastData；Legacy 另有默认 simId/重试视图 | U、P、D；普通三阶段可条件恢复 P | C0 只读，专用或三阶段才发令 | Primary 先于 Data，按入口可能再发账户通知；全局角色输出与真实 Radio 域独立 |
| SetDefaultCellularDataSlotId，Legacy CMI→SimManager | 上游校验在卡/激活；广播读取目标 current 记录 | lastCellularDataSlotId、defaultCellularSimId | D | Controller setter 无切 Radio 命令；连接消费在下游 | Data 广播、行为事件、Manager 条件账户通知；与 Primary 的 D 重叠。Target C0 内部有联动写，独立 setter 尚无等价闭环 |
| 默认 Voice，Legacy setter | slot→当前 simId、激活、移除哨兵、旧去重值 | DB 角色、全槽 current.isVoiceCard、defaultVoiceSimId | Provider::SetDefaultVoiceCard，无 P/D | 无 | 变化后发 Voice 事件，失败可在 DB/缓存成功之后；Manager 成功时账户通知 |
| 默认 SMS，Legacy setter | 同类查找，独立的短信去重值 | DB 角色、全槽 current.isMessageCard、defaultSmsSimId | Provider::SetDefaultMessageCard，无 P/D | 无 | 短信角色事件及条件账户通知。Target SetDefaultRole 有调用骨架，真实 AssignmentStoreAdapter::WriteDefaultRole 返回失败，Voice/SMS/Data 不可据此宣称接管 |
| 激活、停用，Legacy 普通/卫星分支 | 账户、激活进度、切换中标志、卡型 | T 角色以外的 active/进度；后续重新选主交 A | Core SetActiveSim 入口写 U，普通分支可更新缓存、DB，force 与卫星另走分支 | 普通激活用 RIL；旧 activeSimMutex 与有限等待 | UpdateSubState、条件选主/通知；早期 U 与稍后选主的写集要分别计算 |
| 账户资料修改或查询回写 | 当前账户/卡号或卡上号码、旧字段版本 | T 的号码/显示等字段，重载时可读入 A 的角色片段 | Provider 对应列；整行初始化/重载需区分字段 | 涉 EF 的号码操作才碰卡 | 按原成功点通知。GetShowNumber/GetSimTelephoneNumber 有异步 SetShowNumberToDB 路径，不能归为纯读 |
| 换卡、解绑、重插 | R 原会话观察，T 原 BindingRef 与当前账户 | T 的位置版本、绑定和 current/historical；A 只复评相关角色 | 视既有发现/移除流程；Host 可注入已确认结果 | R 使旧会话失效，命令可能仍在途 | A 可观察失效，T 不接管角色输出；不使用 T 的全局锁保护参数 |
| eSIM SwitchToProfile | slot/File、Port、候选 Profile、卡返回枚举 | R 接纳本地卡结果；后续 T 绑定/A 资格以新观察为准 | Legacy 卡调用返回后即写 U=true；外层 int 与 enumResult 分离 | 本地卡原语可能占 eUICC/通道 | U 与用户显式选择、激活共键，意义和时点不同；不能据 U 或 int 发出“Profile 已启用”通知 |

Legacy 的 Data 独立写者有跨仓源码证据：telephony_cellular_data@`29d6fa9c6105c6e39a9a765048868e4631ffbb8f` 的 `services/src/cellular_data_service.cpp::SetDefaultCellularDataSlotId` 检查调用者、权限、在卡/激活后调用 CMI；`incall_data_state_machine.cpp` 另有相同 CMI setter。这里读取的是提交对象。跨服务实际指向同一 Manager 与参数命名空间、线程调度及产品版本需单独证明。[本仓 CMI](../../../frameworks/native/src/core_manager_inner.cpp) · [eSIM 入口](../../../services/sim/src/esim_manager.cpp)

### 冲突关系与可达性

| 两操作 | 判断依据 | 最小控制边界（PROPOSED） |
| --- | --- | --- |
| Primary × Primary / 恢复旧参数 | 同写 P、D 或 U；阶段可发布事件，两键共同表达一次主卡保存 | 对实际会触及的成员一次登记；恢复也须核验原资格，避免旧快照覆盖后继操作 |
| Primary × 独立 Data | 同写 D、lastData 及 Data 事件；不要求两个槽都可成为 M0 | 在同一 D 输出序内接纳。公开接口允许看到 Primary 已写而 Data 尚未写的前缀 |
| Primary × 激活 / Profile 偏好 | 同写 U 的入口各有偏序；激活还会改变当前绑定资格/触发后续选主 | 分开早期偏好与后续具名用例。两个 true 在参数值上可交换，调用、失败和事件仍须按契约观察 |
| Voice × SMS；纯账户改名 × Data | 不同角色列及缓存；共享同一行/通知类型不能单独证明冲突 | 仅在 Provider 有真正的字段隔离时并行。整行读改写、联合建行或明确有序的共同输出按实际字段增列；普通读无需取得写资格 |
| 换绑 × 任一当前角色写；Refresh × EF 回写 | 读写相交的是绑定、active 或会话版本 | T/R 分别失效原引用，A 捕获并复核；无需把插拔或整卡锁到慢 I/O 返回 |
| 不同逻辑槽 × 同一硬件命令域 | 即使写集不交，真实通道/Modem 仍可重叠 | R 按物理证据仲裁；反之，独立设备并不能使同一参数成为私有 |
| 两次通知或查询 | 通知发布顺序、去重投影与订阅者可能重入；单纯读取角色无业务写 | 只保护契约要求的发送顺序，不推断消费者到达序；有回写的查询走字段写者 |

**受控 Host 见证（FACT）。** 初始 P 槽/D 槽为 0，两个已激活账户的当前绑定为槽 1→订阅 1、槽 0→订阅 2。A 选 1、B 选 0，完成出口分别是 31、32。两请求均 userSet=true，Radio 观察版本 9、modem=0；没有专用 RIL 或厂商映射。单线程进入 A，在主卡通知的受控端口内执行 B，再返回 A，不使用定时器或多线程访问未加锁的容器。附录保留可复核观测程序。

| 顺序 | A（槽 1） | B（槽 0） | 当时已发生的输出 |
| --- | --- | --- | --- |
| 1 | 二读通过，在域 0 获得登记；写 U、P 两键与 lastPrimary，发 P1 | | P=1、ICCID=卡1、D=0 |
| 2 | 主卡通知调用尚未返回 | 在域 1 通过二读/计划/准入；写 U、P 两键、lastPrimary，发 P0 | P=0、ICCID=卡0、D=0 |
| 3 | | 写 D、lastData，发 D0/订阅2，完成 0 | P=0、D=0 |
| 4 | 恢复，写 D、lastData，发 D1/订阅1，完成 0 | | P=0、D=1；事件 P1、P0、D0、D1 |

在合成目录里两个描述都允许 rilInstance=0 且 radioDomain 不同，两个成功操作如完整串行，最终必为 (0,0) 或 (1,1)。上述结果违反建议的共享角色输出序。它没有违反“所有时刻 Primary 必等于 Data”这条假规则：独立 Data 请求**本来就可以**选择另一槽。本反例只涉及两个均要求联动的成功 C0。

| 可达性 | 对象、前提与结论 |
| --- | --- |
| CONFIRMED · 合成 Host | 上表 8 次参数写、4 次通知、两次成功、0 次设备切换。使用实际业务类，设备观察、参数及回调为受控端口。 |
| NOT_REACHABLE · 该目录的双 C0 | 同一调度，把槽 0 的 rilInstance 改回 1，则 B 未命中 AlreadyPrimary。在 eea678d67 返回 -1，A 成功，仅有 P1、D1。把两个合格槽放进同一域则由域忙挡住 B，输出相同，拒绝原因不同。 |
| REACHABLE · 源码写点 | Legacy 独立 Data 与 Primary→Data 均能到达 SaveDefaultCellularDataSlotIdInfo。TelLifecycle::Lease 管理受理与排空，不给这两个方法建立业务互斥。无需两个 M0；“可写同键”不能推出某次实际交错。 |
| CONDITIONAL · 产品 | 要产生双域双 C0，须同时证明两位置的 M0 观察/有效路由、独立域、当前绑定及分支选择，且两个请求能重入或并行。三卡/四卡数量本身不够。产品当前走 Legacy，分类/准入不能照搬 Target。若产品全局唯一 M0 且映射一致，静态同一时刻双 M0 排除；跨时刻映射变化还需重新验证。 |
| HYPOTHETICAL · 产品通知重入 | Target 输出接口允许同步回调；本轮只在受控端口执行过。产品广播到底同步返回、异步投递、消费者是否回调设置接口，须跟踪注册者、执行队列、时序和输入，无产品事故结论。 |

**另两条反例（eea678d67）。** ① 在同一原双卡目录内，A 的 P1 通知中经 T 合法解绑再重插新订阅 3。Host 见证后续 Data 通知仍为 D1/旧订阅1，T 当前读为订阅3，外层返回0。固定原归属防止把旧命令重定向新卡，却不能撤回已发 P1。② 独立 Data(B) 在 Primary(A) 的 P1 和 D1 之间完成时，可得到 P1、D0、D1 的观察前缀；两种完整串行输出分别为 D0、P1、D1 或 P1、D1、D0。这个顺序的意义取决于产品对事件提交顺序的契约，尚无该路径运行证据，不能以最终 D 值相同判等。

### 单 M0 的 C1 与 C0

**CONFIRMED · ba95cf2c0 受控 Host。** C1 已有与双 C0 不同的反例。使用阶段五原目录、同一个真实 A、两个真实 T 绑定；槽 0 的 rilInstance/观察为 1，槽 1 的均为 0，两个域不同。关闭专用与厂商分支。C1 先选槽 0，预写 P=0，再向两个成员发阶段 1；在回包前，C0 选槽 1，能从另一域准入并完整写 P=1、D=1、交付成功。随后按原协议关联给 C1 的两个成员送阶段 1、2、4 的有效结果。C1 成功路径复用原先的预写，不重写 P，却更新 lastPrimary=0 和 D=0。两次成功后的参数 P=1、D=0，而内存 lastPrimary=0、lastData=0；任何一个完整成功请求最后执行，都不会得到这个组合。这个排程只需在自然的异步间隙调用一次其他 setter，无需重入通知或两个 M0。R 的真实设备是否允许这个执行重叠仍需产品证明。

[当前业务入口](../../../services/multisim/role_assignment/assignment_service.cpp) · [B/C1 现有用例](../../../services/multisim/test/cases/target/primary/b_async_primary_test.cpp)

B 和 C1 的回调还揭示三处边界。① B 在发设备前登记 pending，成功时后写参数；C1 按阶段/成员集接纳，阶段全齐才推进。两者不能共用一种参数写入时点。② 以 `9c4c82fef` 静态核验，AcceptLateRadioEvidence 要求 NeedsReconciliation，但随后调用的 AcceptResult 拒绝非 Open；不能从函数名或注释推出晚证据已保存。③ C1 在完整收到一个阶段的成员回包后会 EndWait(Delivered)，即使它随后还要发下一阶段；此时不能把 wait 当作客户端已交付的证据。EndWait/MarkTerminal 各自赋值，多处完成端口调用位于 MarkTerminal 之前。应按现有出口做两种顺序测试，不能据代码顺序宣称产品已经重复交付。当时的成功回包路径复核 subscription、slot、bindingRevision 和 ICCID，未在 bindingCurrent 表达式中复核 active。若产品允许只变更 active 且绑定仍相同，需分别确认原请求应如何交付、谁负责已发命令。构造 SessionRef 使用固定应用键与绑定版本，产品接入还需 R 的真实会话/设备证据。

### 阶段九复验 · 同样的输入与事件顺序

| 观察 | ba95cf2c0 | bcc1614fd 的同一受控 Host |
| --- | --- | --- |
| 槽 0 的 C1 预写后发槽 1 的 C0，再交齐原 C1 消息 | 两个返回均为 0，Primary=1 而 Data 与 lastPrimary 为 0 | C0 返回 -1，未写参数、未发通知；C1 返回 0，仅一组 Primary/Data 通知。Primary 参数及 ICCID 指槽 0，Data 和两个投影均为 0。 |
| 同一 NotSent 回执两次、执行下一步、尝试改写终态 | NotSent 两次 Accepted；合法下一步已恢复；后写 Failed 覆盖 Succeeded | Accepted→Duplicate，两步各接纳一次；再次写 Failed 保持 Succeeded；ClaimCompletion 只取到一次。 |
| 请求已 NeedsReconciliation，交入迟到证据 | AcceptResult 拒绝非 Open，无法补记 | 错 StepRef 被拒，原步的 ConfirmedApplied 可独立读回，终态保持。该观察只证明原记录有归宿。 |

**FACT。** `AdmitTransitionWithRoleWriteSet` 把设备域的转换登记和 `kPrimaryRoleWriteSet` 的成员检查放在一个聚合方法中。Data setter 复用 Data 成员，Voice/SMS 各用自己的成员；同槽同请求的多次外部 IPC 没有公共幂等键。C1 在已确认失败并完成该轮后可恢复原参数，第一次失败经 ClaimEarlyCompletion 交付，后续受限轮仍属原内部请求。CompleteOnce 在通知完成端口前尝试封存并领取出口。角色映射和请求记录均是 Owner 私有容器，其正确性以执行域串行化为前提；上述确定性见证全程单线程。[聚合](../../../services/multisim/role_assignment/assignment_aggregate.cpp) · [C0 保护用例](../../../services/multisim/test/cases/target/primary/c0_assignment_service_test.cpp) · [B/C1 保护用例](../../../services/multisim/test/cases/target/primary/b_async_primary_test.cpp)

**仍需验证。** ① AcceptEntryPrimaryPreference 直接调用 Store。选择共享偏好前须清点 Core、激活、Profile 与域外旧写入口；当前 Primary/SetDefaultRole 的保护不能扩展到所有写者。② 该版 AdvanceStep 校验旧 StepRef 和递增步号，不要求前一步已消费；C1 的业务门禁要由 A 验证。重试采用新步号加 attemptCount。只在确有同一步重发的用例需要时再扩展机制。③ 在 bcc1614fd，EndWait 仍可再次赋值，AcceptLateEvidence 会覆盖同请求的既有晚记录；需要显式区分兼容等待状态、首个有效证据和后续矛盾证据。④ 业务准入与完成领取在 Owner 顺序内有效。真实并发线程接入、跨服务写者与外部命令结束仍需产品证据。Host 内部 -1 的取值不能批准新的公开拒绝策略。另一个 API 边界是同一 owner 再次 AdmitRoleWriteSet 时传入已持集合的严格子集：当前实现把已持位图赋为传入位图。现有 Primary 固定传入完整集合，未发现该子集重入调用。建议重复准入只返回原记录，部分释放必须显式验权；以“持 P+D→同 owner 重入 D→其他 owner 申请 P”作纯规则负控。

**并行开发中的 M9.3 输入。** 在 bcc1614fd 之后的工作树再次检查到 AcceptResult 已移入 request_mechanism 的 runtime_operation_store.cpp，AcceptLateEvidence 拒绝 Unknown 和已有记录的再次覆盖。受控 RetrySchedulerPort 经 weak sink 将原 request、attempt 和调度代次交回 Owner，Stop 可取消/排空 Host 定时任务。调度代次只区别同一原操作的旧定时事件，无需加到所有同步请求。实跑该工作树 Target 38/38、Both C0 各一例差异 0。独立的产品写者、真实 Radio 的结束/可复用、Core OnStop 与共享 RIL 仍缺产品验收。设备释放不能由 Host scheduler 的 drain 代替。

## A · 角色准入、提交与恢复

| 比较项 | A1 · 设备域加例外 | A2 · 按业务写集合登记 | A3 · 角色状态单元串行提交 |
| --- | --- | --- | --- |
| 正确性、冲突精度 | 同域互斥；跨域加每对例外才挡共享写，独立 Data 还无自然设备域 | 由业务成员求交，同时保留绑定版本与实际设备域检查，可覆盖独立写者 | 单单元写最直接；Primary/Data 跨单元必须共同排序 |
| 并发、扩卡、开销 | 同域无设备的无关请求也会冲突；产品配置越多例外越多 | 固定少数成员，一次原子登记；互不相交并行，不随槽数增加锁 | 单队列易验证，慢参数、重入或隔离时可能阻塞独立 Voice/SMS |
| 重入、死锁、外部效果 | 再挂全局锁易有锁顺序问题，域锁无法撤回通知 | 登记资格后锁外调用；重入只尝试本次完整集合，不同步等前任 | 仅包住内存提交仍会外部交错；把阻塞 I/O 放队列中会卡住回调 |
| 失败、生命周期 | 域级恢复难解释旧 P/D/U 写给谁 | 保留原资格及已知输出前缀；未知只隔离可能被旧操作影响的成员 | 小状态单元恢复简单，跨单元需要交接未决效果 |
| 工程、T/A、可测性 | 复用现表成本低，副作用例外难维护 | A 复用原 transition/token，T 继续提供窄事实。按输入枚举写集，确定性验证；逐 WU 接管 | 单一同步且无外部效果的 setter 可以直接用；统一队列或事务会扩大迁移范围 |

**推荐 A2，简单 setter 可退化为 A3 的单成员提交。** 写集是固定的业务成员集合，例如偏好、Primary 参数对与投影、Data 输出、Voice、SMS。它随分支确定，不给全部参数做字符串锁。两操作必须隔离的条件为写集合相交且有不可交换的事实/可见输出，或另有已签收的跨成员不变量。T 的读依赖以绑定/字段版本验证；R 的资源冲突独立判断。共同通知类型不自动形成全局排序。

| 时点 | 守卫、动作及保证（PROPOSED） |
| --- | --- |
| 准入 | 按实际入口/分支列举将共同排序的成员，在 A 短提交域原子试取全部；同一已受理身份返回已有状态，新旧两个无关联的 IPC 调用不能凭相同参数合并。冲突先返回内部 Busy 及原持有者。由方法级签收决定拒绝/等待/有界排队及原错误；通知/回调线程不能等待其调用栈上的前任。 |
| 执行 | T 窄读验证原订阅/位置/版本及 active；A 只操作自己的资格，R 只操作硬件资格。在**首次不可撤销调用**前尽量复核；长时设备步骤返回后还要复核当前资格；最后校验至外部调用之间仍有窗口。中途换卡不会把同槽新订阅替换进旧请求。提前发生的 Legacy 偏好写保留在原入口原时点，整批迁移前盘点所有旧写者。 |
| 提交 | 准入登记、本地投影接纳及完成出口领取在各自受控域内有线性化点。P 两个参数独立写，通知提交与消费者接收也是两件事；不存在整个请求横跨参数系统、广播及 Radio 的线性化点。以原分支偏序约束已发前缀。先记待执行节点，出锁执行，带 token 回到域内核验。 |
| 部分失败 | 每个不可合并的输出记节点、是否已调用及可证明的结果；至少分 U、P 槽、P 散列、P 通知、D 键、D 通知。不引入持久事件日志。返回错误只证明接口报告的结果，实际副作用需独立证据；已知投递不重播，送达未知不推定成功或安全重放。旧分支允许忽略的完成错误仍按旧规则投影。 |
| 结束 | 已确定后续不会再发有冲突的旧动作、所需通知已按序提交且后继无法被旧效果覆盖，才能释放相应资格。未知交 A 核对并保留仍受影响的成员；不能证明成员独立时保留完整集合。相同 token 校验后才可释放。超时只结束等待。补偿须满足原分支条件、当前仍有写资格且确认旧效果，不以旧快照覆盖新选择。 |

**三个独立责任。** A 的登记约束角色与输出；步骤只携原请求身份；R 持有真实域的 lease/fence。一个 C0 可在同一 A 请求里持原资格到有序输出结束，无需 R 租约。多步设备请求在可能影响同一角色的几个步骤间保留 A 资格；若已证明旧设备效果只影响独立域、无法改变后续业务输出，可先移交核对并释放无关 A 资格。R 何时可复用仍取决于命令结束、资源可复用及该类操作要求的效果证据。请求超时→释放 A 和 R→新请求→旧命令生效，是目标必须拒绝的调度；当前 Target C0 没有设备命令，不能作为现有 C0 故障报告。[设备屏障](../../../services/multisim/device/resource/resource_barrier.cpp)

等待图的边只能朝外部证据推进：A 给 R 带原资格的非阻塞执行请求；R 能独立接纳结束证据并排空，不能持物理锁同步请求 A 完成。A/T 互读或父子交接使用不可变输入及异步返回，子操作不重取父持有的同一成员，不升级部分已持集合。无可用设备资格时返回可解释的结果或在域外按已批准预算重新准入。未知超预算停止主动重试，保留隔离、告警和人工/设备核对入口。单纯少加锁无法解决一个回调在同栈等待自己返回的问题。

## B · 从现有容器到最小步骤协议

### 已有实现和实测反例（FACT，分基线）

下表还原 eea678d67→ba95cf2c0 的问题形成过程；bcc1614fd 的实现与复测见[阶段九复验](#阶段九复验--同样的输入与事件顺序)。

| 载体 | 已有字段或实现 | 由此能够证明的范围 |
| --- | --- | --- |
| RequestId、StepRef | RequestId={value, epoch, owner}；StepRef={request, step, attempt} | 同一本地序号可出现在不同 Owner/实例。C0 每次 Begin 都分配新号，重复客户端调用不自动去重 |
| RequestContext、RuntimeOperation | 原 target、可选 parent、可选 sentSession、CompletionPlan；另有 deviceEffect、wait、terminal | 回调、同步等待和内部回复是三种 route；wait 有 Waiting/Delivered/TimedOut/CancelledWait/CallerDied，terminal 有 Open/Succeeded/Failed/NeedsReconciliation |
| Begin、Read、AdvanceStep | Begin 初始化 step0/attempt0、NotSent、Waiting、Open。AdvanceStep 只许 nextStep 大于原 step，改 step/attempt/sentSession | 现版本无法用同 step 的 attempt+1 表达重试；eea678d67 推进时不复位 deviceEffect，ee32ed917 起 AdvanceStep 复位为 NotSent，让下一步可正常接纳 |
| AcceptResult、MarkTerminal | 核 epoch、Owner、请求存在、Open、原 StepRef；用 deviceEffect 是否为 NotSent 判重。MarkTerminal 直接覆盖 | 仓外实测 NotSent 两次均 Accepted；第 1 步 ConfirmedApplied 后合法第 2 步在旧输入被判 Duplicate，在修订输入已恢复；Succeeded 再写 Failed 得到 Failed，wait 仍为 Waiting。实测对象是容器，不能据此推断生产回调已交付两次 |
| 方法接入范围 | eea678d67 的 RecordSent、EndWait、RetireAfterEvidenceRetention 仅有声明。新容器 RecordSent 核原 step、Open 和 sentSession 唯一性；EndWait 给 wait 赋值；RetireAfterEvidenceRetention 可移除终态且 effect 非 Unknown 的记录。B、C1 已有 pendingPrimary_，可按协议会话/成员接纳受控回包 | C0 只 Begin→AdvanceStep→本地同步流程→MarkTerminal。声明的 assignmentConflicts_ 与 nextConflictToken_ 没有实现 C0 的写集登记 |

[请求值](../../../services/multisim/infrastructure/request/request_ref.h) · [上下文与出口](../../../services/multisim/infrastructure/request/request_context.h) · [可执行容器](../../../services/multisim/infrastructure/request/runtime_operation_store.h) · [A 的接口与成员](../../../services/multisim/role_assignment/assignment_service.h)

### 为什么分开四种生命周期

业务请求从受理到业务终态；一项步骤从安装到该尝试的结果被消费或撤销；外部动作从跨出可信发送边界到得到可解释的设备/参数证据；资格分别由 A/R 从取得到释放或移交。同步无外部步骤时请求和本地调用可共享栈与出口。有独立超时、暂停、部分写入、多槽回包或释放凭证时，合并会丢失信息。一个步骤可已经消费“超时且未知”，设备随后才结束；A 可已完成其角色投影而 R 仍占用共享通道。一个外层请求也可以跨数个各有不同结果的步骤。

| 状态维度 | 建议的最小表达及作用域 |
| --- | --- |
| 请求与交付 | 复用现有单向 terminal；按原等待/取消契约管理 wait/route。终态结果封存；领取一次完成出口与业务结果同域决定，锁外投递。对只表示等待者离开的方法，可先关掉该等待出口，业务仍 Open 继续核对。进程内最多一次领取，不保证跨崩溃必达 |
| 当前步骤 | 一个当前 StepRef 及“尚未对外 / 已可能对外且可接收 / 已消费”三种互斥资格。结果 payload 携成功或错误、EffectKnowledge。收到错误同样消费本次资格。发送调用一旦可能触达对端，就不再根据本地超时推定未发送 |
| 外部效果 | 原步骤/命令关联上的观察。已受理只说明对端收件；参数服务确认某键、卡侧确认动作、命令已结束及资源可复用各自取证。未返回或证据不足为 Unknown；NotSent 只可由发送前失败或可信的“未移交”证明给出。成功状态可与较早的部分失败并存 |
| 跨步保留 | 普通顺序只留当前 attempt；旧效果若仍影响安全重试/后续写入，移交给原 A 核对记录或 R lease。已终结且无持有者/晚回调危险时按预算回收，不需要永久保留所有步骤。成组 Radio 回包按原 phase/member/scope 去重后再产生该步结果 |

### 转移与守卫（PROPOSED）

| 输入 | 准入条件 | 原子动作与锁外工作 |
| --- | --- | --- |
| 创建、准备发送 | 请求 Open，原绑定/会话符合该用例，前步所需结果已消费；有 A/R 所需资格 | 安装不可变 StepRef 及原 serial/回调关联，先置“已可能对外”再出锁调用；允许发送函数返回前同步回调。准备失败且能证明未跨边界时走 NotSent |
| 发送返回 | 同一 attempt；检查回调是否已经先行消费 | 区分未移交、已接收、失败但移交未知。本地错误或发送函数返回不能覆盖先到的有效响应；同一证据只归入该步 |
| 结果/错包 | owner/epoch/request/step/attempt 对应，按动作另验发送时 session、协议 serial、phase/member；Open 且本步未消费 | 同域领取并消费结果，包括 NotSent 和错误；把“业务可提交”和“设备证据有效”分别判断。旧会话或旧 attempt 不更改当前角色/Records |
| 下一步 | 原步已消费，结果符合具名业务分支，仍有继续所需的业务及资源资格 | 安装 step+1、attempt=0、独立的未消费位。旧效果供对账，不充作新步的去重位；不因每一行同步代码分配步骤 |
| 重试 | 同一逻辑动作；前 attempt 不能破坏后继，或对端具备已验证的去重/幂等语义；目标仍有效、无冲突、在预算内且唯一重试者获准 | 只增 attempt，安装新关联，一切旧回包只进原效果核对。整个请求重发是新的受理，须重算冲突、守卫和已执行前缀；单凭同 requestId 不获重发资格 |
| 超时、取消、CallerDied、Stop | 先区分“停止等待”与“业务终止”；归属匹配，可被消费的完成/等待资格尚在 | 在同域封等待或终结；未发送可确认撤销，已发未证记 Unknown 并交接 A/R。新 epoch 拒绝业务回写，但不得丢弃原设备证据。到期策略与用户可见时点待签收 |
| 完成/晚到 | 业务已在原合法出口或进入需核对终态；一次性领取 route。终态后只允许原外部效果处理者接纳有凭证的证据 | 并发完成、取消、超时、Stop 最多一方能领取同一出口；锁外调用通知/回调。不改已交付的结果。对方死亡或投递失败只记交付结果，不自动重试通知 |

**关联字段的最小集合。**

| 字段 | 产生者、排除的歧义 | 失效、替代与持久化 |
| --- | --- | --- |
| owner | 发起用例的实例命名空间，区分相同局部序号 | 本地私有栈可隐含；跨 Owner/设备资格时随 RequestId 传递 |
| requestId.value | Owner 受理时分配，区分同槽同类的两次调用 | 终态与证据留存结束后回收；不能用 slotId 或回调地址替代 |
| epoch | H 创建实例时提供，防重新创建后序号重用 | 原实例结束即失效；只阻止旧业务提交，不证明硬件停止 |
| step | 用例挑选真实异步阶段；本地同步节点可隐含 | 步骤推进即失去业务消费资格；原关联仍可供晚证据使用 |
| attempt | 唯一获准的重试方为同一步的实际重发增加 | 无重发固定 0；与 step 不能同时用于命名同一次重试 |
| session | R 在发送时固定应用和有效代次；有资源动作时关联原租约 | 拔卡、重插或受影响的 Refresh 按相应范围失效。纯参数写无卡 session；T 的 BindingRef 与 R 会话不能互代 |
| transport correlation | 原 RIL serial/InnerEvent、适配器回调上下文等定位发出的动作 | 回执消费后按传输协议退役；若已携原 StepRef，复用它。无因果标记的 unsolicited 只作为观察 |
| completion token | 由原适配入口提供回调/等待/内部 route，请求实例持一次性领取状态 | 同步栈可隐含出口。route 本身不能判定“已交付”；回调释放或等待者结束时退役，不另发全局完成 ID |

以上均为运行期关联。跨进程重启如需可靠交付、恢复或外部幂等键，必须有真实协议/存储、隐私和产品需求；当前仅保证原实例内因果关系。

### 协议候选

| 维度 | B1 · 各用例手写回调与布尔位 | B2 · 用例自编排，共用最小关联与消费规则 | B3 · 通用可持久任务机 |
| --- | --- | --- | --- |
| 正确性、可达冲突 | 单同步入口很直接，多步容易重复实现终态和迟到判定 | 逐步消费，交付原子领取；效果由 A/R 核对，可覆盖已见反例 | 可重放步骤，但外设不提供去重时仍可能重复发令 |
| 状态、接口、可读性 | 单步代码最少；多处自行组合标志 | 复用 RequestId/StepRef/CompletionPlan，加各步独立资格，业务分支在原处 | 增加任务图、序列化、版本与恢复服务，简单路径须经引擎 |
| 并发、资源与性能 | 锁域由作者决定，跨用例保证不一致 | 每 Owner 短提交，设备等待锁外；每在途请求留当前步骤及必要未决效果 | 中央队列或持久化会带来额外等待、存储及运维；都未做设备性能测量 |
| 失败与生命周期 | 单同步适用；局部标志不能证明资源可复用 | 超时、终态、租约可独立结束；晚证据回原负责方 | 日志能恢复本地进度，不能证明跨重启硬件执行结果 |
| 可测性、迁移、维护 | 独立纯函数易测，多个用例会重复错例 | 用 WU-04 同步及已有 WU-07 的 B/C1 验证后再提取共性；以同一组反例验收 | 需要跨版本重放、持久 schema 和产品签收，本切片无此需求 |

推荐 B2。同步纯读取或没有回调/未决效果的短动作继续用 B1 的普通调用。只有出现两个真实使用方才提取共同的操作代码；已存在的值与守卫可直接复用。A2 与 B2 的关系是“谁能写”和“哪次结果能推进”，两者不能互代。

## 接口、临界区与代码归属

下列 C++ 风格代码是接口语义草案。返回值是内部结果，不是新的公开错误。A 可用固定成员位集和当前请求身份，不必创建一把参数键锁或新的全局 ID。

```cpp
// role_assignment/assignment_aggregate.*，沿用原 transition 作为持有记录
Admission TryAdmit(RequestId owner, RoleWriteSet members); // Granted / SameOwner / Busy
bool AcceptPrefix(RequestId owner, OutputNode node, OutputEvidence result);
bool ReleaseOrTransfer(RequestId owner, RoleWriteSet members, EndEvidence proof);

// AssignmentService 中调用；绑定资格与已确认写入的节点分别记录
auto facts = topology.ReadPrimaryTarget(command.target.slot);
auto grant = InAssignmentDomain([&] { return roles.TryAdmit(id, PlanWrites(mode)); });
if (grant.sameOwner()) return ReadOriginalRequest(id); // 重复提交不再次执行输出
if (grant.busy()) return MapByMethodContract(grant);  // 公开行为按方法签收
if (!StillEligible(facts, topology.ReadPrimaryTarget(command.target.slot))) {
    return FinishBeforeNewOutput(id);   // 原入口此前可能已写偏好
}
for (auto node : OrderedLegacyOutputs(mode)) {
    auto action = InAssignmentDomain([&] { return PrepareIfOwner(id, node); });
    if (!action) return HandToReconciliation(id);
    auto evidence = InvokeOutsideDomain(action);    // 参数、广播都可能重入
    auto next = InAssignmentDomain([&] { return AcceptNodeAndProjection(id, node, evidence); });
    if (!next.mayContinue()) return FinishOrReconcile(id, next);
}
auto reply = InAssignmentDomain([&] {
    // 已发前缀归属明确、旧动作不能再改变后继；否则移交仍受影响的成员。
    return SealAndClaimOriginalRouteAndSettleGrant(id);
});
DeliverOutsideDomain(reply);
```

```cpp
// 发起 Owner 调用 infrastructure/request 的规则；只对真实异步动作使用
auto issued = InOwnerDomain([&] { return ArmAttempt(id, step, attempt, sentSession); });
if (!issued) return;
auto receipt = port.Send(issued, OnResult);   // 可在 Send 返回前回调
AcceptDispatchEvidence(issued, receipt);     // 同域合并，不覆盖先行消费

void OnResult(OriginalKey key, Evidence evidence)
{
    if (!HasOriginalDispatchEvidence(key)) return;  // 含发送时的成员和传输/会话关联
    RecordOriginalExternalEvidence(key, evidence);   // 幂等，归原 A/R；先核原关联再记录
    auto action = InOwnerDomain([&] {
        if (!MatchesOriginalAndMayCommit(key)) return NoAction();
        if (!ConsumeCurrentAttemptOnce(key, evidence)) return NoAction();
        return DecideNextOrClaimCompletion(key.request, evidence);
    });
    RunOutsideDomain(action); // 完成权已领取；通知、回复或下一次发送在锁外
}
```

上述角色循环描述 C0；C1 在设备阶段间返回，继续持同一业务资格，恢复时先验原资格。B 在设备确认后才写 Primary。PrepareIfOwner 在域内领取单个输出节点；再次进入已发未返回的节点只能观察，不能重发。每键错误及通知失败依原方法解释。

超时/取消/Stop 与正常结果使用同一完成领取入口。端口发送前已证明未移交的错误可在原 Owner 本地消费 NotSent，无需制造设备回包。只有证实本方法应结束时才封存终态；仅停止等待时关闭该 route，稍后业务终结不会二次交付。已领取但尚未投递的回调在 Stop 排空中计为在途；若发送尚未进入传输系统，取消与发起的最后交接需要同一资格检查，无法证明未交接就保留 Unknown。

| 现有位置 | 下一阶段改动的责任 |
| --- | --- |
| role_assignment/assignment_service 与 assignment_aggregate | 从具名业务分支生成写集，登记、逐节点接纳、释放或移交；复用已接入的 roleWriteOwners_，与 transitions_、预留 assignmentConflicts_ 明确各自用途。phase 检查失败后不可继续输出 |
| role_assignment/assignment_store_adapter、输出端口 | 保留原每键操作、散列、返回码和顺序。按需要在现有专用返回载荷提供已尝试节点及可信结果，不增加另一底层写者；void 通知最多证明返回 |
| account/topology_service、binding_index | 提供当前资格及失效原因。按原目标复核；T 不获取 A 资格，也不把账户锁跨越参数/Radio I/O |
| infrastructure/request、infrastructure/compatibility | 已有原步骤消费位、终态守卫、一次领取与晚记录，继续验证逐步推进及核对后的回收；C 使用原三种出口。无需多层完成转发，C0 无设备步骤可直接执行具名分支 |
| card/state、card/records、device/resource | 验发送时 session、scope/member 与原资源键；R 核对设备结束和可复用证据。迟到只交原债务，Stop 封入口和保活以宿主协议为界 |

## 不变量与可执行验收

下列 I1～I6 是本文断言的行号，A/B 是专题，均不新增项目功能或迁移编号。纯规则中的事件顺序等价于在同一 Owner 短提交域内依次接纳输入；运行结果不证明真实线程排空。

| 不变量 | 正例 | 最小反例、违反条件 | 守卫与可执行见证 |
| --- | --- | --- | --- |
| I1 单次完成：每一有效 route 领取次数 ≤ 1 | 正常完成先行，随后 timeout/cancel/Stop 均不再投递 | 两个事件先读 Open，锁外回调后才设置终态，可能双发 | 同域封存及取走 route，锁外投递。附录模型遍历四个竞争者 24 种顺序，覆盖终态变异 |
| I2 步骤隔离：每个原 step/attempt/member 至多消费一次 | 步 1 与步 2 各确认一次 | 步 1 的 effect 充当步 2 的已消费位；NotSent 兼任“还没结果” | 本步单独的消费资格和原始关联。附录 Host 容器见证、两步模型及 carry_consumed 变异 |
| I3 终态单调：离开 Open 之后业务终态不可改写 | 已结束请求收到设备成功，仅补原效果 | MarkTerminal(Succeeded) 后 MarkTerminal(Failed)；第二次覆盖首个结果 | 终态单次接纳，晚证据路由原外部操作。附录实测覆写与模型负控 |
| I4 副作用认知：证据不能从可能已发送降为未发送 | 未调用端口的校验失败为 NotSent；已调用无结果为 Unknown | 发送后超时被当作未发送，或把设备 ACK 当作已应用 | 发前登记、区分技术收件与效果证据；模型拒绝无证明的 NotSent；参数部分结果用真实 Adapter 验 |
| I5 安全重试：同一层有且只有一个授权重试方 | 确认无效果、旧设备动作已结束、原目标/资格仍有效才增 attempt | 旧操作可能已应用，因本地清零再次发命令；或把旧 attempt 回包当作新步骤 | 检查效果、幂等/去重合同、结束或隔离、有效绑定、资格及预算；模型拒绝 Unknown 重发与旧尝试 |
| I6 资源独立：设备可复用须由原 lease 的结束证据决定 | 业务超时，资源隔离；核对后第二请求取得新资格 | 完成/Stop 直接清设备占用；旧释放动作错误释放后继租约 | R 原键/fence、实际结束/可复用及相应效果条件；模型持旧租约跨超时，拒绝错释放 |

**测试层与定位。** 表中“早期观察或未知”记录最初两个输入的观察，已修复项按[阶段九复验](#阶段九复验--同样的输入与事件顺序)解释；目标断言贯穿修复前后的同一调度。以下用例以 `SIM028-LEGACY-C0-DUAL-PRIMARY0` 为差分基准。表中的后缀是排程标签，供实现者参数化已有案例；超出 C0 的设备步骤放到 WU-07，绑定/会话复用已有 T/R 规则。L1 纯规则；L2 真实类+受控端口；L3 确定性屏障/锁外回调；L4 Legacy/Target 同一冻结 Oracle；L5 匹配产品。前四层允许 Host；“设备”列表示哪部分结论必须由 L5 给出。新增策略的断言只用于待批准 Target，不能反写 Legacy Golden。

| 调度标签 / 不变量 | 输入与确定性顺序 | 早期观察或未知 | 推荐断言及失败定位 | 后端/设备 |
| --- | --- | --- | --- | --- |
| single / I1 | C0 已 M0，正常参数返回 | 真实 C0 可正常完成，无设备切换 | 逐键、主卡→Data→完成及返回与既有 Oracle 比较；差异定位 A/C | L2、L4 Both；产品参数行为 L5 |
| multi / I2 | Open，步 1 已应用→推进→步 2 合法响应 | 旧容器判 Duplicate，修订后已复位，可接纳步 2 | 两步分别消费，第二次同一步无效；定位 O | L1、L2 Target；无需设备 |
| old-step / I2 | 已进入步 2，再到步 1 结果 | 旧 StepRef 被拒；旧版残留 effect；修订后要检查真实 phase/member 而非继续报告该旧问题 | 业务不动，原 lease 仅接受匹配的事实；定位 O/R | L1、L3 Target；设备因果 L5 |
| duplicate / I1、I2 | 同一回执两次，包括 NotSent 和错误 | NotSent 可两次 Accepted，其他已记效果会挡重复 | 正/错只消费一次，最多一个出口；定位 O | L1、L2 Target |
| before-send / I4、I5 | 端口移交前失败→同一步重发 | C0 不发设备；修订后有 RecordSent，记录原 SessionRef；该字段本身无法证明设备收件 | 可证明未发时登记 NotSent 并单次消费；重发 attempt+1；定位发送边界 | L1、L2 Target；真实驱动未接收证明 L5 |
| timeout / I4 | 发送返回未证→预算到期 | C0 参数未完成会保留资格；修订后 B 有 45000 ms 受控超时入口，产品计时与设备结束另验 | 无 NotSent 推断，无无条件重发，方法级等待出口按原契约；定位 A/O | L1、L3 Target；预算与真实效果 L5 |
| after-timeout / I3、I6 | A 超时→B 求同资源→旧设备回报→证实可复用 | 内存屏障能拒绝无结束证据，产品状态未知 | B 在有证据前不得触碰同资源；旧业务不二次完成；定位 R/调用者 | L1、L3 Target；设备必验 |
| swap / I2、I4 | A 二读前换卡；另一次在 P1 通知后换卡 | 前者被真实 T/A 拦住；后者 Host 仍产生旧订阅 Data 通知 | 分别断言无写、已写前缀、旧/新订阅归属；中止剩余动作或维持原序须另签；定位 T/A | L2、L3 Target；产品投影 L5 |
| reinsert / I2 | 拔出再插同卡→旧 BindingRef/SessionRef 回来 | 真实 T 夹具验证 ABA；相同 ICCID/slot 仍会有新绑定 | 匹配原 revision/session，旧结果不得写新卡；定位 T/R | L1、L2 Target；物理事件 L5 |
| refresh / I2 | 同应用 EF 读在途→Refresh 失效该 scope→新读→旧成员晚到 | LoadSession 有 scope/generation/member 规则；非所有 Refresh 都换卡 | 只拒绝受影响成员，无关 EF 正常；定位 card/records | L1、L3 Target；厂商 Refresh 范围 L5 |
| retry / I2、I5 | 步 1 未发→获准重试→attempt0 晚到→attempt1 回来 | AdvanceStep 拒绝同 step 增 attempt；B/C1 的业务重试须检查该分支和批次，通用同 step 重试仍受 nextStep 守卫限制 | 步号保持，原 attempt 只进核对；Unknown、余额耗尽或资格过期均禁止重发；定位 O/A/R | L1、L3 Target；设备幂等协议 L5 |
| cancel / I1、I3 | 同一个 route 的取消及成功分别先行 | 修订后 EndWait 可直接改写 wait，缺少出口一次领取的共同守卫 | 按方法是否仅取消等待分类；终结型两种次序均最多一次交付，锁外回调可读状态；定位 O/C | L1、L3 Target；取消政策待签 |
| stop / I1、I6 | Stop 封入口→回调取得旧 token，或回调先领取并暂停→Stop | 产品排空未知；Host Stop 历史结果仅属原入口 | 新请求拒绝，已领取的回调保活直至退出，旧证据可交 R，未证明结束不卸载；定位 H/C/R | L3 Target；宿主与设备必验 |
| partial / I4、I5 | U 完成→P 槽成功→P 散列未返回；另测所有参数调用完成但报错 | 真实 Adapter 区分完成错误与未完成；合并结果不足以反推逐键实际值 | 记录准确前缀，保留需核对的资格；不得重发已发布通知或改旧返回；定位 Store/A | L2、L3 Target、L4 已冻结行为；参数服务 L5 |
| notify / I1、I4 | P1 通知内执行 B；另测发布失败/仅报告已提交 | 合成双域产生 P1/P0/D0/D1；当前 void 输出无法看到送达失败 | 冲突时不得同栈等待；同键按签收策略处理，无关 Voice 可继续。失败按方法既有映射，无投递回执不得伪造。定位 A/C/I | L2、L3 Target、L4 已签行为；产品重入/送达 L5 |

**五层放行顺序。** 先用附录在仓外复核反例与纯规则；用现有 TargetTopologyIntegration、TargetPrimary 及 TargetAdapterIntegration 接入可控参数/通知关口；在写前、两键之间、通知内、完成领取后暂停，分别运行两个顺序，禁止 sleep。按测试入口执行 `test.py run --backend both --filter SIM028-LEGACY-C0-DUAL-PRIMARY0` 比较冻结参数、事件、公开视图、调用结果与设备命令计数。最后在匹配产品记录同一原始请求、域/路由、写者、每键调用结果、通知提交、设备 commandEnded/resourceReusable 证据。缺产品证据时只可放行已测 Host 切片。

**实施与签收。** ① WU-04 先枚举所有旧写者及能同时到达的分支，复核标准/合成/同域三种排程。② A 在既有聚合原子登记实际成员，先验证不相关角色可并行，已发前缀可核对。其中最小 Host 路径已在 bcc1614fd 验证，其他真实写入口仍需清点。③ WU-07 用现有 B 和 C1 的多步、错误、重试路径验统一守卫及单次出口。④ 在 R 的隔离与原操作结束证据上联验“超时→后继→晚回包”。⑤ 同一 WU 全部旧写者可退出、Both 差分通过、所涉产品证据闭合才能切生产。宿主 Stop 排空/共享 RIL/卸载与硬件可复用仍是独立 P0。

| 待批准的用户可见选择 | 需要的证据与签收方 |
| --- | --- |
| Busy 是否拒绝、等待或排队，公平性、预算与错误码 | 方法级 Legacy 入口、既有权限/错误优先级、重入及调用线程；业务接口 Owner 和产品签收 |
| 绑定在首写后变化时继续、停止还是核对，已有通知何时算提交 | 每键/每事件旧轨迹、账户消费者行为、输出通道失败可见性；T/A、消费者与产品签收 |
| timeout/cancel/Stop 是否只结束等待、何时调用原出口 | 旧同步等待与回调语义、已领取未投递的对象寿命；Owner 与宿主签收 |
| 自动重试、补偿、跨重启恢复、设备何时可再次分配 | 命令幂等/可靠失效、原设备终止及可复用、恢复预算；R、设备、产品签收 |


## 第二轮反向审查

以标准双卡 C1/C0 而非合成双 M0 再推导一次设计。串行化最后的投影赋值仍会留下旧预写参数；把所有 SIM 业务放入一个队列则会无故挡住独立角色。以下限制已写回推荐协议。它们适用于受影响的切片，尚待产品签收。

| 对推荐方案的质疑 | 反例、裁决与验收办法 |
| --- | --- |
| ① 会演变成全局互斥吗？ | C1 预写 P 后等待期间，独立 Voice 字段仍应能提交；独立 Data 会与此次 D 共同输出冲突。一次原子登记经核实的成员，字段隔离未知的 Provider 先取证。运行 role_checks 验独立与冲突两个方向。 |
| ② 四种寿命是否被重新合并？ | 已交付请求失败后，旧契约若仍要求后台恢复，保留原设备操作和 A 的恢复资格。重试后可以更新效果或收尾，不能重开已交付的出口。多成员步骤收到第一个 ACK 也不能结束全请求。 |
| ③ 每层是否都要一个版本和一把锁？ | 同一 Owner 的短提交域可复用，私有同步调用可隐含身份。传输已带原 StepRef 时不再签发关联号。只有设备或会话确有独立寿命时才携 session/fence。 |
| ④ “已写前缀”能否证明参数原子可见？ | 第一键已改、第二键写入未知，或订阅者尚未收件，都属于合法观察。A 按原调用分别记录并复核。单次读回相同值无法证明旧命令已停止，也不赋予覆盖新选择的资格。 |
| ⑤ 通知或完成重入会重发、死锁吗？ | 同一 id 再受理只查询原状态；每个输出在调用端口前声明已在途。完成先领取后出锁，重入不同 id 只尝试完整写集；同步等待前任在同一调用栈上不可行。 |
| ⑥ 请求超时是否错误地让设备可复用？ | 反排“超时→后继同资源→原操作结束”。R 验原关联、命令结束和可复用；A 在能证明没有旧写动作后才释放相应成员。旧结果可进核对，不能恢复业务终态。 |
| ⑦ 是否暗中改变拒绝、等待、通知或重试？ | 技术 Busy 不直接映射新增错误码，通知按原分支偏序；超时返回、后台恢复、排队和自动重试分别签收。差分中一个已批准出口对应一次交付。 |
| ⑧ 更简单的实现何时足够？ | 单成员、本地且不会重入的 setter 在 A 内一次提交即可。C0 有外部参数和通知，需保留已发节点；它没有步骤响应，不建 R 租约。B/C1 才保留真正异步的原步骤。 |
| ⑨ 普通 C++ 开发者能否安全实现？ | A 只编排具名分支，O 负责一次消费和出口领取，R 保管原租约。返回锁外动作前复制所需数据并保活，禁止把 map 迭代器或可失效引用带过可重入端口。 |
| ⑩ 能否测出错误实现？ | 固定暂停点复现 C1/C0，分别检查参数、投影和两次返回；模型分别移除终态守卫、带入前步已消费位，两个请求变异均失败。另枚举保持 C1/C0 内部顺序的 20 种三节点排程；移除写集守卫后有 13 种得到混合输出，加回守卫时能完整执行的两种顺序均一致。被拦截的调度如何映射到客户端仍取决于契约。模型只检验 Owner 域中的顺序，跨线程排空与设备证据保留在产品门禁。 |

## 实验记录与复核方法

| 输入 | 方法 | 实际结果/限度 |
| --- | --- | --- |
| eea678d67 | 宿主 g++/GoogleTest，从当前 GN 的 sim_target_production_host 清单编译十个业务源，复用原拓扑夹具，增加锁外记录端口 | 原 4 项加定点观察 5 项，9/9。观察程序以已知问题为预期；通过表示复现成功。 |
| eea678d67 | 官方 `test.py run --backend both --filter SIM028-LEGACY-C0-DUAL-PRIMARY0`，指定两个相关 target，在仓外用依赖锁匹配的 GoogleTest、OpenSSL/native 重新构建 | Legacy 与 Target 各执行 1 项，差分 Oracle 与两侧比较 PASS，差异 0。该次使用默认 single-active，单列的双激活扩展契约显示 BLOCKED。后续明确传入双激活初态重跑。Target 同一新二进制经官方逐用例进程运行 16/16。一开始复用旧工作机二进制因 Target 零匹配返回 BLOCKED，改用源码重建后取得上述结果。 |
| eea678d67 | `run_architecture.py --with-gn --migration`；检查点恢复工具加 `--remote origin --fetch` | 结构/迁移/目录、声明负控、语法、四组纯规则、GN 骨架通过；CHECKPOINT_VERIFIED。 |
| ba95cf2c0（与 9c4c82fef 同业务源码） | 按提交对象核实 10 个编译单元及所用模块头文件；用编译器输入覆盖正在由其他任务修改的少数文件，仅作仓外验证 | 原拓扑 4 项及“单 M0 的 C1/C0”“修订容器”2 项共 6/6；P 参数=1、D 参数=0、lastPrimary=lastData=0。两步均可接纳，NotSent 双消费、终态覆写仍可观察。该输入的设备和全量回归未在本实验运行。 |
| bcc1614fd | 相同实际业务类、目录及回包顺序，固定提交的 GN 输入，附录开启已修复断言 | 4 个原夹具＋2 个协议探针共 6/6；旧混合输出不可达，两个完成时点、逐步消费和晚证据分别可观测。 |
| 两个工作批次间的架构检查 | 首次从含未提交 RadioSwitchResult.notify 变更的工作区执行架构脚本 | runtime_protocol_rules.cpp 的旧聚合初始化未传 notify，编译失败。固定 ba95cf2c0 的业务编译输入复查通过；随后 3c5a974c0 同步了初始化。M9.3 工作区再次运行 `run_architecture.py --with-gn --migration`，90 个 C++ 单元、17 项结构测试、4 组纯规则与 25 个 testonly 目标通过。 |
| M9.3 工作树（启动时 HEAD=bcc1614fd） | 官方入口源码构建 Both C0，以同一二进制运行 Target 全套 | Target 38/38。显式传入 `--init-model dual-active-primary0`，对 C0、C1 SUCCESS-STALE、C1 CHECK-ERROR-RETRY 分别执行 Both；三例各一项 Legacy 与 Target，扩展业务契约、Oracle、双侧比较均 PASS，差异 0。此行使用含调度器的工作树，区别于上一行按 Git 对象固定的 6/6。 |
| 设计模型 | 附录 Python，按 owner 提交顺序穷举成功、超时、取消、Stop 四事件的排列 | 24 种顺序，各出口最多领取一次；不同写集可并行、部分移交、步骤隔离、拒绝无据重试和原租约使用通过；故意携带前步消费位或允许重写终态，2/2 请求变异被命中。另将每次角色写缩为三个保持原顺序的节点；完整共享资格下两种串行顺序均一致，移除资格则 20 种排程中有 13 种得到混合结果。此计数不表示产品发生概率。属于有限模型，未验证所有 C++ 线程/设备行为。 |

下列附录保留原始见证和最小规则模型，便于在相同提交、带 C++17 及 GoogleTest 的 Linux 上复核。编译读取被检工作区，输出放在仓外。若目标源码早于本文，可从包含本轮修订的 Git 提交导出本文到仓外，在下列命令中将 review 指向该文件。`old` 选择 eea678d67 的见证，`current` 检验 ba95cf2c0 的 B/C1 见证，`fixed` 检验 bcc1614fd 之后的保护。它们只使用合成卡身份和受控设备端口。

```sh
# 在仓库根执行。fixed、current、old 分别对应上表的三个源码输入。
out=$(mktemp -d)
review=telephony_core_service/docs/history/reviews/SIM_Role_Concurrency_and_Async_Request_Review_20261119.md
variant=fixed
python3 - "$out" "$variant" "$review" <<'PYTHON'
from pathlib import Path
import re, sys
s=Path(sys.argv[3]).read_text()
out=Path(sys.argv[1])
for name, tag, lang in [('observe.cpp','current' if sys.argv[2]=='fixed' else sys.argv[2],'cpp'),('build.py','build','python'),('rules.py','model','python')]:
    marker='<!-- source '+tag+' -->'
    block=s.split(marker+'\n```'+lang+'\n',1)[1].split('\n```',1)[0]
    (out/name).write_text(block+'\n')
PYTHON
python3 "$out/build.py" "$out" "$variant"
python3 "$out/rules.py"
```

<details><summary>单 M0 与步骤隔离的修订基线观察</summary>

<!-- source current -->
```cpp
#include "test/cases/target/primary/c0_topology_integration_test.cpp"

// Older C1 ports have an implicit phase-2 notification, newer ones carry it.
template<class T> auto SetNotify(T& r, bool n, int) -> decltype(r.notify=n, void()) { r.notify=n; }
template<class T> void SetNotify(T&, bool, long) {}
namespace {
struct PerSlotRadio final : RadioExecutionPort {
    std::vector<RadioSwitchRequest> sent;
    PrimaryRadioObservation ReadPrimaryRadioObservation(SlotId s) const override {
        return {s, s.legacySlotId == 1 ? 0 : 1, false, false, 9};
    }
    void SwitchPrimaryRadio(const RadioSwitchRequest& r) override { sent.push_back(r); }
    void EndLegacySenseSwitch(const StepRef&,const RadioDomainId&) override {}
};
TEST(ReviewCurrent, C1AndC0ShareOutputWithOneM0)
{
    Fixture f;
    PerSlotRadio radio;
    AssignmentService a(f.catalog,f.store,radio,f.output,f.topology,f.completion,{5,9});
    auto c1=Command(f.seed(0,{2},kRaw0));
    auto c0=Command(f.seed(1,{1},kRaw1));
    c0.completion.route=SynchronousCompletionRoute{SynchronousWaitRef{32}};
    a.StartPrimarySwitch(c1);
    ASSERT_EQ(radio.sent.size(),2u);
    EXPECT_TRUE(f.completion.results.empty());
    a.StartPrimarySwitch(c0);
#ifdef VERIFY_SHARED_ROLE_GUARDS
    ASSERT_EQ(f.completion.results,(std::vector<int32_t>{-1}));
    EXPECT_EQ(f.parameters.values[kPrimary],"0");
#else
    ASSERT_EQ(f.completion.results,(std::vector<int32_t>{0}));
    EXPECT_EQ(f.parameters.values[kPrimary],"1");
#endif
    for (size_t n=0;n<6;++n) {
        ASSERT_LT(n,radio.sent.size());
        const auto command=radio.sent[n];
        RadioSwitchResult reply{};
        reply.step=command.step;
        reply.memberSlot=command.target;
        reply.expectedBinding=command.expectedBinding;
        reply.protocolSession=command.protocolSession;
        reply.phase=command.phase;
        SetNotify(reply,command.phase==2,0);
        reply.effect=EffectKnowledge::ConfirmedApplied;
        reply.legacyResult=0;
        a.AcceptRadioStep(reply);
    }
    ASSERT_EQ(radio.sent.size(),6u);
#ifdef VERIFY_SHARED_ROLE_GUARDS
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{-1,0}));
    EXPECT_EQ(f.parameters.values[kPrimary],"0");
    EXPECT_EQ(f.parameters.values[kIccid],"5BC332BC693934EB3D0DEAED37C106D2DEFD6F9767721BFE2B1AC6E209D6B67D");
    EXPECT_EQ(f.parameters.values[kData],"0");
    EXPECT_EQ(a.ReadLegacyAssignment().lastPrimary,(SlotId{0}));
    EXPECT_EQ(a.ReadLegacyAssignment().lastData,(SlotId{0}));
    EXPECT_EQ(f.output.events,(std::vector<std::string>{"primary","data"}));
#else
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0,0}));
    EXPECT_EQ(f.parameters.values[kPrimary],"1");
    EXPECT_EQ(f.parameters.values[kIccid],kEncoded1);
    EXPECT_EQ(f.parameters.values[kData],"0");
    EXPECT_EQ(a.ReadLegacyAssignment().lastPrimary,(SlotId{0}));
    EXPECT_EQ(a.ReadLegacyAssignment().lastData,(SlotId{0}));
    EXPECT_EQ(f.output.events,(std::vector<std::string>{"primary","data","primary","data"}));
#endif
}
TEST(ReviewCurrent, Container)
{
    RuntimeOperationStore r({5,9},RequestOwner::Assignment);
    auto id=r.Begin(SlotTarget{SlotId{0}},{SynchronousCompletionRoute{SynchronousWaitRef{7}},false});
    using R=RuntimeOperationStore::ResultAdmission;
    EXPECT_EQ(r.AcceptResult({id,0,0},EffectKnowledge::NotSent),R::Accepted);
#ifdef VERIFY_SHARED_ROLE_GUARDS
    EXPECT_EQ(r.AcceptResult({id,0,0},EffectKnowledge::NotSent),R::Duplicate);
#else
    EXPECT_EQ(r.AcceptResult({id,0,0},EffectKnowledge::NotSent),R::Accepted);
#endif
    ASSERT_TRUE(r.AdvanceStep({id,0,0},1,0,std::nullopt));
    EXPECT_EQ(r.AcceptResult({id,1,0},EffectKnowledge::ConfirmedApplied),R::Accepted);
    EXPECT_FALSE(r.AdvanceStep({id,1,0},1,1,std::nullopt));
    ASSERT_TRUE(r.AdvanceStep({id,1,0},2,0,std::nullopt));
    EXPECT_EQ(r.AcceptResult({id,2,0},EffectKnowledge::ConfirmedApplied),R::Accepted);
    r.MarkTerminal(id,TerminalState::Succeeded);
    r.MarkTerminal(id,TerminalState::Failed);
#ifdef VERIFY_SHARED_ROLE_GUARDS
    EXPECT_EQ(r.Read(id)->terminal,TerminalState::Succeeded);
    EXPECT_TRUE(r.ClaimCompletion(id));
    EXPECT_FALSE(r.ClaimCompletion(id));
    auto late=r.Begin(SlotTarget{SlotId{1}},{SynchronousCompletionRoute{SynchronousWaitRef{8}},false});
    const StepRef original{late,0,0};
    ASSERT_EQ(r.AcceptResult(original,EffectKnowledge::Unknown),R::Accepted);
    ASSERT_TRUE(r.TryMarkTerminal(late,TerminalState::NeedsReconciliation));
    EXPECT_FALSE(r.AcceptLateEvidence({late,1,0},EffectKnowledge::ConfirmedApplied));
    EXPECT_TRUE(r.AcceptLateEvidence(original,EffectKnowledge::ConfirmedApplied));
    EXPECT_EQ(r.ReadLateEvidence(late),EffectKnowledge::ConfirmedApplied);
    EXPECT_EQ(r.Read(late)->terminal,TerminalState::NeedsReconciliation);
#else
    EXPECT_EQ(r.Read(id)->terminal,TerminalState::Failed);
#endif
}
}
```

</details>

<details><summary>初始基线与三种目录的对照观察</summary>

<!-- source old -->
```cpp
#include "test/cases/target/primary/c0_topology_integration_test.cpp"
#include <utility>

namespace {
struct Notice : AssignmentOutputPort {
    std::vector<std::string> trace;
    std::function<void()> next;
    void PublishPrimaryPhase(bool, bool) override {}
    void PublishMainCardChanged(SlotId slot) override {
        trace.push_back("P" + std::to_string(slot.legacySlotId));
        auto callback = std::exchange(next, {});
        if (callback) callback();
    }
    void PublishDefaultDataChanged(SlotId slot, SubscriptionId sub) override {
        trace.push_back("D" + std::to_string(slot.legacySlotId) + "/" + std::to_string(sub.legacySimId));
    }
};

void ObserveNested(int ril0, const char *domain0, bool bothAdmitted)
{
    Fixture f {SlotCatalog {{
        {SlotId {0}, EndpointKind::Physical, ril0, RadioDomainId {domain0}, VisibilityClass::Ordinary, false},
        {SlotId {1}, EndpointKind::Physical, 0, RadioDomainId {"modem-0"}, VisibilityClass::Ordinary, false}},
        {2, 2, 2, 2, 2}}};
    Notice out;
    AssignmentService a(f.catalog, f.store, f.radio, out, f.topology, f.completion, {5, 9});
    auto b = Command(f.seed(0, {2}, kRaw0));
    b.completion.route = SynchronousCompletionRoute {SynchronousWaitRef {32}};
    auto first = Command(f.seed(1, {1}, kRaw1));
    out.next = [&] { a.StartPrimarySwitch(b); };
    a.StartPrimarySwitch(first);
    EXPECT_EQ(f.completion.results, (std::vector<int32_t>{bothAdmitted ? 0 : -1, 0}));
    EXPECT_EQ(out.trace, bothAdmitted ? (std::vector<std::string>{"P1","P0","D0/2","D1/1"})
                                     : (std::vector<std::string>{"P1","D1/1"}));
    EXPECT_EQ(f.parameters.values[kPrimary], bothAdmitted ? "0" : "1");
    EXPECT_EQ(f.parameters.values[kData], "1");
    EXPECT_EQ(f.parameters.writes.size(), bothAdmitted ? 8u : 4u);
    EXPECT_EQ(a.ReadLegacyAssignment().lastPrimary, (SlotId {bothAdmitted ? 0 : 1}));
    EXPECT_EQ(a.ReadLegacyAssignment().lastData, (SlotId {1}));
    EXPECT_EQ(f.radio.switches, 0u);
}
TEST(ReviewObservation, StockCatalog) { ObserveNested(1, "modem-1", false); }
TEST(ReviewObservation, TwoEligibleDomains) { ObserveNested(0, "modem-1", true); }
TEST(ReviewObservation, SameDomain) { ObserveNested(0, "modem-0", false); }
TEST(ReviewObservation, RemovalAfterPrimaryNotice)
{
    Fixture f;
    Notice out;
    AssignmentService a(f.catalog, f.store, f.radio, out, f.topology, f.completion, {5,9});
    const auto old = f.seed(1,{1},kRaw1);
    out.next = [&] { f.topology.AcceptCardRemoval(old); f.seed(1,{3},"replacement"); };
    a.StartPrimarySwitch(Command(old));
    ASSERT_TRUE(f.topology.ReadPrimaryTarget({1}).has_value());
    EXPECT_EQ(f.topology.ReadPrimaryTarget({1})->binding.subscription, (SubscriptionId {3}));
    EXPECT_EQ(out.trace,(std::vector<std::string>{"P1","D1/1"}));
    EXPECT_EQ(f.completion.results,(std::vector<int32_t>{0}));
}
TEST(ReviewObservation, OperationStoreSemantics)
{
    RuntimeOperationStore ops({5,9}, RequestOwner::Assignment);
    const CompletionPlan cp {SynchronousCompletionRoute {SynchronousWaitRef {1}},false};
    const auto req=ops.Begin(SlotTarget {SlotId {1}},cp);
    using Admission=RuntimeOperationStore::ResultAdmission;
    EXPECT_EQ(ops.AcceptResult({req,0,0},EffectKnowledge::NotSent),Admission::Accepted);
    EXPECT_EQ(ops.AcceptResult({req,0,0},EffectKnowledge::NotSent),Admission::Accepted);
    ASSERT_TRUE(ops.AdvanceStep({req,0,0},1,0,std::nullopt));
    EXPECT_EQ(ops.AcceptResult({req,1,0},EffectKnowledge::ConfirmedApplied),Admission::Accepted);
    EXPECT_FALSE(ops.AdvanceStep({req,1,0},1,1,std::nullopt));
    ASSERT_TRUE(ops.AdvanceStep({req,1,0},2,0,std::nullopt));
    EXPECT_EQ(ops.AcceptResult({req,2,0},EffectKnowledge::ConfirmedApplied),Admission::Duplicate);
    ops.MarkTerminal(req,TerminalState::Succeeded);
    ops.MarkTerminal(req,TerminalState::Failed);
    EXPECT_EQ(ops.Read(req)->terminal,TerminalState::Failed);
    EXPECT_EQ(ops.Read(req)->wait,WaitState::Waiting);
}
}
```

</details>

<details><summary>从 GN 清单编译观测程序</summary>

<!-- source build -->
```python
from pathlib import Path
import re,subprocess,os,sys
m=Path('telephony_core_service/services/multisim')
gn=(m/'test/framework/internal/host/BUILD.gn').read_text()
units=re.findall(r'"\$production_root/services/multisim/([^"]+\.cpp)"',gn.split('static_library("sim_target_production_host") {')[1].split('\n}')[0])
assert units and len(units)==len(set(units))
out=Path(sys.argv[1])
flags=['-DVERIFY_SHARED_ROLE_GUARDS'] if sys.argv[2]=='fixed' else []
subprocess.run(['g++','-std=c++17','-O0',*flags,'-I'+str(m),str(out/'observe.cpp'),*(str(m/u) for u in units),'-pthread','-lgtest_main','-lgtest','-o',str(out/'observe')], check=True)
subprocess.run([str(out/'observe')],check=True)
```

</details>

<details><summary>独立可执行的建议协议模型</summary>

<!-- source model -->
```python
"""Finite, single-owner reference model. Events are serialized at the owner boundary."""
from itertools import permutations

class Request:
    def __init__(self, mutation=None):
        self.key = (1, 0)  # step, attempt; one owner/request/epoch in this model
        self.armed = False
        self.used = False
        self.terminal = None
        self.deliveries = []
        self.history = {}
        self.mutation = mutation

    def arm(self):
        assert not self.armed and not self.used and self.terminal is None
        self.armed = True  # port can now call us synchronously

    def consume(self, key, result, *, not_handed_off=False):
        if key != self.key or self.terminal is not None or self.used:
            return False
        if result == 'NotSent':
            if self.armed and not not_handed_off:
                return False
        elif not self.armed:
            return False
        self.used = True
        self.history[key] = result
        return True

    def advance(self, retry=False, *, safe=False):
        if not self.used or self.terminal is not None:
            return False
        result = self.history[self.key]
        if retry and (not safe or result not in ('NotSent', 'NotApplied')):
            return False  # deliberately tests only the proved-no-effect retry branch
        if not retry and result != 'Applied':
            return False  # the successful two-step use case
        step, attempt = self.key
        self.key = (step, attempt + 1) if retry else (step + 1, 0)
        self.armed = False
        self.used = self.mutation == 'carry_consumed'
        return True

    def finish(self, event):
        if self.terminal is not None and self.mutation != 'overwrite_terminal':
            return
        self.terminal = event
        self.deliveries.append(event)

class Resource:
    def __init__(self):
        self.holder = None

    def acquire(self, holder):
        if self.holder is not None:
            return False
        self.holder = holder
        return True

    def release(self, holder, *, ended, reusable, effect_known):
        if self.holder != holder or not (ended and reusable and effect_known):
            return False
        self.holder = None
        return True

def checks(mutation=None):
    for order in permutations(('success', 'timeout', 'cancel', 'stop')):
        r = Request(mutation)
        for event in order:
            r.finish(event)
        assert r.deliveries == [order[0]] and r.terminal == order[0], 'completion'
        assert not r.consume((1, 0), 'Applied'), 'terminal'
    r = Request(mutation)
    r.arm()
    assert r.consume((1, 0), 'Applied') and not r.consume((1, 0), 'Applied')
    assert r.advance()
    assert not r.used, 'step isolation'
    r.arm()
    assert not r.consume((1, 0), 'Applied')
    assert r.consume((2, 0), 'Applied'), 'step isolation'
    r = Request(mutation)
    assert r.consume(r.key, 'NotSent') and not r.consume(r.key, 'NotSent')
    assert not r.advance(retry=True) and r.advance(retry=True, safe=True)
    r.arm()
    assert not r.consume(r.key, 'NotSent')
    assert not r.consume((1, 0), 'Applied') and r.consume((1, 1), 'Unknown')
    assert not r.advance(retry=True, safe=True)
    lease = Resource()
    assert lease.acquire('old')
    r.finish('timeout')
    assert not lease.acquire('new')
    assert not lease.release('old', ended=True, reusable=False, effect_known=True)
    assert lease.release('old', ended=True, reusable=True, effect_known=True)
    assert lease.acquire('new')
    assert not lease.release('old', ended=True, reusable=True, effect_known=True)

def role_checks():
    owners = {}
    def admit(request, members):
        if any(unit in owners and owners[unit] != request for unit in members):
            return False
        owners.update((unit, request) for unit in members)
        return True
    def retire(request, members, unresolved):
        for unit in members - unresolved:
            if owners.get(unit) == request:
                del owners[unit]
    group = {'preference', 'primary pair', 'data'}
    assert admit('one', group) and admit('one', group)
    assert admit('one', {'data'}) and owners['primary pair'] == 'one'
    assert not admit('two', {'data', 'sms'}) and 'sms' not in owners
    assert admit('voice-writer', {'voice'}) and admit('sms-writer', {'sms'})
    retire('one', group, {'data'})
    assert not admit('two', {'data'})
    retire('one', {'data'}, set())
    assert admit('two', {'data'})
    retire('one', {'data'}, set())
    assert owners['data'] == 'two'

role_checks()
checks()
for mutation, expected in [('carry_consumed','step isolation'), ('overwrite_terminal','completion')]:
    try:
        checks(mutation)
    except AssertionError as failure:
        assert str(failure) == expected
    else:
        raise AssertionError('mutant survived: ' + mutation)
print('24 exit races; step, effect, retry and lease checks; 2/2 request mutants detected')

from itertools import combinations

def output_schedules(guarded):
    # C1: prewrite -> observe phase completion -> commit the linked Data.
    # C0: save Primary -> publish Primary -> commit its linked Data.
    serial_results = {(0, 0, 0), (1, 1, 1)}
    completed = mixed = 0
    for c1_turns in combinations(range(6), 3):
        steps = [0, 0]
        primary = data = last_primary = None
        owner = None
        for turn in range(6):
            slot = 0 if turn in c1_turns else 1
            if guarded and owner not in (None, slot):
                break  # conflicting action cannot execute now; external Busy policy is unspecified
            owner = slot
            step = steps[slot]
            if step == 0:
                primary = last_primary = slot
            if step == 2:
                data = slot
                if slot == 0:  # only C1 accepts its prewritten Primary again at completion
                    last_primary = slot
                owner = None
            steps[slot] += 1
        else:
            completed += 1
            mixed += (primary, data, last_primary) not in serial_results
    return completed, mixed

assert output_schedules(True) == (2, 0)
total, mixed = output_schedules(False)
assert (total, mixed) == (20, 13)
print(f'role output schedules: 2 complete legal orders; removing the grant yields {mixed}/{total} mixed results')
```

</details>
