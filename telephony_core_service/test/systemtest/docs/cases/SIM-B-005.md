# SIM-B-005 — 受控PIN Locked状态与合成响应

## 1. Test Case Metadata

Case ID=SIM-B-005；Name=受控PIN Locked状态与合成响应；Domain=PIN lock；Requirement ID=R-PIN；Contract ID=CT-STATE/CT-PIN；Invariant ID=INV-LOCK-SEMANTICS；Priority=P0；Level=B（拟议Linux受控组件集成，未经过IPC）；Type=边界验收设计+可选C观察记录；拓扑=1张合成pSIM、Slot0，PIN locked；没有真实卡后端；Owner=SIM业务负责人（契约审核）/测试设施维护者（执行），个人未指定；Status=BLOCKED；executed=false。

## 2. Business Intent

保护锁定状态对外语义；可选受控解锁请求仅验证边界响应传递，不消耗真实卡尝试次数。

Out of Scope：真实SA、Binder/IPC、Modem、实体SIM/eUICC、NAPI转换、设备重启、真实网络。内部类、线程次序和数据库结构不是业务判据。

## 3. External Contract

Trigger/Input：RIL注入PIN locked；可选UnlockPin(0, syntheticPin, callback)返回固定合成响应，脚本保持锁定。

Expected Observable Output：状态LOCKED（O-C01）；UnlockPin的接收/回调与LockStatusResponse候选O-L05记录，精确错误码/remaining attempts不做永久契约断言。

Given：以下初态及拓扑经过正式边界构造且所有准备证据可用。When：逐条执行公开输入。Then：只执行来源表准许的强制断言。And：OPEN_QUESTION与未运行LEGACY候选仅采集，不自动判产品错误；每个强制判断必须引用Oracle ID。

## 4. System Preconditions

卡数量=1；pSIM；Profile/eSIM=N/A；Slot0；SimId由公开结果记录，Locked时账户可见性待O-Q10；Subscription=N/A。Active/默认Voice/SMS/Data/Primary只记录；Radio=已初始化；网络=无注册；权限native+GET/SET；合成用户；独立空存储。backend必须synthetic，真实PIN路径禁用。

组件入口=真实ISimManager，公开构造SimManager并调用OnInit；非Mock ISimManager。SA/服务入口不在本例覆盖。参数在生产静态缓存初始化前固定。受控桩配置版本拟议sim-host-boundaries-v1（尚未实现）。状态/账户生产依赖闭包与事件运行适配未完成时不能构造这些前提；组件测试不经过服务capability，不等于绕过生产服务检查。

## 5. Initial State

新进程RIL合法状态响应ICC_CONTENT_PIN，必要卡数据按正式请求响应；公开查询收敛LOCKED。不得向真实设备发送PIN。

前置条件通过公开查询、外部请求和设施准备结果验证（O-H01）；不读私有成员/内部容器/数据库表。fresh process保证旧单例/静态参数缓存不进入新例。

## 6. Test Steps

| 步骤 | 主要动作/输入边界 | 预期结果及验证边界 | Oracle ID |
|---|---|---|---|
| 1 | 检查只有合成RIL后端并创建隔离进程 | 真实设备连接存在则危险子路径禁用/跳过，不能尝试解锁 | O-H03/O-H01 |
| 2 | 经RIL通知/响应送入PIN状态 | 合法协议输入及注册回调关联 | O-H01/O-Q09 |
| 3 | 有界公开查询状态 | LOCKED语义；账户可见性另记录 | O-C01/O-H02/O-Q10 |
| 4 | 可选公开UnlockPin合成请求 | 记录对外RIL目标slot和输入相等布尔值，PIN不输出；语义待O-L05 | O-H03/O-L05/O-Q09 |
| 5 | 正式回应合成锁响应并再次查询 | 记录LockStatusResponse和锁状态候选；不按尝试次数归责生产 | O-L05/O-Q01 |

每步仅一个主要动作。生产输出日志只用于调试，不能代替接口结果；输出含身份时转换为内存相等布尔证据（O-H01）。

## 7. Expected Final State

基础场景必须观察LOCKED；可选响应路径未准备时明确SKIP子项而非声称解锁覆盖。生产锁态未输入新的Ready响应时的终态先记录候选。

允许变化限于场景公开记录和已确认契约；禁止用内部实现变化判失败。未确认的禁止变化不加入EXPECT。

