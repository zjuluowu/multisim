# MultiSIM Subscription 重构进度与续接指南

## 1. 文档目的

本文档是 MultiSIM Subscription 重构的阶段状态索引。后续执行者在开始新的重构切片前，应先阅读本文，再按需阅读：

- `04_ddd_target_architecture.md`：目标架构和边界；
- `05_detailed_class_and_data_model.md`：领域模型和兼容映射；
- Core 子模块中本阶段源码与测试。

本文记录已完成的事实、未完成的边界、已作出的决策和下一步的最小切片。它不授权修改未列入当前切片的 API、IPC、数据库、RIL 或服务拓扑。

## 2. 仓库与分支

本次重构的 GitHub 仓库为 `github.com/zjuluowu/telephony`。

| 内容 | GitHub 分支 | 当前基线提交 | 说明 |
|---|---|---|---|
| 外层文档和子模块引用 | `multisim/subscription-projection-shadow` | `596e0a9` | 包含已记录的设计文档及 Core gitlink；本文档的提交会在该提交之后。 |
| Core 源码 | `multisim/subscription-projection-core-shadow` | `7f8ade76` | 包含 Subscription、eUICC Profile 和默认角色 Shadow 实现。 |

`telephony_core_service` 仍是 Git submodule。外层 `.gitmodules` 中该子模块 URL 指向同一 GitHub 仓库，以便外层分支可解析上述 Core commit。其他子模块不在本次重构范围内。

## 3. 总体策略

### 3.1 迁移原则

1. **先投影，后迁移**：现有 `localCacheInfo_`、`sim_info`、系统参数和 RIL 状态仍是事实源；新模型先作为只读 Shadow，不双写。
2. **旧接口优先兼容**：现有 Slot API、DataShare、广播、错误码和 RIL 路径继续工作；没有经单独确认，不以 `SubscriptionId` 替换对外 Slot 语义。
3. **关系先于事务**：先明确 Slot、Subscription、Port、Profile 和默认角色之间的关系，再引入持久化、异步 OperationId 或跨模块事务。
4. **拒绝歧义**：一个 Slot、Subscription 或角色存在多个候选时，Shadow 查询必须显式返回歧义，不能静默选择。
5. **最小化敏感数据**：Shadow Dump、Profile Sidecar 和诊断不得记录 ICCID、号码、Profile 昵称、运营商名称或 Subscription ID。
6. **一次只改变一个事实源边界**：数据库、DataShare、IPC、RIL、eSIM SA 与公共 ABI 的变化必须拆成独立阶段并先取得确认。

### 3.2 最终目标

目标是从 Slot 驱动的多卡实现演进为以 Subscription 为核心、但保留兼容适配层的多订阅平台。完整目标包含：

- 可版本化的 Slot、Subscription、Port、Profile 和 Modem 关系；
- 每个默认角色到唯一 Subscription 的可验证映射；
- Profile 切换、默认角色切换和 Modem 重映射的 OperationId 事务；
- 对迟到 RIL/eSIM 回调的 TopologyRevision/OperationId 过滤；
- 经确认后才提供的 Subscription ID 公共查询和设置能力。

## 4. 当前完成内容

| 切片 | 状态 | 当前实现 |
|---|---|---|
| Subscription 基础模型 | 已完成 | `Subscription`、`SubscriptionSource`、强类型 ID、`LogicalSlotBinding`、`TopologyRevision`。 |
| Legacy 投影 | 已完成 | `LegacySubscriptionRepository` 从 `localCacheInfo_` 构建请求期拓扑；旧 `GetSimAccountInfo` 继续输出旧 DTO。 |
| Registry/歧义处理 | 已完成 | `SubscriptionRegistry` 按 Logical Slot 和 Subscription ID 建索引；重复记录显式失败。 |
| Runtime Slot Inventory | 已完成 | `SlotRegistry` 从实际 `SimStateManager` 集合建立 Logical Slot 清单，并在额外 Manager 加入时刷新。 |
| eSIM/MEP 能力 | 已完成 | `CoreService` 将 per-Slot eSIM Capability Mask 传递至 `SlotRegistry`；MEP 与 eSIM 支持语义保持分离。 |
| eUICC Profile Shadow | 已完成 | `EuiccProfileTopology` 仅保留 Logical Slot、Port、ProfileState；成功 Profile List 更新，失败清空对应 Slot。 |
| 默认角色 Shadow | 已完成 | `SubscriptionRoleAssignment` 投影 Voice、SMS、Data；只在能唯一解析至当前拓扑 Subscription 时建立分配。 |
| 自动化测试 | 部分完成 | `subscription_repository_gtest.cpp` 覆盖基础拓扑、Capability、Profile Sidecar、角色解析和角色歧义。 |

