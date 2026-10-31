# 多 SIM 源码骨架

本目录最终完整替代`services/sim/`，目前是未接入生产的骨架。权威边界见[目标主设计](../../docs/SIM_Target_Architecture_Design_v1.2.md)与[源码详细设计](../../docs/SIM_Multisim_Source_Architecture_Detailed_Design.md)。实际树、未来能力与本轮目录裁决见详细设计§3；物理目录不等同逻辑层、namespace、GN target或SO。

生产仍使用 `services/sim/`。本区域的 GN target 全部为 `testonly`，没有加入产品源表、启动或服务发布路径。未定义的业务方法、工厂和销毁方法继续只保留声明。命名空间和原有 C++ 类型、构造参数、成员布局不因目录调整而改名。

## 目录与职责

| 位置 | 当前内容与边界 |
| --- | --- |
| `foundation/` | 身份、只读 SlotCatalog、RequestId/StepRef/EffectKnowledge 纯值；不持账户或动态分配。 |
| `foundation/orchestration/` | 完成/请求协议与 Owner 自有 RuntimeOperationStore 分 target；容器主要方法仍为声明，不签发全局请求、不执行设备或业务重试。 |
| `topology/` | 根部 TopologyService 是唯一账户/绑定/资料接纳入口；discovery、activation、account_profile 内聚账户资料场景；accounts 只保留账户/绑定纯事实规则。 |
| `assignment/` | 根部 AssignmentService 是唯一角色/分配入口；primary 保留应用兼容计划，account_roles 保留联合建行资格；domain 保留纯角色/切换模型。 |
| `runtime/` | session/resource/records 保留原有限规则；PIN、EF、Profile 请求契约分别位于 state、records、euicc；实际 Profile 短编排位于 euicc。 |
| `compatibility/` | 原门面留根部；api 导出宿主接缝；output 保留完成、双结果及通知翻译。 |
| `*/adapters/` | AccountStoreAdapter、AssignmentStoreAdapter、AssignmentOutputAdapter、ActivationHookAdapter 各随所属 Owner；借用原对象，不复制物理 store 或业务缓存。 |
| `assembly/`、`include/` | 完整装配图在私有 sim_module.h；工厂/HostPorts 由两个 forwarding 头精确导出，未安装为公共 API。共享 RIL 与原对象生命周期不变。 |

`accounts/`禁止编排与I/O；`account_profile/`放资料用例及兼容计划。跨模块才导出`api/`，业务字段适配留Owner的`adapters/`，避免在多个同级目录任选落点。

原 `application/`、`model/` 及 T/A 的 `application/ports/` 已撤除。现有五个 Infrastructure 文件均为专属 Adapter，迁回上述 Owner 后暂没有共享技术客户端源码；不为保留目录创建占位文件。`roles`、`monitor`、`radio`、`carrier`、`phonebook`、`sms_storage`、`stk`、`codec` 等待对应成熟功能切片迁入，不以空类填充。

## 按业务任务定位

以下是基于常用术语和源码的模拟定位，非真实用户研究。先读场景文件，沿窄端口找执行方；不要先通读两篇设计。

| 任务 | 首选入口及必要阅读 | 状态Owner/边界 | 验证 |
| --- | --- | --- | --- |
| T1 激活 | `topology/activation/activate_sim_use_case.*`、`activation_progress.*` → `topology_service.*` → api/activation端口 | T账户/active/进度；A准入，R执行；C完成，不改R来代写active | topology、assignment、runtime_protocol；未来真实Hook/force/卫星差分 |
| T2 Primary | `assignment/primary/primary_switch_plan.*` → `assignment_service.*`、`domain/assignment_aggregate.*` → api/radio_execution_port.h | A角色/Primary/业务重试，R资源和设备反馈；T只提供绑定 | assignment、resource、runtime_protocol；未来厂商/三阶段与Data恢复差分 |
| T3 号码 | `topology/account_profile/{set_show_number_use_case,show_number_plan,show_number_query}.*` → accounts/account_registry、api/{account_store,show_number_runtime_port,number_query_host_port} | T账户资料，R records卡缓存/协议；Core提供IMPU，C交付；不改A/资源策略 | topology、runtime_protocol；未来卡→DB→缓存→通知/callback及查询读源差分 |
| T4 eSIM Profile/Port | `runtime/euicc/{local_card_requests.cpp,profile_requests.h}` → runtime/api/local_card_requests.h、session/resource → topology/accounts/binding_index | R设备期望/执行，T身份绑定，A偏好；account_profile不是eSIM入口 | assignment、resource、runtime_protocol、topology；未来双结果/Port映射与失效差分 |
| T5 三卡到四卡 | `foundation/slot_catalog.*` → topology/accounts/binding_index、assignment/domain、assembly/host_ports.h | H静态能力，T绑定，A候选，R真实资源；不是只改数组或只读目录 | topology、assignment、resource；另需宏/TU容量与产品矩阵 |
| T6 硬件流程/RIL边界 | `runtime/{state,records,session,resource}/` → runtime/card_runtime.*、result_sinks.h → 对应activation/primary端口 | R原始观察/会话/执行，T/A接纳业务事实；不改公共基础机制代管业务 | 四套规则及GN；未来READY/LOADED/迟到、错误/完成节点与RIL差分 |

