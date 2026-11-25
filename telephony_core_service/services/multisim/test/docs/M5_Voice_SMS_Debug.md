# AF-2 M5 默认 Voice / SMS 开发与调试

## Legacy 契约

Voice 的公开路径从 Core Client/Proxy 进入 Core SIM。Core 依次通过业务准入、
系统应用、`SET_TELEPHONY_STATE`、Manager 存在性和适用槽能力检查。SMS 的公开
权限检查位于 SMS SA。调用经过 SMS Interface/Misc Manager 后，由本仓
`CoreManagerInner` 委托给 `SimManager`，该内部路径沿用 Manager 的槽范围校验。
[M5 Legacy Core Voice](../../../core/src/core_service_sim.cpp#L452) ·
[固定版本 SMS SA](https://github.com/openharmony/telephony_sms_mms/blob/2336a3c3ba9a5cc53e95535cf25a6cfe15c74de7/services/sms/sms_service.cpp#L671) ·
[SMS Misc](https://github.com/openharmony/telephony_sms_mms/blob/2336a3c3ba9a5cc53e95535cf25a6cfe15c74de7/services/sms/sms_misc_manager.cpp#L569) ·
[CMI 和 Manager](../../../sim/src/sim_manager.cpp#L415)

| 条件 | Legacy Voice | Legacy SMS |
| --- | --- | --- |
| 设有效槽 | Controller 先确认卡存在、已激活，再用当时缓存的 simId 写 Voice。 | 同样的资格和身份查询，写 SMS。 |
| 范围外、无卡、停用 | 分别返回 `SLOTID_INVALID`、`NO_SIM_CARD` 和 `CORE_SERVICE_SIM_CARD_IS_NOT_ACTIVE`。 | 相同。 |
| 设置默认与清除 | `set_card` 命令提交 `sim_id` 和 `card_type=1`，清除使用槽 -1、存储载荷 simId 0。 | 同一 URI，`card_type=2`；清除载荷相同。 |
| Provider 返回 | Controller 只将 -1 映射为 `DATABASE_WRITE_FAIL`。成功分支清、置当前缓存中该角色所有槽的标记。 | 相同，写各自列。 |
| 重复、公告和账户通知 | 同身份仍提交 Provider 并更新当前缓存，跳过 CES；Manager 成功仍通知账户。不同身份先记去重身份再公告。CES 失败返回 `PUBLISH_BROADCAST_FAIL`，此前角色仍保留，Manager 不发账户通知。 | 相同顺序。两个 CES 的 action 和 eventData 分别对应 Voice/SMS，code 1、非 ordered、载荷 `simId`。 |
| 清除后查询 | 无活跃的 Voice 标记时返回 -1。 | 无活跃的 SMS 标记时回退到首个激活槽。 |
| 单卡配置与读源 | Manager 槽 getter 可固定回 0，SimId getter 按 Controller 得到的槽找身份，无有效值返回成功并给 -1。 | 同类分层，Controller 保留 SMS 回退。 |

[Controller setters 和 getters](../../../sim/src/multi_sim_controller.cpp#L1248) ·
[原 Provider 命令](../../../sim/src/sim_rdb_helper.cpp#L290) ·
[账户通知](../../../sim/src/multi_sim_monitor.cpp#L801)

固定 Provider 源码先清同类列，再置目标行。Voice 列是 `is_voice_card`，SMS 列是
`is_message_card`。命令共用表及事务入口，字段互异。目标置位失败后仍会走 commit
并返回其结果，因此整数结果没有逐效果归因。Host 端口显式报告同票的未写、完整、
部分或未知，真实产品事务隔离与逐效果供证仍属于产品门禁。旧 Primary/Data setter
写系统参数，默认 Voice/SMS setter 不共写这些键。发现期联合建行可能写两种角色列。
[Provider 事务](https://github.com/openharmony/telephony_telephony_data/blob/74b8597ecbeea4dd39eacb4a9e7fa22f8a9389b4/sim/src/rdb_sim_helper.cpp#L88)

两个 Legacy 特征用例实际运行 Voice Core 入口和 SMS CMI 入口，经过 Manager、
Controller、SimRdbHelper。记录原 URI、命令值、公告、账户通知与返回。
[Legacy 原始证据](evidence/af2_m5_20261125/README.md)
