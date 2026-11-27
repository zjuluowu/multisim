# D03 UnlockPin 与 D04 卡侧组合 Host 检查点

输入 ICCID 检查点为 `0b6a9516dc3d03793529e29b095332d88d0a8825`。该提交第一次推送遇到
HTTP 408。核对远端仍为前驱后，原工作区常规重试推送成功并复核 SHA。

正式 `SimModule::UnlockPin` 接入宿主权限和回调规则，由 `PinUnlockRuntime` 在执行时固定合法应用及原票。
PIN、ICCID 共用 `CardCommandTransport`、本地卡请求编号和物理屏障，传输种类也参加原票核对。
`SessionRegistry` 保存原应用的锁态。确认 PIN 成功只推进原 LOCKED 到 NOT_READY。
错误 PIN 以 API 成功交付 UNLOCK_INCORRECT 和剩余次数。RIL 通用失败与等待结束分别交付原错误码。
Host 带原票的等待结束由受控设备端口报告。真实计时、RIL、状态 Registry 及厂商升级缓存需产品取证。
[PIN 实现](../../../../card/security/pin_unlock_runtime.cpp) · [开发断点](../../Host_Usage.md#调试卡安全和-iccid-ef)

| 验证 | 实际结果 |
|---|---|
| 共用错 PIN 后成功场景 | [Legacy PIN_001](legacy-pin.json.gz) 与 [Target 全量内同源用例](target-full.json.gz) 均通过。保留原 Legacy READY 单独 RIL 观察断言。 |
| Target | 204/204。覆盖预身份 PIN、同步/重复回包、双卡、换卡、等待结束及权限顺序。 |
| 生命周期组合 | 同键 PIN 与 EF 互斥，邻卡独立，错误命令类型拒绝。Stop 分别等待 PIN 原设备和 ICCID 原 Store。以实测前驱 Drained 判定复用，拒绝旧 epoch 回包，确认相同数字请求与 fence 不跨实例接纳。 |
| Sanitizer | [ASan 203/203](asan.json.gz) 与 [TSan 203/203](tsan.json.gz)。本次覆盖全 Target 203 登记基线，晚加的跨实例组合在 204 项标准 Target 中执行。 |
| 工程 | [架构、静态与 GN](architecture.log.gz) 和 [独立 Host 归档](host-archive.log) 通过。 |
| 验证执行范围 | [真实 PIN 生产断点及单步](pin-lldb.log.gz) 和 [执行计数](production-execution.json)。 |

首轮 PIN 装配漏换 GetShowNumber 的独立回写投递入口，构建拒绝。统一投递保活后恢复全量。
[原构建失败](initial-build.log.gz)

以上是正式卡侧 Host 图证据。B/C1 正式装配、同卡独立 EF 作用域及全图组合仍待。独立扩展、
最终负控矩阵、产品/设备和生产写权均未在这一检查点验收。
