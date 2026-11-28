# 正式 Stop 的未确认封口重试

输入 main 为 `662e5b16fdd68eac6118c0992fff31802d80ee59`。独立审查定位的旧行为已在正式模块复现。
第一次 Scheduler 封口抛异常后，后续 Stop 保持未排空，封口计数及 Owner 收尾投递数为零。
[原始基线失败](baseline.log)

现在后续 Stop 重试未确认的封口。在 R 查询 Owner 抛异常的场景，已获得的 Scheduler 封口保持有效。
两条场景均检查已封闭的新准入或准确的 Scheduler 和 Owner 调用次数，并确认重复 Stop 结果稳定。
Target 全量 257/257，ASan 和 TSan 各 25/25，覆盖正式模块及 Primary 生命周期组件。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [静态护栏](architecture.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 失败后继续持有封口执行标记 | 下一次 Stop 无法完成封口和投递，永久未排空 | [变异](latch_failed_gate-mutated.log) [恢复](latch_failed_gate-restored.log) |
| 丢弃已确认的 Scheduler 封口 | R 查询失败后再次封口 Scheduler | [变异](forget_scheduler_ack-mutated.log) [恢复](forget_scheduler_ack-restored.log) |

本批覆盖可重试的根门失败。Dispatcher 无法确定是否已经接受原任务时仍需按原投递责任核对。
