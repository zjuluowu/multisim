---
marp: true
theme: default
paginate: true
size: 16:9
header: OpenHarmony Telephony Core Service
footer: 多 SIM 架构重构建议｜2026-08-28
style: |
  section {
    font-family: "Microsoft YaHei", "PingFang SC", Arial, sans-serif;
    font-size: 26px;
    color: #24292f;
  }
  h1 {
    color: #0969da;
    font-size: 42px;
  }
  h2 {
    color: #24292f;
    font-size: 32px;
  }
  table {
    font-size: 20px;
  }
  blockquote {
    border-left: 6px solid #0969da;
    background: #f6f8fa;
    padding: 10px 18px;
  }
  strong {
    color: #cf222e;
  }
  code {
    background: #f6f8fa;
  }
  .lead {
    text-align: center;
  }
  .lead h1 {
    font-size: 48px;
  }
  .small {
    font-size: 18px;
  }
  .medium {
    font-size: 22px;
  }
  .green {
    color: #1a7f37;
  }
  .red {
    color: #cf222e;
  }
  .blue {
    color: #0969da;
  }
  .gray {
    color: #57606a;
  }
---

<!-- _class: lead -->

# Telephony Core Service  
# 多 SIM 架构重构建议

## 从“面向双卡的可定制实现”  
## 演进为“面向多拓扑的通用订阅平台”

**目标模块：** `openharmony/telephony_core_service`

**汇报日期：** 2026 年 8 月 28 日

<!--
讲解重点：
本次汇报不是要否定现有实现，而是希望保护已有技术投资，
使系统能够继续支撑 pSIM、eSIM、MEP、多 Modem 和未来多订阅产品。
-->

---

# 1. 本次汇报希望达成什么

## 希望形成四项共识

1. 当前多卡扩展问题是**核心模型问题**，不是几个局部 Bug
2. 继续增加 Extension Hook 和特殊 slot 只能延迟问题
3. 重构必须渐进实施，保持现有 API 和产品行为兼容
4. 批准启动第一阶段架构治理

## 建议批准的第一阶段

- 统一 Slot Registry
- 建立 Slot Capability
- 建立 Topology Snapshot
- 引入稳定 Subscription ID
- 建立多拓扑测试基线

> 本次不请求批准“推倒重写”，只请求启动低风险、可验收的架构演进。

---

# 2. 执行摘要

## 当前优势

- SIM、UICC、PIN/PUK、账户、STK、eSIM 功能覆盖完整
- 能够支撑单卡、双卡和部分 pSIM/eSIM 场景
- 编译 Feature 和厂商 Hook 较丰富
- 已具备单元测试与 Fuzz 基础

## 核心问题

- `slotId` 同时承担位置、订阅和特殊业务语义
- 核心缓存隐含“一槽一有效账户”
- Radio Protocol 主要面向双卡
- eSIM API 已端口化，账户和状态模型未完整端口化
- Profile、Port、Subscription、Modem 缺少统一关系
- 跨模块操作缺少统一事务

> **建议：从 slot-centric 逐步演进为 subscription-centric。**

---

# 3. 为什么现在需要讨论重构

## 过去

```text
一个 Slot
  = 一张物理 SIM
  = 一个 ICCID
  = 一个 SIM 账户
  = 一个网络订阅
```

## 现在

```text
一个 eUICC
  ├── Port 0 ── Profile A ── Subscription A
  └── Port 1 ── Profile B ── Subscription B

多个 Subscription
  └── 竞争有限的 Modem 和 Radio Capability
```

## 产品形态正在增加

- 双 pSIM
- pSIM + eSIM
- eSIM MEP
- 双 pSIM + eSIM
- 多 eUICC
- DSDS / DSDA / TSTS
- 蜂窝 + 卫星
- VSim / 远程 Modem

---

# 4. 设备中的“卡”已不再等同于卡槽

```text
┌──────────────── Physical Slot ────────────────┐
│                                               │
│                 eUICC                         │
│          ┌───────────────┐                    │
│          │ Port 0        │── Profile A        │
│          │               │── Profile B        │
│          ├───────────────┤                    │
│          │ Port 1        │── Profile C        │
│          │               │── Profile D        │
│          └───────────────┘                    │
└───────────────────────────────────────────────┘

Profile A → Subscription A → Modem 0
Profile C → Subscription B → Modem 1
```

## 必须独立表达的对象

- Physical Slot
- eUICC
- Port
- Profile
- Subscription
- Modem
- Default Role

> 继续只使用 `slotId`，无法准确表达现代多订阅拓扑。

