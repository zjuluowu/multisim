# 宿主关闭、已接纳请求与异步排空 Host L3 报告

基线：`5735f696666b1ba736f729f0fce483cbe5716329`。设计输入：
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

已有 Legacy LIFE/Shutdown 套件继续覆盖正常关闭、半初始化、重复 Stop、任务 cancel、运行中 join、自回调拒绝、C1 三阶段 Stop 和 watcher 失败。当前完整回归为 Target 67/67、Legacy 124/124、Both 191/191；六个冻结业务场景 Oracle/Legacy-Target 差异为 0；生命周期专项 TSan 7/7、ASan 5/5。

## 6. 共享 RIL 责任

PrimaryHostLifecycle 不调用 `TelRilManager::DeInit`、Clear 或全局 Stop，也不持有共享 RIL 重置权。产品中 RIL 还有 CellularData、CellularCall、NetworkSearch、eSIM/IMS 等消费者；只有 Core/真实宿主在所有消费者静止后才能退役连接。

Host 无关消费者测试只能证明 Target 局部 Stop 没有触碰平台端口，不能证明真实 RIL 的最终退役。

## 7. 验收分级

- **HOST_L3_PASS**：本 Target Primary B/C1 Host 切片的准入封口、旧请求保全、Owner callback 排空、retry cancel、R gate 分离及异步交错通过实际测试/TSan/ASan。
- **PRODUCT_L3_BLOCKED**：CoreService::OnStop 未接线 SimManager::Stop，CMI/SA/共享 RIL/真实 handler 生命周期未产品验证。
- **DEVICE_L4_BLOCKED**：真实 Modem/RIL command end、状态验证及资源复用未证实。
- **SO_L5_BLOCKED / NOT_APPLICABLE**：是否动态卸载属于产品需求；当前宿主代码不具备足够证据。

## 8. 下一阶段条件

进入真实 Core OnStop 集成前，必须：

1. 由 Core/CMI 明确旧句柄封口和已接纳入口范围。
2. 在真实 SA Host 中验证 Core OnStop ↔ SimManager::Stop 顺序。
3. 保留其他 RIL 消费者及其在途请求。
4. 核实 watcher/event/dispatcher 的生产提交和退订关系。
5. 分别获取 Scheduler、Owner、R、Device 的关闭回执。
6. 保证失败/超时路径不卸载仍需接收旧结果的代码。
