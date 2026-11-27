# B 分支的原参数效果与过期目标

输入为 `64a9964aa898bf1d7e9fb01acc1362ee629fdf36`。本批使用真实
`AssignmentService`、`AssignmentStoreAdapter`、`ResourceRadioRuntime` 和
`TopologyService`。正式模块的 B 装配仍待。

B 的专用命令前登记原偏好写。收到原设备结果后，复用 C0 已验证的逐键写许可和收据，
保留 PrimarySlot、PrimaryIccid、DefaultData 和输出顺序。同一 A 写集覆盖原流程，
平台参数写与 B 阶段通知在 A 提交域之外执行。

两个确定性交错分别在偏好写和设备成功后的 ICCID 写返回 Unknown。第一项零设备发令，
第二项设备租约已归还。A 两次都保留原 Store 单键和写权，拒绝错误操作证据。原 Store
效果证明可结清已失效目标的原操作，过程中保持新绑定、角色和原 Completion。

独立反审发现原 B 的已撤权 Binding 在收到设备成功时仍能输出业务成功。确定性用例在
`EndLegacySenseSwitch` 中换绑，核对原设备成功证据保留、零后继键和零成功角色通知。
旧 Target 组件测试曾接受这个返回 0 的轨迹，本批按照原写资格修正目标路径的断言。
Legacy 实现、冻结契约与 Oracle 保留。实际产品换卡回包语义作为产品门禁。

## 验证

Target 全量 218/218，Primary 与 Radio 专项 ASan、TSan 各 86/86。
静态结构、独立语法、逻辑 GN/Ninja 和保护资产通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz)
[架构](architecture.log) [完整性](integrity.log)

变异分别绕过设备前后的 Store 原票、恢复绑定失效时的假成功返回。三项精确用例分别
在原 Store 票或业务结果断言失败，恢复原对象后均通过。
[设备前](before-mutated.log) [恢复](before-restored.log)
[设备后](after-mutated.log) [恢复](after-restored.log)
[过期 Binding](replaced-mutated.log) [恢复](replaced-restored.log)

独立反审还指出正式入口异步 ACK 后才产生 Store 收据时，H 的旧缓存可能漏计。
该场景需正式接线后复验。C1 Store、整轮结清、正式 B 和原生产品验证仍待。