---

# 5. 当前架构如何支撑多卡

## 每个 slot 一组管理器

```text
simStateManager_[slotId]
simFileManager_[slotId]
simAccountManager_[slotId]
simSmsManager_[slotId]
iccDiallingNumbersManager_[slotId]
stkManager_[slotId]
```

## 全局协调对象

```text
MultiSimController
MultiSimMonitor
RadioProtocolController
SimRdbHelper
```

## 现有设计的历史价值

- 有效隔离双卡状态和文件缓存
- 单卡逻辑可按 slot 复用
- 支持默认语音、短信、数据卡
- 支持主卡和 Radio Protocol 切换
- 顺利完成了单卡向双卡的演进

> 当前问题不是设计错误，而是业务维度已超出原始模型的表达范围。

---

# 6. 问题一：动态容器掩盖了固定拓扑

## 表面上

```cpp
simStateManager_.resize(slotCount);
simFileManager_.resize(slotCount);
```

## 深层逻辑仍依赖固定数量

```text
SIM_SLOT_COUNT
SIM_SLOT_COUNT_MD
SIM_SLOT_COUNT_REAL
DUAL_SLOT_COUNT
MAX_SLOT_COUNT
ESIM_MAX_SLOT_COUNT
```

不同模块使用不同遍历边界：

```text
i < SIM_SLOT_COUNT_REAL
i < SIM_SLOT_COUNT_REAL + 1
i <= SIM_SLOT_COUNT_MD
i < maxCount_
```

固定数组示例：

```cpp
isSimSlotsMapping_ = { false, false, false, false };
```

> **容器可以扩容，不代表业务拓扑能够扩展。**

---

# 7. 固定拓扑带来的直接影响

| 问题 | 影响 |
|---|---|
| Slot 数量来源不唯一 | 模块间合法范围不一致 |
| 固定数组 | 新增 slot 容易越界或遗漏 |
| 不同循环边界 | 初始化、状态、账户数量不一致 |
| 编译常量控制 | 运行时无法准确发现设备能力 |
| 双卡测试为主 | 三卡、MEP 问题难以及时暴露 |

## 典型风险

```text
上层成功创建第 3 个 SIM Manager
              ↓
Radio Protocol 只初始化前 2 个 slot
              ↓
账户、状态和 Modem 资源视图不一致
```

---

# 8. 问题二：特殊 slot 成为隐式产品协议

## 当前特殊逻辑

- `SIM_SLOT_2` 在多个流程中被跳过
- `InitTelExtraModule()` 仅处理特定 slot
- eSIM 初始化排除特定 slot
- Card Ready 和 Modem Init 检查跳过特定 slot
- Radio Protocol 对特定 slot 特殊处理
- `SIM_SLOT_3` 可能触发卡交换

## 当前隐含语义

```text
slotId
  = 物理位置
  + eSIM 电路标识
  + VSim 标识
  + 特殊业务标识
  + Modem 映射入口
```

> 数字 ID 应只标识对象，不应隐式携带产品策略。

---

# 9. 特殊 slot 模式为什么难以持续

## 当前扩展方式

```cpp
if (slotId == SIM_SLOT_2) {
    // 特殊业务
}

if (slotId == SIM_SLOT_3) {
    // 特殊映射
}
```

## 每增加一种产品形态，需要增加

- 新 slot 常量
- 新系统参数
- 新条件分支
- 新 Extension Hook
- 新兼容逻辑
- 新组合测试

## 长期结果

```text
产品数量增加
    ↓
特殊条件增加
    ↓
核心代码被产品拓扑渗透
    ↓
公共逻辑越来越难验证
```

---

# 10. 问题三：eSIM API 端口化，核心账户未端口化

## eSIM API 已经支持 `portIndex`

- `GetProfile`
- `DisableProfile`
- `SwitchToProfile`
- `GetEuiccInfo2`
- `LoadBoundProfilePackage`
- `GetRulesAuthTable`

## 核心账户仍主要使用

```text
slotIndex
isEsim
simLabelIndex
iccId
```

## 有效账户缓存仍隐含

```cpp
localCacheInfo_[slotId]
```

> API 已进入 MEP 模型，核心账户仍是一槽一账户模型。

---

# 11. MEP 场景会暴露哪些语义冲突

## 场景

```text
slot 1 / port 0 / profile A / subscription A
slot 1 / port 1 / profile B / subscription B
```

## 当前接口无法明确回答

