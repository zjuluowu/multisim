# 多SIM问题与演进分析：当前证据与目标约束

日期：2026-10-15。事实基线见[01](01_sim_subsystem_baseline.md)，方案选择见[DU裁决](08_design_unification_decisions.md)，详细目标见[SAD](../SIM_Refactored_Software_Architecture_Design_Version3.md)。本文不另立公共SubscriptionId、六聚合或数据库重建方案。

## 1. 概念先区分作用域

| 概念 | 语义 | 当前演进约束 |
|---|---|---|
| 物理位置/硬件端点 | 卡或eUICC的接入位置 | 不直接等于旧slot、simId或Modem |
| 逻辑slot/路由 | 请求使用的逻辑位置 | 合法、配置、可见和执行能力分别判断 |
| UICC应用/Profile | 卡侧凭据与运行上下文 | 未识别账户前也需安全/文件操作 |
| 账户/Subscription | 本阶段沿用simId的不透明身份 | 不新增第二个公共ID；长期身份政策待确认 |
| Profile/Port绑定 | 某端点当前关联 | MEP和可信Port来源按产品/外仓确认 |
| 默认角色/Primary | 角色选择及主卡策略 | 目标区分意图/有效选择，保留既有Data联动 |
| 资源分配 | 逻辑栈与设备执行 | 不能由slot索引或默认角色推导业务就绪 |

管理数量、Profile启用数、逻辑栈、驻网和通信并发不是同一维度。两pSIM+eSIM逻辑位是新输入的目标产品profile，不是所有产品固定现状。参数默认1及分布式分支、RadioProtocol两个槽的初始化限制应分别核对，不用常量或扩大桩卡数证明四卡。

## 2. 问题证据及保护目标

| 代码形态 | 可核实事实 | 风险推断 / 设计响应 |
|---|---|---|
| 多组vector[slot] | SimManager:61–66关联运行对象 | 生命周期跨数组传播；三类目标与保留Runtime桥接 |
| 账户缓存与simId解析 | MultiSimController/SimManager沿旧映射查询 | 账户身份与位置分离，不伪造ID或重建DB |
| 激活、Primary、Data联动 | MultiSimController:950–1033、1478–1527、1685–1697 | 用显式内部后续命令保持旧行为，领域策略不新增应用调用 |
| 状态与加载分散更新 | SimStateHandle、SimStateManager、IccFile、SimFile | 独立SlotFacts、绑定与加载代次、逐case兼容输出 |
| 依赖CoreManagerInner | 卡状态/文件/监视及主卡路径反查 | 保留消费侧兼容，内部Host Ports处理反向依赖 |
| DataShare、参数和缓存 | SimRdbHelper CRUD + Data参数写 | 明确真实事务范围，提交队列不等于跨系统原子性 |

这些静态事实不直接证明故障率、成本非线性或线上缺陷。旧合入统计和PR耗时缺少当前样本/脚本，作为[历史线索](../history/pre_unification_20261015/MultiSIM/02_current_architecture_and_evolution.md)保留，不冒称本轮复算。

## 3. 四类反例候选

1. 统计超时在事件表回调后可进入通用完成通道；PIN已有专用等待。只证明风险入口，实际串扰需公开/外部边界复现。
2. 卡状态字典仅列五项，协议另含4–11，日志访问未判end；实际影响取决于输入和宏求值。
3. 同状态分支可能跳过iccid_更新；切换阶段和specifiedIccid过滤已存在，必须保留并核对前提。
4. 文件刷新可发生在同一绑定内；现有serial与计数不等于加载轮次协议。目标RecordsGeneration不能凭源码候选就成为永久业务Oracle。

反例测试先定义确认契约/适用不变量，未确认行为记录OPEN或实采LEGACY；不修改生产代码制造红灯，不将允许暂态和重试次数当失败。

## 4. 当前选择优于旧方案的原因

- V3分类目标避免“所有请求先要订阅”的误拒绝，保留卡应用生产逻辑。
- 三聚合使账户/绑定、操作、角色/资源各有领域Owner，兼容重构不先扩展为六聚合、多个新表和新接口。
- CoreManagerInner的消费侧调用/线程/注入契约明确，私有端口用于内部解耦，既保兼容又能设计单向SO链接。
- 内部领域事件与旧输出分开，避免借新总线改变通知完成点、重复与回放行为。
- 运行期Operation不承诺不存在的持久日志和跨重启同ID恢复；未知设备效果通过事实重查处理。

## 5. 迁移顺序和边界

Phase0/G1：来源、契约和最小Linux状态/账户切片；G2：同进程SO机械拆分；G3：按事实Owner逐条迁移；G4：经明确批准的产品拓扑扩展。没有运行期Feature Flag，不双发设备命令；只读Shadow可比较，不能成为独立writer。

数据库schema、公共API/枚举/错误码、权限、厂商ABI保持。ICCID NULL、四槽、默认策略和持久化恢复的来源/能力门禁尚未闭合，不能当作本轮已授权实施。

## 6. 当前状态

当前仓没有旧文档所述Shadow类型或三聚合实现。B容量查询已运行，原五业务场景仍BLOCKED；详见[07](07_subscription_refactoring_status.md)。拓扑与事务设计不能以旧分支的静态检查代替当前构建/执行证据。
