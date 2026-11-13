# INFRA-PTR-001 本地共享所有权

## 1. Test Case Metadata

Case ID=INFRA-PTR-001；Name=本地共享所有权；Domain=Platform Ownership Adapter；Requirement ID=HOST-POINTER；Contract ID=PTR-OWNERSHIP；Invariant ID=HOST-LIFETIME；Priority=P0；Level=INFRA；Type=外部设施自检；拓扑=Linux本地合成对象；Owner=TBD；Status=AUTOMATED（2026-10-08实际PASS）。

## 2. Business Intent

保护外部指针载体的ownership和生命周期，支持后续生产初始化。Out of Scope=SIM状态/账户、OHOS intrusive ABI、IPC/Proxy、真实权限及所有并发交错。

## 3. External Contract

Given=独立本地对象；When=adoption/复制/释放、weak promote或本地iface_cast；Then=符合PTR-OWNERSHIP定义的ownership及空值结果；And=作用域结束对象销毁。不产生卡业务结果。

## 4. System Preconditions

卡数量、pSIM/eSIM、Profile、Slot、SimId/Subscription、Active、默认Voice/SMS/Data、Primary、Radio、网络、权限、用户和持久化全部N/A：只测试外部兼容载体，没有SIM实例或服务。

## 5. Initial State

每例局部对象、销毁计数从0开始。禁止重复adopt已被control block拥有的raw对象。

## 6. Test Steps

1. 创建独立合成对象或空载体。
2. 通过sptr/wptr/iface_cast外部接口执行本例动作。
3. 验证相应ownership、promote和接口结果（PTR-OWNERSHIP）。
4. 释放所有owner，验证析构完成（PTR-OWNERSHIP）。

## 7. Expected Final State

所有强引用释放，合成对象销毁；保留的weak只允许identity比较且不能解引用。无持久化/worker。

## 8. Business Invariants and State Transition

仅HOST-LIFETIME：前提为单次adoption和本地实现接口；从空→存活→最后强owner释放→weak promote失败。不是SIM业务不变量。

## 9. Event/Callback

没有外部公共事件；析构是设施资源证据。同步执行，无固定sleep、无SIM线程顺序Oracle；进程看门狗10s。

## 10. Multi-SIM Isolation

N/A，无SIM2/SIM1实例，不能代替多卡隔离场景。

## 11. Persistence/Failure/Concurrency

无persist；不支持远程Proxy创建或OHOS弱引用跨进程行为。不证明intrusive引用计数与std control block的全部等价性。

## 12. Verification Evidence and Pass/Fail Criteria

GoogleTest所有强制断言关联PTR-OWNERSHIP；XML/JSON记录Case/Oracle/INFRA。失败为FAIL，不计入SIM PASS数。只有合成值/布尔比较，无身份日志。

## 13. Architecture Independence

不访问SIM内部类、成员、数据库或SO布局；设施直接测试外部载体。生产逻辑不能由这些对象重写。

## 14. Traceability and Definition of Done

Host pointer support → PTR-OWNERSHIP → HOST-LIFETIME → INFRA-PTR-001 → native_pointer_test.cpp。原接口来源固定c_utils refbase.h，限制见platform_compile_subset.md。完成需要实际编译运行和资源释放证据。

## 15. Suite Matrix/Combination Coverage

INFRA同步单进程，独立统计；可执行结果见run_record.md。原五条SIM场景仍BLOCKED；是否继续扩展受外部协议闭包约束。

实际执行：/tmp/sim-closure-oct08/run/adapter.xml 与 result.json，进程退出0；本例PASS。SHA、工具链与命令见run_record.md的2026-10-08记录。
