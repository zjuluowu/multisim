# T1 账户发现、Provider 单次持久化与 T/A 接纳报告

输入基线：`be63ae1249db5927a216602aceb0346db6d53a09`。本轮保持 production 默认写权在
`services/sim/`，没有修改 SA/Core 产品装配、ISimManager/IDL 或共享 RIL 生命周期。

## 1. Legacy 账户发现真实轨迹（VERIFIED_SOURCE）

1. `MultiSimMonitor::InitData(slotId)` 接收卡文件/ICCID 变化，先验证槽、卡态和
   `isSimAccountLoaded_`；`MultiSimController::InitData(slotId)` 是账户建行入口。
2. `MultiSimController::InitData`：`InitIccId` → `GetListFromDataBase(false)` →
   `GetAllListFromDataBase(false)` → `InitActive` → `InitShowNumber` → `InitPrimary`。
3. `MultiSimController::InitIccId` 持 `writeDbMutex_`，通过
   `SimRdbHelper::QueryDataByIccId(newIccId, SimRdbInfo&)` 匹配 ICCID。
   - Provider 连接/查询失败为 `INVALID_VALUE`，整次建行 false，并由 Monitor 按既有预算投递
     `INIT_DATA_RETRY_EVENT`。
   - 查询成功但返回行 ICCID 为空表示确定无匹配。
4. 已知账户走 `UpdateDataByIccId`：更新 `SLOT_INDEX`，按原标签策略更新 `SIM_LABEL_INDEX`；
   单槽还在该次更新写主卡/Voice/SMS/Data 角色列。
5. 新账户走 `InsertData` → `SimDataBuilder`，原一次 Insert 写入：
   `slot_index/iccid/card_id/is_active/is_esim/sim_label_index`，以及四个初始角色列。
   多槽 Legacy 的四角色初始值为 `NOT_MAIN`；单槽为 `MAIN_CARD`。
6. Legacy 只要求结果不为 `INVALID_VALUE`，随后重新查询 `localCacheInfo_`/`allLocalCacheInfo_`
   以取得 Provider 分配的 `simId`。原接口返回并非单独的写入确认；真正身份来自重新加载的行。
7. `InitData` 后 Monitor 才置 `isSimAccountLoaded_[slotId]`，通过
   `NotifySimAccountChanged` 调注册回调和 `TelephonyStateRegistryClient::UpdateIccAccount`，
   再发送 `RADIO_SIM_ACCOUNT_LOADED`。
8. 卡槽进入 ABSENT/UNKNOWN 时 `RefreshData` 调 File ClearData、Controller `ForgetAllData(slotId)`，
   重新加载两种列表并重置 account-loaded 标志。已载卡的行会改 slot/active，而不是以 SlotId
   重新生成永久账户身份。

源码：`services/sim/src/multi_sim_monitor.cpp:324-420`、
`multi_sim_controller.cpp:235-299, 425-595`、`sim_rdb_helper.cpp:350-554`。

## 2. Target 最小业务实现

`TopologyService::DiscoverAccount` 是 T1 执行入口，`AcceptCardDiscoveryObservation` 也调用该路径：

1. T commit 域接纳 Observation，并经 `DiscoveryObservationLedger::BeginStore` 取得
   observation/session/binding revision admission。
2. 锁外 `AccountStore::MatchAccountByIccid`；Provider 失败只结束 preparation，不发 Write。
3. 旧结果返回后复核 observation；再通过 `AccountRolePlanningPort::PrepareAccountRoleFragment`
   获取 A token。新账户无需预先知道 simId。
4. 最多一次 `WriteDiscoveredAccount`。T 字段和 A 已授权 role fragment 进入同一 Provider plan。
5. Provider bridge 调用既有 `SimRdbHelper` 的 `QueryDataByIccId`/`InsertData` 或
   `UpdateDataByIccId`，再按 ICCID read-back：
   - 写入返回与 `slot/is_active/is_esim/label/roles` 一致后才 `ConfirmedApplied`；
   - 查询失败、行缺失或字段不一致为 `Unknown`；
   - `assignedSubscription` 只来自 Provider 返回行的 `simId`，不从 SlotId 推导。
6. 返回后 T 在短提交域复核 observation、slot binding revision、A reservation 和 provider identity。
   A 先接纳 `AccountRoleFragment`，T 再接受 AccountRecord 和 BindingRef，最后分别结束 T/A 资格。
7. `CompleteStored(..., allowBinding=false)` 只记录已写/未决效果，不把 stale/身份不匹配结果绑定。

## 3. 所有权

