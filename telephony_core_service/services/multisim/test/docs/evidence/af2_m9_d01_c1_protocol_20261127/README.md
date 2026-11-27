# C1 原协议载荷及 UPDATE 回执

输入 main 为 `0d578e95313f406b7d3b4a37765444589406d312`。本批执行 Target Host
组件内的真实 A/R 发令、回包及受控时钟，通过正式生产归档链接。

- A 在预写前从 R 取得各原生协议成员的当前逻辑 modem 及 technology。CHECK 和失败 COMPLETE 发原能力。
  UPDATE 和成功 COMPLETE 在同一冻结观察上交换主 modem 与原目标的能力。四端点两协议栈场景显示每阶段
  只有原两成员发令。重试从原失败收据出发，在 999 毫秒无新增发令，1000 毫秒才接纳后续原轮。
- R 保存原 token、请求、成员与已核实 serial。UPDATE 仍由合法 NOTIFY 推进。错误 ACK 独立锁存，
  同一成员 NOTIFY 接纳后仍允许原错误 ACK 在本轮 COMPLETE 前参与最终结果。
- 在最后一个 NOTIFY 后出现的原错误 ACK 可以让原业务失败并恢复旧参数。已经发送的 COMPLETE 保持原载荷。
  重复或缺结束标志的 NOTIFY 不得再次隔离已结清的原成员。
- 同步回包待 Submit 返回后核对 serial，再在 R 短锁内按实际缓冲顺序接纳。外发 Owner 投递走原生产路径，
  R 锁在投递之前释放。错误 serial 不会污染原 ACK 或合法 NOTIFY。

首次 Target 构建有一个拓扑夹具因缺少新增的原生能力输入而无法进入 C1。补充该夹具与目标
modem 观察一致的原始能力，两端冻结输入及断言保持。见 [首轮输出](first-target-fixture.log)。

## 实际回归

Target 237/237，Primary、Radio 及涉及 C1 的拓扑专项 ASan 和 TSan 各 106/106。
逻辑 GN/Ninja、独立语法、架构护栏和冻结资产完整性通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [架构](architecture.log) [资产完整性](integrity.log)

原十二个冻结共同契约在两端执行 24/24，Oracle 与后端差分均为零。
[Both 与 Oracle](both.json.gz)

独立复验五个精确用例，退出码均为 0。缓冲重放和新回包共用 R 短锁已经独立审查。
新回包抢在缓冲重放前的窗口还缺独立双线程屏障用例。
[四项原始输出](independent-review.log) [重复 NOTIFY](independent-review-duplicate.log)

六次负控替换独立生产归档中的 Assignment 或 Radio 翻译单元并重链。各变异执行精确用例返回 1，
恢复原对象后同一用例返回 0。

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| UPDATE 沿用旧能力 | 两成员实际命令载荷没有交换 | [变异](wire_swap-mutated.log) [恢复](wire_swap-restored.log) |
| 原重试改用 1 毫秒 | 999 毫秒时已多发两笔 CHECK | [变异](retry_delay-mutated.log) [恢复](retry_delay-restored.log) |
| 丢弃晚 ACK 失败 | 最终 API 返回 0，旧参数未恢复 | [变异](no_late_ack-mutated.log) [恢复](no_late_ack-restored.log) |
| 接纳闭合成员的重复 NOTIFY | 重复的 Unknown 隔离了仍在途的其他成员 | [变异](duplicate_reopens-mutated.log) [恢复](duplicate_reopens-restored.log) |
| 合并具有不同 serial 的同步 ACK | 正确 ACK 丢失，失败 COMPLETE 被改为成功 | [变异](collapse_serials-mutated.log) [恢复](collapse_serials-restored.log) |
| 反转同步回包接纳顺序 | 最后一个 NOTIFY 之后的 ACK 改写了 COMPLETE 载荷 | [变异](reorder_buffered-mutated.log) [恢复](reorder_buffered-restored.log) |

正式 C1 的协议轮期限、API 等待期限、终步逐键提交、部分成员结合 Store 的结清及正式组装验证仍待。
受控 Host 适配器提供协议输入。真实产品 RIL/HDI 供证仍为 `NOT_RUN`。
