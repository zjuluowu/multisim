# SIM-CHAR-001 — 未初始化组件公开查询基线

原始模板未提供；使用用户15项临时结构。本例不是SIM-B-002。

## 1. Test Case Metadata
Case ID=SIM-CHAR-001；Name=未初始化公开查询基线；Domain=组件生命周期；Requirement ID=R-READINESS-DISTINCTION；Contract ID=CT-COMP-OBSERVATION；Invariant ID=N/A（未确认生命周期业务规则）；Priority=P1；Level=C；Type=Characterization采集；拓扑=Linux合成进程，无初始化、无卡状态输入；Owner=测试设施维护者/业务审核人未指定；Status=BLOCKED（链接失败，未运行，无Legacy基线）。

## 2. Business Intent
记录当前版本在未调用OnInit时GetSimState、HasSimCard、GetActiveSimAccountInfoList的返回及输出赋值，用于Legacy/Split人工评审。Out of Scope=正式Absent、正常就绪、账户加载、权限、服务入口与设备。

## 3. External Contract
Given=公开构造真实SimManager，不调用OnInit；When=通过ISimManager查询Slot0状态、hasCard和active账户；Then=采集返回码、状态枚举、bool输出及列表大小；And=不假定应返回UNKNOWN或NO_SIM_CARD（O-Q01/O-Q10）。原行为只能在实际运行后归入O-Q11；不作为永久业务通过断言。

## 4. System Preconditions
卡数量/pSIM/eSIM/Profile/SimId/Subscription/Active/默认Voice/SMS/Data/Primary/Radio/网络=N/A（未装配，不建立卡状态）；请求Slot=0，只是请求标识，不宣称有效Slot；权限/用户=N/A（组件无权限链路）；持久化=N/A；受控参数边界不连接真实系统。

## 5. Initial State
每次观察在Linux新进程内公开创建具体生产对象，初始化输出为合成哨兵bool=true、state=UNKNOWN、空账户vector；不用内部字段。公开析构具体生产对象。没有事件线程或持久化。

## 6. Test Steps
1. 核实真实生产target、源hash与链接证据（O-H01）。
2. 公开构造组件，不执行OnInit；这是明确受控生命周期输入，不伪装Ready（O-H01）。
3. 调用HasSimCard(0, true哨兵)，记录返回与输出；不对结果强制判断（O-Q11待采集）。
4. 调用GetSimState(0, UNKNOWN哨兵)，记录返回与输出（O-Q11待采集）。
5. 调用GetActiveSimAccountInfoList(false, 空vector)，记录返回和大小，无身份输出（O-Q11待采集）。
6. 销毁对象，导出脱敏XML/JSON与生产覆盖（O-H01）。

## 7. Expected Final State
完成上述公开观察并释放对象；不宣称无卡、SA可访问、合法Slot或SIM就绪。允许Legacy值变化，差异进入人工审核，不能根据未确认规则自动判产品错误。

## 8. Business Invariants and State Transition
N/A：无业务状态输入，不建立SIM状态转移。只观察公开构造→未初始化查询→析构阶段；O-Q11不是永久不变量。

## 9. Event/Callback
N/A：这些查询同步；不启动正式RIL或事件。runner 10s单调总截止，无固定sleep。超时为O-H02设施失败，不是产品SLA。

## 10. Multi-SIM Isolation
N/A：没有初始化Slot，不构造双卡。隔离靠新测试进程和栈作用域；不宣称卡间隔离。

## 11. Persistence/Failure/Concurrency
N/A：没有持久化或外部故障输入，不证明并发交错。进程退出恢复对象；不读取数据库，桩重置不等同Modem恢复。生产子模块不能以Mock补齐链接；必要实际实现无法链接时BLOCKED。

## 12. Verification Evidence and Pass/Fail Criteria
Oracle=O-H01/O-H02（设施强制），O-Q01/O-Q10（不自动判错），O-Q11（运行后基线）。PASS只代表C采集完成且源级证据证明真实公开方法执行；FAIL=设施执行/超时/证据缺失；BLOCKED=编译链接缺失，无XML；SKIP=实际GTest拓扑不适用。不对当前业务数值作强制EXPECT。

## 13. Architecture Independence
只读取公开方法输出和返回；启动器知道具体生产类型仅为创建/销毁，断言不依赖类、私有成员、SO、内部表或调用次数。覆盖计数只作生产参与审计。

## 14. Traceability and Definition of Done
生命周期→CT-COMP-OBSERVATION→N/A业务不变量→SIM-CHAR-001→SimCharacterization.BeforeInitialization。DoD=真实编译/运行/覆盖、XML/JSON脱敏、明确C层限制，精确输出只作为人工差分数据。真实Legacy/Split对比仍需两个版本执行。

## 15. Suite Matrix/Combination Coverage
M-PREINIT：Linux组件未初始化；独立于0卡/Absent。当前BLOCKED，链接缺少真实SimStateManager/MultiSimController方法，未执行；原五条保持BLOCKED。扩展到正式状态输入必须完成生产初始化闭包。
