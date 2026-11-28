# 正式 Host SimModule 的 C1 与两个期限

输入 main 为 `b98f0a777dc30170796a9158704281318b0991a5`。用例经过
`CreateCoreSimModule`、正式 `SetPrimarySlotId`、A/T/R、原逐键 Store 与兼容 Completion。
受控平台提供原生协议成员、路由、逻辑 modem、technology 及原传输回包。产品 RIL/HDI 验证为 `NOT_RUN`。

- 四个端点共享两个协议栈，实际只有两个成员分别执行 CHECK、UPDATE、COMPLETE。
  原会话、旧回包拒绝、原载荷、Primary/Data 投影和单次账户通知均由正式入口实测。
- A 使用现有调度端口登记每轮 45 秒协议期限，兼容入口按原分支持有 C1 的 60 秒等待。
  在原发送许可前分别推进两个入口，可确认零发令、逐键恢复、单次失败及原资源结清。
- 已发 CHECK 时，实际 60 秒等待出口可在协议任务尚未递达时认领失败。Stop 后，仅凭同一原会话两成员
  明确验证的结束、复用和未应用效果撤销旧预写。无回包、只得一份证明及重复回包均有正式图断言。
- 已接纳的最终 COMPLETE 撤销原期限登记后才进入 Store。确定性屏障验证 45 秒事件不会消费后台轮已开始的
  原逐键恢复。调度器拒绝下一轮时，已结束的原轮可以排空。
- 以 999/1000 毫秒实际验证后台重试。在原 API 已返回失败之后，成功后台轮只发原账户通知，且不复开旧等待。

独立源码审查定位了正式图的晚证据、逐键恢复期间的期限、调度拒绝及后台成功通知问题。
同一候选的六个正式 C1 用例逐个独立复跑，退出码均为 0。[独立原始日志](independent.log)

## 实际回归

Target 248/248，Primary、Radio 与 C1 拓扑专项 ASan、TSan 各 117/117。
六个正式用例实际命中 37 个审计要求的生产方法。LLDB 在正式 C1 入口命中
`AssignmentService::ArmC1ProtocolDeadline`、单步并完成原用例。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [生产执行](execution.json) [LLDB](lldb.log.gz)

两个独立 Host 后端执行十二个原冻结共同契约 24/24，后端及 Oracle 差分均为零。架构护栏、
独立语法、纯规则、逻辑 GN/Ninja 和冻结资产完整性通过。
[Both 与 Oracle](both.json.gz) [架构](architecture.log) [冻结资产](integrity.log)

六次负控各替换同一 Target 生产归档的 A 对象并重链。精确用例对每项变异返回 1，恢复后返回 0。

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 45 秒协议期限延至 60 秒 | 45 秒时未收到原协议期限 | [变异](late_deadline-mutated.log) [恢复](late_deadline-restored.log) |
| 60 秒兼容等待缩至 45 秒 | 旧调用可过早消费等待 | [变异](short_legacy_wait-mutated.log) [恢复](short_legacy_wait-restored.log) |
| 去掉两成员未应用恢复判定 | 原预写及写权无法通过两份正确证据结清 | [变异](forget_recovery-mutated.log) [恢复](forget_recovery-restored.log) |
| 调度拒绝后保留旧 pending | Store/设备已完却无法排空 | [变异](retained_rejected_retry-mutated.log) [恢复](retained_rejected_retry-restored.log) |
| 去掉后台成功账户通知 | 原调用仍失败，合法成功事件丢失 | [变异](retry_account-mutated.log) [恢复](retry_account-restored.log) |
| 原 COMPLETE 不先撤期限 | 开始逐键恢复时仍有原 Radio 期限任务 | [变异](deadline_in_restore-mutated.log) [恢复](deadline_in_restore-restored.log) |

本批证明的是正式 Host 的受控协议路径。未决或不完整设备证据继续保留原责任。原作用域发生变化后逐成员
恢复、全对象图两遍关闭收据、全部正式 B/C1 共用测试与设备协议供证仍需各自继续验证。
