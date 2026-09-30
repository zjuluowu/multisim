# SIM-B-003 — 单pSIM Ready身份与账户可见性

## 1. Test Case Metadata

Case ID=SIM-B-003；Name=单pSIM Ready身份与账户可见性；Domain=Readiness / identity / account；Requirement ID=R-READY；Contract ID=CT-STATE/CT-IDENTITY/CT-ACCOUNT；Invariant ID=INV-IDENTITY-SEMANTICS；Priority=P0；Level=B（拟议Linux受控组件集成，未经过IPC）；Type=边界验收设计+可选C观察记录；拓扑=1张合成pSIM，配置Slot0，Active配置固定且记录；Owner=SIM业务负责人（契约审核）/测试设施维护者（执行），个人未指定；Status=BLOCKED；executed=false。

## 2. Business Intent

执行真实状态和文件加载逻辑，保护公开身份语义；将Ready与账户完成加载分别观察。

Out of Scope：真实SA、Binder/IPC、Modem、实体SIM/eUICC、NAPI转换、设备重启、真实网络。内部类、线程次序和数据库结构不是业务判据。

## 3. External Contract

Trigger/Input：经注册RIL通知及请求响应输入Ready卡和文件数据。

Expected Observable Output：公开状态收敛READY或LOADED（O-C01）；成功ICCID查询内存匹配CARD-A（O-C03）；返回账户的slotIndex=0（O-C02）。账户必可见与数量先O-L03/O-Q10，不提前强断言。

Given：以下初态及拓扑经过正式边界构造且所有准备证据可用。When：逐条执行公开输入。Then：只执行来源表准许的强制断言。And：OPEN_QUESTION与未运行LEGACY候选仅采集，不自动判产品错误；每个强制判断必须引用Oracle ID。

## 4. System Preconditions

卡数量=1；pSIM；eSIM/Profile=N/A；Slot0；SimId由公开成功账户查询获得，不预设数值；Subscription=N/A。Active=脚本/产品策略固定后记录；Voice/SMS/Data与Primary只记录，不强制联动；Radio=已初始化；网络=无注册；权限=native+系统敏感查询资格、GET授权；用户=合成；存储=空、正常响应。

组件入口=真实ISimManager，公开构造SimManager并调用OnInit；非Mock ISimManager。SA/服务入口不在本例覆盖。参数在生产静态缓存初始化前固定。受控桩配置版本拟议sim-host-boundaries-v1（尚未实现）。状态/账户生产依赖闭包与事件运行适配未完成时不能构造这些前提；组件测试不经过服务capability，不等于绕过生产服务检查。

## 5. Initial State

新进程，先明确Absent；再RIL响应ICC_CONTENT_READY以及生产请求所需合法卡类型、EF文件/ICCID/IMSI响应。合成身份使用场景别名CARD-A，真实值只内存保留；响应编码合法性需host SDK验证。

前置条件通过公开查询、外部请求和设施准备结果验证（O-H01）；不读私有成员/内部容器/数据库表。fresh process保证旧单例/静态参数缓存不进入新例。

## 6. Test Steps

| 步骤 | 主要动作/输入边界 | 预期结果及验证边界 | Oracle ID |
|---|---|---|---|
| 1 | 完成Absent公开初态 | 执行SIM-B-002初态构造契约，独立进程不继承上一例 | O-C01/O-H01 |
| 2 | 送入Ready通知并响应状态请求 | 记录RIL正式请求slot/参数；必要请求集合待确认 | O-Q09 |
| 3 | 按真实请求返回合法合成身份和EF数据 | 不调用SimFile内部方法；有不支持请求就BLOCKED | O-H01/O-Q09 |
| 4 | 有界查询GetSimState | 状态READY或LOADED；Ready与账户加载不混同 | O-C01/O-H02 |
| 5 | 查询GetSimIccId | 若成功按O-C03内存匹配；若失败记录候选错误，身份能力场景未完成不能整体PASS | O-C03/O-L03 |
| 6 | 查询单账户和active list | 成功时slotIndex必须0；记录可见性、simId别名、isActive | O-C02/O-L03/O-Q10 |
| 7 | 查询GetSimId/GetSlotId | 记录同轮映射布尔关系候选，无跨重启断言 | O-Q05 |

每步仅一个主要动作。生产输出日志只用于调试，不能代替接口结果；输出含身份时转换为内存相等布尔证据（O-H01）。

## 7. Expected Final State

状态Ready/Loaded；身份检查必须有实际成功结果才完成身份目标；账户目标未可见时标该能力BLOCKED/候选差异待审核。不能凭状态通过宣称账户覆盖成功。

