# SIM 重构代码目录结构设计方案

**文档定位：** SIM 多卡管理重构的代码目录组织规范  
**适用范围：** `telephony_core_service` SIM 管理模块  
**设计基线：** 四个业务目录 + 一个基础支撑目录  
**核心原则：** 以业务职责组织代码，以业务对象及关系组织数据，以明确的职责和依赖边界保障架构。

---

## 1. 设计背景与目标

SIM 多卡管理重构不仅需要调整业务逻辑和数据结构，还需要重新组织代码目录，使目录结构能够体现目标架构的业务边界和职责划分。

目录结构是软件架构的重要组成部分。合理的目录组织应该帮助开发人员快速理解模块职责、定位代码和开展功能开发，同时通过清晰的边界控制模块之间的依赖关系。

本次目录设计需要考虑现有开发团队的实际情况：

1. 开发人员具备基本的软件开发能力，但软件架构设计能力存在差异。
2. 大部分开发人员对 DDD、Clean Architecture 等架构理论并不熟悉。
3. 日常开发主要围绕具体业务需求开展，而不是围绕架构层次组织工作。
4. 目录结构应当降低代码理解和维护成本，而不是增加开发人员的架构认知负担。
5. 通过目录职责、接口约束、数据所有权和自动化检查，将架构决策固化为代码边界。

### 1.1 设计目标

建立一套开发人员容易理解、业务职责清晰、数据关联明确、能够支撑多 SIM 长期演进的代码组织方式。

具体目标：

- 以业务职责作为一级目录的主要组织依据。
- 围绕业务对象和业务关系组织业务代码。
- 将跨业务复用的基础机制和系统边界能力统一组织。
- 明确各业务模块管理的数据及其唯一写入责任。
- 控制跨模块依赖，避免循环依赖和职责交叉。
- 通过 GN target、接口契约和自动化检查落实架构约束。

### 1.2 核心设计思想

> 四个业务目录围绕 SIM 管理的业务对象和业务关系组织；基础支撑目录围绕业务运行所需要的公共技术机制、系统装配和外部兼容能力组织。

业务模块负责自己的业务状态、业务规则和操作流程；基础支撑提供可复用的技术能力，不参与业务事实的管理和业务决策。

---

## 2. 一级目录组织原则

本次采用五组一级目录：

| 一级目录 | 组织依据 | 主要职责 |
|---|---|---|
| `account` | 业务对象 | 账户与订阅管理 |
| `card` | 业务对象 | 卡应用与卡侧操作 |
| `role_assignment` | 业务关系 | 主卡及默认业务角色分配 |
| `device` | 业务对象 | 设备与硬件资源管理 |
| `infrastructure` | 基础支撑职责 | 公共机制、系统装配和外部兼容 |

其中，前四个目录是核心业务模块，`infrastructure` 是支撑这些业务模块运行的基础能力集合。

### 2.1 业务职责优先

一级目录首先回答：

- 这个目录负责什么业务？
- 它管理哪些业务对象或业务关系？
- 什么样的需求应该在这里实现？

不直接采用 `domain`、`application`、`infrastructure` 等理论架构分层作为全部一级目录。

主要原因：

1. 业务是开发人员最熟悉的认知入口。
2. 开发人员接到需求时，首先想到的是修改账户、SIM 卡、主卡或设备功能。
3. 如果要求开发人员先判断代码属于 Domain、Application 还是 Infrastructure，会增加代码定位和理解成本。
4. 架构分层和依赖约束可以通过模块内部组织、接口设计及 GN 构建规则实现。

因此，目录首先服务于开发人员的业务认知，而不是要求开发人员理解复杂的架构理论。

### 2.2 以业务对象和业务关系为中心

业务目录不仅按照物理对象划分，也要考虑业务关系。

例如：

- Account：账户身份及订阅关系。
- Card：SIM 卡应用及运行实例。
- Role Assignment：账户或逻辑位置承担何种业务角色。
- Device：实际设备及其资源状态。

其中，主卡、默认数据卡、默认语音卡、默认短信卡等，本质上是业务角色与订阅对象之间的分配关系。

