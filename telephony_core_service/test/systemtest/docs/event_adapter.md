# 外部事件平台适配设计

替换模块：OHOS AppExecFwk InnerEvent 的事件信封/owner/payload接口；来源为当前生产 utils/common/include/tel_event_handler.h、utils/common/src/tel_event_handler.cpp、services/sim/src/sim_state_handle.cpp:634–687 及各SIM文件控制器调用。没有匹配平台原始头文件；这是消费端核实的最小Linux接口，不声称ABI兼容。

真实职责：运载event id、param、时间、处理器弱引用和带类型的对象。适配只运载调用者给出的数据；不解释SimCardStatusInfo、不生成状态/账户、不调用SIM内部控制器。GetSharedObject必须只返回匹配类型；GetUniqueObject转移所有权且第二次取回为空；GetOwner不延长处理器生命周期。

输入：数字事件、shared/weak/unique对象、owner、handle time；输出：同一事件元数据与类型匹配payload。错误类型返回空，不进行不安全cast。异步调度不在本事件信封实现中；EventHandler仍未实现，完整SIM初始化仍BLOCKED。ffrt仍只有容量所需同步锁适配；不得把信封构建成功写成异步场景已运行。

生命周期：Pointer拥有event；shared payload维持自身所有权；weak payload只在读取时lock；unique payload只允许移动一次；owner为weak_ptr。释放event回收剩余payload。适配测试仅验证传输语义，分类INFRA，不计SIM业务自动化数量。

自检设计：INFRA-EVENT-001类型匹配与错误类型为空；INFRA-EVENT-002唯一对象只移交一次；INFRA-EVENT-003弱payload生命周期和元数据保留。Oracle为以上明确适配协议及实际消费端要求，不是SIM业务Oracle。执行状态单独导出；失败阻止初始化适配向后推进。数据只使用小整数合成payload，不含卡身份。

限制：无真实平台池化/优先级/线程调度/IPC，当前不支持跨线程同时修改同一信封。后续保留真实TelEventHandler/TelEventQueue源码，仅适配外部ffrt与AppExecFwk。不得以重写TelEventHandler绕开生产队列。

自检Oracle：EVT-TYPE=严格类型匹配；EVT-OWNERSHIP=唯一对象移动且只能获取一次；EVT-LIFETIME=弱payload不延长生命周期，元数据保留。来源为本适配层明确协议；不归类为SIM业务CONFIRMED_CONTRACT。