允许变化限于场景公开记录和已确认契约；禁止用内部实现变化判失败。未确认的禁止变化不加入EXPECT。

## 8. Business Invariants and State Transition

Absent→Ready→可能Loaded；身份/账户完成不是与Ready同步的硬要求。禁止将合成CARD-A的身份错返回为另一卡身份（成功查询前提O-C03）。

前提：正式协议有效、Slot获准、生产初始化已完成。未核实的业务不变量不宣称VERIFIED_INVARIANT；O-C状态/身份语义与O-H设施契约分别归类。

## 9. Event/Callback

收集公开SIM_STATE_CHANGED、State Registry、账户回调/事件；事件必须/禁止、次数及关键偏序待O-Q06；没有精确一次断言。状态与身份达条件后停止有界等待。

设施预算提议：callback 5s、状态/账户观察30s、进程60s总截止；均是待校准可配置host watchdog，非产品SLA。通过单调时钟条件等待/可调度事件投递，有界重查公开接口，禁止固定长sleep判PASS（O-H02）。未达到目标记录timeout，协议/拓扑前提无效记BLOCKED；运行后正确性未达已确认Oracle记FAIL。

## 10. Multi-SIM Isolation

SIM2=N/A；未插入槽不要求任何未确认角色约束。

不根据内部函数次数或全局事件数量定义隔离。

## 11. Persistence/Failure/Concurrency

存储失败/延迟、重复通知是扩展；本例正常响应。不能覆盖真实EF格式差异、真实网络或Modem行为。

目录为独立临时命名空间；初始化前确保空，结束父进程验证子进程已退出及目录删除。清理失败记基础设施FAIL、阻止后例，不能把桩reset当Modem恢复（O-H01）。当前不证明所有真实线程交错。

## 12. Verification Evidence and Pass/Fail Criteria

Oracle集合：O-C01, O-C02, O-C03, O-H01, O-H02, O-L03, O-Q05, O-Q06, O-Q09, O-Q10。强制判断只来自标为允许强制的O-C/O-H；O-L/O-Q为候选/人工审核。证据：接收返回、callback业务返回、公开最终状态/账户关系、脱敏身份相等布尔值、外部请求必要参数、公共事件语义、收敛耗时/超时、cleanup结果。

PASS：实际执行生产逻辑、场景必要覆盖点完成、确认Oracle通过且清理成功（O-H01）。FAIL：有效前提下已确认Oracle不满足或设施清理失败。SKIP：运行拓扑不满足且GTest已运行到GTEST_SKIP，带原因。BLOCKED：编译/SDK/适配/入口/Oracle缺失阻止场景执行，由runner记录，不伪装GoogleTest结果。身份/账户子能力未达到可观察前提时不得以条件断言真空通过宣称覆盖完成。

当前：未实现、未编译、未执行；PASS=0、FAIL=0、SKIP=0；场景BLOCKED。没有Legacy基线证据。

## 13. Architecture Independence

测试只看ISimManager公开方法和真实RIL/DataShare/事件/StateRegistry外部契约。装配可知生产对象类型，但业务断言不依赖其类名、SO归属、内部线程顺序、内部调用次数或表结构。以后Split使用同边界驱动；若公共边界不同，先做兼容契约审核，不能静默改Oracle。

## 14. Traceability and Definition of Done

Capability(Readiness / identity / account) → CT-STATE/CT-IDENTITY/CT-ACCOUNT → INV-IDENTITY-SEMANTICS（未确认项保持待审） → SIM-B-003 → 未来GoogleTest SIMBoundary.SIM_B_003。

DoD：Oracle审核、host产品链接证据、合法边界输入、有效负向控制（确认Oracle）、真实执行XML+JSON、脱敏/清理、Legacy与Split可重放。当前以上执行项均未完成；源码/文档定位见oracle_catalog与source_manifest。不得通过该文档声称第一阶段验收完成。

## 15. Suite Matrix / Combination Coverage

M-1PSIM；1卡/Ready/身份/账户/启动。状态BLOCKED，自动化=false，未执行。扩展条件：host适配确认、固定SDK/GoogleTest版本、产品拓扑核实、业务Oracle确认。A另在设备验证；C只有真实采集后才可标已记录。


Linux目标修订：无实体设备；GoogleMock仅替换RIL/DataShare/事件/StateRegistry等外部边界。初始化依赖实测编译阻塞见 [依赖闭包审计](../initialization_closure.md)。服务入口SIM-B-001仍独立BLOCKED。
