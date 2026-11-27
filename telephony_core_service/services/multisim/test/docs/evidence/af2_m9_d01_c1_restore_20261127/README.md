# C1 原参数快照与失败轮次恢复

输入为 `740986291e98188a95d9aee0dd76b7e9540632be`。本批仍在 Target Host
Assignment/Radio 组件验证 C1。

原参数快照在 A 锁外读取。A 持原角色写权，在读后重新核对 Binding 和活跃资格。
适配器区分有证据的空旧值和未完成的读，读失败时不预写或发 CHECK。

已发 CHECK 失败且原 COMPLETE 按原成员确认结束时，A 复用同一请求和逐键 Store
机制。恢复直接使用最初保存的 slot 文本和 ICCID 值。未决恢复保留旧值、原键票据和
尚需写入的后继键。第一键获原证据后，在原请求的核对阶段继续第二键，原 Completion
不重复。恢复未应用时，保持实际投影和原写权，且不调度新轮次。

独立审核找到三个确定性反例。第一键核对可能漏掉尚未登记的 ICCID，读取失败可能当作
旧值为空，恢复明确未应用也可能发布旧投影并重试。专项均在真实 Store 适配层和 A 对象
执行。恢复 Unknown 时阶段结束通知和失败 Completion 按原偏序交付。

前一正式 B 候选的冻结 Both 实跑 24/24，Oracle 差异为零。
[原始报告](formal-b-both.json.gz)

## 实际回归

Target 全量 227/227。Primary 和 Radio 专项 ASan、TSan 各 95/95。
静态护栏、独立语法和逻辑 GN/Ninja 全部通过。第一键核对用例在后续 ICCID 请求的
原写调用中设置屏障，并确认此时收到匹配票据的并行核对仍不能清除原责任。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [架构](architecture.log)

四次独立重链分别变异快照锁域、恢复后继键、读失败判定和恢复未应用判定。
精确测试在预期断言失败，恢复原二进制均通过。
[快照锁域](snapshot-mutated.log) [恢复](snapshot-restored.log)
[恢复后继键](first_key-mutated.log) [恢复](first_key-restored.log)
[读失败](read_failure-mutated.log) [恢复](read_failure-restored.log)
[恢复未应用](untouched-mutated.log) [恢复](untouched-restored.log)

正式 C1 仍需完成逐键预写、后续重试的当前路由、已核验终步的 Store、全图关闭与模式等待。
恢复明确未应用目前保留责任，后续安全重做策略尚未接线。原生产品仍为 NOT_RUN。
