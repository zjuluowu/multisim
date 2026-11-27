# Radio 交接分类与 B 全未发结清

输入为 `aea7fccac5c77da02d18f2d03b4b0a9b7888b5cb`。本批覆盖 Target Host
Assignment 与 Radio 组件。正式模块目前仍只为 C0 开放角色写入。

R 返回三类原交接结果。整个步骤已证明未发时返回 `ConfirmedNotSent`。已经存在原成员
责任、平台交接不确定或设备关联不完整时返回 `HandoffUnknown`，保留原资源与对应责任。
平台给出许可、关联及已接纳证据时返回 `HandoffAccepted`。

B 确认整组未发且原偏好写已完成时，A 结清原步骤和共享角色写权。设备未发的业务错误
沿用 Legacy `SetPrimarySlotIdWithoutModemReboot` 的 `TELEPHONY_ERR_LOCAL_PTR_NULL`。
若原请求已超时，A 已认领的结束事件和 Completion 必须先结束，后继请求才能获得原共享
写权。设备租约可独立归还。Unknown 期间旧调用仍按原等待预算完成，设备效果待原证据核对。

独立审查构造了 Submit 返回 `ConfirmedNotSent` 与超时平台 End 未返回的交错。
回归以屏障控制两个调用，确认设备域已排空、原 A 写权仍阻挡后继请求，最后核对新请求
能够实际交接。已发、部分已发、缺 serial、关联矛盾均继续隔离。

Target 全量 215/215。Radio 和 Primary 专项 ASan、TSan 各 76/76。静态架构、独立语法、
逻辑 GN/Ninja 和冻结资产一致性通过。
移除原超时投递门，实际提前释放角色写权，引入后继写入。将缺 serial 的 Unknown 变异为
ConfirmedNotSent，实际击中原交接分类断言。两个独立重链 ELF 均退出 1，恢复后退出 0。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [完整性](integrity.log)
[架构](architecture.log)
[超时变异](timeout-mutated.log) [恢复](timeout-restored.log)
[分类变异](serial-mutated.log) [恢复](serial-restored.log)

上一 B 原证据检查点完成冻结 Both 24/24，Oracle 零差异。
[上一候选原始结果](prior-both.json.gz)

正式装配、B/C1 逐键 Store 原效果、C1 整轮处置及全对象图关闭仍待。原生产品验证
为 NOT_RUN。
