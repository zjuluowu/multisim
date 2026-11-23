# 宿主关闭、已接纳请求与异步排空 Host L3 报告

L3 基线：`da99dd1a4b28f9eb8ac01fe0e55b8f4aac651866`（由 L3 原基线
`5735f696666b1ba736f729f0fce483cbe5716329` 修订而来）。设计输入：
`docs/history/reviews/SIM_Host_Stop_Async_Drain_Device_RIL_Lifecycle_Review_20261120.md`。

## 1. 调用链事实

### 创建/发布

- Core 在 `Init()` 中先建立 `TelRilManager` 并 `OnInit`，随后有条件建立 eSIM，再建立 `SimManager`、注入 `CoreServiceSim`，最后建立 NetworkSearch，并交给 CMI。
- Core、NetworkSearch、SIM/eSIM 共享同一个 RIL 对象；SIM 不拥有全局 RIL 生命周期。
- Target 仍是 testonly。当前产品根构建继续使用 Legacy `services/sim`。

### 停止

- `CoreService::OnStop` 关闭 IMS、NetworkSearch、RIL HDF 监听和扩展；当前源码没有直接调用 `SimManager::Stop()`。
- Legacy `SimManager::Stop()` 已经独立实现 Running/Stopping/Stopped、准入封口、SealWhenIdle、owned-work join、Monitor/Controller/逐槽对象 Shutdown，并在失败时保留 Stopping 可重试。
- Target 原 `SimModule::StopAcceptingAndDrain()` 仍只是声明；本轮没有把它伪装成产品实现。

以上均为 VERIFIED_SOURCE。产品 Core/SA 尚未接线 Target/Legacy Stop 调用关系，结论为 PRODUCT_L3_BLOCKED。

## 2. Host 最小关闭接缝

新增 `PrimaryHostLifecycle`，只协调已经实现的 Primary B/C1：

- `AssignmentService`：停止新 Primary 准入、取消排队重试、保留已接纳设备请求。
- `RetrySchedulerPort`：停止新重试投递并取消未运行任务。
- `ResourceRadioRuntime`：停止新设备 Submit；查询 Owner callback、pending delivery receipt 和 Device Resource Drain。

它不拥有 Core、SimManager、TelRilManager、Card/File/eSIM，也不是通用生命周期 Manager。

`StopAcceptingAndDrain()` 返回三个独立结果：

- `Drained`
- `CallbacksStillInFlight`
- `DeviceEffectsUnresolved`

首次并发 Stop 调用只有一个执行者触发实际关闭；后续调用只查询进度。旧 Manager/A handle 在停止后仍受原实例门控。

## 3. 已接纳请求保全

- A 关闭新请求后，已在途 B/C1 不被强制改成统一业务错误。
- Radio 结束证据仍能从 R 进入 Owner dispatcher。
- Owner dispatcher 允许已接纳 R 结果在停止阶段投递，但不接受新的业务调度任务。
- Completion 继续由原 `RuntimeOperationStore` 一次领取。
- R 已释放物理资源不等于 A 已处理角色结果；`pendingNormalDeliveries` / delivery receipt 仍阻止 Owner Drain。
- Unknown 设备结果保留原 lease/资源资格，不因 Host Stop 释放。

## 4. 排空凭据

分开核对：

1. A 不再接纳新 Primary。
2. Scheduler 无 pending/running retry。
3. R 的 platform submit-in-progress、transmission 和提前缓冲证据为空。
4. R→A pending normal/late delivery 和未完成 receipt 为空。
5. dispatcher 的 Owner running/pending 为空。
6. 所有设备资源为 Available。
7. A 没有未结束 Primary。

只满足 1～5 是 Host callback drain；仅再满足 6/7 才可报告本切片 Host L3 Drained。它仍不证明真实 Device Drain。

## 5. Host LIFE 场景

已增加/覆盖：

- 旧句柄跨停止点：新请求拒绝，旧 B 完成一次。
- 并发 Stop：只有一个实际关闭者，重复查询幂等。
- Owner 投递失败：R resource 已可复用，但 lifecycle 返回 CallbacksStillInFlight，显式 Flush 后才 Drained。
- 停止前已排队 retry：retry 取消，旧 epoch/event 不执行。
- Owner 回调内 Stop：不 join/self-wait；先报告 callback in flight，回调退出后才继续。
- 空图重复 Stop：幂等 Drained。
- 共享 RIL 其他消费者：Host 平台的无关请求仍可提交；这只证明本地所有权，不是产品共享 RIL PASS。