当前PIN/EF主要是契约，radio、phonebook、sms_storage、stk、carrier等成熟能力仍在`services/sim/`；没有文件时从详细设计§17的生产来源表进入，不能推断能力已经迁入。T1/T3业务方法仍主要为声明/伪代码，测试只覆盖已有有限规则。

## 文件映射与保持项

上轮迁移输入为 **73 文件（50 头、23 源）**：48 文件改变路径，25 文件保留主落点，其中 6 文件按现有职责拆取。本轮以main@38c1f232的88个实际文件为输入，10个C++文件改路径（accounts 4、account_profile 6），其余主路径保留；完整映射见同一JSON的layout_revision。当前有 **86 个 C++ 文件（63 头、23 源）**；增加的 13 个头用于单一定义的纯类型、能力请求、独立 Adapter、窄入口和导出视图，没有新增设备执行者。

[source_migration.json](../../test/multisim_architecture_rules/source_migration.json) 逐项保存全部 73 个原文件的 SHA256、目标、职责、实现状态、动作及 GN 影响。`--migration` 核对 Git 基线文件集合、每个去向、原类型定义/成员顺序和原 23 个 cpp 的实现 token 顺序；移出的定义仍只有一份。两处原调用表达式改为 API 委托，检查器明确列出该替换。这是结构完整性证据，不是产品 ABI 或生产行为等价证明。

T account_profile 与 R euicc 分别拥有账户资料和卡侧执行；A primary 与 R radio 分别拥有业务计划/重试和设备协议。LocalCardRequests 的原类与构造保留在 runtime/api，PIN/EF 方法仍只声明；唯一已实现的 Profile 短编排保持“登记 → 原执行 → 原节点交 A 偏好 → 结束等待”，不增加统一 access 网关或新的序号签发器。

C 继续引用原入口对象，通过 `topology/api/legacy_entry.h`、`assignment/api/legacy_entry.h` 中少量具名函数调用已存在的接缝。具体方法仍在对应 Owner；尚未实现的激活/Primary 业务方法没有补成功桩。此处不新增对象、状态、虚表或运行期 Facade。

## 验证入口

从 Git 仓库根执行；需要 Python 3.9+ 和支持 C++17 的编译器。

```sh
# 默认：include/target 边界、护栏负控、四套纯规则的编译/链接/运行
python3 telephony_core_service/test/multisim_architecture_rules/run.py

# 结构完整性需要迁移基线的 Git 历史
python3 telephony_core_service/test/multisim_architecture_rules/check_structure.py --migration

# 增加每个头/源独立语法、合并头 TU、真实 GN check 与全部骨架对象构建
# 另需 GN/Ninja；结果默认在临时目录，也可用 --output 指定仓库外路径
python3 telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration
```

`BUILD.gn` 区分纯值、请求协议/机制、Owner types/contracts/domain/cases/adapters、R 治理和装配；有限的应用计划仍属于 Application。它没有创建 SO、SA、进程或第二套生产服务。GN public/visibility/check_includes 与文本扫描共同约束边界，构建入口覆盖每个源集合。

检查器核对所有 quoted include、受限标准库 angle include、跨 target 头白名单与可达依赖、文件/target 环及关键 Owner 类型的单一定义；Foundation 不反向依赖业务，Domain 不依赖编排或平台，T/A 不包含对方私有实现，R 不包含 T/A 具体用例或 C。续行先按 C++ 规则拼接；宏 include、GN 动态/重复列表等未支持语法明确失败，避免静默跳过。护栏测试在临时副本中实际注入逆依赖、环、漏挂目标和重复 Owner，不修改源码。

CleanArchCLI 不在本轮环境中，以上可复现检查替代其依赖扫描范围；它们不分析全部 C++ 语义、平台写点、设备效果或并发时序。GN 只编译骨架对象；未定义业务方法使完整骨架服务尚不能作为生产产物链接。四套纯规则的链接/运行单独验收，不能由对象编译推导产品服务通过。

## 后续切片

CardRuntime 仍暂存单套 SessionRegistry、ResourceBarrier、加载及请求关联，没有完成稳态薄化；成熟卡协议和数据仍在原 SIM 实现。各能力内实际写点、provider、真实事件桥、排空及产品 Feature 矩阵尚需后续迁移与差分验证。

优先在独立工作分支将已有 G2a `GetRealSimCount` 只读闭包适配到新结构，再完成匹配 OHOS 产品树的原 SO 构建及初始化/发布/只读冒烟。它仍受[执行状态](../../docs/SIM_Refactor_Execution_State.md)中的合入门禁约束；本轮没有合入 G2a 或转移生产写权。