- `GetSimId(1)` 返回 A 还是 B？
- `IsSimActive(1)` 表示 slot、Port 还是 Subscription？
- 默认语音卡为 slot 1，具体是哪一个 Profile？
- `localCacheInfo_[1]` 保存哪一个账户？
- slot 1 的状态如何表达 Port 0 和 Port 1 的差异？
- 主卡切换针对 slot、Profile 还是 Subscription？

> 当一槽可以拥有多个活动订阅时，slot 不再是有效业务主键。

---

# 12. 问题四：Radio Protocol 仍以双卡为核心

## 当前资源模型

```text
Slot 0 ─┐
        ├── 主/副 Modem
Slot 1 ─┘
```

Radio Protocol 初始化规模主要受：

```text
min(SIM_SLOT_COUNT_REAL, DUAL_SLOT_COUNT)
```

约束。

## 主要接口

```cpp
SetRadioProtocol(slotId);
GetRadioProtocolModemId(slotId);
SetPrimarySlotId(slotId);
```

## 难以自然支持

- 3 个订阅竞争 2 个 Modem
- MEP 两个 Port绑定不同 Modem
- 语音和数据使用不同订阅
- 蜂窝与卫星 Modem并存
- 远程或分布式 Modem
- 动态 Radio Capability 分配

---

# 13. 为什么 Extension Hook 不能解决根本问题

## Hook 擅长解决

- 产品特定策略
- 运营商规则
- 特殊硬件命令
- VSim 适配
- 语音信箱差异
- 临时兼容行为

## Hook 无法补足

| 缺失能力 | 根本原因 |
|---|---|
| 一槽多订阅 | 核心缓存只保存一个 slot 项 |
| Port/Profile 状态 | 核心状态粒度只有 slot |
| N 订阅/M Modem | 缺少资源仲裁模型 |
| 默认角色指向 Profile | API 和数据库主要绑定 slot |
| 原子 Profile 切换 | 缺少跨模块事务 |
| 动态拓扑发现 | 依赖常量和系统参数 |
| 事件有序性 | 缺少 topology revision |

---

# 14. 继续增加 Hook 的结果

```text
更多产品需求
      ↓
更多 Extension Hook
      ↓
更多隐式接口契约
      ↓
更多核心状态旁路修改
      ↓
更多组合测试和一致性风险
```

## Hook 的正确定位

> Hook 应当用于实现策略差异，  
> 而不是代替缺失的领域对象和基础架构。

## 必须由核心模型解决的问题

- 谁是 Subscription？
- Subscription 位于哪个 Port？
- Subscription 使用哪个 Modem？
- 默认业务角色属于谁？
- 切换失败后如何回滚？
- 哪一个事件代表最新拓扑？

---

# 15. 根因：四个基础模型不足

## 1. 数据模型

当前：

```text
slot + ICCID + isEsim
```

目标：

```text
Slot + eUICC + Port + Profile + Subscription + Modem
```

## 2. 拓扑模型

当前：

```text
常量 + 参数 + 特殊 slot
```

目标：

```text
运行时发现 + 能力描述 + 版本化拓扑
```

## 3. 资源模型

当前：

```text
主卡 + 主 Modem
```

目标：

```text
N Subscription ↔ M Modem
```

## 4. 事务模型

当前：

```text
分步更新 + 局部重试
```

目标：

```text
跨 Profile、Modem、数据库、角色和事件的统一事务
```

---

# 16. 如果不重构，会发生什么

## 工程成本

- 每种新拓扑需要修改多个模块
- 产品差异持续进入公共核心
- 特殊参数和分支持续增加
- 回归测试组合非线性增长
- 新成员难以理解隐式约定

## 质量风险

- eUICC 已切换但账户仍是旧 Profile
- 默认卡指向旧订阅
- 迟到事件覆盖新状态
- Slot Mapping 期间误判拔卡
- Modem 成功但数据库失败
- 多 Port 状态相互覆盖

## 产品风险

- MEP 支持受限
- 三卡以上适配成本过高
- 多 eUICC 难以引入
- DSDA/TSTS 改造范围不可控

---

# 17. 重构目标与非目标

## 重构目标

1. 支持任意拓扑描述
2. 分离 Slot、eUICC、Port、Profile、Subscription、Modem
3. 默认角色绑定 Subscription
4. 支持一槽多订阅
5. 支持 N 订阅/M Modem
6. Profile 切换具有事务一致性
7. 保持现有 API 兼容
8. 保留产品策略扩展能力

## 本次非目标