### 4.1 当前事实源

| 关系 | 事实源 | Shadow 语义 |
|---|---|---|
| Subscription/Slot | `localCacheInfo_` | `simId -> SubscriptionId`，`slotIndex -> LogicalSlotId`。 |
| eSIM 支持 | eSIM 初始化时的 Capability Mask | 仅表示服务支持该 Slot，不表示卡存在、Profile 活动或 MEP。 |
| Profile/Port | 成功的 eUICC Profile List | 仅 Slot、Port、ProfileState；不是持久化 Profile 身份映射。 |
| Default Voice | `sim_info.is_voice_card` | 仅 active 且唯一可解析记录进入投影。 |
| Default SMS | `sim_info.is_message_card` | 无标记时保持旧逻辑，回退第一个 active 记录。 |
| Default Data | `persist.telephony.MainCellularDataSlotId` 与 `lastCellularDataSlotId_` | 按当前 Slot 解析，不使用 `is_cellular_data_card` 作为运行时事实源。 |
| Primary | `persist.telephony.MainSlotId`、Radio/RIL | 不属于 `SubscriptionRoleAssignment`；保持既有与 Default Data 的联动。 |

### 4.2 当前源码入口

| 位置 | 职责 |
|---|---|
| `services/sim/include/subscription.h` | ID、Subscription 与来源模型。 |
| `services/sim/include/subscription_topology.h` | Slot Binding、角色分配和 Shadow 聚合。 |
| `services/sim/src/subscription_topology.cpp` | Role/Slot 解析及无敏感标识摘要。 |
| `services/sim/include/euicc_profile_topology.h` | 最小 Profile Sidecar。 |
| `services/sim/src/multi_sim_controller.cpp` | 刷新 Shadow、从 legacy 缓存构造 Role Projection。 |
| `services/core/src/core_service.cpp` | eSIM Capability 和启动期 Profile List 发布。 |
| `interfaces/innerkits/include/i_sim_manager.h` | `SetEsimCapabilityMask`、`UpdateEuiccProfileSnapshot` 的 InnerKit 边界。 |
| `test/unittest/sim_gtest/subscription_repository_gtest.cpp` | 当前领域模型测试。 |

## 5. 兼容性与已授权边界

- 旧 Slot API、DataShare、参数名、广播 action/载荷、错误码、RIL 路径和 Primary Slot 状态机未修改。
- `ISimManager` 末尾已追加 `SetEsimCapabilityMask(uint32_t)` 和 `UpdateEuiccProfileSnapshot(...)` 默认实现。这是已获授权的 InnerKit vtable 演进，所有实现方和消费者必须协调全量重建；未新增 IPC transaction。
- 新 Profile 和 Role 数据只存在于 `MultiSimController` 私有 Shadow Snapshot；不向数据库或外部服务写入。
- 角色投影不参与业务返回值、持久化或事件发布，因此不能作为新的事实源。

## 6. 已知风险与暂停项