因此，使用 `role_assignment` 替代 `card_selection`，比将其理解为一个选卡流程更准确。

### 2.3 基础支撑与业务模块分离

`infrastructure` 统一组织业务模块共同依赖的基础技术能力及系统边界能力。

但并不是所有公共代码、Adapter 或可复用代码都应该放入该目录。

判断依据是：

- 是否属于跨业务使用的通用技术机制？
- 是否承担整个 SIM 模块与外部系统之间的连接和适配？
- 是否属于模块级的对象装配和生命周期管理？
- 是否拥有特定业务对象或资源的运行状态？

业务专属的 Adapter、业务状态和运行机制，仍应归属于对应业务模块。

---

## 3. 整体目录结构

建议采用如下目录结构：

```text
services/multisim/
│
├── account/                     # 账户与订阅管理
│
├── card/                        # 卡应用与卡侧操作
│
├── role_assignment/             # 业务角色与分配关系
│
├── device/                      # 设备与硬件资源
│
├── infrastructure/              # 基础支撑
│   │
│   ├── compatibility/           # 原有接口兼容
│   │   ├── api/
│   │   └── events/
│   │
│   ├── assembly/                # 模块创建与装配
│   │   └── exports/
│   │
│   ├── request/                 # 通用请求执行机制
│   │
│   └── sim_identity.h           # 公共身份及关联类型
│
├── test/                        # 测试与防护网
│
├── tools/                       # 开发和辅助工具
│
├── BUILD.gn
│
└── README.md
```

说明：

1. `role_assignment` 是原 `card_selection` 的目标调整方向。
2. `infrastructure` 是新增的一级组织目录，用于统一原 `integration` 和 `support` 中属于基础支撑职责的代码。
3. `compatibility`、`assembly`、`request` 是根据当前实际职责划分的二级目录。
4. `sim_identity.h` 暂时放在 `infrastructure` 根目录，不为单个头文件额外建立 `identity` 子目录。
5. 不要求为了目录结构完整而提前建立没有实际代码的空目录。
6. 当前 `CardRuntime`、`result_sinks` 等过渡性代码不能仅因其原来位于 `integration/lifecycle`，就直接归入 `assembly`，需要根据实际运行状态所有权进一步拆分。

---

## 4. 核心业务目录职责

### 4.1 Account：账户与订阅管理

**核心职责：管理系统识别和使用的通信订阅账户，以及账户与逻辑位置之间的关联关系。**

主要包括：

- 账户发现与身份识别。
- 账户资料管理。
- 账户激活状态。
- 账户与逻辑槽位的绑定关系。
- 账户历史信息。
- 账户查询与更新。

核心数据对象：

- `SubscriptionId`
- `AccountRecord`
- `AccountProjection`
- `BindingRef`
- `BindingIndex`
- `AccountRegistry`

需要明确：

Account 管理的是逻辑订阅身份，而不是 SIM 卡硬件或卡应用运行实例。

账户与卡的关系通过明确的绑定关系表达，而不是将账户身份直接等同于 `SlotId`。

账户的绑定事实由 Account 负责维护，其他模块通过接口和关联引用获取必要信息，不直接修改 Account 内部状态。

### 4.2 Card：SIM 卡应用与操作

**核心职责：管理 SIM 卡应用的运行状态及其卡侧操作能力。**

主要包括：

- 卡状态管理。
- 卡应用会话。
- 卡文件和 EF 操作。
- PIN、PUK 等安全操作。
- 逻辑通道管理。
- APDU 操作。
- eSIM Profile 卡侧操作。

核心数据对象：

- `CardApplicationTarget`
- `SessionRef`
- `SessionRegistry`
- `LoadSession`
- 卡状态及安全操作相关对象。

需要明确：

Card 主要关注卡应用及其运行实例，不负责账户身份的唯一管理，也不负责默认业务角色决策。

例如，拔出物理 SIM 后，卡运行会话需要失效，但账户历史信息不一定需要删除。

### 4.3 Role Assignment：业务角色与分配关系

**核心职责：管理 SIM 业务角色及其分配关系。**

主要包括：

