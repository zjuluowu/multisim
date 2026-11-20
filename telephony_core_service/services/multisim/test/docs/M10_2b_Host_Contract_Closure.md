# M10.2b Host Radio 契约封口报告

基线：`82a76db12`。本文只验收 Host 契约，不构成真实 Radio/HDI 或 Device 验收。

## 1. 修复的反例

- **提交前同 token 冲突结果**：原单值 map 会被后一结果覆盖。现在同 token 的完全相同结果只保留一份；字段、Serial、Session、Step、Member、Phase、NOTIFY、效果或结束证据任一冲突都会标记冲突并隔离资源。
- **ConfirmedNotSent 与已到回包冲突**：提交返回 ConfirmedNotSent 但 Submit 期间已有回包时，不能 Cancel lease；转入 Quarantined。
- **Owner 投递失败丢结果**：物理资源允许在真实结束证据后复用，但对应 normal/late 业务结果保留在 R 的待投递集合，并可通过 `FlushOwnerDeliveries()` 重试。已被 dispatcher 接受但尚未执行的任务另持 delivery receipt（含原结果）；队列 Stop/取消后 receipt 仍未完成时同样不得宣称 Owner drain。R 不释放 A 角色资格。
- **Dispatcher/Owner 生命周期**：Sink 与 Dispatcher 使用 weak 生命周期；产品候选未绑定 Dispatcher 时不允许发令。任务被停队列接纳后仍持有 delivery receipt，未真实执行到 A 前不宣称 Owner drain。

## 2. 提交分类

`RadioSubmitResult` 保留三态：

| 状态 | ResourceBarrier 动作 |
| --- | --- |
| ConfirmedNotSent | 仅在同 token 无提前回包时 `CancelConfirmedNotSent` |
| HandoffAccepted | MarkSent，关联 transport serial，接纳响应 |
| HandoffUnknown | MarkSent/保留 transmission，但立即 Quarantine；不得自动 Cancel |

提交前回包进入提交缓冲；Submit 返回后才分类 Handoff，再进入正常或冲突路径。实现不在 R 短提交锁内调用平台 Submit。

## 3. Serial 与事件矩阵

| 事件 | 最小关联字段 |
| --- | --- |
| B SetPrimarySlot response | token + transport serial + StepRef + member + BindingRef + protocol phase |
| C1 CHECK response | token + serial + StepRef/attempt + sessionId + member + phase 1 + non-NOTIFY |
| C1 UPDATE NOTIFY | token + sessionId + member + StepRef + phase 2 + NOTIFY；不编造 serial |
| C1 COMPLETE response | token + serial + StepRef/attempt + sessionId + member + phase 4 + non-NOTIFY |
| Late/recovery | 上述原关联 + commandEnded/currentStateVerified/resourceReusable |

若平台声明普通 Response 的 transportSerial，则回包缺失或不相等均拒绝。Serial 只用于同一传输关联，不能替代 epoch/Step/session/binding。

## 4. Owner 交接与 Drain

- R 在锁内接纳设备证据及资源状态；投递函数只在锁外执行。
- `PostRadioOwnerTask` 失败、dispatcher 停止、sink 已销毁时，normal/late 结果保留为 pending responsibility；已入队但未执行的 receipt 也继续阻止 Owner drain。
- 已成功进入 dispatcher 的结果持有 delivery receipt；任务真正执行 A 后才标记 delivered。
- `OwnerCallbacksDrained` 需要 R queue、submit-buffer、待投递结果、未完成 receipt 均为空，并且 dispatcher 报告 running/pending 为空。
- `DeviceResourcesDrained` 只检查资源闭合和传输责任，不代表 A 已消费业务结果。
- R 可复用与 A 角色资格释放是独立事实；R 从不直接释放 A 写集。

## 5. Host 验证

新增/扩展场景包括：

- 未确认移交、未知移交、ConfirmedNotSent 与提前回包冲突。
- 提前相同结果去重、成功/错误冲突隔离。
- 缺 Serial、旧 Serial、token/serial 解绑。
- Dispatcher 停止后的结果保留和显式 Flush。
- 无 dispatcher 不允许产品候选发令。
- Sink 销毁后的回包不访问悬空对象。
- 多线程成员回包通过同一 Owner worker。

当前源码验证：Target 60/60；M10.2b 专项 TSan 12/12、ASan 8/8；完整 Host Target 的架构/GN、技术、integrity 与 production execution audit PASS。Legacy 124/124 及六个冻结场景 Both 差分由本批统一回归复核。

## 6. 仍不证明的事项

- RIL/HDI response 不自动证明 Modem/channel 静止。
- UPDATE NOTIFY 不证明物理资源结束。
- COMPLETE 不证明可复用。
- Host queue/drain 不证明真实 Device Drain。
- 公共 Busy/拒绝/排队和产品 Stop 策略仍为 PROPOSED。

M10.2b 可单独验收；M10.3 及 M11 仍需真实产品和 Device 证据。
