# D02 卡应用发令准入检查点

输入为 `230ca8271dfe04735050773c81e50ada5ce41033`。独立 Agent 源码反查发现 MSISDN
在读取会话资格和原命令登记之间会释放卡提交域，随后换卡可使已读资格过期。
现有卡应用提交域提供 `WithCurrentApplication`，在同域核对原会话并登记原命令。
MSISDN 复用现有屏障、原票和结果路径。锁外 Submit 前由原登记保有责任。
无发送许可的租约按确证未发归还。无新增会话表或资源屏障。
[源码](../../../../card/runtime/msisdn_runtime.cpp)

正式 `SimModule::SetShowNumber` 先确定原号码目标。在受控设备路由查询内换卡，检验原命令零发令、
未写 Provider、原回调失败一次。复用同一资源后，新会话继续实际写卡及入库。Stop 排空。
运行时负控只移除 `WithCurrentApplication` 的会话核验。编译、链接均成功，
用例在「Revoked application issued an MSISDN command」断言实际失败。恢复后单例通过，
源码恢复 SHA-256 为 `24a4027a77352da4aadd60b7c97edb4e107e391c17e09d1a6d3a7d2d16c0e326`。
[负控](revoked-send-mutation.log.gz) · [恢复](revoked-send-restored.log.gz)。

[Target 全量 182/182](target-full.json.gz)。上一提交协议路由候选的 静态、GN/Ninja [通过](architecture.log.gz)。
[Both 24/24](d01-both.json.gz) 与 [Oracle 差异 0](d01-differential.json) 一并保存。

当前生产候选 101 个 C/C++ 文件、11,265 物理行、8,619 有效行。Legacy 149、Target 182 个实际登记，
已证共同语义 13。未证的语义去重保持开放，登记数用于后端执行计数。
[一级、二级及源归属与测试统计](source-and-test-inventory.json.gz)。

PIN、EF 共用传输机制、LoadSession 接入及全图生命周期仍在实施范围。上述证据对应既有号码链路及原会话准入。