已有 Legacy LIFE/Shutdown 套件继续覆盖正常关闭、半初始化、重复 Stop、任务 cancel、运行中 join、自回调拒绝、C1 三阶段 Stop 和 watcher 失败。L3.1 本次 Target Host 生命周期专项包含 15 个场景（由 L3 的 7 个专项扩展），TSan 15/15、ASan 15/15。本次完整回归 Target 75/75、Legacy 124/124、Both 199/199；
六个冻结业务场景 Oracle/Legacy-Target 差异为 0，CI、mutation、profile safety、
architecture/GN/technical/integrity/production execution audit 均为本次 fresh PASS。

## 5.1 L3.1 并发关闭线性化复现与修复

L3 原实现的线性化反例是真实可达的：第一个 Stop 在 lifecycle mutex 下将
`stopping_` 设为 true 后释放锁；第二个调用看到 true，跳过 A/Scheduler/R
封口，随后直接检查空计数并可返回 `Drained`。`ConcurrentStopCannotDrainBeforeSeals`
用 Scheduler Stop gate 确定性停住第一个执行者，不使用 sleep 或循环碰撞；第二个 Stop
必须得到 `CallbacksStillInFlight`，而不是 Drained。

修复后的协议只有一个封口执行者：

| 状态 | 事实 |
| --- | --- |
| Running | 未请求关闭 |
| ClosingRequested | 原子 A gate intent 已设置，第二个 Stop 不再有资格直接判为 Drained |
| GatesSealing | 唯一执行者依次请求 A 新准入关闭、Scheduler 新准入/queued retry 关闭、R submit gate |
| OwnerSealPending | 三个调用侧 gate 收据齐全，但 A 的容器提交/取消尚未在 Owner 串行域确认 |
| Draining | Owner 封口完成；仍按 receipt、pending delivery、Scheduler running、R group/lease、A pending 查实 |
| Drained | 各责任为空；只读为稳定 terminal，不重复 cancel/reset |

关闭期间不持 `PrimaryHostLifecycle::mutex_` 跨调用 A/Scheduler/R/dispatcher。
回调内 Stop 只登记关闭意图并提交 Owner 任务，不等待自身。Owner 投稿失败保留
`CallbacksStillInFlight`，后续 Stop 只重试 Owner 收据，不重复 A/Scheduler/R gate。

## 5.2 A/O/R/Scheduler 所有权与执行域

| 状态/操作 | Owner | 修改执行域 | Stop 行为 |
| --- | --- | --- | --- |
| A `pendingPrimary_`、`operations_`、role write-set、phase | Assignment | 合法 Owner 串行优先；同一窄 Primary 提交域 `primaryCommitLock_` 覆盖外部/Owner 交错；不是跨 H/R/Scheduler 的全局锁 | Stop 的容器修改只在 Owner task；caller 只设置 atomic stop intent |
| `StartPrimarySwitch` | Assignment | commit lock + atomic stop admission | intent 后只登记并返回原 Completion 的失败结果，不抢角色/R 资格 |
| `AcceptRadioStep` / Late / Timeout / Retry / 参数恢复 | Assignment | Owner task（正常）；commit lock 证明异常外部交错安全 | 已接纳结果继续消费；新 retry 被 intent 拒绝 |
| Completion claim | Assignment/operation store | commit lock，原 request/epoch/plan | `ClaimCompletion` 仍确保最多一次；Stop 不替换出口 |
| R group/transmission/barrier | ResourceRadioRuntime | `lock_` + 设备回调 | Stop 只关 Submit，不释放 Unknown/HandoffUnknown/quarantined 资源 |
| R→A normal/late receipt | R/H dispatcher | R lock + receipt atomic；A action 只在 Owner queue | delivery failure/pending/running 阻止 Owner drain |
| Retry queue/running audit | Scheduler/Host | Scheduler lock/worker | Stop 取消未运行任务；已拿出的 running 由 A stop intent 拒绝业务执行 |
| lifecycle state | H | lifecycle mutex | 只有一个 gate/seal executor，其余只观察 |

L3.1 还确定性覆盖：关闭前封口、并发 Stop、Stop×Start、Stop×外部结果/重复结果、
Stop×Timeout、Owner self-stop、旧 epoch/new instance、C1 CHECK 部分成员、
HandoffUnknown、Quarantined、delivery failure 和共享平台其他消费者。

## 5.3 Host 回调与设备责任

- Scheduler cancel 只表示 queued retry 不再运行，不表示已发 RIL/HDI 命令取消。
- R `commandEnded + resourceReusable` 只能清 R barrier；A pending/receipt 不因 R 成功而消失。
- HandoffUnknown、quarantine、partial C1 member、late evidence 都继续阻止 `Drained`。
- `Completion` 最多一次，但已完成 API、A 已消费、R 已结束和真实设备静止是四项独立事实。
- 本报告不把 test platform 的 Available 推导为真实 Modem/RIL Device L4。

## 6. 共享 RIL 责任

