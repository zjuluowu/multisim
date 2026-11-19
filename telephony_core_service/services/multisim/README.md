# 多 SIM 源码导航

从[开发文档入口](../../docs/README.md)找到设计和迁移路径。下图按[五组目录方案](../../docs/SIM重构代码目录结构设计方案.md)导航。逐文件与 GN 归属见[源码详细设计](../../docs/SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree)，实施和产品门禁见[执行状态](../../docs/SIM_Refactor_Execution_State.md#current-status)。

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
├── test/                      # TEST_F、两个公开测试头、内部Host/Device实现与证据
├── tools/                     # 恢复工具
├── BUILD.gn
└── README.md
```

| 修改任务 | 首查路径及核心类/接口 | 唯一事实Owner |
|---|---|---|
| 插卡/账户发现、绑定、激活 | account/discovery、activation、AccountRegistry、BindingIndex、TopologyService、AccountStore | Account：身份、已接纳绑定、资料、active及激活业务进度；SubscriptionId不等于SlotId |
| 主卡/默认角色/业务重试 | role_assignment/assignment_service.*、AssignmentAggregate、AssignmentStoreAdapter、RoleSelection、primary/PrimarySwitchPlan | Role Assignment：角色意图/分配、冲突准入和业务重试；Primary槽/Radio语义不与Voice/SMS/Data的订阅标识强行合并 |
| 号码 | account/number 的查询/用例和窄运行端口 | Account：账户号码；Card：卡缓存及EF效果 |
| PIN/EF、Profile/Port、会话/加载 | card/security、records、esim、state/SessionRegistry、LoadSession | Card：卡协议、库存、会话代次；Profile绑定归Account、偏好归Role Assignment |
| Radio/资源结束/恢复 | device/resource/ResourceBarrier；primary/radio_execution_port.h为消费端契约 | Device：真实共享设备域与执行资格；不接管账户绑定或角色事实 |
| GetRealSimCount | infrastructure/compatibility/api/real_sim_count_query.*；test/framework/internal/host/g2a_reference中的Core/legacy_sim_query_binding.*测试参考 | 旧Manager/EXT读源投影，无新账户缓存 |
| 宿主/静态槽描述/发布 | infrastructure/assembly 的SlotCatalog、SimHostPorts、ManagerPublicationSequence、SimModule/Factory | Assembly构建只读描述并注入，同一Core RIL及原对象控制块；不持动态业务事实 |
| 请求关联及旧结果交付 | infrastructure/request、compatibility/events | 请求由各Owner实例化；完成/回调登记是技术状态，无业务写权 |

CardRuntime仍同时持SessionRegistry、LoadSession与Device ResourceBarrier、Radio引用。本批仅从旧integration/lifecycle迁入card/runtime并保留类型/成员/时序；它是明确的待拆接缝，不能作为新增能力集中入口。RuntimeResultSinks返回原Owner的结果/证据，不负责Legacy通知，也不决定业务提交。未排空不宣称可卸载，等待结束不等于设备结束。

物理目录与GN target分别治理：保留25个source_set及原sources/public/deps/public_deps/visibility职责，只改必要路径，不合并总target、不扩大public_deps。业务仅依赖对方窄契约，不include具体Service/私有聚合/Adapter；Assembly按装配需要连接实现。AccountStoreAdapter/ActivationHookAdapter及AssignmentStoreAdapter/AssignmentOutputAdapter随Owner，Profile专属legacy_profile_result.h仍在card/esim且属于compatibility_core。

禁止重建顶层common/support/integration/startup/compatibility代码池；Infrastructure不得接管事实、重试或业务调度，Request不得成为中央控制器，身份头不得建立全局映射/IdentityManager。无成熟实现的能力不建空目录。

从Git根验证：

```sh
python3 -B telephony_core_service/services/multisim/test/scripts/run_architecture.py --with-gn --migration --output /tmp/multisim-directory-check
python3 -B telephony_core_service/services/multisim/test/scripts/test.py integrity
python3 -B telephony_core_service/services/multisim/test/cases/architecture/test_g2a_source_tables.py
```

构建和验证入口、可执行场景及 Oracle 索引见[测试导航](test/README.md)。各批测试范围与证据见[执行状态](../../docs/SIM_Refactor_Execution_State.md#current-status)。

跨形态设计约束见[主设计§3.2.1/10.4/17.4](../../docs/SIM_Target_Architecture_Design_v1.2.md)；端点/会话/绑定、能力/可达/可见/角色资格分别表达。这些新增约束仍为提案，不从目录或类型存在推断动态恢复与三/四卡业务已实现。
