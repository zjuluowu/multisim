# SIM-B-002 — 经RIL边界建立无卡状态并查询账户

## 1. Test Case Metadata

Case ID=SIM-B-002；Name=经RIL边界建立无卡状态并查询账户；Domain=Presence / account；Requirement ID=R-ABSENT；Contract ID=CT-STATE/CT-ACCOUNT；Invariant ID=INV-PRESENCE-SEMANTICS；Priority=P0；Level=B（拟议Linux受控组件集成，未经过IPC）；Type=边界验收设计+可选C观察记录；拓扑=0张卡、1个有效配置Slot，pSIM；Owner=SIM业务负责人（契约审核）/测试设施维护者（执行），个人未指定；Status=BLOCKED；executed=false。

## 2. Business Intent

保护明确Absent与未就绪的区别，记录无卡时账户及错误码行为，避免固定success+空列表假设。

Out of Scope：真实SA、Binder/IPC、Modem、实体SIM/eUICC、NAPI转换、设备重启、真实网络。内部类、线程次序和数据库结构不是业务判据。

## 3. External Contract

Trigger/Input：RIL正式状态变化通知及Absent响应，随后公开查询状态/账户。

Expected Observable Output：可执行后收敛状态NOT_PRESENT（O-C01）；hasCard=false及账户NO_SIM_CARD候选只采集O-L02，尚不固定错误码。

Given：以下初态及拓扑经过正式边界构造且所有准备证据可用。When：逐条执行公开输入。Then：只执行来源表准许的强制断言。And：OPEN_QUESTION与未运行LEGACY候选仅采集，不自动判产品错误；每个强制判断必须引用Oracle ID。

## 4. System Preconditions

卡数量=0，Slot配置=1（索引0），pSIM；Profile=N/A（关闭eSIM）；SimId=N/A（没有成功可见账户），Subscription=N/A（未提供新公共模型）；Active=N/A（无卡）；Voice/SMS/Data、Primary：公开查询记录无有效卡表示，待O-Q03；Radio=已初始化；网络=无注册；权限=N/A（组件入口不验证真实权限）；用户=合成；持久化=空临时命名空间。

组件入口=真实ISimManager，公开构造SimManager并调用OnInit；非Mock ISimManager。SA/服务入口不在本例覆盖。参数在生产静态缓存初始化前固定。受控桩配置版本拟议sim-host-boundaries-v1（尚未实现）。状态/账户生产依赖闭包与事件运行适配未完成时不能构造这些前提；组件测试不经过服务capability，不等于绕过生产服务检查。

## 5. Initial State

新进程空存储；RIL脚本以GetSimStatus正式响应给出ICC_CARD_ABSENT，匹配合法请求句柄；用公开状态查询确认收敛，不直接设置SimManager状态。

前置条件通过公开查询、外部请求和设施准备结果验证（O-H01）；不读私有成员/内部容器/数据库表。fresh process保证旧单例/静态参数缓存不进入新例。

## 6. Test Steps

| 步骤 | 主要动作/输入边界 | 预期结果及验证边界 | Oracle ID |
|---|---|---|---|
| 1 | 启动空命名空间生产实例 | 独立初态、正式边界准备记录 | O-H01 |
| 2 | 向正式RIL订阅发送状态变化通知 | 记录生产对外GetSimStatus请求的Slot/参数，次数不限 | O-Q09 |
| 3 | 回复合法Absent卡状态 | 在有界公开查询中判NOT_PRESENT；协议/入口不可用则BLOCKED | O-C01/O-H02 |
| 4 | 查询HasSimCard | 记录false候选及错误码 | O-L02 |
| 5 | 查询GetSimAccountInfo与active list | 记录错误码/列表/输出是否赋值；不读取DB表 | O-L02/O-Q10 |
| 6 | 查询默认角色 | 仅记录各接口无效值候选，不能据角色无效判FAIL | O-Q03 |

每步仅一个主要动作。生产输出日志只用于调试，不能代替接口结果；输出含身份时转换为内存相等布尔证据（O-H01）。

## 7. Expected Final State

状态NOT_PRESENT；账户列表/角色与无卡候选基线一并记录。允许历史行为差异进入审核；无卡初态不包含历史持久化记录。

允许变化限于场景公开记录和已确认契约；禁止用内部实现变化判失败。未确认的禁止变化不加入EXPECT。

