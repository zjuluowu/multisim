# B 原设备证据与当前 modem 映射

输入为 `f15f5257b0227d48f460cdc674a92f70258a9062`。

## B 的原证据

独立审查在隔离源码树对真实 Assignment 和 Radio 运行两个反例。

1. R 接纳正常 ACK 后等待 Owner。A 先处理超时，Owner 后消费 ACK。原实现实际丢失
   `ConfirmedApplied`。
2. Submit 内同步收到原票匹配且可核验设备结束的回包，随后返回 `HandoffUnknown`。
   R 在 Submit 返回 A 前恢复并交付证据，原 A 在 `Open` 阶段丢失
   `ConfirmedNotApplied`。

A 在第一种交错中按原票将设备结果作为晚证据接纳，在第二种交错中把 R 已核验的
B 证据留在原待核对请求。两个路径均检查原步骤、轮次、阶段、成员、Binding 和写权。
原证据不能再次完成旧调用或修改新角色。实际设备结束和 A 尚须核对的业务效果各自保留。
[独立基线反例](independent-baseline.json.gz)

## C0 的当前 modem

Legacy `MultiSimController::SetPrimarySlotId` 使用协议控制器读到的动态 modem 归属
决定 C0。正式 Target 现在采用同一条件。原启动目录只描述所属端点，业务请求使用平台的
当前观察。回归在目录保持 0→1、1→0 时，使用当前 0→0、1→1 的平台映射。卡槽 0 的
原 C0 正常完成，卡槽 1 的设备分支在当前正式 Host 装配禁用时拒绝。两次均无设备发令。
首次全量的六个四角色组合用例只调整了目录配置。受控 Radio 当时仍报告槽 1 为当前 M0，
导致原预期 C0 拒绝。补充这些用例的动态平台输入后复验，原业务断言保持。
[首次全量结果](c0-fixture-initial.json.gz)

前一发令许可检查点的冻结 Both 实跑 24/24，Oracle 差异为零，边界整理后的
Legacy PIN 精确单例通过。
[Both 原始结果](permission-checkpoint-both.json.gz) [Legacy PIN](permission-checkpoint-legacy-pin.json.gz)

## 本批验证

Target 全量 215/215。Radio 和动态 C0 专项 ASan、TSan 各 77/77，对应同一生产源码。
全量夹具随后显式补充了上述六个非 sanitizer 筛选内用例的原生路由输入。静态检查、
逻辑 GN/Ninja 和独立语法通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [架构](architecture.log)

独立替换产物中的 A 对象，分别恢复目录推断、移除已经排入 Owner 的原证据接纳、拒绝
Submit 返回前的原证据。三项都在预期断言失败，原二进制恢复后精确复验均退出 0。
[C0 变异](c0-mutated.log) [恢复](c0-restored.log)
[Owner 变异](queued-mutated.log) [恢复](queued-restored.log)
[同步交接变异](early-mutated.log) [恢复](early-restored.log)

当前正式模块仍待 B/C1 装配。部分 C1 成员组结束、整轮取消许可、业务和 Store 各自结清，
仍需后续联调。设备、原生产品和正式生产切换验证均为 NOT_RUN。
