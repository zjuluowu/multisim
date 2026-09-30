# Linux FFRT队列适配与真实生产事件路径（待实施方案）

状态：DESIGN_ONLY / BLOCKED，未实现worker、未编译该生产事件目标、未执行以下设施用例。下述内容为计划，不是已验证行为。

原接口参考固定resourceschedule_ffrt SHA 1d559e70feda31fa8afd45d564daa242660e4edd的cpp/queue.h:316–332、406–419，以及task.h的task_attr::delay；真实平台职责为异步执行、延迟、任务句柄、取消与等待。延迟单位按上游delay注释核实为微秒。此适配不是FFRT ABI/性能替代物。

保留未修改的utils/common/src/tel_event_handler.cpp与tel_event_queue.cpp建立独立production target，不重写其优先级、owner、延迟或取消逻辑。外部ffrt::queue由每队列一个Linux worker执行，以ffrt兼容锁/condition_variable_any实现；新代码不使用std::mutex。submit异步返回，避免在生产SubmitToFFRT持锁期间同步执行回调。队列支持pending取消、running任务等待、析构取消pending并回收worker。取消running明确失败，不伪造取消完成。

队列生命周期由独立shared worker state保持；若最后owner在worker自身回调结束时析构，worker标记stop并detach自身，状态继续存活至loop退出；普通析构join。测试使用有界条件等待验证回调/销毁后停止，不把具体线程顺序作业务Oracle。General submit/wait为独立队列，当前不证明真实FFRT的并发调度/优先级/性能。

设施设计（先于实现）：

- INFRA-EVENT-004 / EVT-PRODUCTION：GoogleMock事件观察者作为外部消费端，输入经公开TelEventHandler::SendEvent进入真实生产TelEventQueue，合成payload匹配回调；在2s有界条件内到达。这只是实际生产事件传输设施，不是ISimManager卡状态场景。
- INFRA-EVENT-005 / EVT-CANCEL：生产SendEvent带远期delay后RemoveAllEvents，退出owner作用域；观察者不得收到被取消事件，worker无残留。等待条件为worker回收与外部观察结果，不sleep等待远期事件。

测试层级INFRA，单独计数，不能以直接调用内部事件Handler作为SIM业务主路径。未来SIM测试主输入仍是ISimManager请求或RIL正式回复owner投递。原无卡/Ready等完整场景仍以初始化闭包和fork数据协议为前置。

注入与捕获仅是任务、延迟和调度故障记录，不判断SIM状态。异常被记录为设施失败并唤醒等待，不以异常吞掉后成功掩盖。Mock/provider重置必须在相关worker退出后，atomic安装不能代替对象生命周期同步。无真实Modem/设备/权限，当前无持久化与卡身份。
