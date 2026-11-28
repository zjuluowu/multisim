# 正式模块关闭期间的原票接纳与两遍观察

输入 main 为 `86550cbb25d75d1af45dcf615d053f92f34ba7df`。用正式 `CreateCoreSimModule` 装配运行。
受控 Scheduler 在交还取据结果前同步交付原 PIN 设备证据，平台回调完成退出后旧版错误返回 Drained。
同一窗口提交的合法 Store 原逐键效果在旧版被拒，原责任须等额外调用才能结清。
[PIN 基线反例](baseline.log) [Store 基线反例](store-baseline.log)

关闭继续接纳原票，封闭新业务。即使原调用在一次取据期间完整进入并退出，H 仍保留本轮活动标记并重取。
第二遍重新核对 Scheduler、R、A、卡传输、T 和本地在途。PIN 复验还将原回包重复提交安排在第二遍，
证实模块保守保留直到随后的稳定观察，原 Completion 只交付一次。Store 复验采用可归因于原写的证据。
重复 Stop 复用封口状态。逐项关闭输出仍待补齐。
[专项执行](candidate.log) [独立逐例 2/2](independent.log)

Target 全量 255/255。ASan、TSan 各 121/121，覆盖 Primary 正式和组件、卡侧组合与 EF 作用域。
冻结资产完整性验证覆盖 317 份历史原始资产、299 项处置、八项不可变合同或参考。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [冻结资产](integrity.log)

12 个原冻结契约双端执行 24/24，后端及 Oracle 差分为零。静态护栏、独立语法及逻辑 GN/Ninja 通过。
[Both](both.json.gz) [差分](differential.json) [架构护栏](architecture.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 省略第二遍实际取据 | 第二遍原回包未进入正式路径，模块提前返回 Drained | [变异](skip_second_read-mutated.log) [恢复](skip_second_read-restored.log) |
| 忽略瞬时进入并返回的原帧 | 取据期间完成原 PIN 后首次错误返回 Drained | [变异](ignore_transient_admission-mutated.log) [恢复](ignore_transient_admission-restored.log) |
| 恢复旧取据期间原 Store 禁入 | 可归因的 Store 原票被拒，原业务票未结清 | [变异](block_original_store-mutated.log) [恢复](block_original_store-restored.log) |

独立反审核对了 C1 跨阶段的现有五条实际执行路径。后续正式反例应覆盖全部 CHECK ACK 已由 R 接纳、
Owner 尚未消费时 Stop，以及 UPDATE 在发令许可前被 Stop 封口。两者都需要可归因于原 CHECK、原协议会话
及整轮成员的终止证据。现有正式图仍缺该证据入口，原物理隔离保持。独立审查还识别了 Scheduler 封口抛异常
后无法重试的旧路径。
[C1 独立复跑 5/5](cross-stage-independent.log)