## 8. Business Invariants and State Transition

Absent/Unknown→PIN input→LOCKED（O-C01）；可选请求→合成响应，后继锁态待采集，禁止将解锁请求接收当解锁成功。

前提：正式协议有效、Slot获准、生产初始化已完成。未核实的业务不变量不宣称VERIFIED_INVARIANT；O-C状态/身份语义与O-H设施契约分别归类。

## 9. Event/Callback

公开锁事件、State Registry和PIN回调只记录；事件次数范围0..N待确认，成功回调必要性和偏序见O-L05/O-Q06；等待有界，不约束生产线程顺序。

设施预算提议：callback 5s、状态/账户观察30s、进程60s总截止；均是待校准可配置host watchdog，非产品SLA。通过单调时钟条件等待/可调度事件投递，有界重查公开接口，禁止固定长sleep判PASS（O-H02）。未达到目标记录timeout，协议/拓扑前提无效记BLOCKED；运行后正确性未达已确认Oracle记FAIL。

## 10. Multi-SIM Isolation

SIM2=N/A；双卡PIN隔离是后续矩阵扩展，未运行。

不根据内部函数次数或全局事件数量定义隔离。

## 11. Persistence/Failure/Concurrency

只合成PIN响应；PUK、PIN2、错误PIN真实重试不覆盖。延迟/乱序PIN响应待独立Oracle设计；清理无真实卡恢复任务。

目录为独立临时命名空间；初始化前确保空，结束父进程验证子进程已退出及目录删除。清理失败记基础设施FAIL、阻止后例，不能把桩reset当Modem恢复（O-H01）。当前不证明所有真实线程交错。

## 12. Verification Evidence and Pass/Fail Criteria

Oracle集合：O-C01, O-H01, O-H02, O-H03, O-L05, O-Q01, O-Q06, O-Q09, O-Q10。强制判断只来自标为允许强制的O-C/O-H；O-L/O-Q为候选/人工审核。证据：接收返回、callback业务返回、公开最终状态/账户关系、脱敏身份相等布尔值、外部请求必要参数、公共事件语义、收敛耗时/超时、cleanup结果。

PASS：实际执行生产逻辑、场景必要覆盖点完成、确认Oracle通过且清理成功（O-H01）。FAIL：有效前提下已确认Oracle不满足或设施清理失败。SKIP：运行拓扑不满足且GTest已运行到GTEST_SKIP，带原因。BLOCKED：编译/SDK/适配/入口/Oracle缺失阻止场景执行，由runner记录，不伪装GoogleTest结果。身份/账户子能力未达到可观察前提时不得以条件断言真空通过宣称覆盖完成。

当前：未实现、未编译、未执行；PASS=0、FAIL=0、SKIP=0；场景BLOCKED。没有Legacy基线证据。

## 13. Architecture Independence

测试只看ISimManager公开方法和真实RIL/DataShare/事件/StateRegistry外部契约。装配可知生产对象类型，但业务断言不依赖其类名、SO归属、内部线程顺序、内部调用次数或表结构。以后Split使用同边界驱动；若公共边界不同，先做兼容契约审核，不能静默改Oracle。

## 14. Traceability and Definition of Done

Capability(PIN lock) → CT-STATE/CT-PIN → INV-LOCK-SEMANTICS（未确认项保持待审） → SIM-B-005 → 未来GoogleTest SIMBoundary.SIM_B_005。

DoD：Oracle审核、host产品链接证据、合法边界输入、有效负向控制（确认Oracle）、真实执行XML+JSON、脱敏/清理、Legacy与Split可重放。当前以上执行项均未完成；源码/文档定位见oracle_catalog与source_manifest。不得通过该文档声称第一阶段验收完成。

## 15. Suite Matrix / Combination Coverage

M-PIN；1卡/Locked/受控响应；真实卡PIN DEVICE_REQUIRED且未授权。状态BLOCKED，自动化=false，未执行。扩展条件：host适配确认、固定SDK/GoogleTest版本、产品拓扑核实、业务Oracle确认。A另在设备验证；C只有真实采集后才可标已记录。


Linux目标修订：无实体设备；GoogleMock仅替换RIL/DataShare/事件/StateRegistry等外部边界。初始化依赖实测编译阻塞见 [依赖闭包审计](../initialization_closure.md)。服务入口SIM-B-001仍独立BLOCKED。
