# 正式模块逐项关闭收据

输入 main 为 `9489d5d58d1134f8475fb66ebc92f12eabb3eceb`。通过 `SimModule::ReadCloseReceipt()` 查看原 epoch、
封口状态及各责任方的原条目。兼容调用与已经入队的任务持有各自 H 帧，队列任务在工作捕获释放后退帧。
R 列出原设备租约及待投递回执，卡传输列出原票和真实资源。A 提供原角色、定时任务、逐键参数及角色 Provider
证据，T 提供原发现和元数据。各项从对应责任方已有状态读取。

R 在锁外取据前保存活动版本。即使在查询期间新增的 Owner 工作已完整执行和返回，这次取据也失效。
H 的第二遍再次核对 R 当前活动和 Scheduler 审计。正式混合用例列出同时在途的卡票、Primary 原 CHECK、待交付的
整轮 Owner 票，以及设备都结束后 T 的原 Store 票和 A 同票的资格。原效果结清后该组清单为空。

Target 全量 265/265。正式模块、C1、EF、卡组合及 Radio Owner 专项 ASan/TSan 各 33/33。
已执行独立正例 4/4，历史验证资产校验通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [独立验证](independent.log) [资产](integrity.log)

原冻结契约双端 24/24，Oracle 及后端差分为零。静态架构检查、独立语法及逻辑 GN/Ninja 通过。
[Both](both.json.gz) [差分](differential.json) [架构](architecture.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 跳过 R 取据中的活动版本复核 | 原工作完整进出后错误地认可过时观察 | [变异](ignore_finished_owner_activity-mutated.log) [恢复](ignore_finished_owner_activity-restored.log) |
| 隐藏已排队的正式入口帧 | 队列里仍有原 PIN 任务时收据为空 | [变异](hide_queued_module_frame-mutated.log) [恢复](hide_queued_module_frame-restored.log) |
| 隐藏未消费原 Owner 回执 | R 整轮原回执在物理资源可复用时从清单消失 | [变异](hide_unconsumed_owner_receipt-mutated.log) [恢复](hide_unconsumed_owner_receipt-restored.log) |
| 关闭期间遗漏 T 原发现票 | 已发 Store Unknown 在原效果结束前缺少诊断条目 | [变异](hide_original_discovery_receipt-mutated.log) [恢复](hide_original_discovery_receipt-restored.log) |

本批的后续反例证明 R 接纳 COMPLETE 后、Owner 开始前发生的业务超时仍会丢失整轮核对入口。
设备已结束时错误报告设备责任未结，受控平台的原整轮证明随后被拒。该反例未计入正例。
[确定性基线失败](complete-before-owner-baseline.log)
