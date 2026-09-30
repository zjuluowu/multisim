# SIM-B-004 — 双pSIM账户映射与SIM2局部操作隔离

## 1. Test Case Metadata

Case ID=SIM-B-004；Name=双pSIM账户映射与SIM2局部操作隔离；Domain=Multi-SIM isolation；Requirement ID=R-DUAL-ISOLATION；Contract ID=CT-ACCOUNT/CT-IDENTITY/CT-NAME；Invariant ID=INV-LOCAL-CHANGE (待确认)；Priority=P0；Level=B（拟议Linux受控组件集成，未经过IPC）；Type=边界验收设计+可选C观察记录；拓扑=2张合成pSIM，Slot0=CARD-A、Slot1=CARD-B；产品2-slot能力待确认；Owner=SIM业务负责人（契约审核）/测试设施维护者（执行），个人未指定；Status=BLOCKED；executed=false。

## 2. Business Intent

记录两卡账户/标识关系，在SIM2改名时观察SIM1稳定状态和身份，形成后续拆分比较依据。

Out of Scope：真实SA、Binder/IPC、Modem、实体SIM/eUICC、NAPI转换、设备重启、真实网络。内部类、线程次序和数据库结构不是业务判据。

## 3. External Contract

Trigger/Input：调用SetShowName(1, synthetic SIM-B-renamed, callback)，不切换Primary或默认Data。

Expected Observable Output：成功账户的slotIndex与查询slot一致（O-C02）；成功身份结果与对应合成卡一致（O-C03）。SIM1状态/身份/账户映射/显示名不得变化及SIM2改名收敛属于O-Q07待确认，先差分候选采集。

Given：以下初态及拓扑经过正式边界构造且所有准备证据可用。When：逐条执行公开输入。Then：只执行来源表准许的强制断言。And：OPEN_QUESTION与未运行LEGACY候选仅采集，不自动判产品错误；每个强制判断必须引用Oracle ID。

## 4. System Preconditions

卡数量=2；pSIM+pSIM；Profile=N/A；Slot0/1；SimId通过账户结果获得并映射别名，不假定连续或等于Slot；Subscription=N/A。Active=两卡脚本策略固定并记录；默认Voice/SMS/Data、Primary固定初始配置且只观察允许联动；Radio已初始化；网络无注册；native+GET/SET及敏感查询授权；独立合成用户/存储。没有产品2-slot证据时运行GTEST_SKIP或准备BLOCKED。

组件入口=真实ISimManager，公开构造SimManager并调用OnInit；非Mock ISimManager。SA/服务入口不在本例覆盖。参数在生产静态缓存初始化前固定。受控桩配置版本拟议sim-host-boundaries-v1（尚未实现）。状态/账户生产依赖闭包与事件运行适配未完成时不能构造这些前提；组件测试不经过服务capability，不等于绕过生产服务检查。

## 5. Initial State

新进程，分别经合法RIL通知/响应完成CARD-A、CARD-B状态和身份；公开查询两卡状态/账户/身份，并记录初始角色。不能直接填账户缓存。

前置条件通过公开查询、外部请求和设施准备结果验证（O-H01）；不读私有成员/内部容器/数据库表。fresh process保证旧单例/静态参数缓存不进入新例。

## 6. Test Steps

| 步骤 | 主要动作/输入边界 | 预期结果及验证边界 | Oracle ID |
|---|---|---|---|
| 1 | 核实产品双Slot能力并初始化隔离实例 | 不以扩大桩容量证明产品双卡；缺条件SKIP/BLOCKED | O-H01 |
| 2 | 经RIL分别加载两卡数据 | 公开状态Ready/Loaded，各卡身份成功内存匹配 | O-C01/O-C03 |
| 3 | 公开查询账户/双向映射/显示名/角色 | 成功账户slot语义检查，余关系只记录候选 | O-C02/O-Q05/O-Q07 |
| 4 | 公开SetShowName Slot1 | 记录接收与回调结果以及外部存储请求的目标关联；不精确次数 | O-L04/O-Q09 |
| 5 | 公开查询终态直到有界截止 | 比对SIM1公开快照与SIM2显示名；隔离/改名正确性待O-Q07批准后强断言 | O-H02/O-Q07/O-L04 |

每步仅一个主要动作。生产输出日志只用于调试，不能代替接口结果；输出含身份时转换为内存相等布尔证据（O-H01）。

## 7. Expected Final State

记录两卡稳定快照和局部改名效果；允许的默认角色联动尚待确认。强制身份/slot语义仍适用；隔离未获Oracle确认时不宣称已建立强隔离防护。

