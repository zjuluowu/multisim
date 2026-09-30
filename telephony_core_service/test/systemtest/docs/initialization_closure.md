# Linux 状态/账户初始化依赖闭包审计

## 目标与设计先行

目标为 SIM-B-002 的组件版本：Linux 原生进程公开构造 SimManager，以 ISimManager::OnInit(1) 装配真实模块，GoogleMock ITelRilManager 注册/请求边界经正式 InnerEvent 响应提供 ICC_CARD_ABSENT。以 ISimManager::GetSimState、HasSimCard、GetSimAccountInfo、GetActiveSimAccountInfoList 观察；不读取内部成员或数据库。

原设计15项保持；入口修订为 ISimManager。无卡状态必须来自正式 RIL 数据，不能以未初始化、0个配置Slot、空Mock账户替代。强制状态断言 O-C01；账户错误码/保留行为 O-Q10/O-L02 待真实采集。其他拓扑与后续Ready/Locked/双卡继续沿用原模板，均在初始化闭包成立之后实施。

## FACT / SOURCE_EVIDENCE

工作仓 HEAD 2841c6f60635853a929c1755dc08f5543a89eb83。

- services/sim/src/sim_manager.cpp:41–51：OnInit 不仅创建 SimStateManager；还创建 OperatorConfigHisysevent、CardFileDetectManager 并调用多卡/单卡初始化。
- 同文件:54–97、:119–150：每Slot创建状态、文件、账户、短信、号码簿、STK生产对象，并提交 RefreshSimState 任务；创建 MultiSimController/Monitor。
- services/sim/src/sim_state_handle.cpp:145–180：正式 RIL RegisterCoreNotify 订阅状态/Radio事件，注册后建立 observer。
- 同文件 ProcessEvent/GetSimCardData/ProcessIccCardState：事件解析和状态更新属于被测生产逻辑，不能用桩重写。
- services/sim/src/multi_sim_controller.cpp:103–156：控制器构造/Init需要真实RadioProtocolController、SimRdbHelper、MultiSimHelper与平台参数。

## 编译/链接探针

`sim_initialization_link_probe` 是仅用于依赖审计的可选目标，不是 GoogleTest、不执行或输出PASS。它沿用同Linux GN工具链，将当前生产BUILD.gn的无条件SIM源列表放在独立 `sim_initialization_production_host` target。列表复用此前静态候选清单，不宣称完整链接闭包。没有修改或复制生产源码；ESIM条件分支未启用。业务实现缺失必须添加真实源，不能以Mock实现替代。

探针启动代码仅引用公开OnInit和组件查询；生命周期为公开构造/具体类型析构。因为尚未装配受控 RIL/平台环境，禁止运行此二进制，即使偶然链接成功也不能判场景通过。运行器只执行 GN/Ninja，并输出审计JSON；失败为BLOCKED、executed=false、无GoogleTest XML。

## 环境限制与决策

当前compile-only兼容声明仅支撑容量查询，不能代表事件、DataShare或StateRegistry运行能力。优先寻找匹配版本真实头文件；缺失项在编译审计结果中记录。只有接口来源与支持范围核实后才能做Linux兼容声明。后续适配顺序：真实事件传输/可调度队列 → 正式RIL Mock → DataShare与状态注册/公共事件外部Mock → 未修改状态/账户业务闭包 → 无卡垂直切片 → Ready/Locked/双卡。

事件层只负责存储与传送payload、owner、event id、取消与时间调度，不判断SIM状态或生成账户。外部Mock不得断言精确请求次数。持久化采用临时命名空间，结束撤销订阅/排空并清理；未实现前相关场景持续BLOCKED。

真实服务入口、capability及A层级仍不在此次组件实现覆盖范围。设备不是B的前置条件，匹配外部接口和host运行适配才是。
