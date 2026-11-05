# 多 SIM 源码导航

修订 B+ 六组已由用户选为本轮源码结构实施基线。main 已按用户授权包含 G2a 真实只读查询及旧对象附着；其余能力仍为 testonly 骨架，成熟 SIM 读源和业务实现仍由原 `services/sim/` 承担，产品准入未完成。最终由 multisim 完整替代旧实现，不长期双执行。

| 目录 | 当前代码与责任 |
| --- | --- |
| account | T：账户、绑定、资料接纳；discovery/activation/number 用例及专属契约。纯规则仍由 topology_domain target 隔离。 |
| card_selection | A：角色、Primary、业务冲突/重试；primary 计划。真实设备租约不属于 A。 |
| card/state | 卡应用 SessionRegistry：应用代次、期望转换与旧结果资格；不是通用 Modem Session。 |
| card/security、records、esim | PIN/EF/Profile 契约、LoadSession 与本地 Profile 短请求；成熟协议尚未迁入。 |
| device/resource | ResourceBarrier 纯规则；物理域/设备结束适配尚未接入。当前没有 device/session 或成熟 Radio 实现。 |
| integration/api、events | 原兼容门面、共享完成/事件交付；real_sim_count_query.* 为 G2a 旧读源投影。Profile 专属结果在 card/esim。 |
| integration/lifecycle | HostPorts/私有装配；CardRuntime/result_sinks 为过渡接缝，保留对象/状态，职责拆分另验。exports 保留两个仓内精确转发视图，非公共 ABI。 |
| support、support/request | 身份/关联纯值、H 构建的只读 SlotCatalog，以及各 Owner 自有请求机制；不持业务事实或资源租约。 |

Phonebook、STK、Carrier、Radio、profile/channel/provisioning 等完整生产能力尚未迁入；稳态树见[源码详细设计](../../docs/SIM_Multisim_Source_Architecture_Detailed_Design.md)。不创建空目录。

| 业务任务 | 首查入口 | Owner / 验证 |
| --- | --- | --- |
| GetRealSimCount | integration/api/real_sim_count_query.*、Core 的 legacy_sim_query_binding.* | C 只读投影/H 旧对象桥；[真实七场景主机测试](../../test/systemtest/README.md#g2a-bplus)；非账户事实计算 |
| 插入/发现/激活 | account/discovery、account/activation、topology_service.* | T 接纳，A 准入，R 执行；topology/assignment/runtime_protocol 规则 |
| 默认卡/Primary | card_selection/assignment_service.*、primary/ | A 唯一角色/业务重试；assignment/resource 规则 |
| 号码/MSISDN | account/number/ | T 账户资料，R 卡缓存/EF；真实卡→DB→两缓存→通知差分仍待生产切片 |
| PIN/EF | card/security/pin_requests.h、card/records/ef_requests.h | 目前为契约，不是成熟协议实现 |
| eSIM Profile/Port | card/esim/local_card_requests.*、profile_requests.h | 卡协议/期望归 R，绑定归 T、偏好归 A；assignment/runtime_protocol/resource 规则 |
| 四卡扩展 | support/slot_catalog.*、account/binding_index.* | H 描述、T 动态绑定、A 策略、R 真实域；不只改数组 |
| Modem/RIL和迟到结果 | card/state/session_registry.*、device/resource/、integration/lifecycle/card_runtime.* | 卡会话与 Radio 协议会话不同；Card/Device 非强制转发链 |

T/A 跨 Owner 仅通过窄契约；O 是各用例自有机制。卡能力可使用必要的窄硬件接口，不增加 CardManager/DeviceManager。等待结束不等于设备结束，本地清理不构成资源可重用证据。CardRuntime 仍持多种运行状态，本轮只移动，不宣称已完成职责拆分。

从仓库根运行：

```sh
python3 -B telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/multisim-bplus-check
```

覆盖显式逻辑源集合/依赖、迁移溯源、严格语法、四套纯规则、负例及 GN/Ninja 对象；不替代产品链接、原 Manager/Core 初始化、IPC/SA 或设备验证。逻辑 target 不按六目录合并。

[主设计：业务不变量](../../docs/SIM_Target_Architecture_Design_v1.2.md) · [执行状态：阶段与证据](../../docs/SIM_Refactor_Execution_State.md) · [架构检查](../../test/multisim_architecture_rules/README.md)

G2a 调用链为 `CoreServiceSim → LegacySimQueryBinding → RealSimCountQuery → RealSimCountHostPort → 原 SimManager/EXT`。Core 宏仍在原翻译单元求值，绑定保存原 Manager/RIL 的 shared_ptr 控制块，不创建第二生产实例；顺序替换/清理会使旧绑定失效，不承诺并发热替换。代码来源为历史检查点，主机新证据见[执行状态 §3.4](../../docs/SIM_Refactor_Execution_State.md#g2a-bplus-m1)。

当前共90文件（64头/24cpp及BUILD/README）。原24个逻辑 source_set 不变，新增无业务依赖的 `readonly_query` testonly 叶子用于骨架验证；根产品及三份 Core 显式测试源表直接列入同一 query/binding 实现，不依赖 testonly 总骨架，也不在同一二进制重复编译。已合入 main 的源表接入仍待产品验证，主机 PASS 不代表原 SO/Feature/初始化发布或产品准入已通过。其余成熟 SIM 能力、目标工厂和写权仍未迁入。

2026-11-05 G2a 复验进一步收窄：上述 Core 显式源表及主机 target 仅增加 `integration/api` 的头搜索路径，`readonly_query` 不再导出全骨架 include 配置，其他24个逻辑 target 不变。Query 仍只是原 Manager/EXT 的同步只读投影；没有 Account/T/A 依赖或额外状态。当前/历史七场景均重新构建通过，新的函数审计、源表负例与产品阻塞见[执行状态 §10](../../docs/SIM_Refactor_Execution_State.md#g2a-revalidation-20261105)，不改写 §3.4 的历史结果。
