# SIM-028 生命周期诊断与修复边界

本目录复用 `sim_primary_slot_real_test`，没有替代生产 SimManager。原 C1 的业务、严格 RIL 期望和清理断言均保留。当前六个在卡场景仍因生产订阅对象图未释放而失败；不得将 Host Runner 修复解释为完整生命周期修复。

仓库根执行（先按上一层 README 配置依赖）：

```sh
./telephony_core_service/services/multisim/test/systemtest/tools/build_host.sh --target sim_primary_slot_real_test
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh selected \
  --target sim_primary_slot_real_test --filter 'RealSimPrimary.C1ThreePhasesWithStaleCheckResponseRejected'
python3 -B telephony_core_service/services/multisim/test/systemtest/primary_slot/lifecycle/run_diagnostics.py \
  --output /tmp/sim028-lifetime-trace
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh selected \
  --target sim_primary_slot_real_test --filter 'RunnerLifetime.*:RealCardLifetime.*'
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh all --with-real-primary
```

诊断命令不自动构建；退出码区分0=测试通过、1=测试失败、2=诊断阻塞。LLDB自身退出0不代替被调试程序结果。`trace_graph.py`仅通过命名DWARF字段/断点读取对象、控制块和观察者列表；不在目标内求值，不增加目标shared_ptr引用，不写内存，不按偏移读取或强制清理私有字段。基类子对象地址另表保存，避免将 `IccFile` 的 `EventHandler*` 错算成另一对象。引用图是停止点快照，构造/析构断点的解析数量也保存；未解析/内联的析构不能仅凭断点零次认定泄漏。

## 根因与源码依据

以下箭头均是实际成员或Observer容器的强所有权。每槽独立存在，Monitor跨槽共享；具体地址及控制块见本轮独立证据中的 `trace/graph.json`。

```mermaid
flowchart LR
  Manager[SimManager] --> Monitor[MultiSimMonitor]
  Monitor --> Controller[MultiSimController]
  Monitor --> State[SimStateManager]
  State --> Handle[SimStateHandle]
  Handle --> Obs[ObserverHandler]
  Obs --> Monitor
  Obs --> Records[IccFile]
  Obs --> Files[SimFileManager]
  Files --> Records
  Records --> State
  Records --> RecObs[Records ObserverHandler]
  RecObs --> Files
  State --> RIL
  Records --> RIL
  Files --> FileCtrl[IccFileController]
  FileCtrl --> RIL
  Monitor --> AccountObs[Test AccountObserver]
```

| 引用边/退出动作 | 生产源码位置（相对telephony_core_service） |
| --- | --- |
| Manager→Monitor/Controller/State/File | `services/sim/include/sim_manager.h` 成员；`sim_manager.cpp::OnInit/InitMultiSimObject/InitSingleSimObject`；析构函数为空 |
| Monitor→State、Controller；File只是weak | `services/sim/include/multi_sim_monitor.h:154`～156，构造函数复制所有权 |
| State→Handle；Handle→Observer | `sim_state_manager.cpp::Init`；`sim_state_handle.cpp::Init`；Handle反向State/RIL是weak，不能误画为强环 |
| Observer→订阅者 | `services/tel_ril/include/observer_handler.h::observerHandlerMap_`，`observer_handler.cpp::RegObserver/Remove`：map/list存shared_ptr |
| State的Observer→Monitor | `multi_sim_monitor.cpp::RegisterSimNotify(slot)` 注册STATE_CHANGE/LOCKED/READY；File ICCID观察者另强持Monitor |
| File→Records；Records→State | `sim_file_manager.h::simFile_/iccFileCache_`；`icc_file.h::stateManager_`；`icc_file.cpp::Init` 注册READY/LOCKED/SIMLOCK，形成独立环 |
| Records→Observer→File | `sim_file_manager.cpp::InitSimFile` 注册自身RADIO_SIM_RECORDS_LOADED，形成File/Records独立环 |
| State→Observer→File | `sim_file_manager.cpp::Init` 注册CARD_TYPE_CHANGE及SIM_ICCID_LOADED；反注册后者在 `sim_state_handle.cpp::UnRegisterCoreNotify` 缺失 |
| Monitor→测试AccountObserver | `SimManager::RegisterCoreNotify` 转发ACCOUNT_LOADED，Monitor强存；Manager的UnRegister缺此分支，Monitor也无对应公开注销方法 |
| State→Observer→Phonebook→State | `icc_dialling_numbers_manager.cpp::Init`注册状态变化，头文件强持State且析构为空；后续审查新增第四类强环，见正式方案 |
| RIL残留 | `SimStateManager::telRilManager_`、`IccFile::telRilManager_`、`IccFileController::telRilManager_` 每槽各一个；同一RIL控制块，不能算成十个对象 |
| 状态退出未到达 | `SimStateManager::~SimStateManager` 才调用Handle::UnInit；State已在环内，析构触发不了注销 |
| Monitor析构职责不足 | `~MultiSimMonitor` 仅UnSubscribeListeners（CES/SA），不调用UnRegisterSimNotify；自身强订阅归零前析构不可达 |
| 队列退出 | `utils/common/src/tel_event_queue.cpp::~TelEventQueue`取消/等待；`RemoveAllEvents`只清事件列表，不是禁止新提交或在途排空屏障 |

