# 正式 Host SimModule 的专用 B 切换

输入为 `236655221bbab507f9371df64807f9d34313f106`。

`CreateCoreSimModule` 在宿主提供原调用等待适配器和原 Store 收据能力时开放专用 B。
`SimModule::SetPrimarySlotId` 通过原兼容校验进入 Assignment。A 核对业务和原目标，
R 验证平台报告的成员与路由，并使用原资源、传输票和设备证据。H 接纳真实执行结果后
由原 Completion 单元交付。等待适配器只负责唤醒和受控时钟，结果由原 Completion 保存。

Radio 正常结果和原晚证据经正式模块接纳。Owner 已接纳投递持有模块寿命凭证，
关闭读取 A 当前 Store 收据。B 调用先返回、Radio ACK 后才发现 ICCID 原写未决时，
没有设备租约仍可见原业务责任。后续按原操作核对即可完成 Drain。

## 正式回归

| 用例 | 实际边界 |
|---|---|
| `DedicatedSwitchAwaitsOriginalCompletionThroughFormalModule` | 原兼容入口等待、实际成员、回复到原 Completion。并行 Data 冲突被拒，独立 Voice 正常完成，两者均不会唤醒 Primary 原等待 |
| `FormalDedicatedSwitchHandlesReplyBeforeSubmitReturns` | 平台同步原回包先缓冲，交接定案后真实 B 写及原 Completion 只执行一次 |
| `FormalDedicatedSwitchRetainsStoreReceiptDiscoveredAfterStartReturns` | R 设备责任和后生的 A Store 责任分别关闭，Store 未决时连续 Stop 都不可排空 |
| `FormalDedicatedTimeoutClosesOnlyAfterOriginalDeviceRecovery` | 受控 45 秒期限触发原错误；Stop 后拒绝缺状态证据的旧结果，原未应用效果核验后排空 |

精确执行可用 `test.py run --backend target --debug-build --filter AF2Primary.FormalDedicatedSwitchHandlesReplyBeforeSubmitReturns`。
可在兼容入口、`AssignmentService::SubmitRadioOutsideCommit` 和
`ResourceRadioRuntime::AcceptDeviceResult` 分别跟踪原请求、发令许可和回包。

## 验证

Target 全量 222/222，Primary、Radio 和正式模块专项 ASan、TSan 各 97/97。
四项正式 B 的独立 profile 命中 26 个生产方法。LLDB 从正式兼容入口进入 A 的
`SubmitRadioOutsideCommit`，命中生产断点并单步，原用例退出 0。
静态护栏、107 个独立语法单元及组合头、逻辑 GN/Ninja 和保护资产检查通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz)
[执行审计](execution.json) [LLDB](lldb.log.gz) [架构](architecture.log) [完整性](integrity.log)

三个重链变异分别使非 Primary 结果唤醒原等待、遗漏后生 Store 原收据、丢失已核验
未应用效果的关闭资格。每项真实运行均在对应断言失败，恢复后通过。
[等待串线](wait-mutated.log) [恢复](wait-restored.log)
[Store 收据遗漏](store-mutated.log) [恢复](store-restored.log)
[晚回包结清遗漏](late-mutated.log) [恢复](late-restored.log)

上一 B Store 候选的冻结 Both 实跑 24/24，Oracle 差异为零。
[上一候选结果](prior-b-store-both.json.gz)

正式 B 使用受控平台的保守 `primary-shared` 资源域。真实 modem 与卡命令的物理冲突图、
已应用的晚设备效果核对、正式 C1、多命令同图组合和原生产品门禁仍待验证。