PrimaryHostLifecycle 不调用 `TelRilManager::DeInit`、Clear 或全局 Stop，也不持有共享 RIL 重置权。产品中 RIL 还有 CellularData、CellularCall、NetworkSearch、eSIM/IMS 等消费者；只有 Core/真实宿主在所有消费者静止后才能退役连接。

Host 无关消费者测试只能证明 Target 局部 Stop 没有触碰平台端口，不能证明真实 RIL 的最终退役。

## 7. 验收分级

- **CORE_LEGACY_HOST_L3_PASS**：真实 CoreServiceSim/Legacy SimManager + Handler Host bridge 已验证入口封口、原 IPC task/callback drain、真实 Stop、失败保留及共享 RIL 非所有权。它不等于产品 Core OnStop。
- **PRIMARY_HOST_L3_PASS**：本 Target Primary B/C1 Host 切片的准入封口、旧请求保全、Owner callback 排空、retry cancel、R gate 分离及异步交错通过实际测试/TSan/ASan。
- **PRODUCT_L3_BLOCKED**：CoreService::OnStop 未接线 SimManager::Stop，CMI/SA/共享 RIL/真实 handler 生命周期未产品验证。
- **DEVICE_L4_BLOCKED**：真实 Modem/RIL command end、状态验证及资源复用未证实。
- **SO_L5_BLOCKED / NOT_APPLICABLE**：是否动态卸载属于产品需求；当前宿主代码不具备足够证据。

## 6.1 L3.2 真实关闭关系与 Host 接线

基线为 `befd76d4eba73ade9552248c303e3d6f8427c8ce`。

```text
SA/CoreService::OnStop（产品尚未接线）
  └─ 候选 CoreLegacyHostStop（test-only Host，调用真实源码）
      ├─ CoreServiceSim::StopAccepting
      ├─ Accepted/queued/running IPC task drain
      └─ SimManager::Stop
          ├─ TelLifecycle admission seal / SealWhenIdle
          ├─ owned-work join
          └─ Monitor/Controller/逐槽 components Shutdown

CoreServiceSim 的 weak ISimManager task ----> 原 Manager（由 Core 持有）
CoreServiceSim handler ----> IRawParcelCallback::Transfer（任务退出前保留 admission）
Core / NetworkSearch / eSIM / Call / Data / CMI ----> 共享 TelRilManager
SIM Stop 仅释放 SIM 自有观察/引用；不调用 TelRilManager::DeInit。
```

源码事实分级：

- **VERIFIED_SOURCE**：Core Init 建立共享 RIL、eSIM、SimManager、CoreServiceSim、NetworkSearch；Core OnStop 仍只做 IMS/NetworkSearch/RIL HDF/extension，未接 SimManager Stop。
- **VERIFIED_HOST**：真实 `CoreServiceSim`、真实 `SimManager::Stop`、真实 EventHandler submit path、Legacy manager graph 在 Host 一起执行。
- **SIMULATED_HOST**：首入口 mock 仅用于精确卡住 Core 验证/参数写窗口；不作为 Legacy Manager 成功证据。
- **BLOCKED**：实际 Core SA、Binder/FFRT product runner、共享 RIL 全消费者和产品 `OnStop`。

## 6.2 CoreServiceSim 入口封口与 IPC 交接

`CoreServiceSim` 增加内部 admission 票据：

- 调用在权限/manager 前已取得票据的，Stop 后旧句柄不能进入；同步写到 `SetParameter`、Manager 调用前均在同一票据有效期内。
- `AsyncSimGeneralExecute` / `AsyncSimPinExecute` 返回真实 PostTask bool。成功后票据转移到 posted closure，直到 Manager 调用和 `IRawParcelCallback::Transfer` 返回才退休；失败立即退票并返回既有 `LOCAL_PTR_NULL`。
- `IRawParcelCallback::Transfer` 为 void，Host 只能证明 Transfer 被调用/返回，不能把远端 Binder 实际消费成功推导为已回包。
- `StopAccepting()` 只关闭新票据；`WaitForAcceptedCallsDrained()` 读取真实 active ticket，不统计空队列或线程数。
- `SetSimManager()` 代表新 Host generation，只有显式 replacement 才重开 admission；旧 queued task 捕获旧 weak Manager，不接触 replacement。
- Stop 后同步只读入口仍保持既有查询兼容语义，但不授予写权；所有本类写/异步业务由 admission 封口。

M1 确认两个 Core 前置用户偏好写点：`SetPrimarySlotId` 与 `SetActiveSim`。两者均在 Stop admission 下；其他同步写入口和所有 async Core 入口也使用同一票据或 PostTask 收据。只读查询仍不宣称 Manager 业务完成。

## 6.3 Legacy Stop Host 执行

