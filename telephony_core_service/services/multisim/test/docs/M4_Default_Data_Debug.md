# AF-2 M4 独立默认数据卡开发与调试

## Legacy 业务契约

取证基线为本仓 `bced9e9f3df22bcf54f5d017dea04d34cdec8543` 和 OpenHarmony Cellular Data
`29d6fa9c6105c6e39a9a765048868e4631ffbb8f`。公开路径从 `@ohos.telephony.data` 的 N-API
进入 `CellularDataClient`，经过 Cellular Data SA 的 IDL 方法，再由 `CellularDataService`
调用本进程的 `CoreManagerInner`。本仓的 Core Service SIM IPC 不提供这三个 Data 方法。
[外部 N-API](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/frameworks/js/napi/src/napi_cellular_data.cpp#L773) ·
[Cellular Data IDL](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/frameworks/native/ICellularDataManager.idl#L28) ·
[CellularDataService](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/services/src/cellular_data_service.cpp#L479) ·
[CoreManagerInner](../../../../frameworks/native/src/core_manager_inner.cpp#L2035)

| 输入及前提 | 实际效果、通知及原返回 | Target Host 对应行为 |
| --- | --- | --- |
| N-API 无效槽 | 参数校验先于 Client IPC，返回槽值错误。 | N-API 继续由外部宿主实现。 |
| Cellular Data SA 收到 setter | 依次检查系统应用、`SET_TELEPHONY_STATE`、`HasSimCard`、`IsSimActive`。分别返回 `ILLEGAL_USE_OF_SYSTEM_API`、`PERMISSION_ERR`、`NO_SIM_CARD`、`SLOTID_INVALID`。失败时不调用 CMI。 | 兼容入口重用宿主身份端口，在 R 查询可验证的存在性，由 T 查询当前激活账户和 Binding。 |
| CMI 或 Manager 直接调用 | CMI 空 Manager 或 Manager Stop 后返回 `LOCAL_PTR_NULL`。Manager 只检查 `0 <= slot < slotCount_`，越界返回 `SLOTID_INVALID`。有效槽即使无卡、停用或无有效账户也能推进参数和槽缓存。公告只有缓存向量为空或下标超界时跳过，缓存中无有效身份也可能公告。 | Host 接入覆盖具有有效 Binding 的选卡。直接 CMI 弱准入与已签收目标资格约束冲突，禁止无有效关系的请求登记新效果。 |
| 合格目标、参数调用已返回 | Controller 写 `persist.telephony.MainCellularDataSlotId`，设 `lastCellularDataSlotId_`，从 `localCacheInfo_[slot].simId` 取身份并更新 `defaultCellularSimId_`。公告携 `simId`、原 action、code 和 `defaultCellularDataSimIdChanged`，无 `slotId`，发布非 ordered。随后写 `SET_DEFAULT_CELLULAR_DATA` 行为事件的 `SLOT_ID`，Manager 再调账户 Callback 和 State Registry，返回 SUCCESS。参数返回值和 CES 发布布尔值均被忽略。 | StoreAdapter 使用原键。A 在原调用返回且资格仍成立时接纳槽和账户，按公告、行为事件、账户通知、同步返回的顺序输出。参数效果独立记账，已结束但效果未知时继续占有写权。 |
| 重复选择相同槽，或选择其他有效槽 | 同槽无去重，再调用参数、Data 公告、行为事件和 Manager 通知。只更新 Data，Primary、主卡 ICCID、用户主卡偏好不变。 | 每个被接纳的调用具有独立原票。 |
| 读取 | Manager 单槽版的槽查询固定 0，多槽读 Controller 缓存，启动期由参数加载并缺省到 0。SimId 查询以当前缓存槽重新调用 `GetSimId`，槽为负时返回 NO_SIM_CARD，身份不大于 0 时返回 FAIL，失败不改输出参数。Cellular Data SA 的槽 getter 将槽放进 out 参数并返回 SUCCESS。 | 测试观察 A 的兼容槽投影及 A 接纳的 Data 订阅。产品读取入口、启动加载和换绑后的原查询行为另行接管。 |
| 底层调用未返回、换卡或并发写 | 已观察到 Legacy 对同步返回的处理。源码没有按请求核对迟到参数效果的能力。Primary 的 C0、Radio 成功分支、自动主卡校正与 `SavePrimarySlotId` 都可能重写 Data。 | 使用 A 的实际共享写集、T 的同域目标重验和逐键原操作证据。后续核对只结原责任。 |

本仓源码定位见 [Manager setter](../../../sim/src/sim_manager.cpp#L451)、[Manager getters](../../../sim/src/sim_manager.cpp#L589)、
[槽范围](../../../sim/src/sim_manager.cpp#L1374)、[Controller setter](../../../sim/src/multi_sim_controller.cpp#L1417)、
[保存、启动读取及公告](../../../sim/src/multi_sim_controller.cpp#L1683)、[CES 载荷](../../../sim/src/multi_sim_controller.cpp#L1934)、
[账户通知](../../../sim/src/multi_sim_monitor.cpp#L801) 和 [诊断](../../../core/src/core_service_hisysevent.cpp#L89)。

独立调用之外，`CheckIfNeedSwitchMainSlotId` 可在卡和账户变化后保存或调度主卡，随后联动 Data。
`CellularDataService` 在规则检查后、CMI 调用前另有可选厂商 CHR 入口，其调用者信息与编译配置留在外部 SA。
通话数据状态机也可直接选择临时 Data 并恢复主卡。实际产品消费者版本、CHR 插件、CES/Registry 送达、
同一进程实例、平台参数原操作证明及设备数据连接行为仍需产品证据。
[自动校正](../../../sim/src/multi_sim_controller.cpp#L1120) ·
[厂商 CHR](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/services/src/cellular_data_service.cpp#L999) ·
[通话数据写者](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/services/src/state_machine/incall_data_state_machine.cpp#L260)
