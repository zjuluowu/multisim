# AF-2 兼容行为正式裁决

批准人角色为项目架构负责人，批准日期为 2026-11-26。裁决输入为本地与远端 main
`cd0330c51aafb9c9697ff560c9008268a5a98039`。三项裁决状态统一为
`APPROVED_TARGET_BEHAVIOR`，属于有意批准的非等价兼容增强。

<a id="cd-01"></a>

## CD-01 未知 ICCID 设置 SIM 标签

- **里程碑及基线**。M6，目标实现 `4e6b6b2c9`，Host 验收 `7edbc86ff`。
- **Legacy 与 Target**。Legacy 在 Provider 返回成功且没有匹配条目时可发起一次无实际更新的操作并返回成功。Target 对未知持久账户返回 `TELEPHONY_ERR_ARGUMENT_INVALID`，不写 Provider。已知历史账户没有当前 Binding 仍可设置标签。
- **批准行为及理由**。保留 Target 上述行为。资料写入应定位到合法持久账户，未知账户上的无变更操作无成功语义。
- **产品验证**。核验可信 CMI 调用者对参数错误的处理及现网 Provider 接入。
- **源码与证据**。[Legacy 标签写入](../services/sim/src/multi_sim_controller.cpp#L616) · [Target 账户资料](../services/multisim/account/topology_metadata.cpp) · [M6 原始验收](../services/multisim/test/docs/evidence/af2_m6_20261125/README.md) · [本轮直接实测](../services/multisim/test/docs/evidence/af2_compatibility_20261126/README.md)。

<a id="cd-02"></a>

## CD-02 无当前 Binding 时保存与查询 IMS Preference

- **里程碑及基线**。M7A，目标实现 `d975637cf`，Host 验证归档 `cd04fbad6`。
- **Legacy 实测**。单激活空槽以空 ICCID 发起读写，各一次。保存返回 `0`，查询返回 `(0, -1)`。
- **Target 实测与批准**。无当前 Binding 时不访问 Provider，保存返回 `-1`，查询返回 `(-1, -1)`。已绑定场景保留 Provider 原返回、输出参数、UNKNOWN 及直读语义。
- **理由及产品验证**。当前卡业务须有合法当前账户关系。仍需核验原 CMI 调用者对该返回的处理及产品 Provider 接线。IMS Preference 与真实 IMS 开关各自验收。
- **源码与证据**。[Legacy IMS 方法](../services/sim/src/multi_sim_controller.cpp#L1956) · [Target 账户资料](../services/multisim/account/topology_metadata.cpp) · [M7A 原始实测与差异](../services/multisim/test/docs/evidence/af2_m7a_20261125/README.md)。

<a id="cd-03"></a>

## CD-03 G3b 号码设置与独立回写固定目标

- **里程碑及基线**。M7B，目标实现 `6afce256e`，Host 验收 `cd0330c51`。
- **Legacy 证据**。真实卡失败及账户回退已实测。设置路径在落库时重新按槽取账户，查询回写只捕获槽和号码并在执行时解析账户。旧操作可能影响后继账户是由源码推导的交错，Legacy 换卡写错账户未实际复现。
- **Target 实测与批准**。设置在异步任务实际执行时固定账户、Binding、Session 和字段资格。独立回写保留原目标和字段版本。换卡、会话失效或字段资格变化后不写后继账户。超时或终结后的晚证据只结清原责任，不复活业务或重复写入、通知、回调。
- **理由及产品验证**。异步步骤使用原业务对象和关系。真实产品仍须验证外部调用者、原生 MSISDN 桥、各卡型 EF、定时器、原传输关联、实际 Records 会话及硬件可复用性。IPC 受理时固定目标仍需另案。
- **源码与证据**。[Legacy 查询及写库](../services/sim/src/multi_sim_controller.cpp#L1731) · [Target 设置](../services/multisim/account/number/set_show_number_use_case.cpp) · [Target 查询回写](../services/multisim/account/number/show_number_query.cpp) · [M7B 原始实测与差异](../services/multisim/test/docs/evidence/af2_m7b_20261125/README.md)。

<a id="inheritance"></a>

## 继承与验收

后续 AF-2/AF-3（含 M8）直接继承 CD-01、CD-02、CD-03。三项行为进入 Target 正式业务契约和独立回归，已有裁决可直接引用。历史 Legacy 断言及共同契约 Oracle 继续按原口径维护。这三个编号从兼容缺陷、重复审批和后续 Host 阻塞的范围移除。恢复旧行为或改变已批准行为需要另案决策。

仅当业务需求变化、新证据表明有明确产品契约冲突、外部调用者有真实不兼容依赖，或发现此前未覆盖的正确性问题时，才提出独立变更。新提案须有自身证据和批准记录。

本次批准涵盖上述 Host 行为及其后续回归依据。生产写权和 Legacy 替换、外部调用者产品适配、产品 Provider 归因及读取因果覆盖、Core/SA/IPC、真实 IMS 开关、原生 MSISDN 与硬件证据、AF2-03、B/C1、完整 Reload 和 SO 卸载均按原门禁独立验收。

[唯一执行状态](SIM_Refactor_Execution_State.md#current-status) · [Host 独立回归矩阵](../services/multisim/test/docs/Backend_Architecture.md#approved-compatibility) · [独立测试契约](../services/multisim/test/config/approved_compatibility_contracts.json)