28个Host worker是本测试FFRT替身为每个queue创建一个线程的实现结果：每槽8个残留queue（StateHandle/FileManager/FileController/DiallingHandler/SimFile/PhonebookManager/Cache/UsimService），加Controller、RadioProtocolController、Monitor、测试AccountObserver各1。真实OHOS FFRT线程池的线程数不能据此推断。停止点队列记录区分pending、running、cancelled；三个StateHandle遗留约180秒的匹配SIM定时器，其他残留queue可为空且仍有worker。无需等待定时器才能证明强环存在。

RIL最终10 = 测试根1 + 三槽 ×（StateManager1 + IccFile1 + FileController1）。Manager析构前为23，析构及Account/SMS/STK等所有者释放后下降；控制块和地址证据用于验证此分解。AccountObserver是测试额外引入的残留对象，原因是调用真实生产注销接口没有对称实现；其余主环即使移除该观察者仍存在。

## 已实施的Host修复及测试范围

外部 `AppExecFwk::EventHandler` Runner改为显式Running→Stopping→Stopped；准入/创建/移动队列同步，停止期间拒绝Post/Start，退出锁后取消待执行工作并正常join在途任务，关闭后不能被迟到Post自动重建。自己的callback线程同步Stop返回false，交由控制线程停止，避免自join。测试RIL弱订阅表现在按owner注销，不再留下已注销的弱登记；它从未强持SIM对象。

五个 `RunnerLifetime` 测试验证正常完成/重复Stop、取消延迟工作并释放capture、在途Stop与回调重投拒绝、并发Stop、自join拒绝。这些只验收Host平台边界。三个 `RealCardLifetime` 直接构造真实State/File对象：缺RIL初始化失败可释放；有效State单独初始化后可注销/释放；State+File仅经现有公开注销仍不能释放，保留失败断言和RIL泄漏检查，不把预期泄漏写成绿色characterization。`OwnedCardGraph`仅负责测试自己构造的对象，未访问或清理SimManager私有对象。

完整SimManager的在途Stop、重复Stop、迟到回调安全仍BLOCKED：生产对象没有Stop契约，不能拿Runner测试替代这些验收。

## 生产补丁审查结论与正式方案

配对补丁已在仓外源码副本完成真实GN/Ninja编译和全量Host回归，主工作区未应用。原生命周期组由7 PASS/1 FAIL变为8 PASS/0 FAIL，三槽和销毁重建公开注销测试也通过；真实Manager七例仍1 PASS/6 FAIL，原Host仍55 PASS/5已知FAIL。因此它**不是完整Shutdown修复**，不以局部全绿批准整图释放。

后续审查从历史图与新C1调试记录确认第四类Phonebook/State强环，三槽共确认12条A/B/C/D闭环；最初9条是已验证子集。原JSON/日志保持历史原字节，新独立记录见 [sim028_shutdown_review_20261111.json](../../configs/sim028_shutdown_review_20261111.json)。

[正式停止方案与补丁完整性审查](shutdown_plan.md)替代旧版几行伪Stop草图，明确源码最小职责集合、四类强环、16组订阅、有效共享所有权阶段、在途B/C1及重试的原语义、Core共享RIL/接口类型边界和不能跳过的native验证。生产代码修改仍需另行授权。

可复现配对补丁验证：

```sh
python3 -B telephony_core_service/services/multisim/test/systemtest/primary_slot/lifecycle/validate_patch.py \
  --output /tmp/sim028-pairing-review
# 读取当前.host-out/host.json依赖路径；可用--config指定。
# 源码仅复制到新建仓外目录，补丁只应用该副本；全套失败正确返回1。
python3 -B telephony_core_service/services/multisim/test/systemtest/primary_slot/lifecycle/check_stop_contract.py \
  --compile-commands .host-out/build/compile_commands.json \
  --header telephony_core_service/services/sim/include/sim_manager.h \
  --output /tmp/sim028-stop-signature
```

第二条仅用声明式隔离头实例化未来Stop测试分支，未链接或运行任何Stop实现，不能代替生命周期PASS。八条ShutdownContract在真实API缺失时执行失败并标BLOCKED；保留GTest原非零退出码，原7/8/60组独立计数。新增PairingReview四例覆盖三槽、部分工厂失败/重复注销、销毁重建和无关worker基线。正式Stop未来分支的运行仍NOT_RUN。
