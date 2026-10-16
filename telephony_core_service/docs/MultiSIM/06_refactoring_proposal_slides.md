---
marp: true
paginate: true
title: SIM架构重构：统一设计与实施门禁
---

# SIM架构重构
## 当前证据、统一目标与实施门禁

2026-10-15 · 文档设计评审，非实施完成报告

规范入口：[docs/README](../README.md) · [冲突裁决](08_design_unification_decisions.md)

---

# 1. 本次统一的结论

- 采用Version3四Context、三聚合、三类目标、一个逻辑ACL。
- 保留既有入口、部署、schema、厂商ABI和完成语义。
- 先完成Linux公开组件边界最小状态/账户切片，再拆SO及迁移writer。
- 十类内部异步事件不替代现有六类通知/回执。
- 四槽、NULL、默认策略和恢复规则的来源/能力门禁未齐，不作为已获实施批准。

---

# 2. 当前代码证明了什么

| 事实 | 依据 |
|---|---|
| SIM仍与Core编在同一目标 | BUILD.gn |
| 4010和66250配置共宿主 | telephony_trust.json |
| 消费者还有直接C++门面 | CoreManagerInner |
| 账户操作通过DataShare | SimRdbHelper:94–136 |
| Primary会联动默认Data | MultiSimController:1522、1689 |

静态事实不证明故障率、运行部署或新方案已实现。

---

# 3. 问题不是增加一个slot

位置、账户、卡应用、默认角色与资源有不同语义。

- 空槽和PIN/PUK可能没有simId。
- 同状态换身份与同卡重载是不同生命周期变化。
- 管理数量、启用Profile、逻辑栈、驻网和业务并发分别验收。
- 四槽配置不能自动开放给所有应用，也不证明Modem支持。

---

# 4. 四Context与三聚合

| Context | 聚合 / 写者 |
|---|---|
| Subscription Topology | TopologyAggregate / TopologyManager |
| Profile Operation | ProfileOperationAggregate / OperationCoordinator |
| Radio Assignment | RadioAssignmentAggregate / RadioResourceArbiter |
| Compatibility | 无第二份业务真值 |

账户与SlotFacts独立；角色意图/有效选择/资源执行分开。

---

# 5. 三类目标避免错误统一

| 目标 | 用途 |
|---|---|
| SlotTarget | 无卡、在位、配置、初始化 |
| CardApplicationTarget | PIN/PUK、EF、APDU、STK |
| SubscriptionTarget | 账户、标签、默认角色 |

simId沿用为不透明账户标识，不新增公共SubscriptionId或伪造身份。

---

# 6. 保持外部兼容，允许内部解耦

- CoreManagerInner消费侧签名、符号、注入、错误和线程行为保持。
- Core实现非SIM Host Ports并注入，消除SIM→Core反向符号依赖。
- 保留卡状态、文件和安全等实际生产逻辑。
- ISimManager/IEsimManager无虚析构，使用工厂/具体类型deleter。
- 同进程SO提供依赖边界，不提供崩溃隔离。

---

# 7. 状态有效性分别保护

| 关联 | 保护对象 |
|---|---|
| BindingGeneration | 卡/应用或绑定会话 |
| RecordsGeneration + LoadSession | 同卡的一轮加载与请求集合 |
| OperationId + step + attempt | 本仓命令进度 |
| TopologyRevision / inputVersions | 已接受事实及决策输入 |

serial保留传输关联；合法切换用因果证据衔接新会话。

---

# 8. 完成、事件和事务不混用

- 旧返回/查询/Registry/CES/RadioEvent/Hub/账户/eSIM回执逐用例保留。
- 内部十类候选事件提交后异步发布，不强求无请求通知有OperationId。
- 不统一去重，不凭内部计数规定外部事件只能一次。
- 当前已核实DataShare，未证明本仓直写RDB或跨系统事务。
- 不建Outbox/操作日志表，不承诺跨重启恢复同OperationId。

---

# 9. 三项条件目标仍需证据

| 目标 | 准入 |
|---|---|
| 四槽/N布局 | D-002/D-005来源、产品/权限/协议能力 |
| ICCID写NULL | D-006来源、既有列与读者兼容、迁移与回退 |
| 超时/恢复/默认策略 | 分命令规则、Modem与业务方确认 |

内部不可用身份可以明确表达；缺证据时不迁数据、不猜字段、不扩大能力。

---

# 10. 当前执行状态

2026-10-08执行记录；本轮文档刷新没有重跑。

| 分组 | 实际状态 |
|---|---|
| B容量查询 | 1 PASS；运行SimManager生产逻辑，未OnInit |
| INFRA设施 | 16 PASS；不计SIM业务用例 |
| 容量负控 | 1项预期FAIL；检查器有效 |
| 原五业务场景 | BLOCKED；无卡/账户等未执行 |
| Split/设备A | 无真实双版本/设备运行结果 |

Phase0 NOT READY，不能使用来源缺失的3/35替代本仓结果。

---

# 11. 默认Linux防护网

独立GoogleTest进程 → ISimManager → 真实SIM生产逻辑 → GMock外部边界

- GN/Ninja + 固定GoogleTest/GoogleMock，目录test/systemtest/。
- 正式RIL输入建立状态，不直接写Manager字段或mock业务结果。
- 公开返回、回调/事件、稳定状态和对外请求作为证据。
- A设备套件另验SA、IPC、真实权限/capability和Modem。

---

# 12. 分阶段门禁

| 阶段 | 交付 |
|---|---|
| Phase0/G1 | 来源、契约、匹配依赖、最小状态/账户边界切片 |
| G2 | 同进程SO，链接无环、生命周期、Feature/ABI与双版本差分 |
| G3 | 按三个Owner逐条切换writer，保持旧可观察行为 |
| G4 | 明确批准的四槽/MEP/N订阅M资源产品矩阵 |

无运行期实现切换Flag；只读Shadow不双写或双发设备命令。

---

# 13. 本次评审需要决定

- 补齐原决策总账/决策卡及批准范围。
- 指定匹配SimData协议、事务/NULL、eSIM/Registry契约提供方。
- 冻结首批Oracle、旧输出轨迹与CoreManagerInner兼容表。
- 确认SO范围、工厂销毁和停止排空责任。
- 回退先核实设备副作用和数据兼容，再恢复旧产物；git revert不撤销设备效果。

旧V22/V23汇报只作历史参考；统一设计以SAD、类设计与DU裁决为准。