- 主卡分配。
- 默认数据卡。
- 默认语音卡。
- 默认短信卡。
- 角色选择规则。
- 分配状态。
- 分配变更过程。
- 分配失败与恢复。

核心数据对象：

- `RoleSelection`
- `AssignmentAggregate`
- `AssignmentTransition`
- `PrimarySwitchPlan`

选择 `role_assignment` 而不是 `card_selection` 的原因：

`card_selection` 容易让开发人员理解为一个选卡流程，而实际业务是管理不同 SIM 账户或逻辑位置承担何种业务角色。

选卡只是改变角色分配关系的一种操作，不是该模块的全部职责。

需要注意：

- 默认数据、语音、短信等业务角色通常关联 `SubscriptionId`。
- Primary 涉及主卡位置及 Radio 主从分配。

两者具有不同的业务语义，不能强行统一成一种标识类型。

### 4.4 Device：设备与硬件资源管理

**核心职责：管理 SIM 管理所依赖的设备能力、硬件资源及其实际运行状态。**

主要包括：

- Radio 能力。
- Modem 资源。
- 设备状态观察。
- 设备资源分配。
- 设备操作执行。
- 设备资源冲突保护。
- 设备操作结果接纳。

核心数据对象：

- `SlotDescriptor`（静态描述由基础支撑侧提供）
- `RadioDomainId`
- `RadioAssignmentView`
- `ResourceBarrier`
- 设备资源与执行相关对象。

需要明确：

Device 负责实际设备状态和执行能力，不负责账户身份、默认业务角色或账户绑定关系。

---

## 5. Infrastructure：基础支撑目录职责

### 5.1 总体定位

`infrastructure` 不是一个新的业务模块，也不是所有公共代码的收容目录。

它负责提供四个业务模块共同依赖的基础技术能力，以及 SIM 模块与现有宿主系统之间的连接和适配。

主要包括：

1. 外部接口兼容机制。
2. 模块创建、依赖装配和生命周期管理。
3. 通用请求执行机制。
4. 跨业务身份及关联类型定义。

需要明确：

- 不拥有 Account、Card、Role Assignment、Device 的业务事实。
- 不实现具体业务规则。
- 不统一调度业务请求。
- 不集中管理各 Owner 的业务运行状态。
- 不将业务专属 Adapter 集中到基础支撑目录。

### 5.2 Compatibility：原有接口兼容

**核心职责：保持新架构与原有系统接口、调用方式和结果交付语义的兼容。**

建议目录：

```text
infrastructure/
└── compatibility/
    ├── api/
    │   ├── legacy_sim_facade.*
    │   ├── legacy_boundary_ports.h
    │   └── real_sim_count_query.*
    │
    └── events/
        ├── completion_cell.h
        ├── legacy_completion.*
        └── legacy_output_adapter.*
```

主要职责：

- 接收原有接口调用。
- 将旧接口调用适配到新的业务入口。
- 保留原接口参数和返回语义。
- 兼容原有同步等待和异步完成方式。
- 将新架构的执行结果转换为原有回调和通知形式。

需要遵守：

1. 兼容层负责接口适配，不重新实现业务逻辑。
2. 业务专属的兼容适配仍然归属对应业务模块。
3. 不允许将所有 Adapter 无差别地集中到 compatibility。
4. 兼容层不得绕过业务 Owner 修改业务事实。

例如：

- `LegacySimFacade`：原接口入口适配。
- `LegacyCompletion`：新旧完成语义适配。
- `LegacyOutputAdapter`：原有结果和通知交付适配。

### 5.3 Assembly：模块创建与装配

**核心职责：将各业务模块组织起来，接入现有 CoreService，并管理整个 SIM 模块的对象创建、依赖连接和生命周期。**

建议目录：

```text
infrastructure/
└── assembly/
    ├── host_ports.h
    ├── manager_publication.h
    ├── sim_module_factory.*
    ├── sim_module.*
    ├── slot_catalog.*
    │
    └── exports/
```

主要职责：

- 获取 CoreService 提供的 RIL 等外部资源。
- 定义宿主提供的接口契约。
- 创建 SIM 业务模块。
- 注入必要的接口、配置和依赖。
- 组合 Account、Card、Role Assignment、Device 等业务组件。
- 管理 Manager 发布。
- 管理模块初始化、停止和销毁。
- 构建并注入只读 SlotCatalog。

