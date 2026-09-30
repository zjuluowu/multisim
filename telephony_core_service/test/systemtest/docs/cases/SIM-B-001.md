# SIM-B-001 — 公开服务转发入口可调用与就绪分离

## 1. Test Case Metadata

Case ID=SIM-B-001；Name=公开服务转发入口可调用与就绪分离；Domain=Service lifecycle；Requirement ID=R-ENTRY；Contract ID=CT-ENTRY；Invariant ID=INV-OBSERVABILITY；Priority=P0；Level=B（拟议Linux受控组件集成，未经过IPC）；Type=边界验收设计+可选C观察记录；拓扑=1个配置Slot、未注入卡状态；pSIM候选，卡是否在位未知；Owner=SIM业务负责人（契约审核）/测试设施维护者（执行），个人未指定；Status=BLOCKED；executed=false。

## 2. Business Intent

防止把请求接收成功或对象装配成功当成SIM Ready；保护服务边界探测与状态观察的区分。

Out of Scope：真实SA、Binder/IPC、Modem、实体SIM/eUICC、NAPI转换、设备重启、真实网络。内部类、线程次序和数据库结构不是业务判据。

## 3. External Contract

Trigger/Input：调用公开GetSimState(0, callback)并观察两个返回通道。

Expected Observable Output：有界观察接收/拒绝与回调；不要求Ready；当前slot0 capability分支拒绝属于待核实阻塞/候选基线。

Given：以下初态及拓扑经过正式边界构造且所有准备证据可用。When：逐条执行公开输入。Then：只执行来源表准许的强制断言。And：OPEN_QUESTION与未运行LEGACY候选仅采集，不自动判产品错误；每个强制判断必须引用Oracle ID。

## 4. System Preconditions

卡数量：物理卡N/A（受控RIL）；配置Slot=0一槽。pSIM/eSIM：pSIM；Profile=N/A（eSIM关闭）；SimId/Subscription=N/A（尚未取得账户）；Active与默认Voice/SMS/Data、Primary：未知，只记录，不预填业务值。Radio：合成已初始化或显式延迟；网络：无注册；权限：native caller、授权GET_TELEPHONY_STATE；用户：合成用户；持久化：空隔离空间。

服务入口=真实CoreServiceSim，挂真实SimManager；非Mock ISimManager。参数在生产静态缓存初始化前固定。受控桩配置版本拟议sim-host-boundaries-v1（尚未实现）。生产SDK/host适配未到位、入口capability未解阻时不能构造这些前提。

## 5. Initial State

创建新子进程、安装固定参数/权限、RIL通知订阅存在但不发送Ready；装配真实生产对象。初态用公开查询结果记录，不能据未知状态拒绝“服务可访问”。

前置条件通过公开查询、外部请求和设施准备结果验证（O-H01）；不读私有成员/内部容器/数据库表。fresh process保证旧单例/静态参数缓存不进入新例。

## 6. Test Steps

| 步骤 | 主要动作/输入边界 | 预期结果及验证边界 | Oracle ID |
|---|---|---|---|
| 1 | 启动独立隔离子进程并装配生产入口 | 宿主运行条件与装配结果；不是业务Ready证据 | O-H01 |
| 2 | 调用GetSimState(0, callback) | 记录接收错误及回调是否到达；服务接收语义待确认 | O-L01/O-Q08 |
| 3 | 调用HasSimCard(0, callback) | 记录公开结果，不推断Ready或实体卡存在 | O-Q01/O-L01 |
| 4 | 公开查询账户列表 | 记录业务错误/列表；不要求数量 | O-Q10 |

每步仅一个主要动作。生产输出日志只用于调试，不能代替接口结果；输出含身份时转换为内存相等布尔证据（O-H01）。

## 7. Expected Final State

进程可完成边界调用或明确返回拒绝；没有Ready强制终态。查询不应通过fixture写卡状态；观察值只用于候选记录。

允许变化限于场景公开记录和已确认契约；禁止用内部实现变化判失败。未确认的禁止变化不加入EXPECT。