允许变化限于场景公开记录和已确认契约；禁止用内部实现变化判失败。未确认的禁止变化不加入EXPECT。

## 8. Business Invariants and State Transition

两卡Absent→两卡Ready/Loaded→SIM2局部请求→稳定公开快照；INV-LOCAL-CHANGE只有在负责人确认作用域和联动后生效。过渡期间可不同步，不判毫秒时序。

前提：正式协议有效、Slot获准、生产初始化已完成。未核实的业务不变量不宣称VERIFIED_INVARIANT；O-C状态/身份语义与O-H设施契约分别归类。

## 9. Event/Callback

记录改名账户通知及状态事件；SIM1是否禁止某类事件待O-Q06/O-Q07，不固定全部事件精确一次；只审已确认必要偏序。

设施预算提议：callback 5s、状态/账户观察30s、进程60s总截止；均是待校准可配置host watchdog，非产品SLA。通过单调时钟条件等待/可调度事件投递，有界重查公开接口，禁止固定长sleep判PASS（O-H02）。未达到目标记录timeout，协议/拓扑前提无效记BLOCKED；运行后正确性未达已确认Oracle记FAIL。

## 10. Multi-SIM Isolation

待确认禁止变化：SIM1身份、状态、simId-slot映射、显示名；待确认允许联动：全局默认角色及账户列表通知。这张白名单需负责人批准，当前不自动判错。

不根据内部函数次数或全局事件数量定义隔离。

## 11. Persistence/Failure/Concurrency

本例不执行并发改名/存储失败；扩展需各故障契约确认。每例清理两卡数据与参数；不依赖旧例。

目录为独立临时命名空间；初始化前确保空，结束父进程验证子进程已退出及目录删除。清理失败记基础设施FAIL、阻止后例，不能把桩reset当Modem恢复（O-H01）。当前不证明所有真实线程交错。

## 12. Verification Evidence and Pass/Fail Criteria

Oracle集合：O-C01, O-C02, O-C03, O-H01, O-H02, O-L04, O-Q05, O-Q06, O-Q07, O-Q09。强制判断只来自标为允许强制的O-C/O-H；O-L/O-Q为候选/人工审核。证据：接收返回、callback业务返回、公开最终状态/账户关系、脱敏身份相等布尔值、外部请求必要参数、公共事件语义、收敛耗时/超时、cleanup结果。

PASS：实际执行生产逻辑、场景必要覆盖点完成、确认Oracle通过且清理成功（O-H01）。FAIL：有效前提下已确认Oracle不满足或设施清理失败。SKIP：运行拓扑不满足且GTest已运行到GTEST_SKIP，带原因。BLOCKED：编译/SDK/适配/入口/Oracle缺失阻止场景执行，由runner记录，不伪装GoogleTest结果。身份/账户子能力未达到可观察前提时不得以条件断言真空通过宣称覆盖完成。

当前：未实现、未编译、未执行；PASS=0、FAIL=0、SKIP=0；场景BLOCKED。没有Legacy基线证据。

## 13. Architecture Independence

测试只看ISimManager公开方法和真实RIL/DataShare/事件/StateRegistry外部契约。装配可知生产对象类型，但业务断言不依赖其类名、SO归属、内部线程顺序、内部调用次数或表结构。以后Split使用同边界驱动；若公共边界不同，先做兼容契约审核，不能静默改Oracle。

## 14. Traceability and Definition of Done

Capability(Multi-SIM isolation) → CT-ACCOUNT/CT-IDENTITY/CT-NAME → INV-LOCAL-CHANGE (待确认)（未确认项保持待审） → SIM-B-004 → 未来GoogleTest SIMBoundary.SIM_B_004。

DoD：Oracle审核、host产品链接证据、合法边界输入、有效负向控制（确认Oracle）、真实执行XML+JSON、脱敏/清理、Legacy与Split可重放。当前以上执行项均未完成；源码/文档定位见oracle_catalog与source_manifest。不得通过该文档声称第一阶段验收完成。

## 15. Suite Matrix / Combination Coverage

M-2PSIM；2卡/Ready/局部操作/隔离（待确认）。状态BLOCKED，自动化=false，未执行。扩展条件：host适配确认、固定SDK/GoogleTest版本、产品拓扑核实、业务Oracle确认。A另在设备验证；C只有真实采集后才可标已记录。


Linux目标修订：无实体设备；GoogleMock仅替换RIL/DataShare/事件/StateRegistry等外部边界。初始化依赖实测编译阻塞见 [依赖闭包审计](../initialization_closure.md)。服务入口SIM-B-001仍独立BLOCKED。
