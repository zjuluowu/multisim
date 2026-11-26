# AF-2 M9 架构基础交付验收记录

**实施检查点。** 输入 `bf7d0c689f7e12320c864a6866a2c8d92d0bd660`。
初次 Radio／工程成果为 `7264a3940595fa206c61583315338caae4ba269b`。
主体回归和目录实测候选为 `b6f6c25eed8b690da53f5b86ef3e7883cbe46916`。
后续完成 Submit 结果归属和 C1 同步回包修订。
[唯一执行状态](SIM_Refactor_Execution_State.md#af2-m9-status) ·
[开发指导 v2.2](SIM_Global_Code_Migration_and_Test_Plan_v2.2.md) ·
[原始命令和证据](../services/multisim/test/docs/evidence/af2_m9_20261126/README.md)

| 子阶段 | 实施及核对 | 状态 |
|---|---|---|
| M9.1 | 修复成员重复 Submit、部分未发提前归还资源、拒绝投递错删回执。预身份应用独立于账户发现 | B/C1 真实成员、路由、全图关闭收据未结 |
| M9.2 | 继承 M2～M8 样板。定位 Legacy UnlockPin 和 ICCID 解码入口 | UnlockPin、ICCID EF 至 Records、正式 B/C1 样板待补 |
| M9.3 | Legacy 根产品与 Host 共用 SIM 源定义，Host Legacy 拆分 archive，Target 独立 archive。54 个 Legacy 源及 eSIM 条件顺序核对 | 产品构建待环境 |
| M9.4 | Target CPP 自动发现，M6 两端复用操作序列、原返回及空名称断言 | 其他共同样板继续归一 |
| M9.5 | 复用 CLI、构建、LLDB 与自动化围栏 | 人工和技术分别验收 |
| M9.6 | 逐文件统计和 SIM/WU/P 候选台账 | 逐项证据、独立开发、正式签收未结 |

## 历史十项门禁记录

| Gate | 结果 | 实际证据及剩余事项 | 验收责任 |
|---|---|---|---|
| H01 | FAIL | 成员、投递、同步重入及 C1 成员快照负控已修复。B/C1 真实成员来源及 A 锁外逐步交接欠缺 | 架构师／Owner |
| H02 | FAIL | 已验证会话前置及成员配对。正式图 B/C1、PIN/EF、Refresh、全图关闭与重建待验证 | 架构师／测试负责人 |
| H03 | BLOCKED | M1～M8 调试记录可用。缺 UnlockPin、ICCID EF 和正式 B/C1 | 业务 Owner |
| H04 | BLOCKED | Host 独立链接及源表对照。缺匹配 OHOS 树和 hb，产品独立库及条件源未验收 | 构建负责人 |
| H05 | FAIL | 一个共同样板复用体，Target CPP 自动入 GN。其余仍有双实现业务测试体 | 测试负责人 |
| H06 | PENDING_MANUAL_ACCEPTANCE | CLI 技术证据独立保存，仍需真实 GUI Run、Debug、刷新、生产断点 | 开发组长／测试负责人 |
| H07 | FAIL | GN 与依赖围栏、Radio 正反运行有证据。构建遗漏负控已验证。全图卡态／Refresh／关闭仍需补齐 | 架构师／测试负责人 |
| H08 | FAIL | 62 个 SIM、9 个 WU、32 个 P 已建候选台账。入口、语义去重、认领及核销需逐项复核 | 架构师／组长 |
| H09 | PENDING_MANUAL_ACCEPTANCE | 下表记录证据责任和保留写权。外部负责人仍需认领 | 产品／外部接口负责人 |
| H10 | BLOCKED | 两项样板和非框架作者证据欠缺 | 开发组长／架构师 |

## 已完成的候选验证

Legacy 147/147、Target 171/171，M1～M8 与本轮负控执行。Legacy CI 和真实执行审计通过。
ASan、TSan 各 73/73。CD 独立回归 Legacy 2/2、Target 8/8。12 个共同契约场景
24/24，Legacy/Target 和 Oracle 差异均为零。独立 worktree 的 Target 冷跑、热跑（直接复用
ELF）、新增单 CPP 增量分别为 57.66、0.51、23.97 秒。LLDB 单步实际观察预身份应用、
原代次、TopologyService 发现和 Stop，过程退出 0。标准库字符串格式器有摘要解析错误。
后续修订直接返回每笔 Radio 交接结果。基线在同步回调重入时误报首笔未发，复验恢复
首笔成功、第二笔未发，原结果只投递一次。C1 修订后 Target 173/173，ASan 和 TSan 各 128/128，目标 B/C1 组件执行审计通过。

## 独立反向审查

独立 Agent 在冻结基线用真实资源运行代码复现两个竞态。原 P 发送两次，Q 未发时 K 提前
可用，原 P 序列丢失。修订后 P、Q 各一次，竞争请求零发令，P 可继续核对。投递负控基线
交付为 2、1、1，修订后为 2、1。原源码、失败输出和复验均已归档。

反查确认正式 SimModule 的 B/C1 尚未执行，旧 A 仍用目录和 Binding 代次构造成员及会话。
独立探针确认 C1 同步重入撤销原记录后旧循环崩溃。修订后每轮按值提交，核对原步骤，
6 次发令且单次完成。真实成员来源和锁外交接仍未接通。
LoadSession 资格逻辑仍待实际 EF 路径消费。遗漏源码负控另复现 archive 残留旧对象，修订
归档命令后实际链接失败，恢复源码则通过。附加 B/C1 执行审计曾记录 Debug inline 实例计数为零。终态提交集中至生产翻译单元后通过。所有技术门禁闭合后进行最终独立反审。

## 产品阻塞和责任

| 责任 | 待取证事项 | 可独立推进及切换边界 |
|---|---|---|
| Provider／T、A | 原写归因、读取因果、重建前驱 | Host 字段与 Reload 可继续。WU-01、02、03、05 保留原 writer |
| Device／R、G | RIL/HDI 原成员、结束和复用、原生 MSISDN | 受控 Host 协议及隔离可继续。WU-04、06、07 需实证 |
| 产品 H、C、构建 | Core/SA/IPC、共享 RIL、OHOS 构建、SO/ABI/CFI 及驻留 | Host 装配和构建可继续。WU-07、09 保留原重试及出口 |
| eSIM／F、外部平台 | 66250、eSIM/vSIM、通信共享、卫星及 EXT | EF/codec 可独立开展。WU-08 按实际共享资源和调用端验收 |

**前批记录的验收口径。** 在唯一执行状态记录冻结 SHA、H01～H10 的正反证据、真实 GUI、
两项独立演练的 PR 与实际开发者、最终反审和负责人确认。


## 架构基础交付

本批输入 `411b10672f8e119daa1ad21c9afd9984da99fe44`。B01 的完整 Host Legacy 归档包含
51 个原实现 CPP，Primary 与 Bootstrap 共同链接此归档。Target 归档仍独立编入 25 个 CPP。
生成 Host GN 图确认两个归档均为非 testonly。Legacy 和 Target 的 ELF 及代表生产符号
已实际核对。产品根入口继续使用 Legacy，OHOS 原生构建仍缺匹配环境。
[归档及 ELF](../services/multisim/test/docs/evidence/af2_m9_foundation_20261126/elf-and-archive.log)


### 当前 B01～B08

| 任务 | 证据和实际范围 | 当前结果 |
|---|---|---|
| B01 | Legacy 51 CPP 与 Target 25 CPP；独立 ELF、GN check、实际遗漏/恢复 | Host PASS；OHOS 产品 BLOCKED |
| B02 | 八个共用源码业务路径；Both 12×2、Oracle 差异 0；Reload 两端正向通过 | 稀疏 simId 已批准按 CD-04 执行独立契约 |
| B03 | 原 PIN 接入点、状态/剩余次数契约，已有 Legacy 回归。Target 只有预身份会话 | 原 PIN 传输、资源及完成责任缺证，局部 BLOCKED |
| B04 | 原 EF 解码链、LoadSession 纯规则、预身份发现 | 真实 EF 发令与 Records 接纳待接，局部 BLOCKED |
| B05 | 独立 worktree、CLI、增量、自动定位及两端 LLDB | CLI PASS；GUI 未执行 |
| B06 | 生产依赖、两端隔离、源码遗漏、实际链接、共用体依赖和冻结资产 | 静态 PASS |
| B07 | 实际枚举、源码位置、编译与受审计的方法、SIM/WU/P 对账 | 未归类业务语义和剩余预算尚未冻结 |
| B08 | B/C1、锁外交接、会话与 LoadScope、全图关闭和跨实例反例输入 | 已准备，组合验证未执行 |

### Architecture Ready 当前技术检查

执行 [AD-01～AD-04](SIM_Target_Architecture_Design_v1.2.md#architecture-ready-decisions)。
下表保留基础工程阶段的实际证据边界，当前批进度见唯一执行状态。

| 检查 | Host 完成条件 | 基础批证据 |
|---|---|---|
| H01 | 正式 SimModule 对象图、核心职责和唯一写权，包括 B/C1、真实成员及锁外交接 | FAIL，正式 B/C1 尚未装配 |
| H02 | 卡应用会话、公共请求与资源、Refresh、关闭收据和跨实例重建 | PARTIAL，已有组件机制及预身份会话 |
| H03 | 九类样板通过正式生产入口执行 | PARTIAL，缺正式 B/C1、UnlockPin 和 ICCID EF |
| H04 | 独立 Legacy/Target Host 生产库，源码归属、逻辑 GN 与真实链接核对 | PASS，基础候选归档和 ELF 已核对 |
| H05 | 适用的业务测试共用，兼容差异契约独立，业务与机制用例可追溯 | PARTIAL，八个共用源码路径与 12 个冻结 Oracle 场景已验证 |
| H06 | 双端 CLI、精确单例、生产与测试源码发现、增量、LLDB 和 VS Code 指导 | PARTIAL，基础候选已实测 CLI、自动枚举及两端 LLDB，GUI_NOT_RUN |
| H07 | GN、静态护栏及全图场景的确定性反例和运行时负控 | PARTIAL，静态与组件负控已验证 |
| H08 | 剩余功能、源码及依赖、验收、代码与测试预算实质对账 | PARTIAL，已有候选台账及实测数据 |
| H09 | 记载独立产品门禁及禁止提前切换条件 | PASS。原生 Provider、设备、Feature 矩阵及生产切换独立验证 |
| H10 | 两项独立 Agent 扩展及同候选组合回归 | NOT_RUN，依赖稳定正式 PIN 和 ICCID EF 样板 |

本批达到 `M9_FOUNDATION_READY_FOR_DEEP_REVIEW`。全局保持 `AF2_M9_IN_PROGRESS`。
普通 Verify 已验证 Legacy 调试，Target 另有 Developer；Legacy 无 LTO 的产品专属符号、
GUI 与原生产品均在其证据边界内。真实 PIN/EF 接入和全图时序需要下一阶段验证。

最终候选全量 Legacy 148/148、Target 174/174。12 个冻结 Oracle 共 24/24，差异为零。
另有一项连续 ID Reload 共用断言在两端通过。Legacy 稀疏历史 ID 的差异保留实测。
Legacy CI（含变异与恢复）为新增 Reload 前的 147 登记基线。ASan/TSan 各 130/130
为稀疏探针阶段专项。两个后端 LLDB、源码定位 322 个登记及归档遗漏负控已执行。
[原始证据及下阶段反例](../services/multisim/test/docs/evidence/af2_m9_foundation_20261126/README.md)
