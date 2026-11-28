# C1 终步与 Primary Store 关闭收据

输入 main 为 `b3f9d52c96ce88d4cce2094a937e6b1b43ae106c`。本批直接执行 Target Host 的
A/R 和 PrimaryHostLifecycle，正式 SimModule 仍由 B、C0 提供兼容入口的样板。

- C1 接纳完整原 COMPLETE 后复用逐键机制，仅写尚未执行的 DefaultData。原顺序为 Main、Data 写入、
  Data 公告及行为记录、Done(false)、SenseSwitch 结束、原 Completion。阶段通知与外部结束在 A 短锁外。
- 原 Main 接纳以 T 同域核对为准。换绑先发生时，按原快照恢复旧参数，无新 Data。已经合法接纳的 Data
  公告及行为记录使用同一份原资格。独立复核覆盖了二次资格判定对已完成业务的误撤销。
- 同时验证已返回与未返回的 Unknown 写入。在两种情况下均保留原逐键收据。即使原 Radio 资源已经可复用，
  PrimaryHostLifecycle 仍报告 `BusinessEffectsUnresolved`。设备未决继续返回 `DeviceEffectsUnresolved`。
  匹配的原 Store 票据结清后才允许 Drained。
- 已结束且明确未应用的预写结果保留 Legacy 的返回语义。原设备的成功 ACK 不能直接作为换绑后旧业务的成功。

首轮实跑复现两个问题。PrimaryHostLifecycle 只核对设备并提前宣告 Drained。一个组件用例也仍断言换卡后
把旧设备成功作为业务成功，该断言按原目标资格契约修正，增加了新 Binding 保持及零新 Data 的回归。
[提前排空](first-drain.log) [换卡旧预期](first-revoked-binding.log)

## 实际回归

2026-11-28 复验 Target 全量 242/242，Primary、Radio 和 C1 拓扑专项 ASan、TSan 各 111/111。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz)

十二个原冻结共同契约在两端执行 24/24，后端与 Oracle 差分均为零。架构护栏、独立语法、
纯规则、逻辑 GN/Ninja 和冻结资产完整性通过。
[Both 与 Oracle](both.json.gz) [架构](architecture.log) [冻结资产](integrity.log)

六次负控分别替换独立生产归档的原翻译单元并重链。精确用例对各变异返回 1，恢复原二进制返回 0。

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 已返回的 Unknown 当作已完成 | 错误发布 Data 并交付成功 | [变异](unknown_success-mutated.log) [恢复](unknown_success-restored.log) |
| 关闭只核对设备 | 未结清的 Primary Store 仍被报为 Drained | [变异](only_device-mutated.log) [恢复](only_device-restored.log) |
| 确定的预写错误阻止后续步骤 | 原 CHECK 未发，Legacy 返回语义改变 | [变异](known_write_error-mutated.log) [恢复](known_write_error-restored.log) |
| SenseSwitch 结束先于 Done | 实际输出顺序改变 | [变异](early_end-mutated.log) [恢复](early_end-restored.log) |
| Main 撤权后跳过原恢复 | 旧预写的 Slot/ICCID 仍在 Store | [变异](lost_target-mutated.log) [恢复](lost_target-restored.log) |
| Data 行为重新申请资格 | 公告回调撤权后丢失已接纳的原行为记录 | [变异](requalify_report-mutated.log) [恢复](requalify_report-restored.log) |

正式 C1 的 45 秒协议期限、60 秒兼容等待、完整 Device/Store/Owner 组合关闭及产品接入继续按各自门禁验证。
