# Linux 原生 GoogleMock 场景构造

本次执行目标固定为 Linux 原生 B：受控组件集成测试，无需实体设备。GoogleMock 只构造外部依赖的响应、故障和正式事件输入；被测 ISimManager 与 SIM 业务实现保持真实。服务 SA、IPC、真实权限/capability、Modem 与设备行为不在覆盖范围。

## 当前实现

SIM-COMP-001 沿用完整15项设计与 O-C11/O-L06。输入改为 StrictMock<MockParameterService> 提供 const.telephony.slotCount="1" 和虚拟 modem 开关="false"、const.product.devicetype="phone"；GetParameter 适配桥调用该 mock。参数值是外部配置，不是被测接口的预制返回值。

链路：GoogleTest → 场景装配 GoogleMock → 启动器 → ISimManager → 未修改 SimManager::GetMaxSimCount → GetParameter → GoogleMock 响应 → 生产逻辑返回 → 公共观察断言。

未知参数调用使 StrictMock 报错；未安装场景返回 -ENODEV；非法指针/长度返回 -EINVAL，容量不足返回 -ERANGE。禁止以任意成功值填补未知依赖。查询可重复，Times(AnyNumber()) 不约束调用次数或顺序；业务通过依据仍为公开输出与有界收敛。parameter_boundary_observed 仅用于执行审计。

ScopedParameterService 以 RAII 安装/移除场景，重置计数，禁止嵌套。它仅支持此同步单线程切片；未来真实异步回调需要专门的队列、注销、排空及生命周期设计，不能跨线程直接复用当前安装指针。生产对象在作用域内创建并销毁；不同运行使用独立进程。没有持久化状态或卡身份。

## 后续场景

无卡、Ready、双卡、PIN Locked 的目标同样无需设备：通过真实 RIL/事件边界的 GoogleMock 和受控事件队列输入状态，真实生产模块完成转换，公开接口查询状态/账户。当前尚未实现这些模块的 host 依赖闭包，因此仍 BLOCKED；不能 Mock ISimManager 返回无卡/账户结果冒充集成测试。原服务入口场景独立保持 BLOCKED，不绕过 capability。

## 构建对应

GN googlemock 独立静态目标编译固定 GoogleTest v1.15.2 同一 archive 内的 gmock-all.cc，链接 googletest；使用原固定提交与校验和、BSD-3-Clause、原离线准备脚本。无需新下载、不使用系统未知版本。sim_production_host、platform_boundaries、component_launcher、googlemock、测试源码分别组织。独立 GN 适配不是 ohos_unittest。

负向控制继续只将观测证据改为 -1，验证 O-C11 检查器可失败；不是额外业务用例，不代表参数/RIL故障恢复。所有时序结论只限当前同步查询，有界公共收敛与进程 watchdog；不依赖设备或固定 sleep。