主要代码：

| 文件 | 职责 |
|---|---|
| `host_ports.h` | 宿主提供的接口及外部资源契约 |
| `sim_module_factory.*` | 创建和装配 SIM 模块 |
| `sim_module.*` | 业务对象组合及依赖连接 |
| `manager_publication.h` | Manager 发布和对象身份管理 |
| `slot_catalog.*` | 构建并提供只读槽位描述 |
| `exports/` | 对外导出和精确转发 |

需要明确：

Assembly 管理的是整个 SIM 模块的生命周期，而不是某个业务对象的生命周期。

特别是当前 `CardRuntime`、`result_sinks` 中仍然存在部分 R 运行责任，不能因为它们暂时位于生命周期目录，就认定它们属于 Assembly。

后续需要按照实际运行状态所有权，将卡应用会话、加载状态和设备资源管理等能力收敛到 Card、Device 或对应的业务边界。

### 5.4 Request：通用请求执行机制

**核心职责：为各业务模块提供统一的请求上下文、执行步骤、异步关联和完成状态管理能力。**

建议目录：

```text
infrastructure/
└── request/
    ├── request_context.h
    ├── request_ref.h
    └── runtime_operation_store.h
```

主要组成：

| 组成 | 职责 |
|---|---|
| `RequestRef` | 请求身份、步骤标识和效果知识 |
| `RequestContext` | 请求上下文及完成协议 |
| `RuntimeOperationStore` | 请求步骤及运行状态管理 |

主要能力：

- 创建和标识一次业务请求。
- 管理请求上下文。
- 标识和跟踪执行步骤。
- 关联异步操作及其回调。
- 记录请求执行状态。
- 管理请求最终完成状态。

#### Request 与业务模块的职责边界

Request 是通用技术机制，不是独立的业务模块，也不是统一的业务请求调度中心。

例如，`SetPrimarySlotId`：

- Role Assignment 决定主卡切换的业务规则和执行步骤。
- Account 提供必要的账户及绑定关系。
- Device 提供 Radio 操作和设备状态接纳。
- Request 提供这次操作的请求上下文、步骤跟踪和完成状态管理。

因此：

**业务模块决定一次操作做什么、如何做；Request 提供这次操作的执行上下文、步骤跟踪和完成状态管理机制。**

各业务 Owner 根据自身操作需要创建和管理请求实例。

Request 不负责：

- 定义主卡切换等业务流程。
- 决定业务规则。
- 统一调度所有业务请求。
- 决定业务失败后的重试策略。
- 维护账户、卡、角色和设备的业务状态。

另外，并不是所有外部接口都需要使用 Request。对于简单的只读查询，可以直接返回查询结果；对于涉及跨步骤、异步等待和完成状态管理的操作，才根据实际需要使用通用请求机制。

### 5.5 sim_identity.h：公共身份及关联类型

暂时将 `sim_identity.h` 放在 `infrastructure` 根目录，不单独建立 `identity` 子目录。

主要定义：

- `SubscriptionId`
- `SlotId`
- `RadioDomainId`
- `CardApplicationTarget`
- `SessionRef`
- `BindingRef`

其作用是提供跨业务统一的身份及关联类型。

需要明确：

类型定义集中，不代表数据写权集中。

例如：

- `BindingRef` 所表达的绑定事实由 Account 管理。
- `SessionRef` 所表达的卡应用运行实例由 Card 管理。
- `RadioDomainId` 所关联的资源事实由 Device 管理。

`sim_identity.h` 只提供类型定义，不建立 IdentityManager，不统一管理各种 ID 的映射、查找和转换。

---

## 6. 业务目录与 Infrastructure 的边界

判断代码归属时，最重要的依据不是代码是否可以被多个模块使用，而是它实际管理什么业务事实、承担什么职责。

### 6.1 业务专属 Adapter 不进入 Infrastructure

例如：