## 8. Business Invariants and State Transition

未输入卡状态→可调用/可拒绝→观察未知结果；可访问和就绪分离是测试含义，不对未初始化返回某个状态硬断言。

前提：正式协议有效、Slot获准、生产初始化已完成。未核实的业务不变量不宣称VERIFIED_INVARIANT；O-C状态/身份语义与O-H设施契约分别归类。

## 9. Event/Callback

查询回调记录0..N（具体成功路径数量待O-Q06确认）；没有SIM事件强制要求；以方法返回/回调截止条件收敛，不等待Ready。

设施预算提议：callback 5s、状态/账户观察30s、进程60s总截止；均是待校准可配置host watchdog，非产品SLA。通过单调时钟条件等待/可调度事件投递，有界重查公开接口，禁止固定长sleep判PASS（O-H02）。未达到目标记录timeout，协议/拓扑前提无效记BLOCKED；运行后正确性未达已确认Oracle记FAIL。

## 10. Multi-SIM Isolation

SIM2=N/A（单配置槽，未建立卡）；不设置跨卡断言。

不根据内部函数次数或全局事件数量定义隔离。

## 11. Persistence/Failure/Concurrency

延迟RIL准备可设计扩展；此例不注入存储故障，不做重启。不存在已知实际SA/Binder。

目录为独立临时命名空间；初始化前确保空，结束父进程验证子进程已退出及目录删除。清理失败记基础设施FAIL、阻止后例，不能把桩reset当Modem恢复（O-H01）。当前不证明所有真实线程交错。

## 12. Verification Evidence and Pass/Fail Criteria

Oracle集合：O-H01, O-H02, O-L01, O-Q01, O-Q08, O-Q10。强制判断只来自标为允许强制的O-C/O-H；O-L/O-Q为候选/人工审核。证据：接收返回、callback业务返回、公开最终状态/账户关系、脱敏身份相等布尔值、外部请求必要参数、公共事件语义、收敛耗时/超时、cleanup结果。

PASS：实际执行生产逻辑、场景必要覆盖点完成、确认Oracle通过且清理成功（O-H01）。FAIL：有效前提下已确认Oracle不满足或设施清理失败。SKIP：运行拓扑不满足且GTest已运行到GTEST_SKIP，带原因。BLOCKED：编译/SDK/适配/入口/Oracle缺失阻止场景执行，由runner记录，不伪装GoogleTest结果。身份/账户子能力未达到可观察前提时不得以条件断言真空通过宣称覆盖完成。

当前：未实现、未编译、未执行；PASS=0、FAIL=0、SKIP=0；场景BLOCKED。没有Legacy基线证据。

## 13. Architecture Independence

测试只看CoreServiceSim公开方法和真实RIL/DataShare/事件/StateRegistry外部契约。装配可知生产对象类型，但业务断言不依赖其类名、SO归属、内部线程顺序、内部调用次数或表结构。以后Split使用同边界驱动；若公共边界不同，先做兼容契约审核，不能静默改Oracle。

## 14. Traceability and Definition of Done

Capability(Service lifecycle) → CT-ENTRY → INV-OBSERVABILITY（未确认项保持待审） → SIM-B-001 → 未来GoogleTest SIMBoundary.SIM_B_001。

DoD：Oracle审核、host产品链接证据、合法边界输入、有效负向控制（确认Oracle）、真实执行XML+JSON、脱敏/清理、Legacy与Split可重放。当前以上执行项均未完成；源码/文档定位见oracle_catalog与source_manifest。不得通过该文档声称第一阶段验收完成。

## 15. Suite Matrix / Combination Coverage

M-ENTRY；启动/未报告状态；所有设备路径另列DEVICE_REQUIRED。状态BLOCKED，自动化=false，未执行。扩展条件：host适配确认、固定SDK/GoogleTest版本、产品拓扑核实、capability入口问题处理、业务Oracle确认。A另在设备验证；C只有真实采集后才可标已记录。