## 8. Business Invariants and State Transition

初始化未知→RIL确认Absent→稳定NOT_PRESENT（O-C01）；UNKNOWN/NOT_READY不能替代终态判PASS；加载/账户期间一致性待确认。

前提：正式协议有效、Slot获准、生产初始化已完成。未核实的业务不变量不宣称VERIFIED_INVARIANT；O-C状态/身份语义与O-H设施契约分别归类。

## 9. Event/Callback

SIM_STATE_CHANGED及State Registry输出只采集；0..N允许，必达、次数及相对查询偏序待O-Q06。超时是O-H02设施失败，不是假定产品SLA。

设施预算提议：callback 5s、状态/账户观察30s、进程60s总截止；均是待校准可配置host watchdog，非产品SLA。通过单调时钟条件等待/可调度事件投递，有界重查公开接口，禁止固定长sleep判PASS（O-H02）。未达到目标记录timeout，协议/拓扑前提无效记BLOCKED；运行后正确性未达已确认Oracle记FAIL。

## 10. Multi-SIM Isolation

SIM2=N/A；双slot都Absent是矩阵扩展。

不根据内部函数次数或全局事件数量定义隔离。

## 11. Persistence/Failure/Concurrency

本例只验证成功RIL响应与可用存储，不含Modem失联；空命名空间删除后恢复；Linux子进程退出不是设备重启。

目录为独立临时命名空间；初始化前确保空，结束父进程验证子进程已退出及目录删除。清理失败记基础设施FAIL、阻止后例，不能把桩reset当Modem恢复（O-H01）。当前不证明所有真实线程交错。

## 12. Verification Evidence and Pass/Fail Criteria

Oracle集合：O-C01, O-H01, O-H02, O-L02, O-Q03, O-Q06, O-Q09, O-Q10。强制判断只来自标为允许强制的O-C/O-H；O-L/O-Q为候选/人工审核。证据：接收返回、callback业务返回、公开最终状态/账户关系、脱敏身份相等布尔值、外部请求必要参数、公共事件语义、收敛耗时/超时、cleanup结果。

PASS：实际执行生产逻辑、场景必要覆盖点完成、确认Oracle通过且清理成功（O-H01）。FAIL：有效前提下已确认Oracle不满足或设施清理失败。SKIP：运行拓扑不满足且GTest已运行到GTEST_SKIP，带原因。BLOCKED：编译/SDK/适配/入口/Oracle缺失阻止场景执行，由runner记录，不伪装GoogleTest结果。身份/账户子能力未达到可观察前提时不得以条件断言真空通过宣称覆盖完成。

当前：未实现、未编译、未执行；PASS=0、FAIL=0、SKIP=0；场景BLOCKED。没有Legacy基线证据。

## 13. Architecture Independence

测试只看ISimManager公开方法和真实RIL/DataShare/事件/StateRegistry外部契约。装配可知生产对象类型，但业务断言不依赖其类名、SO归属、内部线程顺序、内部调用次数或表结构。以后Split使用同边界驱动；若公共边界不同，先做兼容契约审核，不能静默改Oracle。

## 14. Traceability and Definition of Done

Capability(Presence / account) → CT-STATE/CT-ACCOUNT → INV-PRESENCE-SEMANTICS（未确认项保持待审） → SIM-B-002 → 未来GoogleTest SIMBoundary.SIM_B_002。

DoD：Oracle审核、host产品链接证据、合法边界输入、有效负向控制（确认Oracle）、真实执行XML+JSON、脱敏/清理、Legacy与Split可重放。当前以上执行项均未完成；源码/文档定位见oracle_catalog与source_manifest。不得通过该文档声称第一阶段验收完成。

## 15. Suite Matrix / Combination Coverage

M-0PSIM；0卡/Absent/启动。状态BLOCKED，自动化=false，未执行。扩展条件：host适配确认、固定SDK/GoogleTest版本、产品拓扑核实、业务Oracle确认。A另在设备验证；C只有真实采集后才可标已记录。


Linux目标修订：无实体设备；GoogleMock仅替换RIL/DataShare/事件/StateRegistry等外部边界。初始化依赖实测编译阻塞见 [依赖闭包审计](../initialization_closure.md)。服务入口SIM-B-001仍独立BLOCKED。