| 代码类型 | 归属 |
|---|---|
| AccountStoreAdapter | Account |
| ActivationHookAdapter | Account |
| AssignmentStoreAdapter | Role Assignment |
| CardSessionRegistry | Card |
| ResourceBarrier | Device |
| LegacySimFacade | Infrastructure / Compatibility |
| LegacyOutputAdapter | Infrastructure / Compatibility |

业务专属 Adapter 即使使用了公共技术机制，也仍然属于自己的业务模块。

### 6.2 可复用不等于必须放入 Infrastructure

以下能力虽然具有技术机制属性，但管理的是特定业务对象或资源状态，应继续归属对应 Owner：

| 能力 | 归属 | 原因 |
|---|---|---|
| SessionRegistry | Card | 管理卡应用会话及代次 |
| LoadSession | Card | 管理卡数据加载过程 |
| ResourceBarrier | Device | 管理设备资源租约和恢复状态 |
| Account 状态管理 | Account | 维护账户业务事实 |
| Assignment 状态管理 | Role Assignment | 维护业务角色分配事实 |

判断原则：

**只要它管理的是特定业务对象或特定资源的生命周期，就应优先归属于对应 Owner，而不是因为具有可复用的技术实现就放入 Infrastructure。**

---

## 7. 物理目录与逻辑架构边界的关系

需要明确：

**物理目录、逻辑模块、GN target、SO 和业务对象不是一一对应关系。**

物理目录主要解决代码组织和开发人员定位问题。

GN target 主要解决：

- 编译依赖控制。
- 模块接口暴露。
- 依赖方向约束。
- 不允许的跨模块依赖。
- 自动化构建和测试隔离。

因此，不要求一个物理目录必须对应一个 GN target，也不要求一个逻辑架构责任必须建立一个同名目录。

例如：

| 物理目录 | 可能涉及的逻辑 target |
|---|---|
| `infrastructure/compatibility` | `compatibility_contracts`、`compatibility_core` |
| `infrastructure/assembly` | `assembly_contracts`、`assembly_core` |
| `infrastructure/request` | `request_protocol`、`request_mechanism` |
| `infrastructure` 根目录 | `sim_values` |

目录调整后，应尽量保留已有逻辑 target 的职责划分。

不能因为将原来的 `integration` 和 `support` 合并到 `infrastructure`，就将原有 GN target 合并成一个大的公共 target。

### 7.1 架构依赖约束

需要遵守：

1. 业务模块不得直接访问其他业务模块的内部实现。
2. 跨业务访问通过明确的接口和业务对象关联完成。
3. Infrastructure 不得拥有业务事实写权。
4. 业务专属 Adapter 留在对应 Owner。
5. Request 不得成为全局业务调度中心。
6. Assembly 不得承载业务规则。
7. 禁止跨层反向依赖和循环依赖。
8. 公共身份类型不允许演变为统一状态管理服务。

---

## 8. 目录调整的实施策略

本次目录设计属于目标组织方案的收敛，不建议为了调整目录而立即进行大规模文件迁移。

建议分阶段实施。

### 阶段一：明确代码归属

1. 确认 `card_selection` 调整为 `role_assignment`。
2. 将现有 `integration` 和 `support` 的代码逐一分类。
3. 按照职责将真正属于基础支撑的代码纳入 `infrastructure`。
4. 识别当前 `CardRuntime`、`result_sinks` 等过渡性运行代码。
5. 明确各业务专属 Adapter 的归属。
6. 形成文件、目录、GN target 和 Owner 的映射表。

### 阶段二：调整目录及构建组织

1. 按照确认后的目录结构移动文件。
2. 调整 include 路径和 GN source 文件引用。
3. 保持原有 GN target 的职责边界。
4. 检查是否引入反向依赖和循环依赖。
5. 确保现有测试防护网继续通过。

### 阶段三：验证架构边界

选择典型业务进行验证：

- `SetPrimarySlotId`：验证 Role Assignment、Account、Device 和 Request 的协作边界。
- `SetActiveSim`：验证 Account 激活与 Card 运行状态的边界。
- eSIM Profile 操作：验证 Account、Card、Device 的职责边界。

针对每个业务回答：