- 不重写 SIM 文件解析
- 不重写 PIN/PUK
- 不立即替换全部 RIL 接口
- 不一次迁移全部私有 Extension
- 不要求第一阶段直接交付所有未来拓扑
- 不改变现有单卡、双卡产品行为

---

# 18. 目标数据模型

```text
PhysicalSlot
    │
    ├── Physical SIM ── Subscription
    │
    └── eUICC
          ├── Port 0
          │     └── Profile A ── Subscription A
          └── Port 1
                └── Profile B ── Subscription B

Subscription A ── Modem 0
Subscription B ── Modem 1
```

## 标识职责分离

| 标识 | 负责标识 |
|---|---|
| `PhysicalSlotId` | 硬件位置 |
| `EuiccId` | eUICC |
| `PortId` | MEP Port |
| `ProfileId` | eSIM Profile |
| `SubscriptionId` | 业务订阅 |
| `ModemId` | Radio 资源 |
| ICCID | SIM 应用或 Profile |

> 不再让 `slotId` 一人分饰多角。

---

# 19. Subscription 成为业务主键

## 当前

```cpp
GetSimAccountInfo(slotId);
GetSimId(slotId);
SetDefaultVoiceSlotId(slotId);
IsSimActive(slotId);
```

## 目标

```cpp
GetSubscriptionInfo(subscriptionId);
GetSubscriptionsBySlot(slotId);
GetSubscriptionByPort(euiccId, portId);

SetDefaultVoiceSubscription(subscriptionId);
SetDefaultSmsSubscription(subscriptionId);
SetDefaultDataSubscription(subscriptionId);

ActivateSubscription(subscriptionId);
DeactivateSubscription(subscriptionId);
```

## 兼容原则

旧 slot API 继续保留：

```text
旧 slot API
    ↓
Compatibility Adapter
    ↓
解析当前主 Subscription
    ↓
调用新核心
```

---

# 20. 目标拓扑与能力模型

## 多卡能力不再是一个布尔值

```cpp
struct MultiSimCapabilities {
    int32_t maxPhysicalSlots;
    int32_t maxEuiccCount;
    int32_t maxPortsPerEuicc;
    int32_t maxInstalledProfiles;
    int32_t maxEnabledProfiles;
    int32_t maxActiveSubscriptions;
    int32_t modemCount;
    MultiSimMode mode;
    bool supportsMep;
    bool supportsHotProfileSwitch;
    bool supportsPrimarySwitchWithoutReboot;
    bool supportsRemoteSubscription;
};
```

## 从特殊判断到能力判断

| 当前 | 目标 |
|---|---|
| `slotId != SIM_SLOT_2` | `slot.supportsEsimProfile` |
| `slotId == SIM_SLOT_3` | `slot.kind == REMOTE_MODEM` |
| `i < DUAL_SLOT_COUNT` | 遍历 topology.modems |
| `PSIM1_ESIM` | 查询实体绑定关系 |

---

# 21. 目标资源模型

```text
Subscription A ─┐
Subscription B ─┼── Radio Resource Arbiter ── Modem 0
Subscription C ─┘                         └── Modem 1
                                          └── Satellite Modem
```

## 仲裁输入

- 默认数据订阅
- 当前语音呼叫
- Radio Capability
- DSDS/DSDA/TSTS 模式
- Profile 和 Port 状态
- 功耗策略
- 卫星优先级
- 运营商约束

## 仲裁输出

- Subscription → Modem 绑定
- 高能力 Radio 分配
- 可同时注册的订阅集合
- 需要降级或停用的订阅
- 是否需要 Modem 重启

> “主卡切换”逐步降级为兼容概念。

---

# 22. 目标事务模型

## Profile 切换事务

```text
CREATED
   ↓
PRECHECKED
   ↓
OLD_PROFILE_DISABLED
   ↓
NEW_PROFILE_ENABLED
   ↓
TOPOLOGY_REFRESHED
   ↓
MODEM_ASSIGNED
   ↓
ACCOUNT_COMMITTED
   ↓
ROLE_RECONCILED
   ↓
EVENTS_PUBLISHED
   ↓
COMPLETED
```

失败进入：

```text
ROLLBACK_PENDING
    ├── ROLLBACK_COMPLETED
    └── MANUAL_RECOVERY_REQUIRED
```

---

# 23. 事务模型带来的价值

每个操作拥有：

- Operation ID
- Topology Revision
- Subscription/Profile/Port 上下文
- 当前执行步骤
- 超时时间
- 补偿操作
- 持久化恢复状态
- 最终事件

## 可以解决

