# Linux防护网与统一架构设计的关系

日期：2026-10-15。当前目标见[文档入口](../../../docs/README.md)、[统一裁决](../../../docs/MultiSIM/08_design_unification_decisions.md)、[SAD](../../../docs/SIM_Refactored_Software_Architecture_Design_Version3.md)。

## 1. 目标设计不替换当前被测逻辑

新设计采用四Context、三聚合、三类目标和一个ACL。当前仓未实现这些聚合/总线/SlotRegistry，测试仍通过ISimManager执行当前真实SimManager等生产源码，不先mock目标类制造业务PASS。

默认B层是Linux受控组件集成，不需要实体设备。正式外部接口用固定版本GoogleMock配置，桩不提供待验证的SIM状态/账户业务结果；测试不依赖旧test目标、Mock或Fixture。

## 2. Oracle保持独立

架构中的DU/INV及原D-/REL-/FLOW编号是追踪标签，不自动晋升确认契约。业务强制断言仍须关联Oracle ID及其来源：CONFIRMED_CONTRACT、适用前提核实的VERIFIED_INVARIANT、实采LEGACY_BEHAVIOR或OPEN_QUESTION。

以下不能因新设计字样直接成为业务断言：

- DISABLED一定映射无卡；
- 所有请求必须有simId；
- simId跨删除/重装永久稳定；
- 无卡默认角色永远有效或账户必定删除；
- 事件恰好一次、全局顺序、所有并发结果确定；
- 写NULL已兼容、四槽已获准或所有应用可见；
- Git回退证明设备副作用已撤销。

存储写计划和提供方事务可做独立契约测试，但数据库表、内部容器/类、普通日志顺序及无契约次数不是SIM业务PASS证据。

## 3. 阶段与实际状态

Phase0/G1尚未就绪，已批准的Linux host适配与外部设施可继续。G2机械拆SO、G3迁移writer、G4产品扩展分别验收；不以类图评审授权改生产API/schema/权限。

2026-10-08记录仍为B容量1 PASS、INFRA16 PASS、容量负控预期FAIL；原五业务场景和C候选BLOCKED。新输入3/35因主档缺失不作本仓统计。此次仅文档刷新，未重跑/改写历史XML/JSON。

未覆盖SA注册发现、实际Client/Proxy/Stub、未经过的服务入口检查、真实权限/capability、Modem与设备行为；A套件独立。服务入口capability与14个外部存储字段是独立阻塞，不通过组件测试绕过入口或判断其为缺陷。

## 4. 差分和恢复

同一边界用例分别链接Legacy/Split两种构建产物，不需要运行期Feature Flag，不双发设备命令。只读Shadow可比较，不成为writer。

先通过正式RIL输入建立Absent，观察公开状态/账户并做有界收敛、清理、结果和有效负控。绑定代次与Records加载轮次分开测试；合法切换因果关联不能被全局revision变化一律拒绝。

进程重启不等于设备重启，桩重置不等于Modem恢复。恢复、回放和取消按已确认范围测试，不承诺当前设施拥有虚拟时钟、所有交错、跨重启同OperationId或真实危险卡操作。