| 项目 | 状态 | 原因与处理要求 |
|---|---|---|
| Port/Profile/Subscription 持久化映射 | 暂停 | `sim.db`/`SimRdbInfo` 没有可信 `portIndex`，不能安全建立 Port 到 legacy `simId` 的映射。除非 Data Storage 同步提供 schema、迁移与回滚方案，不得猜测映射。 |
| Default Data 旧字段 | 已知不一致 | 运行时使用参数和 `lastCellularDataSlotId_`，而 `is_cellular_data_card` 不是当前运行时事实源。迁移前必须明确唯一写路径。 |
| Primary 与 Default Data | 保持旧行为 | Primary 成功会影响 Default Data；未引入独立 Role 或新状态机。 |
| 异步时序 | 未完成 | `TopologyRevision` 仅标记当前 Snapshot，未过滤迟到 RIL/eSIM 回调；不得假定其已提供端到端时序保护。 |
| 启动期 Profile 查询 | 已知代价 | 每个支持 eSIM 的 Slot 失败时可能等待既有查询超时；失败不阻断启动且会清空 Sidecar。 |
| 测试环境 | 未完成 | 当前无完整 GN/编译/测试工程，不能将静态检查描述为构建通过。 |

## 7. 下一阶段计划

### 阶段 A：完成当前 Shadow 切片的验证

1. 在完整 GN 环境编译 Core，并运行 `subscription_repository_gtest`。
2. 验证 eSIM 开关开启/关闭构建，Capability Mask 和 Profile List 成功/失败路径。
3. 验证 Voice、SMS、Data 切换后 Role Projection 与 legacy 结果一致；广播失败不得改变已完成的 legacy 写入或 Shadow 刷新。
4. 增加仅内部可见的一致性诊断前，确认其不在锁内调用数据库、参数服务、广播、RIL 或 IPC。

完成标准：领域测试通过，旧 `GetSimAccountInfo` 行为无回归，Profile/Role Shadow 不记录敏感标识，且验证结果记录到本文档或对应提交说明。

### 阶段 B：默认角色迁移设计，不实施持久化

1. 对 Voice/SMS/Data 的 DataShare 写路径、参数写路径和 Common Event 做完整调用方审计。
2. 设计唯一 Role 写路径、角色 Revision 和 OperationId 的兼容方案。
3. 不修改数据库、事件载荷、公共 API 或 Primary 语义，直到方案获得单独确认。

完成标准：输出可评审设计，明确数据 schema、迁移/回滚、事件顺序、错误码和兼容边界。

### 阶段 C：Port/Profile 映射前置条件

1. 在 `telephony_data_storage` 确认权威 Port 来源与 `sim.db` schema 演进所有权。
2. 获得数据库迁移、历史数据回填、回滚和多 Port Profile 生命周期的明确授权。
3. 仅在可信 Mapping 存在后，扩展 Registry 的 Port 到 Subscription 查询。

完成标准：Mapping 可唯一确定、迁移可回滚、无 Mapping 时有明确未初始化语义。

### 阶段 D：Modem 与异步事务

1. 引入 `ModemAssignment` 与容量仲裁模型。
2. 将 TopologyRevision/OperationId 接入 RIL、eSIM 回调过滤。
3. 为 Profile 切换、默认角色切换和 Modem 迁移建立可恢复事务。

完成标准：迟到回调不会覆盖新拓扑，失败可恢复，跨模块状态有单一事实源。

### 阶段 E：公共 Subscription 能力

只有阶段 A-D 完成且得到专门授权后，才可评估：

- `Get/SetDefault*Subscription(SubscriptionId)`；
- DataShare/数据库订阅标识迁移；
- IPC/InnerKit/ABI 版本演进；
- 对旧 Slot API 的双向适配和弃用策略。

## 8. 后续执行规则

后续执行者应遵守以下顺序：

1. 检查外层与 Core 分支、gitlink、工作树和未提交内容；不得覆盖其他子模块的变更。
2. 对数据库、公共 API/ABI、IPC、锁、异步回调、动态加载或测试修改，先加载对应工程规则并确认授权边界。
3. 一个提交只包含一个迁移切片；文档、实现和测试可同一切片提交，但不混入无关重构。
4. 每次完成后更新本文件中的阶段状态、提交号、验证结果、阻塞项和下一步入口。
5. 未执行的构建、测试、ABI 检查必须明确标记为未执行。

## 9. 当前续接入口

当前建议从**阶段 A：完整环境验证**开始。若完整环境仍不可用，则只允许补充不改变事实源或公开边界的领域单测与静态审计；不得提前实现 Port 持久化、默认角色写路径迁移、Primary 解耦或新的 Subscription IPC。