- eUICC 与数据库不一致
- 进程重启后无法恢复
- 迟到 RIL 响应覆盖新状态
- 主卡切换失败定位困难
- Profile 成功、默认角色失败
- 多模块各自重试导致状态震荡

> 从“最终希望一致”演进为“可跟踪、可恢复的一致性”。

---

# 24. 目标核心组件

```text
                       Subscription API
                              │
                  SubscriptionTopologyManager
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
 SubscriptionRegistry   PolicyEngine   TransactionManager
                                              │
                                  RadioResourceArbiter
                                              │
              ┌───────────────┬───────────────┼──────────────┐
              │               │               │              │
       PhysicalSimAdapter  EuiccAdapter   ModemAdapter   Repository
                                              │
                                         Event Bus
```

## 组件职责

- Topology Manager：发现并维护拓扑
- Policy Engine：默认角色和资源策略
- Transaction Manager：跨模块事务
- Resource Arbiter：N 订阅/M Modem
- Repository：隐藏数据库和缓存
- Event Bus：统一版本化事件

---

# 25. 渐进式迁移，不做大爆炸替换

```text
旧 API ───────────┐
                  ├── Compatibility Layer ── 新核心
新 API ───────────┘

旧数据模型 ←── 双写与比对 ──→ 新数据模型
```

## 迁移原则

- 现有 API 保持不变
- 新旧模型双写
- 新模型先运行在 Shadow Mode
- 比对新旧计算结果
- 默认仍走旧执行路径
- 按产品和场景灰度切换
- Feature Flag 可快速回退

> 每个阶段都必须能够独立交付、独立验收、独立回退。

---

# 26. 实施阶段总览

| 阶段 | 核心目标 | 是否改变现有行为 |
|---|---|---|
| 阶段 0 | 建立基线和 ADR | 否 |
| 阶段 1 | 统一 Slot、能力和拓扑 | 否 |
| 阶段 2 | 引入 Subscription 模型 | 局部、兼容 |
| 阶段 3 | 事务化和资源仲裁 | 按场景灰度 |
| 阶段 4 | 旧逻辑退场 | 是，完成迁移后 |

## 实施策略

```text
先抽象
  ↓
再双写
  ↓
再比对
  ↓
再切流
  ↓
最后删除旧逻辑
```

---

# 27. 阶段 0：建立事实基线

## 交付物

1. 当前多卡拓扑与代码依赖图
2. Slot、Profile、Modem、默认卡状态源清单
3. Extension Hook 清单
4. 系统参数清单
5. 现有故障模式与历史问题清单
6. 目标模型 ADR
7. 第一阶段详细工作量评估

## 建议周期

**2～3 周技术预研**

## 完成标准

- 开发、eSIM、RIL、Data、测试形成共同术语
- 目标模型通过架构评审
- 明确兼容边界和首个验证场景

---

# 28. 阶段 1：先治理最危险的结构

## 1. 统一 Slot Registry

- 所有 slot 来自统一注册表
- 固定数组改为动态容器
- 统一 slot 遍历边界
- 建立 slot 生命周期

## 2. 引入 Slot Capability

替换：

```cpp
slotId == SIM_SLOT_2
slotId == SIM_SLOT_3
```

## 3. 建立 Topology Snapshot

```cpp
SubscriptionTopology GetCurrentTopology();
uint64_t GetTopologyRevision();
```

## 4. 引入稳定 Subscription ID

先写入数据库和事件，不立即修改全部 API。

---

# 29. 阶段 1 的治理规则

## 暂停新增

- 新的特殊 slot 业务判断
- 新的固定长度 slot 数组
- 新的 pSIM/eSIM 组合枚举
- 没有能力声明的新 Hook
- 新代码直接访问 `localCacheInfo_[slotId]`
- 无 Operation ID 的跨模块异步流程

## 验收标准

- 单卡、双卡行为不变
- 所有 slot 遍历来自同一来源
- 新代码不引入特殊 slot
- 拓扑可通过 Dump 输出
- 当前自动化测试全部通过
- Shadow Topology 与旧模型结果一致

---

# 30. 阶段 2：完成 Subscription 化

## 核心工作

- 新增 Subscription Repository
- 建立 Slot、Port、Profile、Subscription 映射
- 一槽一账户缓存改为多索引
- 默认语音、短信、数据角色绑定 Subscription
- eSIM 状态细化为 Port/Profile
- 事件携带 Subscription ID、Port ID 和 Revision
- 旧 slot API 通过兼容层调用新核心

## 目标索引

```cpp
subscriptionsById
subscriptionsBySlot
activeSubscriptionByPort
subscriptionsByModem
```

