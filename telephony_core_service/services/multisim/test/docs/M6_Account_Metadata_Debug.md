# AF-2 M6 账户资料开发与调试

## Legacy 业务契约

| 入口 | 资格及账户定位 | Provider 写入 | 缓存、通知及原结果 |
|---|---|---|---|
| `CoreServiceSim::SetShowName` → `SimManager::SetShowName` → `MultiSimController::SetShowName` | 先检查系统应用，再检查 SET 权限、管理器与回调、槽支持。按当前槽取已存在卡的持久 `simId`，无需激活 | 普通 SIM Provider，`sim_id = simId`，单列 `show_name` | Provider 返回 −1 时为数据库写失败。其余返回进入缓存更新。先 Current，再按旧数组位置更新 Historical，历史位置缺失返回数组越界。仅整体成功时通知一次。空值和重复值都会写入 |
| `CoreServiceSim::GetShowName` → `SimManager::GetShowName` → `MultiSimController::GetShowName` | 系统应用、GET 权限、管理器与回调、槽支持。读当前槽账户 | 无 | 查询返回 Current 名称，空名称也是有效结果 |
| `CoreManagerInner::SetSimLabelIndex(iccid)` → `SimManager::SetSimLabelIndexByIccId` → `MultiSimController::SetSimLabelIndexByIccId` | 内部 CMI 路径，ICCID 非空、标签 1–10。可针对已知历史账户，不要求当前绑定 | 同一 Provider，`icc_id = iccid`，单列 `sim_label_index` | Provider 返回 0 后更新所有匹配历史和当前条目，整体成功时通知一次。重复值仍写入和通知。旧实现可能在历史缓存非空但没有匹配条目时报告无变更成功 |
| `SimManager::SetSimLabelIndex(simId)` | 同一标签范围，但仅接纳历史缓存中匹配的 eSIM | 委托 ICCID 写入 | 物理卡在这条路径不产生写入 |
| `MultiSimController::GetSimAccountInfo` | 当前槽账户 | 无 | 账户资料含原始标签索引。物理槽 `GetSimLabel` 另按设备配置生成展示标签 |

实际 URI 为 `datashare:///com.ohos.simability/sim/sim_info`。Legacy 实测包括直接账户查询、重复值、空名称、非法标签、物理卡拒绝 eSIM setter、Provider 拒绝及通知次数。[原始证据](evidence/af2_m6_20261125/README.md)

源码可从 [CoreServiceSim](../../../../services/core/src/core_service_sim.cpp)、[SimManager](../../../../services/sim/src/sim_manager.cpp)、[MultiSimController](../../../../services/sim/src/multi_sim_controller.cpp) 和 [CoreManagerInner](../../../../services/core/src/core_manager_inner.cpp) 按上表符号定位。外部 Provider 的固定源码通过显式更新桶传递列和值。[SimAbility::Update](https://github.com/openharmony/telephony_telephony_data/blob/74b8597ecbeea4dd39eacb4a9e7fa22f8a9389b4/sim/src/sim_ability.cpp#L180)
