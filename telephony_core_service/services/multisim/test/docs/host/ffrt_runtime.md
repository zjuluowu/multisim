# Linux FFRT队列适配与真实生产事件路径（Linux受控实现）

状态：实现已运行；INFRA-EVENT-004/005及INFRA-FFRT-001/002实际通过。仅为生产事件传输设施证据，SIM状态/账户仍未初始化或运行。

原接口参考固定resourceschedule_ffrt SHA 1d559e70feda31fa8afd45d564daa242660e4edd的cpp/queue.h:316–332、406–419，以及task.h的task_attr::delay；真实平台职责为异步执行、延迟、任务句柄、取消与等待。延迟单位按上游delay注释核实为微秒。此适配不是FFRT ABI/性能替代物。

保留未修改的utils/common/src/tel_event_handler.cpp与tel_event_queue.cpp建立独立production target，不重写其优先级、owner、延迟或取消逻辑。外部ffrt::queue由每队列一个Linux worker执行，以ffrt兼容锁/condition_variable_any实现；新代码不使用std::mutex。submit异步返回，避免在生产SubmitToFFRT持锁期间同步执行回调。队列支持pending取消、running任务等待、析构取消pending并回收worker。取消running明确失败，不伪造取消完成。

队列生命周期由独立shared worker state保持；若最后owner在worker自身回调结束时析构，worker标记stop并detach自身，状态继续存活至loop退出；普通析构join。测试使用有界条件等待验证回调/销毁后停止，不把具体线程顺序作业务Oracle。General submit/wait为每调用线程的独立队列，当前不证明真实FFRT的并发调度/优先级/性能。

设施设计（先于实现）：

- INFRA-EVENT-004 / EVT-PRODUCTION：GoogleMock事件观察者作为外部消费端，输入经公开TelEventHandler::SendEvent进入真实生产TelEventQueue，合成payload匹配回调；在2s有界条件内到达。这只是实际生产事件传输设施，不是ISimManager卡状态场景。
- INFRA-EVENT-005 / EVT-CANCEL：生产SendEvent带远期delay后RemoveAllEvents，退出owner作用域；观察者不得收到被取消事件，worker无残留。等待条件为worker回收与外部观察结果，不sleep等待远期事件。

测试层级INFRA，单独计数，不能以直接调用内部事件Handler作为SIM业务主路径。未来SIM测试主输入仍是ISimManager请求或RIL正式回复owner投递。原无卡/Ready等完整场景仍以初始化闭包和fork数据协议为前置。

注入与捕获仅是任务、延迟和调度故障记录，不判断SIM状态。异常被记录为设施失败并唤醒等待，不以异常吞掉后成功掩盖。Mock/provider重置必须在相关worker退出后，atomic安装不能代替对象生命周期同步。无真实Modem/设备/权限，当前无持久化与卡身份。

新增设施用例设计：INFRA-FFRT-001 / FFRT-CANCEL-RUNNING。提交任务由受控闸门确认已进入running，再调用cancel应非零；释放闸门后wait返回，owner释放后worker归零。闸门等待最多2s；任意断言失败均先释放闸门再回收，禁止依赖固定sleep或精确毫秒。Oracle来源为固定queue.h取消已执行任务失败的接口说明。无卡/账户/身份N/A，不计SIM业务数。

追加设施设计（实现前）：INFRA-FFRT-002 / FFRT-GENERAL。经未修改生产TelFFRTUtils::SubmitSync发送任务，wait返回后观察合成完成标志；显式关闭调用线程的host general queue后2s内worker归零。只验证该受控模型的执行/等待和清理，不验证完整FFRT祖先任务依赖或并行语义。

## 模型偏差与执行证据

- 每队列单worker，general队列按调用线程分隔；不模拟FFRT完整祖先依赖/工作窃取/优先级/并行性。嵌套同队列wait明确抛异常并记taskFailures，禁止悄悄成功。General owner需在退出场景前StopCallingThreadTasks，以排空并回收。
- pending取消返回0；已开始/已结束返回1；null或非本队列句柄返回-1，后者是host范围限制。真实接口cancel并不通过queue对象校验归属。
- worker自身析构不能join自身；host wait对当前运行任务返回，以允许生产owner自然析构。state仍由worker保留至退出，测试有界等待workers=0。这是适配生命周期模型，不证明真实FFRT完全等价。
- task异常计数保留到显式reset；本轮用例均要求taskFailures=0。新锁使用ffrt::mutex（pthread兼容），不新增std::mutex。
- normal/adapter XML、JSON、map以及LLVM profile位于输出目录。event_transport_production_host编译原生产两文件，profile实际命中SendEvent、SubmitToFFRT、PopEvent；仅审计运行参与，不约束内部调用次数。实际命令/SHA/工具及退出码见run_record.md。