## 阶段价值

即使暂不增加新硬件，也能显著降低当前 pSIM/eSIM 状态混乱风险。

---

# 31. 阶段 3：事务和资源仲裁

## 核心工作

- 引入 Subscription Transaction Manager
- Profile 切换事务化
- SIM 激活、停用事务化
- 默认角色变更事务化
- 主卡切换转换为资源分配请求
- 引入 Radio Resource Arbiter
- 支持 N Subscription / M Modem
- 操作按资源粒度互斥
- 失败恢复状态持久化

## 完成标志

```text
新增一个 Subscription
不再要求新增特殊 slot
不再要求修改主卡核心状态机
```

---

# 32. 阶段 4：旧逻辑退场

## 可以逐步删除

- pSIM/eSIM 组合系统参数
- 特殊 slot 判断
- 固定长度 slot 数组
- 一槽一账户假设
- 重复的默认卡状态
- 部分产品型 Extension Hook
- 旧主卡推导逻辑

## “主卡”最终定位

```text
Primary Slot
    ↓
兼容 API 和显示概念
    ↓
由 Subscription Role + Modem Assignment 派生
```

> 不再把“主卡”作为核心事实来源。

---

# 33. 建议的首个验证场景

## 推荐场景：pSIM + eSIM MEP

```text
Physical Slot 0
    └── pSIM A → Subscription A

Physical Slot 1
    └── eUICC
          ├── Port 0 → Profile B → Subscription B
          └── Port 1 → Profile C → Subscription C

Modem 数量：1 或 2
```

## 为什么选择该场景

它可以同时验证：

- 一槽多订阅
- Profile/Port 建模
- Subscription 默认角色
- Radio 资源竞争
- Profile 切换事务
- 事件版本
- 数据库迁移
- 旧 API 兼容

---

# 34. 建议团队组成

| 角色 | 主要职责 |
|---|---|
| 架构负责人 | 目标模型、ADR、接口边界 |
| SIM Core 开发 | Slot、账户、状态迁移 |
| eSIM 开发 | Port、Profile 生命周期 |
| RIL 开发 | 能力发现、Modem 分配 |
| Telephony Data | 新表、事务和数据迁移 |
| Framework 开发 | 新旧 API 兼容 |
| 测试负责人 | 拓扑矩阵和故障注入 |
| 安全负责人 | ICCID、EID、Profile 数据治理 |

## 组织原则

- 不是 SIM Core 单团队内部重构
- 必须由 SIM、eSIM、RIL、Data 共同设计
- 每个阶段由跨模块验收

---

# 35. 主要风险与缓解措施

| 风险 | 缓解措施 |
|---|---|
| 影响现有双卡产品 | 默认旧路径、Shadow Mode |
| 数据迁移失败 | 保留旧表、幂等迁移、可回滚 |
| 私有 Extension 不兼容 | Compatibility Adapter |
| RIL 能力不足 | Capability 降级模式 |
| 重构周期过长 | 独立里程碑、阶段性交付 |
| 新旧状态不一致 | 双写、比对、指标监控 |
| 新模型过度设计 | 首个场景驱动，避免一次覆盖全部未来 |
| 测试成本过高 | 拓扑参数化和自动生成 |

---

# 36. 预期收益

## 工程收益

- 新拓扑从跨模块修改变为能力配置和策略实现
- 减少特殊分支与系统参数
- 降低 `SimManager`、`MultiSimController` 复杂度
- 缩小新产品回归范围

## 质量收益

- Profile、账户、Modem 状态可事务恢复
- 迟到事件可以识别和丢弃
- 故障可以定位到事务步骤
- 多 Port 状态不再相互覆盖

## 产品收益

- 更容易支持 MEP、多 eUICC、DSDA、TSTS
- 卫星、VSim、远程 Modem 更容易接入
- 新硬件拓扑交付周期更可预测

---

# 37. 如何衡量重构是否成功

## 结构指标

- 特殊 `slotId == X` 分支数量
- `SIM_SLOT_COUNT*` 直接引用数量
- 固定 slot 数组数量
- Extension Hook 数量
- 直接访问 slot 下标缓存的位置数量

## 质量指标

- 多卡状态不一致故障数
- Profile/主卡切换失败率
- 数据库与 Modem 不一致次数
- 迟到事件丢弃次数
- 自动恢复成功率

## 交付指标

- 新增拓扑需要修改的模块数
- 新产品多卡适配周期
- 私有补丁数量
- 新增特殊参数数量
- 多拓扑自动化覆盖率

---

