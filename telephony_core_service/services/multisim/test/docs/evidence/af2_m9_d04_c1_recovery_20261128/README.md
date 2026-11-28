# 正式 C1 跨阶段整轮恢复

输入 main 为 `8db0e65aa6cbc00ba143fded465157b7486a0b50`。正式 Host 模块按协议成员事实发出原 CHECK。
首条实际请求带完整冻结成员，后续平台是否接到每个成员的命令仍按各自原 transport token 核对。
Stop、超时或路由变化阻止后续发送时，R 持有首条 CHECK 的原租约。

在隔离期间，逐命令终止只结清该条传输。两个成员都在 CHECK 实际提供可归因的未应用证据时继续适用原专门路径。
其他非正常结束情形使用受控平台的整轮证明，核对原 epoch、请求、CHECK、协议会话、全体成员、效果和真实可复用。
正常执行中的合法 COMPLETE 依然逐成员结清。恢复收据经 R 的原 Owner 通道投递，投递被拒后可沿原收据重投。

确定性用例覆盖 CHECK 已回包但 Owner 尚未消费、首条 CHECK 同步超时导致第二成员未发送、UPDATE 发令许可前封口、
UPDATE 部分发令、超时先于失败 COMPLETE ACK，以及原 Store 回写 Unknown 后结束通知中重入提交同一逐键票。
在最后一条路径，业务原结果先交付一次，设备可复用后仍报告 Store 原效果未结，待实际核对才排空。

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 单条 UPDATE 提前归还整轮租约 | UPDATE 结束后共有资源提前可用 | [变异](release_on_update-mutated.log) [恢复](release_on_update-restored.log) |
| 部分 CHECK 回执提前结束整轮 | 第二成员未发时原租约被提前删除 | [变异](release_on_partial_check-mutated.log) [恢复](release_on_partial_check-restored.log) |
| 隔离后的失败 COMPLETE 回执替代整轮证明 | 先超时后收到 COMPLETE 时错误视为整轮结束 | [变异](release_on_late_complete-mutated.log) [恢复](release_on_late_complete-restored.log) |
| 将原 CHECK 替换为当前阶段 | 当前是 UPDATE 时失去原 Completion 及 Store 恢复 | [变异](replace_anchor_with_current_step-mutated.log) [恢复](replace_anchor_with_current_step-restored.log) |
| 确认全体成员时忽略重复和遗漏 | 重复成员证明被错误接纳 | [变异](accept_incomplete_members-mutated.log) [恢复](accept_incomplete_members-restored.log) |
| 用实际已发成员决定完整范围 | 合法全体证据因第二成员未发送而被拒绝 | [变异](require_all_members_sent-mutated.log) [恢复](require_all_members_sent-restored.log) |
| 投递被拒后丢弃原收据 | 重投时原结果和回写均未发生 | [变异](lose_rejected_owner_receipt-mutated.log) [恢复](lose_rejected_owner_receipt-restored.log) |
| 去掉原 Store 收据保护 | 结束通知重入提前结清 Store，原 Completion 丢失 | [变异](unprotected_store_receipt-mutated.log) [恢复](unprotected_store_receipt-restored.log) |

独立复跑还定位了组件用例在 R 放行后缺少 Owner 同步的问题。回归增加真实 Owner 空闲屏障，
保留原 Completion 的精确断言。[修复前重复执行](owner-wait-before.log)

正式 Target 全量 263/263。Primary、卡组合及 EF 专项 ASan 和 TSan 各 127/127。
独立逐例 10/10，路由撤销相关用例再连续执行 20/20。冻结原始资产及合同完整性通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [独立复验](independent.log) [资产](integrity.log)

12 个冻结契约双端 24/24，Oracle 和后端差分均为零。静态架构护栏、独立语法和逻辑 GN/Ninja 通过。
[Both](both.json.gz) [差分](differential.json) [架构](architecture.log)

本批实证整轮未应用时的恢复。仍须关闭全图逐项诊断和组合重建，继续验证已施加的整轮效果以及正常 COMPLETE 已被
R 接纳、Owner 消费前发生业务超时的路径。没有可归因的整轮证明时，保留仍未结清的原责任。
整轮证明来自受控 Host 端口，真实 RIL/HDI 产品证据继续独立验证。