1. 业务逻辑属于哪个目录？
2. 涉及哪些业务对象？
3. 哪些数据由哪个 Owner 维护？
4. 是否使用 Infrastructure 提供的公共机制？
5. 是否存在职责重复或不明确的情况？
6. 是否存在不合理的跨模块依赖？

### 阶段四：固化工程规范

将最终目录职责和依赖规则固化到：

- `README.md`
- `AGENTS.md`
- GN 构建依赖规则
- 架构检查脚本
- Code Review 检查项
- CI 自动化门禁

避免后续开发过程中重新出现职责混乱和公共目录膨胀。

---

## 9. 目录设计评审标准

后续任何一级或二级目录调整，都应按照以下标准评估。

| 评审维度 | 评审问题 | 预期结果 |
|---|---|---|
| 业务可理解性 | 普通开发人员是否能够理解目录用途？ | 不依赖复杂架构术语 |
| 业务内聚性 | 同类业务变化是否集中在对应目录？ | 变化原因相对一致 |
| 职责唯一性 | 是否存在多个目录维护相同业务事实？ | 唯一事实所有者 |
| 关联清晰性 | 对象之间的关系是否明确？ | 显式业务关联 |
| 依赖合理性 | 是否存在反向依赖或循环依赖？ | 依赖方向清晰 |
| 公共支撑边界 | Infrastructure 是否混入业务逻辑？ | 公共机制职责明确 |
| 扩展能力 | 新增卡类型、槽位或业务角色是否容易扩展？ | 避免大面积修改 |
| 工程可维护性 | 是否能够通过工具保护架构边界？ | 自动化检查有效 |
| 迁移成本 | 是否影响现有构建和测试？ | 风险可控 |

如果目录调整只是让架构形式更加符合某种理论，但没有降低理解成本、改善职责边界或提高扩展能力，就没有必要为了形式而调整。

---

## 10. 最终设计结论

本次 SIM 重构的代码目录组织，最终形成五组一级目录：

| 一级目录 | 核心职责 |
|---|---|
| `account` | 账户与订阅管理 |
| `card` | 卡应用与卡侧操作 |
| `role_assignment` | 主卡及默认业务角色分配 |
| `device` | 设备与硬件资源管理 |
| `infrastructure` | 公共技术机制、系统装配和外部兼容 |

其中：

**四个业务目录围绕业务对象和业务关系组织；Infrastructure 围绕业务运行所需要的基础支撑能力组织。**

需要固化以下设计共识：

1. **业务职责优先：** 一级业务目录围绕开发人员容易理解的业务概念组织，不以理论架构分层作为主要目录划分依据。
2. **对象及关系优先：** 围绕稳定的业务对象和业务关系组织代码，不以具体接口和函数调用流程作为主要目录划分依据。
3. **职责边界清晰：** Account、Card、Role Assignment、Device 分别管理不同的业务事实和变化原因。
4. **基础支撑集中：** Infrastructure 统一组织通用请求执行机制、系统装配和外部接口兼容能力。
5. **业务专属实现归属明确：** 业务专属 Adapter 和运行状态仍由对应业务 Owner 管理，不因技术属性而集中到 Infrastructure。
6. **数据关联显式化：** 通过稳定身份、类型化关联引用和明确的数据对象表达业务关系，减少 ID、Index 的散乱查找和转换。
7. **依赖关系受控：** 通过接口、数据所有权、GN 规则和自动化护栏保证模块之间的合理协作。
8. **物理目录与逻辑架构解耦：** 目录服务于代码理解，GN target 负责逻辑依赖和构建边界，不要求两者一一对应。
9. **渐进式调整：** 通过典型业务验证目录划分，避免在生产业务迁移前进行不必要的大规模目录变更。

### 核心观点

> **业务目录围绕业务对象及其关系组织，Infrastructure 围绕业务共同依赖的基础支撑能力组织。**
>
> **让开发人员按照业务找到代码，让数据围绕业务对象建立关联，让架构规则通过工程机制得到持续保障。**

最终目标不是构建一套形式复杂的目录结构，而是建立一套开发人员容易理解、能够长期遵循、能够支撑 SIM 多卡演进的代码组织体系。