# 38. 需要团队做出的决定

## 决策一

确认问题边界：

> 厂商 Hook 丰富，但多订阅拓扑扩展不足。

## 决策二

确认目标方向：

> 从 slot-centric 演进为 subscription-centric。

## 决策三

批准阶段 0 和阶段 1：

- Slot Registry
- Slot Capability
- Topology Snapshot
- Subscription ID
- 架构治理规则
- 拓扑测试基线

## 决策四

成立跨模块工作组。

## 决策五

选择 `pSIM + eSIM MEP` 作为验证场景。

---

<!-- _class: lead -->

# 39. 最终建议

> 现在启动重构，是为了把未来多卡需求  
> 变成**能力配置和策略问题**。

> 如果继续沿用特殊 slot、组合状态和 Hook 扩展，  
> 每一个新产品都将成为一次新的系统级改造。

## 建议立即行动

1. 启动 2～3 周技术预研
2. 输出正式 ADR 和目标数据模型
3. 冻结新增特殊 slot 分支
4. 建立拓扑 Dump 和测试基线
5. 完成第一阶段工作量评估

---

<!-- _class: lead -->

# 附录

## 代码事实、概念定义、接口草案、数据迁移与测试矩阵

---

# A1. 关键代码事实

| 代码事实 | 架构含义 |
|---|---|
| 多个 `SIM_SLOT_COUNT*` | Slot 数量缺少唯一来源 |
| `DUAL_SLOT_COUNT` 约束 | Radio 模型偏向双卡 |
| `SIM_SLOT_2/3` 特殊判断 | Slot ID 承担产品语义 |
| `localCacheInfo_[slotId]` | 一槽一账户假设 |
| eSIM API 带 `portIndex` | API 已进入 MEP 模型 |
| 账户模型无完整 Port 关系 | 核心账户未完整 MEP 化 |
| `SetPrimarySlotId(slotId)` | Radio 与 Slot 耦合 |
| Slot 级 eSIM 状态 | 无法表达同槽多 Port |
| Extension Hook 持续增加 | 产品差异渗入核心路径 |

---

# A2. 重点代码位置

```text
services/sim/src/sim_manager.cpp
services/sim/src/multi_sim_controller.cpp
services/sim/src/multi_sim_monitor.cpp
services/sim/src/radio_protocol_controller.cpp
services/sim/src/esim_manager.cpp
services/sim/src/esim_controller.cpp
services/sim/include/sim_rdb_info.h
common/capability_mgr/src/multi_sims_capability_manager.cpp
```

## 重点检查内容

- Slot 数量和遍历边界
- 特殊 Slot 判断
- 一槽一账户缓存
- eSIM Port 处理
- 主卡和 Modem 绑定
- Extension Hook 介入状态
- 事件和数据库更新顺序

---

# A3. 概念定义

| 概念 | 定义 |
|---|---|
| Physical Slot | 物理卡座或 eUICC 所在位置 |
| eUICC | 可以存储多个 Profile 的安全单元 |
| Port | MEP 中可独立启用 Profile 的逻辑端口 |
| Profile | 下载到 eUICC 的运营商配置 |
| Subscription | 对系统业务可用的通信订阅 |
| Modem | 提供 Radio 能力的本地或远程资源 |
| SIM ID | 现有账户 ID，需要明确与 Subscription ID 的关系 |
| Primary Slot | 兼容概念，不应作为未来核心实体 |

---

# A4. 当前模型与目标模型对照

| 当前模型 | 目标模型 |
|---|---|
| `slotId` | `PhysicalSlotId` |
| `isEsim` | `SubscriptionType` |
| `simLabelIndex` | Profile/显示属性 |
| `localCacheInfo_[slotId]` | Subscription Repository |
| `primarySlotId` | Role + Modem Assignment |
| `PSIM1_ESIM` | 动态拓扑关系 |
| Slot 级状态 | Slot/Port/Profile/Subscription 分层状态 |
| Extension 函数指针 | 版本化领域扩展接口 |

---

# A5. 目标接口草案

```cpp
struct SubscriptionTarget {
    int32_t subscriptionId;
    std::optional<int32_t> physicalSlotId;
    std::optional<int32_t> euiccId;
    std::optional<int32_t> portId;
    std::optional<std::string> profileId;
};

struct SubscriptionTopology {
    uint64_t revision;
    std::vector<PhysicalSlotInfo> slots;
    std::vector<EuiccInfo> euiccs;
    std::vector<SubscriptionInfo> subscriptions;
    std::vector<ModemAssignment> modemAssignments;
};
```

