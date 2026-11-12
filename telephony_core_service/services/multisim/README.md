# 多 SIM 源码导航

当前按[目录设计方案](../../docs/SIM重构代码目录结构设计方案.md)组织为四个业务目录与一个基础支撑目录。完整逐文件迁移、风险、GN对照及实测结果见[本批实施记录](../../docs/SIM_Directory_Migration_20261112.md)，唯一阶段状态见[执行状态](../../docs/SIM_Refactor_Execution_State.md#directory-layout-20261112)。G2a查询已列入原产品源表，其余仍为testonly骨架；成熟业务由 services/sim 执行，产品构建和新业务写权尚未验收。

```text
multisim/
├── account/                   # 账户/绑定/资料/激活，含专属Adapter
├── card/                      # 卡应用/会话/加载/卡侧操作
│   └── runtime/               # 过渡CardRuntime及结果窄出口，职责拆分未完成
├── role_assignment/           # 主卡及默认Voice/SMS/Data分配
├── device/                    # 设备资源与真实执行资格
├── infrastructure/
│   ├── compatibility/api/     # 旧接口门面、只读查询
│   ├── compatibility/events/  # 旧完成、结果/通知交付
│   ├── assembly/exports/      # 模块装配/发布/生命周期及精确导出
│   ├── request/               # Owner自用请求/步骤/完成机制
│   └── sim_identity.h         # 跨业务纯身份类型，无映射服务
├── test/                      # 既有Scenario/Contract/Oracle/Fake及Host设施
├── tools/                     # 恢复工具
├── BUILD.gn
└── README.md
```

| 修改任务 | 首查路径及核心类/接口 | 唯一事实Owner |
|---|---|---|
| 插卡/账户发现、绑定、激活 | account/discovery、activation、AccountRegistry、BindingIndex、TopologyService、AccountStore | Account：身份、已接纳绑定、资料、active及激活业务进度；SubscriptionId不等于SlotId |
| 主卡/默认角色/业务重试 | role_assignment/assignment_service.*、AssignmentAggregate、RoleSelection、primary/PrimarySwitchPlan | Role Assignment：角色意图/分配、冲突准入和业务重试；Primary槽/Radio语义不与Voice/SMS/Data的订阅标识强行合并 |
| 号码 | account/number 的查询/用例和窄运行端口 | Account：账户号码；Card：卡缓存及EF效果 |
| PIN/EF、Profile/Port、会话/加载 | card/security、records、esim、state/SessionRegistry、LoadSession | Card：卡协议、库存、会话代次；Profile绑定归Account、偏好归Role Assignment |
| Radio/资源结束/恢复 | device/resource/ResourceBarrier；primary/radio_execution_port.h为消费端契约 | Device：真实共享设备域与执行资格；不接管账户绑定或角色事实 |
| GetRealSimCount | infrastructure/compatibility/api/real_sim_count_query.*；Core的legacy_sim_query_binding.* | 旧Manager/EXT读源投影，无新账户缓存 |
| 宿主/静态槽描述/发布 | infrastructure/assembly 的SlotCatalog、SimHostPorts、ManagerPublicationSequence、SimModule/Factory | Assembly构建只读描述并注入，同一Core RIL及原对象控制块；不持动态业务事实 |
| 请求关联及旧结果交付 | infrastructure/request、compatibility/events | 请求由各Owner实例化；完成/回调登记是技术状态，无业务写权 |

CardRuntime仍同时持SessionRegistry、LoadSession与Device ResourceBarrier、Radio引用。本批仅从旧integration/lifecycle迁入card/runtime并保留类型/成员/时序；它是明确的待拆接缝，不能作为新增能力集中入口。RuntimeResultSinks返回原Owner的结果/证据，不负责Legacy通知，也不决定业务提交。未排空不宣称可卸载，等待结束不等于设备结束。

物理目录与GN target分别治理：保留25个source_set及原sources/public/deps/public_deps/visibility职责，只改必要路径，不合并总target、不扩大public_deps。业务仅依赖对方窄契约，不include具体Service/私有聚合/Adapter；Assembly按装配需要连接实现。AccountStoreAdapter/ActivationHookAdapter及AssignmentStoreAdapter/AssignmentOutputAdapter随Owner，Profile专属legacy_profile_result.h仍在card/esim且属于compatibility_core。

禁止重建顶层common/support/integration/startup/compatibility代码池；Infrastructure不得接管事实、重试或业务调度，Request不得成为中央控制器，身份头不得建立全局映射/IdentityManager。无成熟实现的能力不建空目录。

从Git根验证：

```sh
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/multisim-directory-check
python3 -B telephony_core_service/services/multisim/test/check_relocation.py
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py
```

覆盖迁移摘要/类型与实现token、精确include/GN依赖、循环/反向依赖拒绝、四组纯规则、编译负控和GN/Ninja对象。Host业务与已知缺陷分开统计，入口见[test/README](test/README.md)。不能将结构检查或Host PASS等同产品链接、原生SA/IPC/HDI或设备功能验收。模块根同名带空格设计文档为已提交原文副本，权威依据使用docs下无空格版本。