| 事实 | Owner | Provider/Adapter 责任 |
| --- | --- | --- |
| Subscription/account identity、ICCID、current/historical 字段 | T | 只写/读取原行；不缓存业务事实 |
| BindingRef、slot revision、当前 session | T/Discovery Ledger | 不分配永久身份 |
| Voice/SMS/Primary/Data 初始片段 | A | 由 reservation 授权，Provider 不制定策略 |
| Card session | R/Card observation | 不等价于 Subscription |
| simId、slotIndex | T/Compatibility association attribute | simId 只采用 Provider 返回值；slotId 只定位当前观察 |

## 4. 负向与交错证据

Target T1 专项覆盖 14 项：

- 新卡一次写入；既有 ICCID 身份复用；重复观察不重写；已绑定其他槽的同一 Subscription 在写前拒绝，不能先改 Provider 后再绑定失败。
- Provider 返回前同线程/并发 session invalidation；late confirmed write 不绑定。
- Unknown 不因重复观察自动重发；ConfirmedNotApplied 不建 binding。
- 已知 ICCID 返回不同 simId 被拒绝。
- Provider match 失败不发 Write。
- T/A role projection revision 在 I/O 中变化时拒绝旧片段。
- 同槽新卡不绑定旧 completed result；旧 epoch 不进入新实例。

Provider bridge Host 另覆盖 3 项：

- match 失败与确认无行区分；
- new Insert 一次写后 read-back 获取 simId；
- read-back Unknown 不升级为成功。

T 侧写前资格和 A 预留由真实 `DiscoveryObservationLedger`、`AccountRegistry`、`BindingIndex`、
`TopologyService` 和 `AssignmentService/AccountRoleProtocol` 执行；Fake 只控制外部 Provider/观察输入。

## 5. 已知 Unknown 与恢复责任

- Applied 但 observation/binding 已失效：记录 Provider 已写事实和 assigned identity，保留 A reservation；
  不绑定到新卡，也不自动 Insert。
- Unknown、返回但 read-back 不一致：保留 reconciliation，不重发。
- 确认 NotApplied/Rejected/prepare failure：可结束“尚未写”资格；不会创建账户或 binding。
- 新卡若受旧已写未核对资格阻塞，保持保守失败；恢复/接管机制属于后续 Current/Historical reload。

## 6. L3.2 安全复核

L3.2 原实现中 Async General/Pin 在已取得 admission 后第二次调用 `TryBeginCall`。Stop 穿越该窗口
可让已接纳接口转为 Post 失败。本轮改为 move 同一 admission 票据到 posted closure，不再二次准入。
`GetSimManager` 改为锁内按值快照；`SetSimManager` 在 accepted call 仍非空时关闭准入并等待，
同时记录 `stopRevision`，防止等待期间的 Stop 被 replacement 重新开放。

Core entry controlled safety 5/5、TSan 5/5、ASan 5/5。真实 Legacy graph 的 sanitizer 仍保留
L3.2 既有 BLOCKED：Host FFRT stub 的 zero-page/repeated-join 不是业务 PASS。

## 7. 验证与分级

- Target T1 14/14。
- Provider bridge VERIFIED_HOST 3/3；对应 TSan/ASan 3/3。
- T1 TSan 14/14；ASan 14/14。
- Core entry safety 5/5；TSan/ASan 各 5/5。
- 本轮 fresh `test.py all --backend both --ci`：Legacy 137/137、Target 90/90、总计 227 项均通过；
  六个 frozen Both/Oracle/Legacy-Target 差异为 0，CI/integrity/technical、mutation、profile safety、
  target/legacy production execution audit、device-static 均 PASS。T1 execution audit 实际观察
`TopologyService::DiscoverAccount` 20 次、`DiscoveryObservationLedger::CompleteStored` 136 次、
A `Prepare/AcceptDiscovered/StillOwns/Finalize` 均为正执行；`AccountStoreAdapter` 的 Match/Write
分别为 3/2 次。
- `architecture` 的 structure 与 migration token checks PASS；随后因用户受保护文件
  `docs/SIM重构代码目录结构设计方案.md` 的 canonical digest 已由现有未暂存工作改变而 FAIL。
  本批未覆盖该文件。独立 testonly GN `ninja ... all` 本轮 PASS。


**T1_HOST_L3_PASS** 仅覆盖 Host Provider bridge、真实 Topology/A/ledger/registry/binding 协议。
**VERIFIED_SOURCE** 覆盖 Legacy 调用链。**VERIFIED_PRODUCT_BLOCKED**：仓内无原生 Provider/SA
运行、没有产品 `hb`，不宣称数据库生产迁移完成。

## 8. 后续范围

未迁移：Current/Historical reload、电话号码与标签、active 升降级、PIN/PUK、Records/eSIM、第三卡、
Profile/通信共享、Carrier/CardFileDetect 独立写者。下一切片先消化本轮 Provider Unknown/Applied-stale
reconciliation，再按 WU 迁入 reload/active，而不是为 T1 引入第二套业务状态机。