---

# A6. 目标 API 草案

```cpp
int32_t GetSubscriptionTopology(
    SubscriptionTopology &topology);

int32_t GetSubscriptionsBySlot(
    int32_t slotId,
    std::vector<SubscriptionInfo> &subscriptions);

int32_t SetDefaultVoiceSubscription(
    int32_t subscriptionId);

int32_t SetDefaultSmsSubscription(
    int32_t subscriptionId);

int32_t SetDefaultDataSubscription(
    int32_t subscriptionId);

int32_t ActivateSubscription(
    int32_t subscriptionId);
```

---

# A7. 建议数据库结构

| 表 | 用途 |
|---|---|
| `physical_slot` | 物理位置和能力 |
| `euicc` | eUICC 和 EID |
| `euicc_port` | Port 与当前 Profile |
| `profile` | eSIM Profile 生命周期 |
| `subscription` | 业务订阅 |
| `subscription_role` | Voice/SMS/Data 等角色 |
| `modem_assignment` | Subscription 与 Modem 绑定 |
| `subscription_transaction` | 事务恢复信息 |

## 兼容方案

```text
新表作为事实来源
       ↓
生成 legacy sim_info 兼容视图
       ↓
旧 API 继续按 slot 查询
```

---

# A8. 拓扑测试矩阵

| 场景 | pSIM | eUICC | Port | 活动订阅 | Modem |
|---|---:|---:|---:|---:|---:|
| 单 pSIM | 1 | 0 | 0 | 1 | 1 |
| 双 pSIM DSDS | 2 | 0 | 0 | 2 | 1 |
| 双 pSIM DSDA | 2 | 0 | 0 | 2 | 2 |
| 单 eSIM | 0 | 1 | 1 | 1 | 1 |
| pSIM + eSIM | 1 | 1 | 1 | 2 | 1 |
| pSIM + eSIM DSDA | 1 | 1 | 1 | 2 | 2 |
| eSIM MEP | 0 | 1 | 2 | 2 | 1/2 |
| 双 pSIM + eSIM | 2 | 1 | 1 | 2/3 | 2 |
| 双 eUICC | 0 | 2 | 2+ | 2+ | 2 |
| 蜂窝 + 卫星 | 1 | 可选 | 可选 | 2 | 2 |

---

# A9. 每种拓扑的验证内容

## 基础流程

- 初始化
- 插拔卡
- Profile 下载、启用、停用、切换
- 默认语音、短信、数据角色
- Modem 和 Radio Capability 分配

## 异常流程

- Modem 故障
- DataShare 故障
- Telephony 进程重启
- eSIM Service 重启
- 并发 Profile 操作
- 主卡切换超时
- 事务回滚
- 迟到事件

## 安全

- 权限
- ICCID/EID 脱敏
- Profile 和 APDU 日志治理

---

# A10. 架构治理 Checklist

## 禁止新增

- 特殊 slot 业务判断
- 固定长度 slot 数组
- pSIM/eSIM 组合状态枚举
- 未声明线程模型的 Extension Hook
- 以 slot 作为 Profile 唯一标识
- 新代码直接访问 `localCacheInfo_[slotId]`
- 无 Operation ID 的跨模块流程

## 新增能力必须提供

- Capability 描述
- Subscription/Port 数据模型
- 状态转换
- 失败补偿
- 拓扑测试
- 兼容策略
- Dump 和 HiSysEvent

---

# A11. 推荐的扩展接口重构

## 当前

```text
TELEPHONY_EXT_WRAPPER
    └── 大量可空函数指针
```

## 建议

```text
ISimSecurityExtension
IMultiSimPolicyExtension
IRadioAssignmentExtension
IOperatorConfigExtension
IStkExtension
IEsimExtension
IVsimExtension
```

每个接口必须定义：

- 接口版本
- 能力查询
- 生命周期
- 线程约束
- 超时要求
- 错误码
- 是否允许修改核心状态

> Extension 负责策略差异，核心负责事实和一致性。

---

# A12. 重构后的目标判断标准

## 当新增一种 SIM 拓扑时

### 不应再需要

- 新增特殊 slot 常量
- 修改所有 slot 遍历
- 新增 pSIM/eSIM 组合值
- 修改默认卡数据库结构
- 修改主卡核心状态机

### 应当只需要

1. 提供设备 Capability
2. 发现新的拓扑实体
3. 配置或实现资源策略
4. 增加对应拓扑测试
5. 必要时提供版本化产品扩展

> 这才是“通用多订阅平台”的可扩展性。
