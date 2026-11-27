# C1 原预写、重试路由与 Stop

输入 main 为 `77f0770dde403fc8e83fac8a71c23957ba1833b3`。本检查点验证 Target Host 的
Assignment 和 Radio 组件，另运行现有两端冻结契约。

- A 复用逐键收据登记初轮 Preference、PrimarySlot 和 PrimaryIccid，其后的轮次只登记需要重新执行的主卡参数。
  资格撤销或预写不确定时不发 CHECK，保留原票、原快照与确知写入的前缀。首条 CHECK 确认未发送时，恢复可恢复的
  原主卡参数，结清原角色资格并交付一次失败结果。
- 下一轮重读 Radio 的原生成员和路由，核对原 Binding 后，为新 attempt 冻结当前路由。旧轮的请求和协议身份不变。
- A 锁外向调度器交接。Stop 已撤销原意图而任务迟返时，按返还的原任务号取消。Stop 先于 T 域登记时，旧预写不得跨过准入点。
- Target 首轮 TSan 的失败来自关闭用例对首次返回时机的假设。用现有 Owner 队列屏障明确固定关闭前后的观察点，保留原断言。
  [首轮用例输出](first-tsan-stop.log)

## 本候选实跑

Target 全量 232/232。Primary、Radio 与正式 Primary 入口专项 ASan 和 TSan 各 100/100。
静态护栏、107 个独立 C++ 单元和组合头的语法、纯规则、逻辑 GN/Ninja 及冻结资产完整性通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [架构](architecture.log) [资产完整性](integrity.log)

十二个冻结共同契约在 Legacy 和 Target 独立归档中执行 24/24，两端差分与 Oracle 差分均为零。
[Both 与 Oracle](both.json.gz)

五次负控分别替换独立 Target 生产归档中的 `assignment_service.cpp` 对象并重链原 Host 用例。各变异实跑
返回 1，原二进制运行同一用例返回 0。

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 去掉 T 域登记处 Stop 检查 | Stop 后多出一笔 C1 Store 写入 | [变异](stop_registration-mutated.log) [恢复](stop_registration-restored.log) |
| 丢弃迟返的任务取消 | Stop 后仍有一个原任务排队 | [变异](stop_schedule-mutated.log) [恢复](stop_schedule-restored.log) |
| 下一轮沿用旧成员路由 | 原四笔命令结束后缺新轮两笔 CHECK | [变异](old_retry_routing-mutated.log) [恢复](old_retry_routing-restored.log) |
| 首条 CHECK 确认未发仍保留预写 | 主卡 Slot 和 ICCID 未回原值，原角色资格仍在 | [变异](unsent_prewrite-mutated.log) [恢复](unsent_prewrite-restored.log) |
| 未决预写缺少前缀恢复意图 | 原票缺少继续原恢复所需标志 | [变异](drop_prewrite_receipt-mutated.log) [恢复](drop_prewrite_receipt-restored.log) |

## 下一项正式 C1 技术条件

独立反审核实了 Legacy 的两个期限，各为协议轮 45 秒和调用等待 60 秒。成功 COMPLETE 使用交换后的 modem 和
technology，失败 COMPLETE 使用原值及失败状态。原 UPDATE 错误 ACK 参与失败判断，NOTIFY 推进成员。原协议收到
全部最终结果后才能按原顺序提交 Data、完成通知及等待。后续实测应覆盖 1000 毫秒重试预算、部分成员原证据和换绑后的
旧终步。产品 RIL/HDI 协议证据仍 `NOT_RUN`。
