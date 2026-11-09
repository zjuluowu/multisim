# SIM-COMP-001 — 受控参数下的只读SIM槽容量查询

原始模板未提供；使用用户15项临时结构。本例不同于原五条服务入口场景。

## 1. Test Case Metadata
Case ID=SIM-COMP-001；Name=受控槽容量查询；Domain=组件配置查询；Requirement=R-COMP-CAPACITY；Contract=CT-CAPACITY；Invariant=INV-COUNT-NONNEGATIVE；Priority=P0；Level=B Linux受控组件集成；Type=语义检查+当前版本Characterization观察；拓扑=产品参数单槽、无实际卡输入；Owner=测试设施维护者/SIM负责人（未指定个人）；Status=AUTOMATED / PASS（正常运行1条；另有预期FAIL负控，见run_record.md）。

## 2. Business Intent
建立实际生产对象经ISimManager只读调用的最小路径，避免以Mock返回冒称业务运行。Out of Scope：全部状态/账户场景、真实SA/IPC/权限/capability/Modem和平台行为。

## 3. External Contract
Given：新Linux进程；参数服务边界配置const.telephony.slotCount="1"、virtual_modem_switch="false"，无ext能力。When：启动器公开构造SimManager并上转ISimManager，调用GetMaxSimCount。Then：结果为非负槽数量（O-C11）。And：记录确切结果及参数请求作为C候选，未运行前不认定当前应永远等于1（O-L06）。合法max count语义来源：sim.d.ts:701–710“maximum number of SIM card slots”，只采用数量非负语义，不验证JS实现。

## 4. System Preconditions
配置Slot数量1；实体卡数量N/A（未连接RIL，不制造Absent）；pSIM/eSIM/Profile/SimId/Subscription/Active/默认Voice/SMS/Data/Primary/Radio/网络均N/A（容量查询不要求卡状态或初始化）。权限N/A（组件入口未检查）；用户=合成host进程；持久化=N/A（参数桩为独立进程固定配置）。

## 5. Initial State
新进程设置参数脚本后公开构造生产对象；只选当前实现不依赖OnInit的容量查询，不推广至其他方法。接口无虚析构，栈中销毁具体生产对象，禁止通过ISimManager* delete。没有生产worker，不能把未初始化当无卡。

## 6. Test Steps
1. 核实host target/依赖和独立进程配置（O-H01）。
2. 启动器构造真实SimManager；测试仅取得公开容量观察结果，不接触具体对象字段（O-H01）。
3. 经ISimManager查询并按有界条件检查非负、稳定结果（O-C11/O-H02）；将具体结果仅记录O-L06。
4. 记录参数边界被读取的key别名及实际值类别；桩不返回SIM数量，只返回参数字符串（O-H01）。
5. 独立负向控制运行：先执行相同生产查询，再仅在测试证据验证输入将观察值替为-1；同一O-C11检查必须失败。记录corrupted_evidence=true，绝非生产回归或真实参数故障（O-C11/O-H01）。

## 7. Expected Final State
正常查询得到非负公开数量，具体数量由运行后基线记录；允许未调用的RIL/账户保持N/A。禁止凭该值宣称卡存在/Ready或服务可访问。负向控制应为预期FAIL，不能混为正常套件PASS。

## 8. Business Invariants and State Transition
INV-COUNT-NONNEGATIVE仅是已确认“槽数量”的数学语义（O-C11）；初态固定参数→组件读取→公开数量。参数解析/回退具体结果属于O-L06待采集；没有卡状态转移。

## 9. Event/Callback
N/A：选定方法同步返回，不启动事件。使用单调时钟1s上界的公开重复查询至稳定条件；runner 10s进程watchdog。不得将固定sleep或时序作为产品SLA（O-H02）。

## 10. Multi-SIM Isolation
N/A：单槽配置且未创建卡状态；本例不证明双卡隔离，原双卡例保持BLOCKED。

## 11. Persistence/Failure/Concurrency
新子进程隔离参数静态缓存；独立tempnamespace用于产物/证据，运行后检查进程结束并清理临时目录（O-H01）。不读写真实系统参数或持久化账户；不证明线程交错/进程重启恢复。外部无关API不得任意成功：compile-only声明不给实现，意外调用必须链接失败或明确拒绝。

## 12. Verification Evidence and Pass/Fail Criteria
强制Oracle O-C11/O-H01/O-H02；观察O-L06。证据：实际源target与link map、原源hash、组件调用观察、参数桩请求、真实GTest XML/JSON、convergence/cleanup。正常PASS要求完整构建、生产执行和语义满足；无条件支持记BLOCKED；进程超时记FAIL。负控实际GTest必须退出非0且XML记录O-C11失败，runner另记expected_failure_verified，不把它改写为正常测试PASS。

## 13. Architecture Independence
断言只使用ISimManager公开查询的结果，不用内部实现类、字段、SO、数据库或线程顺序。启动器构造生产类型仅用于装配。链接符号/覆盖证据是真实性审计，不是业务Oracle。

## 14. Traceability and Definition of Done
容量查询→CT-CAPACITY→INV-COUNT-NONNEGATIVE→SIM-COMP-001→SimComponent.CapacityReadOnly。DoD：GN/Ninja固定GoogleTest构建；实际生产调用；同步方法有界观察；正常运行XML/JSON与独立负控；清理成功；声明所有未覆盖项。已完成此容量切片的执行证据；不满足原始完整第一阶段标准。

## 15. Suite Matrix/Combination Coverage
新M-CAPACITY：配置单槽/只读/无RIL，AUTOMATED / 正常执行PASS；原五条保持BLOCKED。本例不覆盖0卡/1卡/2卡，只覆盖配置槽容量。扩展需重新设计/Oracle审查。


当前更新：Linux 场景通过固定版本 GoogleMock 构造外部参数边界，生产目标不变；详见 [Linux GoogleMock 方案](../linux_gmock_scenarios.md)。早期手写桩描述属于历史记录。原五条业务场景仍 BLOCKED。