窄 Host bridge 的顺序为 Core seal → Core posted/sync call drain → 原 `SimManager::Stop()`。它不拥有 RIL。失败时保持 Core/Manager/handler 对象，第二次调用重试；半初始化和 watcher removal 失败沿用真实 Legacy Stop 机制。

Host 覆盖已接纳 IPC 正在 Transfer、PostTask 成功/失败、callback 内请求关闭、Core seal 与 manager Stop、重复 Stop、并发 Stop observer、Manager Stop 失败重试、共享 RIL 对象保留和旧/新 manager generation。统计为真实源码 Host 5/5、Core 入口 controlled seam 3/3；两类不合并。真实 Legacy owned-work/watcher/component Shutdown 由 `SimManager::Stop` 和既有 Legacy 测试执行，不用模拟器重写。

L3.2 完整 fresh 回归：Target 76/76、Legacy 132/132、Both 208/208；六个冻结场景
Oracle/Legacy-Target 差异为 0；architecture/GN/technical/integrity/CI/mutation/profile
safety/production execution audit 均 PASS。

Sanitizer 分级：PrimaryHostLifecycle 16/16 TSan、16/16 ASan；CoreServiceSim
admission/side-effect seam 3/3 TSan、3/3 ASan。真实 Legacy graph 的完整 TSan
尝试在 Host FFRT stub 中于测试初始化阶段以 `SEGV zero page` 退出；ASan
尝试以 `Joining already joined thread` 退出，两者均发生在新增 Stop 断言之前，
因此不报告 real graph sanitizer PASS。普通真实源码 Host 5/5 仍可执行。

## 6.4 PrimaryHostLifecycle M4 修复

Owner seal callback 不再捕获协调对象 `this`。它捕获共享 State 和 posted task 持有的 `AssignmentService` strong reference；因此协调对象可在投稿后销毁，A seal task 仍可执行和回填收据。Radio/Coordinator/scheduler 的调用顺序仍要求宿主保持 R/A/dispatcher 到任务释放，不能凭 callback receipt 提前销毁底层 Owner。

## 8. 真实 Core 接线准备（仅审查，不修改产品）

1. **封口触发者**：SA shutdown 的唯一 Coordinator 应先使 `CoreServiceSim` 拒绝新任务，
   然后由 `CoreService::OnStop` 调用 Legacy `SimManager::Stop()`；Target 接入时替换为等价
   `PrimaryHostLifecycle`，不能让 IPC stub/CMI 各自独立 Stop。
2. **CMI 旧句柄**：`CoreServiceSim` 的入口可在关闭前已取得；方法先检查 manager 为空/停止状态，
   handler 任务捕获 weak manager。发布前需把 handler “关闭新 Post”与“允许已 Queue 执行”分开。
3. **Completion 出口**：`CoreServiceSim` 已排队 IPC callback 和 Manager 内原 request 必须保留至
   Stop drain 回执；先 clear/release manager 会使原 weak 任务只得到 IPC_CONNECT 失败。
4. **调用顺序**：`CoreServiceSim` admission seal → handlers drain / `SimManager::Stop`
   → 停 NetworkSearch/eSIM 等共同消费者 → 最后 `TelRilManager::DeInit`。当前源码顺序仍相反，
   需要专项修改和 SA shutdown 测试。
5. **共享 RIL**：SIM Stop 不得 DeInit 全局 RIL。只有其它消费者（NetworkSearch、eSIM、Call/Data/CMI
   等）均完成各自责任后，Core 才可关闭共享 RIL。
6. **失败责任**：`SimManager::Stop` 或 Target drain 失败时 Core 保留原 manager、handler 和 completion
   对象；OnStop 不能 reset 构成关闭回包仍需的代码。Legacy 当前失败保持 Stopping，可重试。
7. **必要新接缝**：CoreServiceSim 增 private seal/drain/status，不改变 IDL/ABI；Core private
   shutdown coordinator 收集 SIM、NetworkSearch、eSIM、RIL receipt；只有产品授权后才接线。
8. **验证上限**：当前仓库无可用原生 OpenHarmony `hb`/SA 运行环境，只验证 Host、Legacy host、GN
   对象与源码审查，不能签 PRODUCT_L3。

进入真实 Core OnStop 实施前，还必须：

1. 由 Core/CMI 明确旧句柄封口和已接纳入口范围。
2. 在真实 SA Host 中验证 Core OnStop ↔ SimManager::Stop 顺序。
3. 保留其他 RIL 消费者及其在途请求。
4. 核实 watcher/event/dispatcher 的生产提交和退订关系。
5. 分别获取 Scheduler、Owner、R、Device 的关闭回执。
6. 保证失败/超时路径不卸载仍需接收旧结果的代码。
