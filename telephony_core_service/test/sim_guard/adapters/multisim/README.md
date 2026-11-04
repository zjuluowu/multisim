# 真实 multisim 接入位置

当前生产卡状态/PIN尚未迁入multisim，不提供返回预期值的占位Adapter。统一入口对本实现返回BLOCKED；Target Suite也未运行。

迁移相应真实业务后：

1. 在本目录实现 `contracts/sim_sut.h` 的全部 `ISimSutAdapter` 方法及同名 `CreateSimSut(FakeRil&)` 工厂。初始化/销毁绑定真实宿主，RIL事实及应答绑定原有设备语义，公开查询/通知只做类型映射。
2. 在 `test/systemtest/BUILD.gn` 新增真实生产源绑定及 `sim_guard_multisim` 目标，复用 `runner/contract_test.cpp`、scenario、Oracle、Fake。可以在替代后的分支只链接新生产实现，不要求旧新库共进程。不要更改根生产源表或额外部署SA来运行测试。
3. 在 `run_sim_guard.py` 的SUT构建选择处注册新目标、生产源码清单和必须正执行的符号/路径，去除对应BLOCKED入口。用实际迁入路径替换Legacy执行证据表；不更改共同测试数量、filter或断言。
4. 运行相同 `--suite=compat --product=host_dual_psim`。共同测试五份摘要必须保持完全相同，公开错误码原样保留，状态只做枚举映射；不得从Fake返回观察、过滤不合预期通知、拒绝旧响应来修正SUT错误，或调用内部setter构造状态。
5. 新目标还需真实生产突变的正常→失败→恢复验证、跨用例排空和退出验证。单独比较两份结果的公共快照；差异区分兼容性违约、旧缺陷修正及待裁决规则。

可执行一致性检查已经嵌入共同Compatibility：相同设备事件驱动请求、相同输入/查询/解锁/通知Oracle，以及每次析构注销和worker退出。静态摘要/include检查保护共同资产。仅实现C++接口不能证明Adapter语义等价；必须由真实SUT执行这些用例。没有真实新业务时不宣称第二阶段通过。

Notifications返回自Start以来的累计通知快照，不能查询即清空。Settle只等已投递的宿主事件完成，不能自动交付Fake中刻意保留的外设应答。共同Driver有界批量交付已发查询，不要求每次通知恰好一个请求。独立DEFECT_001复现需要该故障调度能够形成旧READY与较新ABSENT在途响应；若新实现序列化使此前置条件无法成立，应另行记录调度不可达，不能将其解释为业务违约或伪造应答。Compatibility的七条业务Oracle保持相同。
