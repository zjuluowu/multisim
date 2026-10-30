# 多 SIM 源码骨架

本目录落实 `main@18e724f7` 中的[目标主设计](../../docs/SIM_Target_Architecture_Design_v1.2.md) §6.6 和[源码详细设计](../../docs/SIM_Multisim_Source_Architecture_Detailed_Design.md) §3.2、§4。两篇设计保留原提交内容；其中的现状清单、旧路径链接和“尚未实施”描述属于该设计提交的基线快照，当前导航以本文件为准。

生产仍使用 `services/sim/`。本区域的 GN target 全部为 `testonly`，没有加入产品源表、启动或服务发布路径。未定义的业务方法、工厂和销毁方法继续只保留声明。命名空间和原有 C++ 类型、构造参数、成员布局不因目录调整而改名。

## 目录与职责

| 位置 | 当前内容与边界 |
| --- | --- |
| `foundation/` | 身份、只读 SlotCatalog、RequestId/StepRef/EffectKnowledge 纯值；不持账户或动态分配。 |
| `foundation/orchestration/` | 完成/请求协议与 Owner 自有 RuntimeOperationStore 分 target；容器主要方法仍为声明，不签发全局请求、不执行设备或业务重试。 |
| `topology/` | 根部 TopologyService 是唯一账户/绑定/资料接纳入口；discovery、activation、profile 内聚现有场景；domain 保留纯规则。 |
| `assignment/` | 根部 AssignmentService 是唯一角色/分配入口；primary 保留应用兼容计划，account_roles 保留联合建行资格；domain 保留纯角色/切换模型。 |
| `runtime/` | session/resource/records 保留原有限规则；PIN、EF、Profile 请求契约分别位于 state、records、euicc；实际 Profile 短编排位于 euicc。 |
| `compatibility/` | 原门面留根部；api 导出宿主接缝；output 保留完成、双结果及通知翻译。 |
| `*/adapters/` | AccountStoreAdapter、AssignmentStoreAdapter、AssignmentOutputAdapter、ActivationHookAdapter 各随所属 Owner；借用原对象，不复制物理 store 或业务缓存。 |
| `assembly/`、`include/` | 完整装配图在私有 sim_module.h；工厂/HostPorts 由两个 forwarding 头精确导出，未安装为公共 API。共享 RIL 与原对象生命周期不变。 |

原 `application/`、`model/` 及 T/A 的 `application/ports/` 已撤除。现有五个 Infrastructure 文件均为专属 Adapter，迁回上述 Owner 后暂没有共享技术客户端源码；不为保留目录创建占位文件。`mapping`、`roles`、`monitor`、`radio`、`carrier`、`phonebook`、`sms_storage`、`stk`、`codec` 等待对应成熟功能切片迁入，不以空类填充。

## 文件映射与保持项

输入实际为 **73 文件（50 头、23 源）**：48 文件改变路径，25 文件保留主落点，其中 6 文件按现有职责拆取。当前有 **86 个 C++ 文件（63 头、23 源）**；增加的 13 个头用于单一定义的纯类型、能力请求、独立 Adapter、窄入口和导出视图，没有新增设备执行者。

[source_migration.json](../../test/multisim_architecture_rules/source_migration.json) 逐项保存全部 73 个原文件的 SHA256、目标、职责、实现状态、动作及 GN 影响。`--migration` 核对 Git 基线文件集合、每个去向、原类型定义/成员顺序和原 23 个 cpp 的实现 token 顺序；移出的定义仍只有一份。两处原调用表达式改为 API 委托，检查器明确列出该替换。这是结构完整性证据，不是产品 ABI 或生产行为等价证明。

T profile 与 R euicc 分别拥有账户资料和卡侧执行；A primary 与 R radio 分别拥有业务计划/重试和设备协议。LocalCardRequests 的原类与构造保留在 runtime/api，PIN/EF 方法仍只声明；唯一已实现的 Profile 短编排保持“登记 → 原执行 → 原节点交 A 偏好 → 结束等待”，不增加统一 access 网关或新的序号签发器。

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
