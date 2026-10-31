---
document_id: SIM-TGT-001
title: SIM 目标架构设计
version: "1.2"
status: draft
authority: 本轮维护的目标技术架构主文档；不覆盖已发布或已批准业务契约；与既有功能/迁移/测试文档的职责见 §1.2
baseline_commit: 964dc5693badc08ce4abafd70d22f1dcd47cf727
baseline_branch: main
revision: "2026-10-30-directory-adjudication-07"
document_input_commit: 964dc5693badc08ce4abafd70d22f1dcd47cf727
consolidated_branch_input: 2448b42e8976ffb75e2bfcd45b27c86eb1e3965d
claude_revision_commit: 1afffa4e0e48874b91edbc96430b722a267650eb
source_evidence_commit: 6017003eff50c1bc8c5dfa72336a4bfe42e948b0
previous_baseline_unresolved: 00b3faa6b02ac0f1adf77fb47f4a111cd95e665d
revision_date: 2026-10-30
owner: OPEN
approval_status: pending
upstream:
  - SIM-DDD-001
  - SIM-ARCH-001
  - SIM-IDEAL-001
  - SIM-BND-001
  - SIM-EVID-001
  - SIM-FUNC-001
  - SIM-SA-001
downstream:
  - SIM-MIG-001
  - SIM-TEST-001
reference_resolution: 上述原引用 ID 尚未定位到对应独立文档；实际文件、职责和限制见 §1.2
supersedes:
  - SIM-TGT-001 v1.1
  - SIM-TGT-001 v1.0
supersession_note: 原对缺失 SIM-IDEAL-001 章节的替代/保留声明未核实，已撤回；不据本文宣称上游批准失效
last_verified: 2026-10-30
verification_scope: 本轮局部物理目录调整及骨架验证；未运行生产构建/设备
verification_note: 本轮目录裁决输入为本地main e295540fe81ca7dd28f248d0b76470497062aa64；早期baseline/document_input字段及§20历史记录保留。当前73骨架文件与116个原SIM头/源重新统计，未将历史PASS晋升为本轮结果；本轮检查见§20.7。
---

# SIM 目标架构设计

## 1. 文档定位与决策状态

### 1.1 目标与权威范围

本文是本次维护的目标架构主文档，回答上下文与聚合边界、唯一写权、分层、部署、兼容可见性、异步关联及恢复如何落地。保留 `SIM-TGT-001` 和原路径，收敛已有设计，不建立另一套功能编号或平行实现。SO 拆分是工程手段，不等于领域重构完成。

本文管理目标技术结构与协议摘要；领域业务契约、公开接口、详细迁移清单和测试用例仍由各自来源维护。**已发布或已批准的业务契约约束行为，固定源码/构建/配置证明现状，本文提出未来方案，历史材料解释背景。** 若现状与已确认契约冲突，记录缺陷及独立行为变更，不把源码缺陷永久化，也不在机械重构中悄然修复。

仓库 README 仍将《SIM 架构重构设计》称为架构主规范；本轮按任务指定以本文收敛目标技术设计，原文继续承载稳定功能清单、写权清单、迁移和既有证据。这里是明确的文档职责建议，不是仓库审批已完成的声明；未改 README 或其他文件，也不宣称废止缺失的上游契约。跨文档有冲突时，以本节来源性质及 §21 的具体裁决判断，不能按文件日期或“统一版”字样判断。

### 1.2 文档关系与实际定位

下表是在本仓 docs、索引及 front matter 检索后读取相关正文得到的关系。原稿引用的 `SIM-DDD-001 / SIM-ARCH-001 / SIM-IDEAL-001 / SIM-BND-001 / SIM-EVID-001 / SIM-FUNC-001 / SIM-SA-001 / SIM-MIG-001 / SIM-TEST-001` **均未定位到对应 document_id 的独立文档**；不把相近文件擅自重命名为这些权威源。

| 职责 / 原引用 | 实际可用材料 | 本文采用范围及限制 |
|---|---|---|
| 领域、统一语言 / SIM-DDD-001 | [DDD 导读](MultiSIM/04_ddd_target_architecture.md) §2～5；[V3 SAD](SIM_Refactored_Software_Architecture_Design_Version3.md)，实际 ID 为 SIM-SAD-001；[重构设计](SIM_Architecture_Refactor_Design.md#target-contexts) | 分类目标、身份/位置分离和单写者作为设计输入；未发现足以批准全部业务不变量的记录，不引用未经核对的 SIM-INV 编号 |
| 边界、SA、外部契约 / SIM-BND-001、SIM-SA-001 | [边界约束](knowledge/routing/repo-boundary.md)、[eSIM 专项](MultiSIM/03_esim_slot_and_sa_boundary.md)、[接口说明](SIM管理对外接口说明_源码核实版.md)；本仓 IDL、Core 入口及服务配置 | 区分本仓卡侧接口、66250 任务接口及部署配置；接口声明不证明服务实现、provider 事务或设备运行 |
| 现状与证据 / SIM-ARCH-001、SIM-EVID-001 | [旧现状基线](MultiSIM/01_sim_subsystem_baseline.md)、[演进分析](MultiSIM/02_current_architecture_and_evolution.md)、[重构设计来源](SIM_Architecture_Refactor_Design.md#document-sources) | 仅作定位与历史比较；本次结论重新读取固定提交，证据在 §20.2 |
| 理想、分层、类设计 / SIM-IDEAL-001 | [分层与迁移设计](SIM_Architecture_Layering_and_Migration_Design.md)、[V3 类设计](SIM_Layered_Architecture_Class_Design_Version3.md)，实际 ID 为 SIM-CLASS-001；[评估与优化设计](多SIM架构评估与优化设计.md) §3～5 | 保留有效职责；裁决“四业务 Context”、Coordinator 入 Domain、统一提交与持久日志等冲突。原 supersedes 对 SIM-IDEAL 章节的声明因源缺失撤回为未核实历史引用 |
| 功能及迁移 / SIM-FUNC-001、SIM-MIG-001 | [SIM-001～062 及子项](SIM_Architecture_Refactor_Design.md#feature-mapping)、[WU-01～09](SIM_Architecture_Refactor_Design.md#writer-cutover)、[迁移阶段](SIM_Architecture_Refactor_Design.md#migration-stages) | 沿用稳定功能 ID 和写权单元；本文 §6 / §22 仅给架构映射与门禁摘要，不复制完整迁移计划 |
| 测试 / SIM-TEST-001 | [systemtest README](../test/systemtest/README.md)、[初始化闭包](../test/systemtest/docs/initialization_closure.md)、[Oracle](../test/systemtest/docs/oracle_catalog.md)、[执行记录](../test/systemtest/docs/run_record.md) | 各自是设计、缺口或历史执行记录；本次不运行或晋升业务测试结果。基线 3fd0f815 已含 489c88fe 提交的测试设施更新，提交只说明设施存在 |
| 源码组织详细设计 | [源码结构与功能详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md#target-tree) §3/4/17/21 | 本文定义边界和依赖不变量；详细设计维护唯一完整目标目录树、当前文件清单和生产功能落点。目录推荐仍为PROPOSED |
| 目录评审输入 | [Opus](SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md)、[Gemini](SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md) | 同等地位的只读建议；融合裁决见详细设计§4.3，不据模型名称或历史统计决定方案 |
| 历史冲突与规则 | [2026-10-15 裁决](MultiSIM/08_design_unification_decisions.md)、[历史索引](history/pre_unification_20261015/README.md)、[仓库约束](knowledge/expert/constraints.md) | DU 等是历史设计取舍，不自动等于批准；约束中的新增 API/schema 禁止在本次继续遵守 |

前轮三稿对照读取 `SIM_Target_Architecture_Design_v1.16.md` 与 `SIM_Target_Architecture_Design_v1.17.md`（下称 v1.16、v1.17 参考稿）。二者的 front matter 均仍为 `document_id: SIM-TGT-001`、`version: "1.2"`、`draft/pending`；文件名不是获批版本号或替代关系。v1.16 指向 6017003e 并声称做过静态复核，本文重新核对采用的结论；v1.17 指向未解析的 00b3faa6，且明示未重读源码，作为历史提案使用。两稿已于 489c88fe 作为保留参考稿提交，本轮核对其 SHA-256 与 §20.4 记录一致，未修改；既有输入摘要及历史取舍见 §20.4，不据其自述继承核验结果。

### 1.3 状态标记

| 标记 | 含义 |
|---|---|
| FACT / F | 固定源码、构建或配置直接支持的静态事实，也包括可直接核对的文档自相矛盾；各轮核查范围见 §20.2/20.3 |
| LEGACY | 已核实的旧实现行为，兼容重构默认保护；不自动等于永久业务要求 |
| PROPOSED / P | 有证据及理由的推荐技术决策，尚未批准 |
| DERIVED | 根据已列源码/文档推导的设计风险；未复现不称为已发生故障 |
| OPEN-EXTERNAL | 已给默认边界，仍需产品、外仓或设备证据；只阻塞明确依赖的切片 |
| RESOLVED-FACT | 本仓事实问题已核清，不代表目标实现或业务审批通过 |
| CLOSED-RECOMMENDATION | 已形成不再并列选项的推荐；重新开启需新需求，仍不是 ACCEPTED |
| ACCEPTED | 仅在有批准人、批准内容和可核验记录时使用；本轮无此类新增记录 |

未单独标记的目标设计均为 PROPOSED。`owner: OPEN` 和 `approval_status: pending` 保持；责任角色是待协作对象，不冒充已接受任务的具名 owner。第三轮曾增设 CONFIRMED、HYPOTHESIS、DESIGN-DECISION 三个标记；第四轮并回 FACT/DERIVED/PROPOSED，避免与 §19.4 的 Oracle 类别 CONFIRMED_CONTRACT（已确认契约）混淆。

### 1.4 本次可信基线与核验限度

- 历史：2026-10-30目录融合修订的实际输入为本地`main@e295540fe81ca7dd28f248d0b76470497062aa64`，编辑前工作区/暂存区干净，两篇目标文档无已有本地修改。仅修订本文与源码详细设计，不移动源码、修改评审或执行状态；本批提交只含这两篇文档。当时骨架73文件/4984行、50头/23源；原SIM生产目录61头/55源，根GN列入54源，另有未装配缓存源。两份报告的旧数量及下列历史取证不作为本轮统计。
- 2026-10-31目录可发现性调整输入为main@38c1f232（88文件，86个C++文件），仅改accounts/account_profile物理路径及相关引用/导航；逐文件映射和实际验证见详细设计§3/§21及执行状态。

- 第四轮（revision 2026-10-29-independent-04）复核 [PR #3](https://github.com/zjuluowu/multisim/pull/3) 的第三轮修订：经 `git fetch` 与 GitHub API 确认 PR head 为 `c70894f83ab6c283d45e9f15eaa5af4b74ddd0fa`、base 为 `main@eeb9ec0e`，只改本文且无既有评论。另建分离 HEAD 的 worktree 编辑，不触动已有的 `/tmp/multisim-audit-round3` 工作区；`6017003e→c70894f8` 生产源码、构建及产品配置未变。逐项裁决见 §20.1 末表；新增 F51 固定到 eeb9ec0e。
- 以下第三轮记录保留原文，其中“本轮”指第三轮。
- 第三轮（revision 2026-10-29-independent-03）的输入为 `main@eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0`。GitHub [PR #2](https://github.com/zjuluowu/multisim/pull/2) 已于 2026-10-29 合并 Claude 修订 `1afffa4e0e48874b91edbc96430b722a267650eb`，远端 main/head 经 `git ls-remote` 核实，本文在两个提交中完全相同。本轮基于该合并提交建立独立审计分支/worktree；原 main 工作区 926 项已有修改不作证据、不改动。
- 已读仓库 README、适用 AGENTS 和 docs 索引；跟踪文件及适用目录中未找到 Copilot 专用指令。AGENTS 中仍称 T/A/O 三业务上下文，按任务指定审计 Claude 的 T/A 版本，历史差异留在 §21.2。PR #2 相对其父提交的 223 行新增/126 行删除仅涉及本文：重点变化是 O 降级、T/A 进行中写权、SlotCatalog、异步与拆库门禁；本轮同时复核未改动的会话、存储、兼容与恢复设计。
- `6017003e→eeb9ec0e` 仅 docs、test、AGENTS 和 `.codegraph` 有变化，生产源码、构建及产品配置未变。F01～F46 固定链接保留；新增 F47～F50 固定到 eeb9ec0e。源码取证来自隔离检出，覆盖五类场景、入口/回包/写入、2→3→4 槽、eSIM Port、SO/SA 与迁移；静态交错推演不等于生产复现。原未解析 00b3faa6 继续作为历史记录，不作为本轮基线。
- 第二轮（输入 3fd0f815；independent-02）及更早检查保留在 §20.3/20.5。F37～F39 与下表为前轮旁挂外仓记录；本轮只验证固定引用的定位，不重做外仓语义审计。“脏”是当轮状态，不能由文件名、接口相近或链接可访问推定匹配本 fork。

| 前轮已挂载仓 / origin 所属 GitHub 项目 | 前轮实际 HEAD | 前轮工作区状态 / 采用范围 |
|---|---|---|
| telephony_data_storage / openharmony/telephony_data_storage | `843a8be7f8d1dc304870a3999b754a03c6cce57d` | 脏，61 项；SimAbility 默认卡命令与 RdbSimHelper 建表/事务；请求键和 schema 与本 fork 不匹配，不能用作匹配 provider |
| telephony_ril_adapter / openharmony/telephony_ril_adapter | `d039c7ce8518487bfcec54c9777e6172c0c9f684` | 脏，130 项；HRilSim 的 serial/session、开关通道请求与响应转发；不含厂商硬件结束保证 |
| telephony_cellular_data / openharmony/telephony_cellular_data | `29d6fa9c6105c6e39a9a765048868e4631ffbb8f` | 脏，180 项；默认 Data 的外层校验、CoreManagerInner 调用；不证明产品联调 |
| telephony_sms_mms / openharmony/telephony_sms_mms | `2336a3c3ba9a5cc53e95535cf25a6cfe15c74de7` | 脏，377 项；SmsMiscManager 默认 SMS 委托 |
| telephony_call_manager / openharmony/telephony_call_manager | `3cba608593a8a62d1d0d88629109f96c7a43f3f9` | 脏，583 项；CallNumberUtils 的 Voice→Data 回退及 CELLULAR_DATA_SUPPORT 分支 |

在上述范围未定位到匹配的 66250 服务实现或厂商动态库实现；这是检索范围限制，不等于这些实现不存在。数据提供方已找到上游实现，但与当前 fork 的命令/schema 不兼容；§13 / TGT-OPEN-004 保留的是**匹配版本缺口**。静态语句顺序仅支持单路径局部偏序，不证明不同队列、IPC 返回与回调或跨 SA 的全局时序。历史 PASS 留在 §20.3 的历史记录，本轮结果单独登记。

---

## 2. 架构目标与非目标

### 2.1 目标

1. Subscription 身份与位置、路由、Radio 资源分离；槽种类、容量口径和允许连接关系集中在一处只读目录解释，扩卡不依赖逐模块修改特殊槽号。
2. 每类核心业务事实具有唯一受控写入协议。
3. 聚合内状态转换具有明确的一致性边界。
4. 跨聚合和跨 SA 协作显式处理失败与最终一致性。
5. 外部回调不能绕过身份、代次和状态门控。
6. 接纳事实、旧查询可见步骤、持久化结果和未完成操作分别表达。
7. 旧 API 可以继续兼容，但不得反向决定核心领域模型。
8. 厂商扩展不能直接修改核心状态。
9. 领域测试跨 DDD 重构保留业务场景和断言。
10. 技术接入、运行时装配及存储实现允许受控演进。
11. `services/multisim/`最终完整替代`services/sim/`；保留外部契约，不以两个长期生产模块或永久迁移桥为稳态。目录组织面向修改定位，分层、Owner、构建目标与SO/进程分别设计。

### 2.2 非目标

- 近期不新增公开 Subscription API、IPC、SA、进程、schema、厂商 ABI 或运行期实现切换 Flag；
- 不强制采用 Event Sourcing；
- 不要求跨数据库、Modem 和 eUICC 建立全局原子事务；
- 不要求所有异步事件全局有序或恰好投递一次；
- 不以 Mock 替换核心业务来证明系统正确；
- 不将普通 Linux 受控测试等同于真实设备端到端测试。

---

## 3. 统一语义与身份规则

### 3.1 概念区分

| 概念 | 目标含义 | 禁止的默认等同 |
|---|---|---|
| Physical Endpoint | 硬件接入位置 | 不恒等于 API 中的 slotId |
| Logical Slot | 当前软件路由或兼容索引 | 不作为 Subscription 主键 |
| eUICC | 承载 Profile 的安全元件 | 不等于单个 Subscription |
| Port | eUICC 或设备提供的可用连接端口 | 不等于 Profile 身份 |
| Profile | 可管理的配置对象 | Disabled 不自动等于硬件不存在 |
| SubscriptionId | 内部对既有不透明 simId 的强类型表达，近期无第二公共 ID | 不由当前位置决定，不承诺跨删除/重装永久稳定 |
| ICCID | 卡或 Profile 的身份相关属性 | 不未经确认就等同于所有场景下的主键 |
| Active / Enabled | 特定对象的激活或使能维度 | 不直接等于 Ready |
| Default Role | Voice/SMS/Data 等业务角色 | 不自动等于 Primary |
| Primary | 产品定义的主资源或主卡概念 | 不自动等于默认数据 |
| Radio Assignment | Subscription 与 Radio 资源的分配 | 不等于 Subscription 身份 |
| Readiness | 特定能力的就绪条件 | 不用一个 READY 覆盖所有初始化阶段 |

### 3.2 身份与生命周期

目标要求：

- 路由变化不应无依据地创建新业务身份；
- 位置复用不能让旧回调作用于新卡或新 Profile；
- 身份恢复、删除、重新发现和保留期限必须具有业务依据；
- 身份与别名的关联应可追溯；
- 测试使用 Card-A、Profile-B、Subscription-C 等业务别名，避免将固定 Slot 当作永久身份。

近期默认沿用现有 simId 分配/匹配与历史账户可见性；不新造身份、不从 slot 或 simLabel 推导身份，不承诺跨删除、重装或跨设备永久稳定。当前 InitIccId 查询既有 ICCID 记录后更新或插入，不能据此证明全部产品的身份恢复政策。[F04](#src-04)

卡移除后的保留、Profile 再启用复用、跨设备恢复及 NULL/占位值迁移属于 TGT-OPEN-006 的外部业务确认。缺口只阻塞改变这些政策的设计，不阻塞沿用旧契约的结构改进。空槽/未识别卡使用 SlotTarget/CardApplicationTarget，不为了建立账户而拒绝 PIN、EF 或状态查询。

### 3.3 slotId 当前承载的多重语义与扩卡热点（FACT）

本节回答“为何双卡扩到三卡会跨模块联动”。“当前实现”栏为固定提交 3fd0f815 的静态事实（生产源与 6017003e 相同）；“后果”栏为 DERIVED，未在设备复现；“目标解释点”为 PROPOSED。

| 热点 | 当前实现（FACT） | 对扩展的后果 | 目标解释点 |
|---|---|---|---|
| 槽号编码端点种类 | 索引 2 预留给 VSIM（`SimSlotType::VSIM_SLOT_ID = 2`；eSIM 初始化注释“slot2 保留给天际通”）；三卡产品的第三张实体卡用索引 3，`SLOT_3_INDEX` 受调用者能力限制；RIL 与搜网各有一份逐字相同的 `IsValidSlotId`，按 VSIM 开关和“≥3 卡”上界放行或排除索引 2 [F40](#src-40) | 种类、可见性和路由规则分布在 SIM、RIL、搜网、Core 入口、API 门面、capability 与 EXT 的 21 个生产文件（49 处匹配行，含常量定义）；新增或移动一种端点需要联动修改 | H 构建只读 SlotCatalog（§17.1），逐索引给出端点种类、允许连接、可见性/权限类别和可运行期加入标记；特殊槽规则限于目录、C 旧投影和具名兼容策略，不在通用用例重复 |
| 容量口径不唯一 | `SIM_SLOT_COUNT`/`_MD`/`_REAL` 与 `DUAL_SLOT_COUNT`、`MAX_SLOT_COUNT`、`THREE_CARD_COUNT`、`DC_*`、`VSIM_MODEM_COUNT` 并存；前两个宏共享头文件匿名命名空间中的同一 `maxSlotCount_` 缓存，推导规则却不同 [F40](#src-40) | 取值依赖首次调用者、编译单元和内联（ODR 风险）；各对象数组按不同口径定长（Controller `maxCount_`、RadioProtocol 至多 2、eSIM `MD+1`） | 目录分别表达配置槽数、可路由能力和具备 Radio 栈的能力；运行期装配/可见数量来自相应 Owner 视图；首次引入须逐产品参数复现各旧函数值，差异先记为 LEGACY 再裁决 |
| 双槽硬编码策略 | `GetDefaultMainSlotByIccId` 只比较槽 0/1 的 ICCID；RadioProtocolController 至多建模两个槽，`SetRadioProtocol(slotId)` 未使用目标参数，CHECK 构造按 `slotCount_ - 1 - i` 反转分配；越界或索引 2 的 modemId 返回 0（与 M0 同值） [F48](#src-48)；PSIM1/PSIM2 标签由 `slotId == 0`、槽 1 与 `persist.ril.sim_switch` 推导 [F41](#src-41) | 第三张卡不参与主卡记忆；槽 3 的 modem 查询与 M0 不可区分；N>2 时的行为是数组扩展的副作用，而非明确规则 | A 的 Primary 策略、T 的标签策略作为具名策略变体，输入为目录中的合格槽与 Radio 域；N 槽新策略属 G4 产品决策 |
| 第三卡与 VSIM 走旁路 | 槽 3 设为 Primary 时经 `NeedSwitchSimCard` 调厂商 `SwapM0M2SimCards` 换路，再登记 `targetPrimarySlotId_` 延后切换；`CoreServiceSim::GetDefaultVoiceSlotId` 在 IPC 入口按调用者能力把槽 3 译为 INVALID 或 M0 所在槽；VSIM 槽经 `InitTelExtraModule` 运行期追加对象 [F41](#src-41) | 端点重映射（T）、Radio 分配（A）、权限投影（C）与厂商执行（I）在 Controller 和入口内临时串接 | A 的切换用例唯一编排，换路经 T 的映射用例与厂商端口；C 只按 A 的视图做旧投影；目录声明可加入能力，T 更新槽运行清单与绑定 |
| 账户缓存按位置下标 | 当前缓存 `localCacheInfo_` 以槽号为下标（每槽至多一个当前账户）；全量缓存 `allLocalCacheInfo_` 以 `simId - 1` 为下标，越界行被丢弃；“当前账户”即 `SLOT_INDEX > -1` 的行；拔卡 `ForgetAllData` 在同一次 Update 中写 `SLOT_INDEX = -1`，并按条件写标签和 `IS_ACTIVE` [F42](#src-42) | 身份暗含“simId 稠密”假设；无法在**同一逻辑槽**容纳多个当前账户；是否限制 MEP 取决于 Port→逻辑槽映射，不能等同“一 eUICC 一账户” [F49](#src-49) | T 以 SubscriptionId 为账户键、以位置（槽及将来经证实的 Port）为绑定键并维护唯一性；旧 DTO 投影复现槽下标布局；稀疏 simId 的旧可见行为在 G3a 前采集；既有 MEP 分支保持；新增 Port 感知绑定随 G4 契约核实，是否改 schema 另证 |

扩卡复杂度的根因不是数组长度，而是 slotId 同时承担位置、端点种类、路由、权限类别和账户下标五种含义，且没有单一解释来源。目标不删除 slotId：C 继续接收旧 slot 参数，R 继续按实际路由访问设备；种类、容量和允许连接关系读 SlotCatalog，身份及当前绑定经 T 解析；当前 Radio 分配由 A 接纳 R 观察，不能从静态目录推导。

扩展修改范围如下；这是依赖分析，不是已测得的收益或四卡支持承诺。

| 变化 | 重构前的传播 | 收口后的必要修改与验收 |
|---|---|---|
| 双卡→当前三卡（第三实体卡索引 3，保留 VSIM 2） | SIM/RIL/搜网的有效槽、实例容量，Controller 换路/标签/Primary，Core capability 投影及测试 | 目录与装配、T 标签/绑定策略、A 换路策略、C 旧投影；R 保留已验证协议。迁移尚未覆盖的 RIL/搜网仍需修改，不能因 SIM 内有目录就称全仓解耦 |
| 三卡→四个逻辑端点，Radio 栈数不增加 | 上述容量、特殊槽分支及双槽选择算法；不只是多建对象 | H 配置、T 合格绑定与 A 的 N 槽选择；若仍复用两栈，需经证实的厂商换路。目录消费者的遍历/查询不应改；卡数变化是否改变默认角色须产品批准 |
| Radio 栈或 Port 能力同时增加 | RadioProtocol 的双槽反转、完成计数/参与集合、RIL/厂商协议及 eSIM 映射 | A 产生显式目标分配，R 按受支持协议执行参与集合，T 接纳 Port 绑定；增加对应协议/设备矩阵，不能仅放大数组。若 Port 分别映射为不同逻辑槽，旧每槽一账户可能足够；多对一映射才需另评模型/存储 |

[F48](#src-48) 的现有单测使用两个空槽对象，host 容量用例固定参数为 1；均不证明 N 槽分配正确。未来用稀疏槽号、三/四端点两栈及多栈分别验证目标选择、非目标不变和参与集合；硬件协议变化是必要代价，业务模块重复解析特殊槽号则是应消除的耦合。


---

## 4. 目标上下文与唯一所有权

### 4.1 从问题推导边界并裁决上下文

本节先从已核实问题推导边界，再比较候选划分。下表“可验证收益”是将来实施的验收方式，不是本轮测试结果。

| 现有问题（证据） | 根因 | 目标决策 | 被隔离的变化 | 可验证收益 |
|---|---|---|---|---|
| 扩卡、VSIM、第三卡要联动修改 SIM、RIL、搜网、Core 入口等（§3.3，[F40](#src-40)～[F42](#src-42)） | slotId 兼作位置、端点种类、路由、权限类别和账户下标，没有单一解释来源 | H 构建只读 SlotCatalog；T 只拥有运行期槽清单与绑定；A、C、R 按目录解释 | 槽数量、端点种类、VSIM/受限槽、路由 | 新增端点主要改目录与具名策略；适应度检查要求通用用例不重复特殊槽决策，具名兼容策略明确登记，各产品配置的容量值与旧函数等值 |
| MultiSimController 同时维护账户、角色、Primary 参数、进行中标志和设备等待（[F04](#src-04)～[F09](#src-09)） | 变化原因不同的事实无唯一写者 | T、A 两个业务上下文，按 WU 的全部旧入口切换写权（§6.3） | 账户/身份/绑定规则与角色/Primary/Radio 策略彼此独立 | 字段赋值与引用扫描只命中对应 Owner；修改 Data 策略不触及账户代码 |
| 激活与 Primary 切换的进行中标志经 ISimManager/CMI 公开给搜网、运营商配置，并门控 Monitor 重载和 R 的卡状态接收；Primary 入口却无准入，IPC 入口与 5 处 detached 线程（其中一处由重试事件触发）可并发进入（[F43](#src-43)、[F44](#src-44)） | 进行中状态是跨模块协调事实，却没有 Owner | A 拥有 AssignmentTransition 与切换准入，T 拥有激活进行中标志；二者都是领域状态 | Modem/Radio 切换协议与时序 | 并发切换、激活等待和 UNKNOWN 归因有单一裁决点，可做并发负控 |
| 异步回包依赖“同一时刻只有一个请求”，而非请求关联：激活回包落入单一 `activeResponse_`，RadioProtocol 错误分支不比较 session（[F17](#src-17)、[F19](#src-19)、[F45](#src-45)） | 关联只存在于传输层 serial | 发起时 RequestContext、R 资源屏障；准入与状态推进在同一执行域（§11、§12） | RIL 时序、超时和迟到回包 | 迟到、重复和错轮回包测试可断言不污染后续请求 |
| 卡、缓存、DB 与通知分节点写入，部分失败可见（[F05](#src-05)～[F09](#src-09)） | 旧可见行为本身是兼容契约 | Owner 逐节点接纳的兼容提交协议（§9） | 存储与通知失败处理 | 方法级失败轨迹差分 |
| SIM 与 Core 共编一个 SO，SIM 使用 Core 内定义的进程级单例并经 CMI 反调自身（[F02](#src-02)、[F46](#src-46)） | 缺少宿主端口和单例归属 | HostPorts/工厂注入、单例唯一定义处、最小导出面（§7.1、§14.2） | 链接、装配与生命周期 | 产物级 NEEDED、导出符号和重复定义检查 |

**上下文划分（PROPOSED）。** 业务上下文为 T、A 两个；O 降为 Application 层的用例编排机制；C 是兼容边界，R/I/H 是实现责任。这些代号不对应同数目的聚合、服务或 SO。旧材料的“四 Context”把 Compatibility 作为技术防腐边界计数；本文只把具有独立业务不变量和业务事实写权的边界计为业务上下文，C 与 O 均按此口径排除。[F04](#src-04)、[F07](#src-07)、[F11](#src-11)支持按变化原因拆分原混合控制器，而非按类名搬迁。

| 边界 | 隔离的变化 / 保护的规则 | 唯一拥有的状态 | 不承担的责任 |
|---|---|---|---|
| T — Subscription Topology（账户与连接拓扑；业务上下文） | 身份匹配、账户生命周期、绑定与激活；位置复用不得混淆身份；同一位置至多一个当前绑定 | 账户资料、active 兼容投影与激活进行中标志、槽运行清单（按 SlotCatalog）、已接受绑定及 bindingRevision、账户加载状态；内部分为按 SubscriptionId 的 AccountRegistry 与按位置的 BindingIndex 两个一致性边界 | 不拥有原始卡状态、设备执行结果、角色决策或槽配置 |
| A — Radio Resource（角色与资源分配；业务上下文） | Voice/SMS/Data、Primary 意图与 Radio 资源限制的变化；角色唯一、Primary→Data 联动、同一 Radio 域同时至多一个切换 | 角色偏好/有效选择、Primary/Data 旧参数及用户偏好提示、期望分配、为决策接纳的资源观察、AssignmentTransition（进行中、目标、尝试及唯一业务重试） | 不拥有 IMS 注册、数据连接、卡身份、原始协议观察或硬件执行租约 |
| O — 用例编排内核（Application 机制，非业务上下文；原 Local Operation，代号仅作追溯） | 等待、请求关联、部分效果记录等技术机制；一类用例只有一个进程所有者 | 仅运行期请求状态：RequestContext、EffectKnowledge、等待状态与有限终态 | 不拥有任何对外可见业务事实或进行中标志，不作跨上下文协调中心，不复制远端下载/OSU 状态机 |
| R — 生产卡运行支撑 | 卡型/EF/APDU/安全/协议变化；同会话结果归属与真实共享设备串行 | 原始卡/锁状态、Records、电话簿/短信/STK/eUICC 缓存、Radio 协议原始观察与 session、运行会话、传输关联、资源屏障 | 不包装成纯 Domain，不重写成熟编解码，不把库存等同账户 |
| C — Compatibility | 旧签名、校验优先级、完成、查询来源和输出轨迹 | 订阅登记、完成出口及格式；业务投影（含槽 3 等旧翻译的输入）由 T/A/R 提供 | 不写业务存储（含入口偏好参数），不维护可变账户状态；仅持旧输出翻译规则，动态映射输入由 A/T 提供 |
| I / H — 技术适配 / 宿主装配 | DataShare、参数、通知、厂商、RIL 宿主访问及生命周期 | 技术句柄、订阅令牌、工厂与宿主资源所有权；H 构建只读 SlotCatalog，并确定进程级单例的唯一定义处 | 不拥有领域策略；Repository 名称不创造事务 |

**O 的重新裁决。** 前稿把 O 列为第三个业务上下文。按上述口径复核：O 所保护的“单一主编排者、无证据不重试、陈旧响应不提交”对所有异步用例都成立，是架构规则（ARCH-RULE-004/006）而非 SIM 业务不变量；其状态（step/attempt、等待、效果认知、终态）是单次请求的过程状态，不持久，也不被其他上下文查询。代码中真正被其他模块消费的进行中事实——激活进行中、Primary 切换进行中——分别门控搜网 Radio 开关、运营商配置、Monitor 重载、R 的卡状态接收和激活等待 [F43](#src-43)，其规则属于 A/T 的不变量。若 O 仍是业务上下文，Primary 切换要在 A（意图、参数、观察）与 O（步骤、重试、进行中）之间往返，参数预写与失败恢复还会变成 O 经 I 写 A 的字段（[设计说明与业务流程对照 §8.3](多SIM架构重构_设计说明与业务流程对照.md#83-目标架构的职责流转) 的时序即为 O→I 预写参数），形成隐藏的跨上下文写入。因此 O 只保留为编排内核；每类多步用例由**目标事实 Owner 的应用服务**作为唯一进程所有者，负责步骤、重试和进行中标志（§4.2）。

| 候选划分 | 业务边界与不变量 | 状态所有权与协作复杂度 | 2→3→4 槽 / eSIM Port 弹性 | 迁移、理解与 AI 任务局部性 | 裁决 |
|---|---|---|---|---|---|
| V22 历史稿（[§4.1](history/pre_unification_20261015/多SIM架构重构_软件设计说明书_V22.md)）：Subscription Management、Hardware Topology、Subscription Operation、Radio Resource 四业务上下文 + Compatibility ACL | 身份与角色同域，硬件拓扑独立，长用例进度独立 | 账户与绑定同为一行，拔卡一次 Update 写槽号并按条件写标签和 active [F42](#src-42)，拆成两个写者需跨上下文同步；角色与 Primary/Radio 分离，使 Primary→Data 联动跨域 | 拓扑独立有利于将来 Port 映射，但当前无可信 Port 持久映射 | 四个业务上下文且 Operation 兼任协调者，写权切换面最大 | 不采用；拓扑独立的前提在 G4 前不成立 |
| V3 / 2026-10-15 统一裁决（[DDD 导读 §2](MultiSIM/04_ddd_target_architecture.md)、[DU-01](MultiSIM/08_design_unification_decisions.md)）：Subscription Topology、Profile Operation、Radio Assignment、Compatibility 四 Context、三聚合 | 账户与拓扑合并、角色与资源合并 | Operation 设为聚合与写者；Compatibility 计为 Context，混淆业务与技术边界 | 槽种类与容量未建模 | 中；Operation 聚合使一个用例的改动跨两个写者 | 保留 T、A 的合并取舍 |
| v1.2 前稿：T/A/O 三业务上下文 + C/R/I/H | T、A 同上；O 为本地操作 | 进行中标志无明确 Owner；Primary、激活需在 O 与 A/T 间往返；O 有成为通用协调中心的倾向 | 扩卡热点未被隔离 | 中；一个用例的改动分散到 O、A/T 与 R | 调整 |
| **本轮推荐**：T、A 两个业务上下文 + 应用编排内核 + R/C/I/H + SlotCatalog | T：账户身份、资料、绑定、激活；A：角色、Primary、Radio 分配与切换 | 一类用例一个进程所有者，即目标事实 Owner；跨上下文只经应用入口提交 | 槽差异集中于目录与具名策略；新增 Port 映射先核实逻辑槽契约（G4） | 与既有 WU 顺序一致，编号不变；一个用例族的改动集中在一个 Owner 的应用与领域模块及 R 执行器 | 推荐（PROPOSED） |
| 备选：T 拆为账户与拓扑，A 拆为角色与 Radio 分配 | 变化原因确有差异 | 可由一个持久化适配器合并计划而避免双写，但账户/绑定和 Primary→Data 需新增跨域协调 | 在 Port 不变量独立或角色/Radio 独立演进时可能获益 | 高 | 暂不采用，保留重开条件 |

**裁决：调整部分边界（第二轮提出，第三、四轮复核保留）。** 保留 T 与 A；O 从业务上下文降为应用编排机制；R、C、I/H 保留并补充上表约束；新增由 H 构建的只读 SlotCatalog。同一 DB 行不是必须同一上下文的证明：“账户/角色”与“连接/Radio”两域或表中的四域也有合理变化边界，但前者把 Primary→Data、激活→账户联动变成跨域，后者增加提交与恢复协作，当前没有足够的独立规则收益抵消迁移成本。O 的归类不取决于是否持久化，而取决于它是否新增独立业务决策。成立前提：（1）账户/绑定需共同维护唯一性，当前无已验证的完整 Port→逻辑槽契约——若 G4 引入独立 Port 感知绑定并出现独立于账户属性的不变量（如 Port 互斥、Profile 启用配额），再评估把 BindingIndex 拆为独立上下文；（2）Primary→Data 联动仍是产品策略——若产品解除联动、Radio 分配完全由硬件能力驱动，再评估拆分 A；（3）66250 仍是高层 eSIM 任务主编排者——即使 Core 将来接管，也作为 T/A 的用例族实现，不恢复 O 上下文。不因槽数 N 增大自动拆域；此结论不授权新的角色政策。重构设计中的 `O/...` 目标路径（如 `O/activation`、`O/primary_switch`）在该文同步前，按 §4.2 的进程所有者解读。

### 4.2 聚合、应用服务与部署

| 纯模型 / 一致性边界 | 唯一应用写入口 | 部署建议 | 边界理由 |
|---|---|---|---|
| T：AccountRegistry（账户资料与生命周期）+ BindingIndex（按位置的绑定索引）及纯身份规则；原 TopologyAggregate 为二者合称 | TopologyService；原 TopologyManager 名称统一至此 | Core 4010 内部 | 账户属性与位置关联有约束，但 EF 加载和设备等待不锁住整个拓扑；同一存储行的绑定、标签与 active 由同一写入口生成写计划 |
| A：AssignmentAggregate（按角色约束域与共享 Radio 域划分规则，含 AssignmentTransition） | AssignmentService；RadioResourceArbiter 限为纯决策规则 | Core 4010 内部 | 跨槽 Primary 切换需要同域仲裁和准入，无关 Modem 不应被全局队列阻塞 |
| 无业务聚合；运行期 RequestContext / EffectKnowledge（原 OperationContext） | 由各用例的进程所有者经编排内核登记；原 OperationCoordinator 职责并入 T/A 应用服务 | Core 4010 的本地用例 | 过程状态不持久、不被其他上下文查询；有业务不变量的进行中状态归 T/A |
| 远端 Profile 任务状态（具体模型未知） | 既有 66250 服务的任务主编排者 | 保留既有服务身份，不由本仓指定其内部类 | 本仓仅有接口/客户端，不能宣称远端已经实现 OperationAggregate |

**编排内核、TopologyService、AssignmentService 都属于 Application；纯状态转换与策略属于 Domain（T、A 的模型及共享值类型）。** 执行 I/O、等待和恢复的职责不能放进 Domain 框。[F01](#src-01)、[F10](#src-10)～[F13](#src-13)证明 Core 卡侧能力与服务任务是两个现有边界。

多步用例使用编排内核记录关联与等待，但步骤、重试和进行中标志由下表的唯一进程所有者负责。用例跨 Owner 时经对方应用入口提交，不持有跨 Owner 可写引用或共享锁。

| 用例族 | 唯一进程所有者（Application） | 协作方式 | 依据 |
|---|---|---|---|
| 卡发现、账户建立及初始化后续 | T 的发现用例：ICCID 匹配、建行/更新、绑定与槽运行清单 | A 提供建行角色片段，并在账户就绪后执行主卡复核；R 提供卡状态与 Records | [F04](#src-04)、[F23](#src-23)、[F35](#src-35) |
| 普通/force/卫星激活 | T 的激活用例：active 投影与激活进行中标志 | 设备步骤前经 A 的切换准入（G2 保留旧的有界等待）；R 执行；成功后调用 A 的主卡复核入口 | [F05](#src-05)、[F43](#src-43)、[F45](#src-45) |
| 号码写卡与资料同步、名称、标签、映射开始 | T 的资料、号码、映射用例 | R 写 MSISDN 或注入人工 ABSENT | [F08](#src-08)、[F18](#src-18)、[F34](#src-34) |
| 默认 Voice/SMS/Data，Primary/Radio/DSDS 切换（含槽 3 换路） | A 的角色与切换用例：AssignmentTransition、参数/偏好、唯一业务重试、Primary→Data | R 执行三阶段协议；槽 3 换路经 T 映射用例与厂商端口（父/子请求协议见本节） | [F06](#src-06)、[F07](#src-07)、[F41](#src-41)、[F44](#src-44) |
| PIN/PUK、认证、单次 IO，Core eUICC 卡侧原语 | 无独立业务聚合：C → R能力包内的具名应用短请求；短请求拥有本地关联/期望的登记及清理责任 | Profile 结果经 R 观察由 T 接纳绑定；关闭 Profile 的槽位标签恢复提示由 T、启用偏好由 A 接纳，无仓内读者的旧外部参数按原节点保留（§13.2） | [F11](#src-11)～[F13](#src-13)、[F15](#src-15)、[F50](#src-50) |
| 下载、OSU 等高层 eSIM 任务 | 既有 66250 服务 | Core 只提供 R 原语，不复制任务状态机 | [F10](#src-10) |

激活归 T 而非 A：`IS_ACTIVE` 与绑定同在账户行，拔卡解绑的同一次 Update 还按条件把 `IS_ACTIVE` 置回 ACTIVE [F42](#src-42)，WU-02 也已归 T；激活与 Primary 切换的互斥由 A 的切换准入表达，无须把 active 事实迁到 A。若产品将来把“启用订阅”定义为 Radio 服务分配策略（如按 Modem 栈配额自动停用），再评估迁至 A。

**跨 Owner 协作规则（G3 设计约束；第三轮补充，第四轮精简）。** “唯一进程所有者”指每个请求只有一个主编排者，不是全进程只有一个协调对象：

1. **子步骤带父关联。** A 的 Primary 请求调用 T 的映射子步骤（如槽 3 换路）时，传递父请求、step/attempt 与受影响绑定；子步骤只完成自身，不递归启动另一个 Primary，也不再次申请父请求已持有的冲突资格。
2. **不嵌套同步等待。** 跨 Owner 调用以异步续接完成，不占住对方完成所需的执行队列或冲突资格；A 等待的是所需绑定被接纳，而不是整个 InitData、激活或主卡复核收敛。T 的发现/激活在本地兼容提交点后向 A 提交主卡复核，A 按已批准的冲突规则接纳或合并，不无条件丢弃。
3. **准入是原子登记，不是查询。** 激活不能先查询“A 当前无切换”再发令：查询与发令之间可插入 Primary（§4.4 场景 2 及反例表）。启用新准入后，A 在同一短执行域登记激活与切换的冲突资格，T 持请求令牌再向 R 申请设备租约；A 不接管 active 字段。失败只释放本请求的资格，未知设备效果交 R；资格释放不等于设备可用。
4. **依赖方向不成环。** T 与 A 只经对方声明的窄应用接口互相调用（准入、主卡复核、映射子步骤），由 H 注入；两个应用模块不互相包含实现，步骤与重试留在具名用例中，TopologyService/AssignmentService 只是受控入口，不成为通用流程 Manager。

排队、拒绝、预算及旧 4 s 等待之后的处理须经 G3c 批准；G2 保持旧轨迹。上述规则不新增协调服务，也不恢复中央 O。

简单状态查询、EF 加载、电话簿多文件读取、STK 会话继续由 R 的生产协议模型处理。PIN/PUK、认证、单次 APDU 需要发起时关联和完成门控，通常仅用短生命周期 RequestContext；不为其强建持久业务聚合。

### 4.3 聚合粒度与不变量

近期保留一个 T 应用写入口统一索引，使用**短时内存提交 + 按受影响账户/绑定键预留**：身份关联唯一性或绑定迁移在同一次内存提交中核查旧/新键；显示名修改只检查相关显示名字段及必要绑定版本，不让另一槽加载导致无意义冲突。I/O 在锁外执行，返回时复核有关输入。不同账户可以并行准备；触及相同账户或绑定键的变更序列化，不将设备等待占据整个 T 通道。

A 的一致性范围先由角色不变量确定，再结合真实资源冲突：当前默认 Voice/SMS 设置遍历当前账户并清除其他槽的同类角色，是跨槽角色选择，不能因两个 Modem 可独立执行就各保留一个可写“默认 Voice”。[F07](#src-07) 同一角色约束域由 A 统一裁决；独立硬件域只允许无冲突设备步骤并行，不自动拆分角色事实。资源域未证明独立前保守共享，不从 slot 数或两个对象推断并发能力。AssignmentTransition 按受影响角色与 Radio 资源的冲突集合准入；当前 Primary/Data 参数是设备级单值，即使未来两个 Radio 域独立，写这些共同字段的切换也不能并行。仅真正不共享角色、参数、绑定或设备资源的步骤可并行；当前入口无此准入（§4.4 场景 2），收紧属于 G3c 的行为变化。多步用例的运行期请求状态按请求建模，设备冲突交 R 的资源屏障；协议 session、文件加载集合均不扩大 T 事务。

这里保证的是本进程内规则和提交准入，不是 DataShare/provider 的原子事务。跨行唯一性、跨存储提交仍受 §13 限制。TopologyAggregate 名称保留为逻辑模型，不要求一张整设备持久快照或一个覆盖所有 I/O 的大锁。详见 TGT-OPEN-007。

### 4.4 关键场景推演

以下按源码静态路径推演目标边界；“问题”中的竞态均为 DERIVED，未做交错复现。行为保持与收紧的阶段见 §22。

| 场景 | 现状与问题 | 目标协作、状态写入与异常处理 | 兼容与收益 |
|---|---|---|---|
| 1. 卡发现、识别、解锁与账户建立 | R：SIM 卡在 state≥READY 且文件读齐后进入 LOADED，经 Registry/CES 输出 [F35](#src-35)；PIN 经 CoreServiceSim 的 PIN 队列→StateManager/Handle→RIL，无需 simId [F15](#src-15)。Monitor 重试后调用 Controller InitData：InitIccId 匹配/建行（同时写角色列）、InitActive、InitShowNumber、InitPrimary，再由 CheckIfNeedSwitchMainSlotId 起 detached 线程经 CMI 回调 SetPrimarySlotId [F04](#src-04)、[F23](#src-23)、[F44](#src-44)。建账、初始角色、主卡决策和进行中标志混在 Controller，主卡复核经公开门面自调用 | R 接纳卡状态与 Records（WU-06）并发布观察；T 的发现用例完成 ICCID→simId 匹配、建行/更新（A 提供角色片段，形成一次写计划）、绑定与槽运行清单（WU-01/02/05/09），随后调用 A 的主卡复核入口，由 A 决定是否启动切换用例，不再经 CMI 自调用。PIN/PUK 使用 CardApplicationTarget。provider 失败按 Monitor 原重试预算由 T 重读或重建 | 保留建行字段、LOADED 判据与输出顺序、通知与首帧；账户规则变化只触及 T，主卡策略变化只触及 A（G3a） |
| 2. 设置默认数据卡或 Primary | Data 只写参数、last 与广播，失败不传播 [F06](#src-06)、[F33](#src-33)。Primary：槽 3 走厂商换路；普通路径先预写参数，置进行中标志与 SenseSwitch 阶段；SetRadioProtocol 遇已有协议会话立即返回 false，否则最长等待 60 s；失败分支恢复参数→`SetPrimarySlotIdDone(false)`（清进行中、结束 SenseSwitch、唤醒等待的激活）→排重试；成功分支广播→设 Data→Done [F06](#src-06)、[F44](#src-44)。并发的第二个请求会走失败分支，提前清除第一个请求的进行中与 SenseSwitch 状态并排重试，使等待中的激活在协议会话期间继续，R 恢复接收可能错误的卡状态回包 | A 的切换用例为唯一进程所有者：AssignmentTransition 准入→A 写参数/偏好（WU-04）→R 执行三阶段协议并维护 session→A 接纳观察→A 内部 Primary→Data→结束切换并发布进行中视图；R 与 T 按 A 登记的 ExpectedTransition 归因 UNKNOWN 和 SenseSwitch。60 s 本地等待超时不等于设备结果，记 Unknown 并交 R 隔离（§12）；唯一重试由 A 在效果可判定后发起；参数恢复不证明设备已回滚 | G2 保留全部旧分支、事件与错误；把并发请求改为排队、合并或拒绝属于 G3c 行为变化，须批准并差分。准入与重试集中于 A，可做并发负控 |
| 3. eSIM Profile 启用、关闭、切换及 Port 映射 | Framework→EsimServiceClient→66250（实现在仓外）；共宿主时经 CMI→EsimManager::SwitchToProfile→EsimFile 开通道/APDU，槽有效时外层即返回成功，enumResult 承载卡结果，随后写 USER_PREFER_PRIMARY_SLOT [F10](#src-10)～[F13](#src-13)。portIndex 透传到卡，而当前账户缓存每逻辑槽至多一个当前账户 [F42](#src-42)。卡侧原语顺带写 A 的偏好、T 的槽位标签恢复提示，以及一个无仓内读者的旧外部参数 [F50](#src-50)；Port 与逻辑槽关系尚未证实，MEP 能力参数的缺省值在各读取点也不一致 [F51](#src-51)，不能直接判定支持或不支持 MEP | 66250 继续主编排；Core 中 C→应用短请求关联→R（eUICC 资源屏障）；启用偏好由 A 接纳（WU-04），槽位标签恢复提示由 T 接纳（WU-05），无仓内读者的 `last_deactive_profile` 按原节点作为旧外部输出保留；Profile 变化引起的卡状态/ICCID 变化经 R 观察后由 T 接纳绑定，Port 未知时不填默认值。保留外层/内层双结果；通道关闭结果未知时隔离对应 eUICC 域；远端重试与补偿归 66250（TGT-OPEN-001） | 接口与双结果不变；Core 不建第二任务状态机。保持现有 MEP 分支；新增映射能力由 T/R 与产品/服务契约决定，不能预设须改 schema（§17.1） |
| 4. 换卡、Refresh、迟到回调与异常恢复 | Monitor RefreshData 在 NOT_PRESENT 或 Modem 已初始化的 UNKNOWN 时清 Records、ForgetAllData、重载两份缓存并重置 Primary 重试计数；Modem 未完成初始化的 UNKNOWN 若遇 Primary 切换进行中，只清 Records 与账户 loaded [F23](#src-23)、[F43](#src-43)；SenseSwitch 阶段 R 丢弃卡状态回包 [F43](#src-43)；局部与全量 Refresh 共用计数 [F16](#src-16)；激活回包落入单一 `activeResponse_`，超时事件也只有一个 [F45](#src-45)。超时后迟到的激活回包可能完成下一次激活；UNKNOWN 的原因要从 Controller 标志推断 | R 推进 bindingGeneration/LoadScope，并按发起时关联准入回包（§10、§11）；T 接纳绑定撤销或新增；A 的切换用例发起前登记 ExpectedTransition，R/T 据此归因 UNKNOWN；激活回包按请求关联，迟到结果只作 R 的效果核对证据。未知效果隔离到对应资源域；重启按存储、参数和设备事实重建，不承诺同 ID 恢复（§12.2） | G2 保留旧过滤与重载顺序；关联与准入收紧在 G3b/G3c 单列验收，可用迟到、重复、错轮回包测试验证 |
| 5. 双卡扩展到三卡时的资源与角色变化 | 第三张实体卡用索引 3 且受调用者能力限制；设槽 3 为 Primary 时先由厂商换路再延后切换；RadioProtocol 只建模两个槽；默认 Voice 为槽 3 时在 IPC 入口翻译；索引 2 留给 VSIM；TSTS 标签依赖厂商配置 [F40](#src-40)、[F41](#src-41)。变化分散在 SIM、RIL、搜网、Core 入口、API 门面、capability 与 EXT | SlotCatalog 声明第三端点的种类、允许连接与权限类别；T 按目录建立槽清单和绑定；A 结合目录中的 Radio 域（如三槽两栈）与当前分配裁决，旧换路逻辑作为具名策略变体；C 按 A 的视图做旧翻译；R 按目录实例化运行对象。换路或切换任一步失败按 A 的切换用例处理；厂商换路效果未知时不重发（§17.2） | 三卡旧行为逐项保持；扩到四槽的改动取决于端点数与 Radio 栈是否同时增加，必要的 R 协议算法变化见 §3.3，不能只扩实例。四槽/MEP 的产品能力与设备矩阵仍属 G4，不由目录推导 |


**第三轮反例检查（静态推演，非设备复现）：**

| 允许的交错 / 证据 | 能否推翻输入设计；最小处理与判据 |
|---|---|
| Primary P1 超时→P2 开始→P1 的错误/完成回包抵达（F17/F19/F44） | §11 已要求旧 session 不改新轮，但仅将标志归 A 不保证结束归属。P1 不得清 P2 的 AssignmentTransition/SenseSwitch，迟到结果只交 R 核对；同时验证超时事件及成功/错误的 request/attempt。设备是否静止仍为 OPEN-EXTERNAL，不能因参数恢复就准许重试 |
| 激活 X1 超时→X2 发令→X1 回包抵达；或激活查到无切换后插入 Primary（F43/F45/F47） | 实例串行不能挡住第一种交错，单次读 A 标志不能挡住第二种。§4.2 的带归属冲突资格、§11.2 的结果/超时关联及 R 统一资源域分别处理；判据为旧输入不能完成 X2、不能清其资格、不能在未知设备效果下双发。排队/拒绝政策待 G3c 批准 |
| 同一逻辑槽由卡 X 变为 Y，旧 EF/号码回写晚到（F08/F09/F16/F35） | 现有 §10/11 的会话/加载 scope 足以保护新绑定，保留。判据是旧读不进 Y、旧写效果仍可核对、无关 scope/卡继续；不把人工 ABSENT 或任意 READY 当作物理换卡/旧操作接续证明 |
| Profile 关闭/启用可能已执行但回包丢失；随后读取 GetEsimPortIndex（F49/F50） | 该 getter 可只反映请求的 Port，不能解除 Unknown 或提交新绑定。按 §12 由 R 核对库存与通道、66250 核对高层任务；无结束证据不重放。失败时既有关闭历史参数也可能已写，须保留提示而非宣告成功；设备实际效果待外部证据 |
| 建账持久化成功，后续 Primary/Data 参数写失败（F04/F06/F33） | §9 的分步骤提交正确，保留：T 账户不因 A 参数失败被删除，A 保留原 last/参数/通知轨迹；内存、持久、设备效果分别记录。验收不得把重试整个 InitData 等同安全补偿，也不得宣称跨 Owner 原子回滚 |

---

## 5. 分层与依赖方向

### 5.1 当前架构与目标架构对比

#### 5.1.1 图示范围与状态

本节采用 **UML 组件/包图**表达当前架构、目标分层和源码依赖，采用 **UML 时序图**表达运行示例；使用 PlantUML 记法，保留原图的分层分组、组件、职责说明与关系含义。当前图按固定提交的实际职责归类，目标图为 PROPOSED；横向框不代表当前已形成严格软件层，也不代表独立 SO 或进程。

§5.1 图中的实线箭头仅表达注明的主要运行协作，不表示所有请求逐层穿过全部组件；返回和异步上报不逐一展开。§5.2 的 UML 虚线依赖箭头从使用方指向被依赖声明，§5.3 的消息/返回箭头按时序解释；PlantUML 中 hidden 连接只控制排版，不表示架构关系。目标 Domain 框与 R/I 框之间不画下行调用箭头：Application 调用纯规则，并经私有 Ports 访问 R/I。源码编译依赖单列 §5.2，运行用例见 §5.3，部署/链接见 §7；目标组件名不表示已经实现。

#### 5.1.2 当前架构：以 Slot 为主要组织维度的服务与控制器协作（FACT）

```plantuml
@startuml SIM_Current_Architecture
!pragma layout smetana
skinparam componentStyle uml2
skinparam shadowing false
skinparam defaultFontName "Noto Sans CJK SC"
skinparam defaultFontSize 13
top to bottom direction
package "外部调用与接口入口" as CurrentEntry {
  note as CurrentEntryNote
    NAPI / CJ / ANI 等 Framework → CoreServiceClient / ICoreService → IPC Stub
    服务分发：CoreService / CoreServiceSim → SimManager / EsimManager
    共宿主直接入口：CoreManagerInner → 已注入的 SIM / eSIM 接口
    本图主线为 Core 本地能力；66250 高层任务入口在图下方单独列出。
  end note
}
package "服务与流程组织区域：尚未严格分离应用和领域" as CurrentServices {
  component "SimManager\nSIM 查询与操作门面\n按 slot 装配、定位卡运行对象" as CurrentSim
  component "EsimManager（条件装配）\nCore 本地 eUICC 卡侧能力\n普通 / 低优先级 EsimFile" as CurrentEsim
  CurrentSim -[hidden]right-> CurrentEsim
}
package "控制器、监控器与卡运行区域：业务职责交织" as CurrentRuntime {
  component "MultiSimController\n账户 / 绑定 / 激活 / 默认角色\nPrimary / 参数 / 缓存 / DB\n\nMultiSimMonitor / Helper\n状态监听 / 重载 / 回调\n初始化及产品差异" as CurrentControl
  component "RadioProtocolController\nRadio 三阶段切换 / session\n设备结果等待与超时处理\n\nState / File / EsimFile 等\n安全 / EF / Records / APDU\n卡短信 / 电话簿 / STK / 配置" as CurrentCard
  CurrentControl -[hidden]right-> CurrentCard
}
package "平台、存储与扩展协作区域" as CurrentPlatform {
  component "TelRilManager / TelRilSim\nTelephonyExtWrapper / 厂商 Hook" as CurrentRil
  component "SimRdbHelper / DataShare / 系统参数\nRegistry / CES / Hub / 账户回调" as CurrentStore
  note as CurrentPlatformNote
    平台访问与状态写入仍分散在既有组件中，此框不表示已经形成统一适配层。
  end note
  CurrentRil -[hidden]right-> CurrentStore
}
package "外部依赖与设备" as CurrentExternal {
  component "HDI / Modem / SIM / eUICC" as CurrentDevice
  component "数据提供方 / 系统服务 / 厂商实现" as CurrentProviders
  CurrentDevice -[hidden]right-> CurrentProviders
}
package "eSIM 高层任务边界" as CurrentRemote {
  note as CurrentRemoteNote
    另一路既有任务入口：不并入上述 Core 卡侧流程
    下载 / OSU 等 Framework 入口 → EsimServiceClient → IEsimService / SA 66250
    服务实现仓外；内部步骤及回调 Core 原语的完整调用链仍需匹配实现验证。
  end note
}
CurrentEntry --> CurrentServices : 入口委托
CurrentServices --> CurrentRuntime : 按用例分支协作，不要求串行穿过所有组件
CurrentRuntime --> CurrentPlatform : 各组件按原路径访问；设备结果经原处理链返回
CurrentPlatform --> CurrentExternal : 设备请求 / 存储访问 / 既有输出
' 隐藏连接仅用于纵向排版，不表示运行调用或源码依赖。
CurrentExternal -[hidden]down-> CurrentRemote
@enduml
```

Core 初始化 RIL，再条件装配 EsimManager、SimManager 和搜网；SimManager 装配每槽 State/File/Account/SMS/电话簿/STK 及多卡 Controller/Monitor。[F01](#src-01)、[F03](#src-03) 普通与低优先级 EsimFile 针对同 slot 创建，不因此获得两个独立 eUICC。[F12](#src-12)

源码列入根 `tel_core_service` 的 SIM/RIL/Core 尚未形成独立 SIM SO；eSIM 文件/控制器及宏受 GN 条件控制，EsimManager 源列入根表与运行时条件装配应分开看。SimCacheSyncManager 源存在但未见根构建或生产调用装配。[F02](#src-02)、[F28](#src-28) SimAccountManager 主要承接运营商配置，账户主缓存实际在 MultiSimController。[F04](#src-04)、[F24](#src-24)

#### 5.1.3 目标架构：按业务所有权划分上下文，按端口隔离技术依赖（PROPOSED）

```plantuml
@startuml SIM_Target_Architecture
!pragma layout smetana
skinparam componentStyle uml2
skinparam shadowing false
skinparam defaultFontName "Noto Sans CJK SC"
skinparam defaultFontSize 13
top to bottom direction
package "外部调用方与输入" as TargetEntry {
  note as TargetEntryNote
    既有 SIM / eSIM API；CoreManagerInner 共宿主调用
    近期不新增公开 Subscription API；66250 完整任务状态机仍在原服务边界。
  end note
}
package "API / Inbound Adapter：兼容边界 C" as Compatibility {
  component "IPC Stub / LegacyFacade / 同宿主门面 / 回调与观察适配" as Inbound
  note as CompatibilityNote
    保留参数与权限检查、错误优先级、调用者上下文、旧返回和回调格式。
    不拥有第二份业务写权；旧查询的异步回写由相应用例承接。
  end note
}
package "Application Layer：用例协调与唯一写入入口" as Application {
  component "T：TopologyService\n账户 / 绑定 / 激活 / 号码 / 映射用例\n资料查询及回写" as TopologyService
  component "A：AssignmentService\n角色 / Primary / Radio 切换用例\n角色/资源冲突准入 · 唯一重试 · Data 联动" as AssignmentService
  component "编排内核（原 O，非业务上下文）\nRequestContext / EffectKnowledge\nRuntimeOperationStore · 短请求关联" as OperationKernel
  note as ApplicationNote
    查询 / 存储 / 设备执行 / Host Ports；CompletionPlan
    调用下方 Domain 的纯规则；经 Ports 调用 R / I，协调各步骤效果与旧可见节点。
    涉及T/A事实的多步业务用例由目标Owner唯一主编排；R本地短请求仍在能力包应用入口。编排内核只提供机制。
    跨 Owner 子步骤保留父请求关联，不嵌套等待；旧进行中投影不代表设备空闲。
    简单查询无需经过编排内核；运行期记录不等于持久日志或跨重启恢复能力。
  end note
  TopologyService -[hidden]right-> AssignmentService
  AssignmentService -[hidden]right-> OperationKernel
}
package "Domain Layer：纯业务规则与状态模型" as Domain {
  component "Topology / T\nAccountRegistry · BindingIndex\n账户 / 身份 / 绑定 / active资料\n激活请求进度在T应用层；无EF/卡状态" as Topology
  component "Radio Resource / A\nAssignmentAggregate\n角色 / 期望分配 / AssignmentTransition\n不拥有设备执行租约" as Assignment
  component "共享值类型\nSubscriptionId / SlotId / ResolvedTarget\nSlotCatalog 只读视图" as SharedKernel
  Topology -[hidden]right-> Assignment
  Assignment -[hidden]right-> SharedKernel
}
package "生产卡运行支撑 R 与技术适配 I（并行协作责任）" as Support {
  component "R：卡运行子系统（CardRuntime仅窄委托）\n复用 State / Security / EF / APDU\nRecords / 短信 / 电话簿 / STK\n运营商配置 / eUICC 卡侧协议\nSessionRegistry / LoadSession\nResourceBarrier：真实共享设备域" as CardRuntime
  component "I：Owner专属Adapters + 共享技术客户端\nDataShare / 参数 / 存储端口实现\nRegistry / CES / 回调输出适配\n厂商能力 / Hook / 既有服务适配\n仅执行 Owner 的字段写计划\n不提供虚构的跨存储原子事务" as Infrastructure
  note as SupportNote
    以下是应用层经 Ports 调用的外侧实现；Domain 不直接调用它们
  end note
  CardRuntime -[hidden]right-> Infrastructure
}
package "外部依赖与设备" as TargetExternal {
  note as TargetExternalNote
    既有 RIL / HDI / Modem / SIM / eUICC；DataShare 提供方 / 参数 / 系统服务
    既有 66250 服务任务边界及厂商实现；不从本图推断设备实际部署。
  end note
}
package "宿主装配 H：Core 组合根" as Host {
  component "私有工厂 / HostPorts 注入 / 共享 RIL 所有权 / SlotCatalog 构建\n进程级单例唯一归属 / 启停及回调生命周期" as CompositionRoot
  note as HostNote
    横切装配职责：连接上述接口与实现，不是额外业务处理层
    具体类型或匹配 deleter 销毁；未排空不得卸载；同进程 SO 不等于故障隔离。
  end note
}
TargetEntry --> Compatibility : 原接口与输入协议保持
Compatibility --> Application : 命令 / 查询 / 观察进入对应应用入口
Application --> Domain : 调用纯规则，取得合法性判断 / 状态转换 / 执行计划
Application --> Support : 经 Ports 调用 R / I
Support --> TargetExternal : R 经 HostPorts 使用共享 RIL；I 访问原存储与服务
' 隐藏连接仅用于纵向排版；Domain 不调用 Support，Host 是横切装配职责。
Domain -[hidden]down-> Support
TargetExternal -[hidden]down-> Host
@enduml
```

图为逻辑层，不是物理目录：Application分布于T/A场景包、R能力包的短请求与`foundation/orchestration/`独立机制target；I适配分布于Owner的`adapters/`、C的`output/`和收窄的`infrastructure/`。R保留EF/APDU、解析、安全、STK等生产实现，CardRuntime不是全部能力的状态持有者；Application只抽出用例和事实写权。仅有内部规则才进入 Domain。简单查询不必经过编排内核或总线。66250 的完整任务模型不画进 Core Domain，任务边界见 §7.2。

#### 5.1.4 差异与收益

| 当前已核实结构 | 目标变化 | 实际消除的问题 |
|---|---|---|
| Controller 同时改账户、角色、参数并等待设备 | T/A 写权及各自用例编排 + R 执行；编排内核只提供机制 | 修改身份规则不需复制设备协议；部分效果与重试由目标事实 Owner 负责，不在第三个上下文往返 |
| 槽号同时编码端点种类、容量、路由和权限，散落于 SIM/RIL/搜网/Core（§3.3） | H 构建只读 SlotCatalog，T/A/R/C 按目录解释 | 增加或调整端点种类时修改目录与具名策略，而非逐模块搜索特殊槽号 |
| 进行中标志公开给搜网和运营商配置，但切换入口无准入 | A 的 AssignmentTransition、T 的激活进行中标志各自唯一维护 | 并发切换、激活等待与 UNKNOWN 归因有单一裁决点；收紧旧行为需 G3c 批准 |
| File 可回写卡状态，Monitor 重载账户 | R 统一状态接纳，T/A 按字段接收重载 | 避免旧路径与新 Owner 同时写入 |
| 返回、卡结果、缓存、DB、通知分离发生 | 保留各节点并记录效果维度 | 不把广播失败误作角色未变或把外层成功当卡成功 |
| slot、RIL serial、协议 session 各自存在 | 保留协议标识，增加内部作用域关联 | 防换卡、局部重载及迟到污染，不制造一个万能 Revision |
| Core/API/SIM 源码依赖交织 | 私有 Ports 与工厂注入，分阶段拆库 | 保留消费侧契约，真实链接图可验，无须新增公开 ABI |

#### 5.1.5 抽象的最小性

组件按责任实现，不按图逐框建Manager。最终源码区域统一为`services/multisim/`，一级按业务责任及支撑边界，T/A内部按用例族内聚，保留可检查的契约/纯规则/适配边界（T为api/accounts/adapters，A为api/domain/adapters）；不再推荐在`services/sim/`建立另一棵`domain/topology/application/topology`目标树。唯一完整稳态树见[详细设计§3.2](SIM_Multisim_Source_Architecture_Detailed_Design.md#target-tree)，本稿§6.4仅描述实际骨架，§6.6定义稳态约束。

逻辑Application不因取消同名目录而消失：计划、I/O协调、期望登记和失败恢复留在场景/能力应用入口；纯身份/角色规则才进入Domain。私有端口跟随消费场景，需要跨模块实现的窄契约从所属`api/`导出；不为每个用例创建一个接口。共享基础值与编排容器采用不同target；目录数量减少本身不证明解耦。新增抽象必须隔离真实变化或阻止反向具体依赖，纯函数和成熟协议直接复用。一个SIM模块可先在原SO承载，G2b拆SO及G2c生命周期仍分别验收，不新建SA。

### 5.2 源码编译依赖（箭头：使用方 → 被依赖接口/模型）

```plantuml
@startuml SIM_Compile_Dependencies
!pragma layout smetana
skinparam componentStyle uml2
skinparam shadowing false
skinparam defaultFontName "Noto Sans CJK SC"
skinparam defaultFontSize 13
left to right direction
title 源码编译依赖：箭头从使用方指向被依赖声明
component "C 兼容实现" as C
package "Application 入口 / 查询与出站 Ports" as Application {
  component "Application 入口" as Entry
  interface "Application Ports" as Ports
}
component "Domain" as D
component "R 卡运行支撑" as R
component "I 技术适配" as I
component "H 宿主装配" as H
C ..> Application
Application ..> D
R ..> Ports : 实现内层接口
I ..> Ports : 实现内层接口
H ..> Application : 构造并注入
H ..> C : 构造并注入
H ..> R : 构造并注入
H ..> I : 构造并注入
note bottom of D
  Domain 仅有纯规则与值类型；不反向依赖 R / I / H 或平台库。
  运行时应用层调用注入的 R / I 对象，不改变以上源码依赖方向。
end note
@enduml
```

R/I/H 实现应用定义的端口，编译依赖向内；运行时 Application 调用注入对象，方向可以相反。领域策略只依赖纯值类型/规则接口，DB/RIL/IPC/FFRT 的端口归 Application，不把技术接口伪装成领域实体。对 R 既有平台调用分切片收口；不声称当前全部代码已满足该依赖图。

### 5.3 运行调用及回调

典型本地写用例：`C → 目标事实 Owner（T 或 A）的应用用例 → 经编排内核登记关联 → R/I 执行 → 同一用例接纳结果 → 本 Owner 提交，必要时经对方应用入口提交其事实 → C 按旧出口完成`。简单资料修改可 `C → T → I`；卡状态只读可由 C 经应用定义的查询端口直接读取 R 注入的视图，无需登记关联。入站设备通知走 `R 接纳观察 → T/A 应用入口`，没有请求就不伪造 OperationId。

```plantuml
@startuml SIM_SetShowNumber_Sequence
skinparam shadowing false
skinparam defaultFontName "Noto Sans CJK SC"
skinparam defaultFontSize 13
hide footbox
title 运行协作示例：SetShowNumber（PROPOSED）
participant "C 旧入口" as C
participant "T 号码用例（应用层，唯一进程所有者）" as U
participant "R 卡运行支撑" as R
participant "T 账户写入口" as T
participant "I 现有存储" as I
C -> U : 保留原完成契约，关联当前卡应用
U -> R : 写 MSISDN
R --> U : 写卡结果 / 等待超时
U -> U : 接纳写卡结果
alt 明确失败
  U --> C : 按原错误完成
else 等待超时
  U --> C : 原错误
  note over U, R : 核对未知效果
else 成功
  U -> T : 账户入口
  T -> I : 现有存储
  I --> T : 存储结果
  T --> U : 缓存接纳结果 / 原错误
  U --> C : 按原完成契约输出
end
note over C, I
  卡成功、存储失败：保留卡侧效果，不自动补偿；完整成功保留原账户通知。
  CoreServiceSim 投递任务后返回；不等待业务完成，也不保证 IPC reply 先于回调。
end note
@enduml
```

图描述目标协调，局部旧行为见 [F08](#src-08)。CoreServiceSim 投递后不等待业务完成，图末尾是业务回调而非 IPC 同步返回；工作线程可立即执行，不能承诺回调一定晚于受理 reply；原写卡函数返回 false 时不进存储，但超时不证明设备未写入。晚到结果按 §11/§12 核对，不补发已结束调用的成功；卡成功/DB 失败不自动补偿卡。[F15](#src-15)

### 5.4 Compatibility 的准确约束

C 无独立业务写权，但可调用有副作用的正式查询用例。LegacySlotView 是 T/A/R 受控发布的只读投影，不是另一套可写模型，也不是所有查询唯一数据源。GetShowNumber 保留卡缓存→账户→IMPU 回退及异步 T 回写；QueryImsSwitch 保留按 ICCID 的存储读取。[F09](#src-09) 保持签名之外，还要保持权限/错误优先级、接收返回、业务回调、读取来源、中间可见性、通知次数和必要偏序，详见 §9 / §16。

现有入口层还持有两类业务逻辑，迁移时须交还 Owner：CoreServiceSim 的 SetActiveSim/SetPrimarySlotId 预写 userPreferPrimarySlot（交 A，WU-04）[F32](#src-32)；`CoreServiceSim::GetDefaultVoiceSlotId` 按调用者能力把槽 3 译为 INVALID 或 M0 所在槽 [F41](#src-41)。后者保留为 C 的旧投影规则，但 Radio 输入改读 A 发布的视图，不直接查询协议控制器。

---

## 6. 目标组件、接口与原功能承接

### 6.1 最小组件及数据契约（PROPOSED）

下列是职责和内部接口建议，不是要求新增同数目的类或公开 API。数据使用私有 C++ 值类型；不变更现有 DTO、IDL、接口虚表或 schema。

| 组件 / 所在层 | 核心输入输出 / 唯一状态 | 为什么需要；不采用的风险 |
|---|---|---|
| TopologyService / Application；AccountRegistry、BindingIndex / Domain | `AcceptAccountPatch(target, expectedInputs, patch)`；账户字段、绑定、激活进行中标志、局部版本；激活/号码/映射用例的进程所有者 | 汇合初始化/重载/后台写，防账户和路由规则散落；不接受角色字段整行覆盖 |
| AssignmentService / Application；AssignmentPolicy、AssignmentTransition / Domain | `PlanAssignment(facts, legacyPolicy)` 产生纯计划；`AdmitTransition(conflictKeys, target, requestRef)` 登记切换/激活冲突资格；`AcceptAssignmentStep` 接纳参数/设备/持久化各结果；发布进行中视图（兼容 IsSetPrimarySlotIdInProgress） | 区分期望与观察，防 Primary/Data/角色多写；同一 Radio 域同时至多一个切换，唯一业务重试在 A；原 RadioResourceArbiter 是此规则职责，不是另一套设备锁 |
| 编排内核（原 OperationCoordinator）/ Application | `Begin`、`AcceptResult`；RequestContext、EffectKnowledge、waitState、terminal | 为多步用例保留部分效果和等待，供进程所有者复用；内核本身不持业务事实、不决定重试；单步请求只用 RequestContext |
| RuntimeOperationStore / Application 私有容器 | 原 OperationRepository 的近期实现：进程内关联与有限终态保留 | 明确非持久日志，防“Repository 即可重启恢复”；无持久恢复需求不新增表 |
| SlotCatalog / H 构建，只读注入 T/A/R/C；Core 侧 RIL、搜网同源注入 | 每个索引的端点种类、允许连接/共享资源能力、可见性/权限类别、可运行期加入标记，及各旧容量口径；不存当前路由或已装配数 | 集中解释槽号（§3.3、§17.1）；不采用则特殊槽号继续散落；首次引入须与旧函数逐产品等值 |
| CardRuntime / R；SessionRegistry、LoadSession / R | 分类目标解析到生产对象；SessionRef、LoadToken、请求集合 | 复用协议且隔离换卡与 Refresh，防当前代次补填及重复计数 |
| ResourceBarrier / R | `Acquire(resourceKeys)`、`AcceptRecoveryEvidence(oldRequest, evidence)`；租约、fence、Checking/Available/InFlight/Quarantined | 约束真实共享 Modem/eUICC，防对象锁各自独占及超时后双发；按 §12 门禁启用 |
| AccountStore / AssignmentStore Ports / Application；I 实现 | 字段级写计划、现有 URI/参数、逐步结果；原 Topology/AssignmentRepository 名称可作为接口别名 | 隔离存储协议；禁止用通用 Save 整行覆盖对方字段，不要求两个独立数据副本 |
| LegacyFacade / C；CompletionPlan / Application+C | 原参数/调用者上下文→分类目标；原返回/回调/输出轨迹 | 保持旧边界，避免异步后权限变成服务身份或统一延迟通知 |
| Capability / Host Ports；H 实现、注入 | SlotCatalog 之外的宿主能力：共享 RIL、IMS 回退、卫星状态及原 CMI 反查的非 SIM 能力；工厂创建/销毁令牌 | 解开 SIM 对宿主具体符号依赖；能力未知不能默认等于 slot 数 |
| VendorCapability / Execution / Notification Ports；I 适配；Command 为应用入站 | 现有 Hook 能力查询、副作用执行、输出及正式用例输入 | 区分入站命令与出站执行；隔离厂商变化，保留既有 ABI，不新增一套厂商 SDK |

`ResolvedTarget` 为 `SlotTarget | CardApplicationTarget | SubscriptionTarget` 类型化联合：空槽可有 SlotTarget；PIN/EF 可在没有 simId 时使用 CardApplicationTarget；离线账户属性使用 SubscriptionTarget。卡侧执行再解析当前有效绑定。不得用任意 optional 字段组合伪造合法目标。

`SessionRef = (runtimeEpoch, resource/application key, bindingGeneration)`；`LoadToken = (SessionRef, LoadScope, recordsGeneration, requestRef)`；`RequestContext = (originTarget, step, attempt, transport correlation, completionPlan)`；`EffectKnowledge = NotSent / Rejected / ConfirmedApplied / ConfirmedNotApplied / Unknown`。这些都是内部数据，不追加到旧 IPC。RequestContext/EffectKnowledge、callback、调用者引用、等待设施、CompletionPlan 和 RuntimeOperationStore 属于 Application/C；协议关联及资源句柄属于 R/I；A的AssignmentTransition、T的激活进行中事实均由所属Owner受控维护；纯业务规则进入Domain，带请求/session资格的兼容进度由T应用接纳协议管理，不混入AccountRecord。不得把这份数据总览实现成一个导入平台类型的领域大对象。

### 6.2 原功能到目标的架构映射

完整明细沿用 [62 项稳定功能及 8 子项](SIM_Architecture_Refactor_Design.md#feature-mapping)。以下摘要覆盖全部主编号；范围表示原编号集合，不创建竞争编号。唯一写者指相应事实，I 只是受托存取者。

| 原功能 ID / 模块 | 目标上下文 → 层/组件 | 唯一写者 | 保留、拆分或委托方式 |
|---|---|---|---|
| SIM-001～004；Core/SimManager 生命周期、入口、槽能力 | H/C，T → 装配/兼容/TopologyService；H 构建 SlotCatalog | H 实例与 SlotCatalog；T 槽运行清单；C 订阅与完成 | 保留公开入口、检查次序和部分就绪；配置容量与槽种类只读目录，不把公开容量当硬件并发数 |
| SIM-005～011；StateManager/Handle、PIN/PUK/认证/IO | R；应用短请求关联 → 卡状态/安全/IO | R 卡状态/锁因；请求关联属编排内核，不是业务事实 | 保留协议与剩余次数，Slot/CardApplication 目标不强求账户；不用全局操作聚合 |
| SIM-012～019；File/Controller/解析、Refresh、号码/语音信箱/CFIS/运营商名 | R → application/records/codec/telephony records | R 对应会话/加载 scope | 保留卡型和算法，I/O 不进 Domain；网络侧通话、转移和驻网仍在原模块 |
| SIM-020～022；Controller/Monitor 账户建立、缓存及映射 | T → 发现用例/TopologyService/纯身份规则；A 初始角色与主卡复核 | T 账户/绑定；A 角色字段 | InitData 仅账户步骤归 T；force 激活委托 T 激活用例与 R，主卡后续委托 A；重载不覆盖其他 Owner |
| SIM-023.a～c；名称、号码写卡、查询回写 | T 资料与号码用例（T 为进程所有者）；R MSISDN | T 账户资料；R 卡记录 | 名称仅存储；号码先卡后库；查询异步回写仍入 T；保留部分失败 |
| SIM-024；激活/卫星分支 | T 激活用例 → Application；A 切换准入；R 执行 | T active 投影及激活进行中标志；R 设备观察 | force/普通/卫星分别沿原语义，不以 active 等同 READY；G2 保留等待 Primary 的有界超时 |
| SIM-025～030；Voice/SMS/Data/Primary/Radio/DSDS | A → Application/Domain（含切换用例）；R 协议 | A 角色/分配、AssignmentTransition 与唯一业务重试；R 协议会话 | 保留参数预写、回滚、重试和 Primary→Data 后续；无双发设备命令；槽 3 换路经 T 映射用例与厂商端口 |
| SIM-031.a～c；标签/在位资料/映射开始 | T 资料/映射标志及映射用例；R 软件状态输入 | T 资料；R 卡状态 | 标签不是身份或 Port；人工 ABSENT 不是物理拔卡或映射完成 |
| SIM-032～035；Observer/账户回调/服务重连/用户变化 | C/I/R/T → 订阅、观察、重载 | C 对外登记；I 平台 token；T 账户加载；R 卡加载 | 保留首帧、重复和死亡清理；重连不是新插卡；全部后台入口委托 |
| SIM-036～040；SIM SMS、ADN/FDN、PBR/ANR/IAP、STK | R → sms_storage/phonebook/stk；C 输出 | R 卡缓存/会话 | 保留实际卡读写及安全前提；完整短信业务/联系人不迁入 |
| SIM-041～044、SIM-045.b；SimAccountManager/Operator*/Tracker | R → carrier/config/privilege；I 提供方/Hook | R 配置、能力观察和匹配缓存 | 类名含 Account 不整类入 T；保留 APN 初始化、证书及规则读取 |
| SIM-045.a；IMS 开关偏好 | T → 资料用例；I store | T IMS 偏好 | 保留 SaveImsSwitch/QueryImsSwitch 路径；IMS 注册不归 A 或 T |
| SIM-046；下载、OSU 等服务任务 | 仓外服务 → IEsimService/66250 | 服务原任务主编排者，待匹配实现核实 | Core 仅承接既有本地原语，不复制完整任务或补偿 |
| SIM-047～054；EsimManager/File/Controller | R → euicc 查询/通道/认证/BPP/Profile/通知/CA；应用短请求关联 | R 卡缓存/资源；T 接受绑定及槽位标签恢复提示；A 接纳偏好提示 | 普通/低优先级共用设备域；有开关通道/APDU 的 Get 同样参加资源准入；保留双结果、卡通知、数量和原参数含义 |
| SIM-055～056；DataShare/helper/卡探测 | I store；T/A 字段计划；R 探测 | T 账户资料；A 角色；I 句柄 | 不新增直写 RDB；卡探测重载也接管；已有 weak_ptr 装配时点风险单独评审 |
| SIM-057～060；EXT/VSIM/卫星、serial、CoreManagerInner、诊断 | I/H/C；应用请求关联；R 协议 | 各事实 Owner；诊断不写业务状态 | 保留产品分支、宿主消费门面、RIL serial 和协议 session；VSIM 槽由 SlotCatalog 声明，不把统计当完成 |
| SIM-061；Core FactoryReset | 原搜网模块 | 原网络 Owner | 不自动删 Profile、ResetMemory 或新增远端任务 [F31](#src-31) |
| SIM-062；SimCacheSyncManager | 未装配源；未来 T/user_scope 候选 | 当前不授予新 writer | 保留文件，不因存在源码而启用或宣称现行业务 |

依据 [F01](#src-01)～[F50](#src-50)中的相关证据及既有功能原文。映射覆盖不等于所有方法和条件分支已动态验证。重构设计 §5.1 中 `O/...` 形式的目标路径保留为追溯标签，其进程所有者按 §4.2 解读，待该文同步。

### 6.3 写权切换：以状态单元而非一个公开方法为单位

沿用 [WU-01～09](SIM_Architecture_Refactor_Design.md#writer-cutover)：账户全量/当前缓存、active、Voice/SMS、Primary/Data、资料、卡状态/Records、操作进度、eUICC、订阅/加载分别切换。迁移前扫描字段赋值、可写引用和间接写者，覆盖 InitData/InitIccId/建行、GetListFromDataBase/GetAllListFromDataBase、Monitor RefreshData/用户切换/服务就绪、号码异步回写、force 激活、重试/失败恢复、卡探测和 EXT/VSIM/卫星分支。[F04](#src-04)、[F05](#src-05)、[F09](#src-09)、[F23](#src-23)、[F27](#src-27)

下表补齐每个 WU 的字段、重载和接管时点；详细旧入口继续沿用原 WU 清单。第一接管时点与最终编排迁移分开，后续阶段不能留下早已转移字段的直接赋值。字段写入及版本检查由 Owner 的短执行域保护，I/O 锁外进行但冲突资格覆盖整个调用（§9.2）；旧 DTO 由只读字段片段组装，不再整行回灌。

| 稳定 WU / 字段与读源 | 唯一写入口及初始化/重载 | 第一接管与后续切片 |
|---|---|---|
| WU-01 当前/历史账户容器、simId/slot/绑定索引；旧列表/映射查询 | T 接纳 InitIccId/建行、SortCache/SortAllCache、Monitor/卡探测/重查；角色列交 A；账户索引改按 SubscriptionId；旧 DTO 投影复现槽下标布局，是否复现 `simId - 1` 布局按稀疏 simId 的旧轨迹采集结果裁决 | G3a 同次关闭旧容器写权；建行与重载已有 A 角色桥；稀疏 simId 的旧可见行为先采集（§3.3）；不等 G3c |
| WU-02 isActive 账户投影；旧 active 查询 | T 接纳 InitActive、普通/卫星激活、UpdateDBActiveByIccId、ForgetAllData 及重载；force 保持无新增账户写 | G3a 即将尚在旧 Controller/R 中的激活缓存/DB 写委托 T；G3c 再迁执行、重试与激活进行中标志（见 WU-07） |
| WU-03 isVoiceCard/isMessageCard、defaultVoiceSimId_/defaultSmsSimId_；读法见 §8.2 | A 规划建行/更新角色片段，接纳默认卡 URI、当前缓存/通知去重值及重载；T 仅投影 | G3a 同时接管所有角色字段入口；不让旧 setter 与新 A 并存写 |
| WU-04 Primary/Data 参数、last/广播去重值及用户偏好提示；参数→启动 last，运行查询见 §8.2 | A 接纳 InitMainCardSlotId、Save/Resume/SavePrimarySlotId、Data 保存及 CoreServiceSim/EsimManager 的偏好写；isMainCard/isCellularDataCard 的建行/重载片段同归 A，非运行查询真值 | G3a 先收口 A 角色片段；参数/last/提示/去重值作为完整子单元在 G3c 同次切换，所有直接入口提前委托；切换用例迁移后不允许其他路径直写 |
| WU-05 名称/账户号码/标签及 Profile 关闭的槽位标签恢复提示/IMS/探测资料；旧卡→账户→IMPU 或 DB 读源 | T 属性入口接管 SetShowName/SetShowNumberToDB、InitShowNumber、查询异步回写、eSIM 建行/资料、DisableProfile 写入的 `last_deactive_profile_slot0/slot1` 提示、卡探测与重载 [F50](#src-50)；R 单独拥有 MSISDN。无仓内读者的 `last_deactive_profile` 不作为 T 的业务字段，只在本 WU 的写路径清单中登记，按原节点作旧外部输出 | G3a 接管旧写调用（包括尚在 R/eUICC 的提示写桥）；固定目标/拒绝陈旧回写的行为增强按 G3b/§9.5 单独验收 |
| WU-06 卡状态/锁因、ICCID、Records/电话簿及 loaded；原 R 视图 | R 接纳 Handle 设备/软件输入、Manager、文件完成、Refresh/ClearData；不建平行卡状态机 | G3b 同次接管全部相关写者，保留 §10.2 的旧卡型和过滤语义；未迁的 G3c 旧用例经私有桥登记/撤销 ExpectedTransition，不能让 R 反向启动业务流程 |
| WU-07 本地进度/等待/重试及进行中标志（`isSetActiveSimInProgress_`、`isSetPrimarySlotIdInProgress_`、`targetPrimarySlotId_`、重试计数）；原方法结果 | 各用例的进程所有者接管步骤、重试与进行中标志：Primary/Radio/DSDS 归 A 的 AssignmentTransition，激活归 T；编排内核只提供关联与等待；R 继续拥有 serial/协议 session | G3c 逐用例关闭旧重试发令和 detached 线程自调用；更早已转移的 T/A 字段从其切换时即委托，禁止等到此阶段再关旁路 |
| WU-08 eUICC channel/Profile 缓存、普通/低优先级实例与关闭结果 | R/euicc 接管两组对象和清理；应用短请求关联；T 接受实际绑定 | G3d 统一相关资源入口与查询；资源键/恢复证据不足只保留该旧用例，不启用新屏障 |
| WU-09 账户回调/死亡登记、平台 token、账户/卡 loaded 和重试计数 | C 登记、I token、T 账户加载、R 卡加载分别唯一维护 | T loaded 随 WU-01 于 G3a、R loaded 随 WU-06 于 G3b 接管；G3d 再迁外围订阅壳，不能届时才关闭旧标志写者 |

切换顺序：旧入口先委托唯一应用写入口 → 核对所有路径 → 停止受影响受理并排空或隔离在途 → 导入原语义的分字段状态 → 一次转移写权 → 删除旧直接写。T 切换时，尚未重构的 A 策略经旧实现桥提供角色片段；不得留下 T 建行/重载偷偷重写 A 字段。桥是私有委托函数，不能保存第二份可写缓存。只读 shadow 不发设备请求、不写存储、不发旧通知；不引入运行期双实现 Flag。

---

### 6.4 初版源码骨架记录与 IDE 导航（2026-10-29，未接入生产）

**版本说明：** §6.4 保留 `36deb95c` 初版的建立与检查记录；本轮合并后对接口和模型的修订见 §6.5，当前逐文件说明见 [源码详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md)。初版的数量、方法深度和局部缺口不作为当前源码的关闭证明。

**初版历史完成的是可浏览源码结构，不是生产迁移。** 源码根目录为 [`services/multisim/`](../services/multisim/)，覆盖本设计职责内的普通 SIM、eSIM 与相关管理；[`services/sim/`](../services/sim/) 仍为唯一活动生产实现。本批没有移动旧源码、接入 BUILD.gn、修改公共接口/ABI/IDL/schema，也没有切换写权或启用第二套对象、缓存、设备执行和重试。所有新增源码标明 TARGET ARCHITECTURE SKELETON / PROPOSED；设计仍为 draft/pending。

初版历史输入分支为 `docs/publish-local-doc-updates-20261029`，HEAD 为 `efbefd48cb20c2c19e5c1586528af435de518cbd`。编辑前工作区有 925 项已有修改；本次保留它们，未暂存、提交或推送。生产源码定向检查的工作树内容与该 HEAD 忽略行尾后的差异为空；这里记录本次源码阅读输入，不改写 §1.4 / §20 中各轮历史证据基线。

<a id="641-目录与职责裁决"></a>

#### 6.4.1 当前骨架目录与职责（不是最终稳态树）

当前实际文件树与功能导航见[源码README](../services/multisim/README.md)，完整清单见[详细设计§3.1](SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree)。

目录按 §4.1 / §4.2 的明确裁决组织，**没有 `operation/domain`、OperationAggregate 或集中执掌业务的 OperationCoordinator**。流程对照文档原 §6 的“五目录”与旧 O→I 预写参数示意不能覆盖该裁决；该文的关键开发视图与三条流程已同步。O 的等待、效果与终态均是 Application 数据，不因使用纯 C++ 值而变成 Domain。T/A 拥有各自进行中事实和主编排；简单查询、R 内部加载不强制进入运行容器。

当前为86个C++文件及BUILD/README；原application/model已撤除，Owner专属Adapter已归位。生产仍为services/sim；本区域target全部testonly。物理目录、逻辑层、namespace、GN target与SO/运行对象不是一一对应关系。

<a id="642-设计元素源码位置原功能来源"></a>

#### 6.4.2 当前设计元素—实际源码位置—原功能来源

下表路径以 `services/multisim/` 为根；没有 `.cpp` 的声明按文件注释保留待实现，未提供假成功。功能/WU 编号沿用 §6.2 / §6.3，不新增竞争清单。

| 设计元素 / 承接部分 | 目标代码位置 | 原文件、类及方法 / 继续复用部分 |
|---|---|---|
| 纯身份、位置、分类目标；H只读目录 | [`model/sim_identity.h`](../services/multisim/foundation/sim_identity.h)、[`slot_catalog.h`](../services/multisim/foundation/slot_catalog.h) / `.cpp` | 既有 simId/slotId 语义及 `sim_utils`、capability_mgr、EXT 的容量与路由输入；不新分配身份，目录首次引入须逐产品等值 |
| T账户与位置绑定；WU-01/02/05/09 | [`topology_service.h`](../services/multisim/topology/topology_service.h) / `.cpp`；[`account_registry.h`](../services/multisim/topology/accounts/account_registry.h)、[`binding_index.h`](../services/multisim/topology/accounts/binding_index.h) / `.cpp` | `MultiSimController::InitData/InitIccId/GetListFromDataBase/GetAllListFromDataBase/ForgetAllData/InsertEsimData`；账户按simId，绑定按位置，不接管R原始状态或A角色 |
| T号码主用例与应用兼容步骤计划；SIM-023.b | [`set_show_number_use_case.h`](../services/multisim/topology/account_profile/set_show_number_use_case.h) / `.cpp`、[`show_number_plan.h`](../services/multisim/topology/account_profile/show_number_plan.h) / `.cpp` | `CoreServiceSim/SimManager/MultiSimController::SetShowNumber`；拆开编排、卡效果、账户写与原完成；`SimFile::UpdateMsisdnNumber` 协议仍复用 |
| T号码查询及独立回写；SIM-023.c | [`show_number_query.h`](../services/multisim/topology/account_profile/show_number_query.h) / `.cpp`、[`number_query_host_port.h`](../services/multisim/topology/api/number_query_host_port.h) | `MultiSimController::GetShowNumber/GetSimTelephoneNumber`、`TelFFRTUtils::Submit`、Core的IMS客户端；保留卡→账户→IMPU读源，查询不等回写 |
| T字段存储与接纳；非事务整体Save | [`account_store.h`](../services/multisim/topology/api/account_store.h)、[`account_store_adapter.h`](../services/multisim/topology/adapters/account_store_adapter.h) / `.cpp` | `SetShowNumberToDB/UpdateDBSetActiveResult/SaveImsSwitch/QueryImsSwitch`；`SimRdbHelper::UpdateDataBySimId` 与原 DataShare URI/键继续复用；Adapter不写账户缓存 |
| T激活主用例，A准入/后续复核；SIM-024 | [`activate_sim_use_case.h`](../services/multisim/topology/activation/activate_sim_use_case.h) / `.cpp`、[`activation_runtime_port.h`](../services/multisim/topology/api/activation_runtime_port.h) | `SetActiveCommonSim/UpdataCacheSetActiveState/SetActiveSimToRil/SetActiveSimSatellite`；保留普通、force、卫星的不同节点、两次Hook和部分失败，不把active等同Ready |
| A角色与Primary主编排，唯一业务重试；WU-03/04/07 | [`assignment_service.h`](../services/multisim/assignment/assignment_service.h) / `.cpp`、[`primary_switch_plan.h`](../services/multisim/assignment/primary/primary_switch_plan.h) / `.cpp`、[`assignment_aggregate.h`](../services/multisim/assignment/domain/assignment_aggregate.h) | `SetDefaultVoice/Sms/CellularDataSlotId`、`SetPrimarySlotId/SavePrimaryCardInfo/ResumePrimaryCardInfo/RetrySetPrimarySlotId/CheckIfNeedSwitchMainSlotId`；参数、意图和设备观察分开；入口和eSIM偏好写一同归A |
| A按原协议存储/输出、R执行 | [`assignment_store.h`](../services/multisim/assignment/api/assignment_store.h)、[`radio_execution_port.h`](../services/multisim/assignment/api/radio_execution_port.h)、[`assignment_store_adapter.h`](../services/multisim/assignment/adapters/assignment_store_adapter.h) | 原默认卡URI、`SetParameter`、广播、`RadioProtocolController` 三阶段协议；参数恢复不冒充硬件回滚，阶段结束不冒充成功 |
| O应用编排机制，非业务上下文 | [`request_context.h`](../services/multisim/foundation/orchestration/request_context.h)、[`runtime_operation_store.h`](../services/multisim/foundation/orchestration/runtime_operation_store.h) | 从Controller/File散落的等待和步骤中收口发起关联/效果/终态；T/A各自消费，不持有业务事实、不发令、不决定重试、不提供持久重放 |
| R完整卡运行与窄端口实现 | [`card_runtime.h`](../services/multisim/runtime/card_runtime.h) / `.cpp`、[`local_card_requests.h`](../services/multisim/runtime/api/local_card_requests.h) | `SimStateManager/Handle/Tracker`、`SimFileManager/IccFile/SimFile/RuimFile/IsimFile/*Controller`、`IccDiallingNumbers*/UsimDiallingNumbersService`、`SimSms*`、`Stk*`、`EsimManager/File/Controller`；保留成熟协议、解析和卡缓存，不复制实现 |
| R配置/权限/APN、eUICC库存与原双结果 | `CardRuntime::ProductionSlotBindings`、`SwitchLocalProfile`、`LocalCardRequests` | `SimAccountManager`、`OperatorConfig*/MatchingRule/IccOperatorPrivilegeController` 类名不决定归T；本地Profile结果保留外层int/cardEnum，下载/OSU仍属既有66250任务边界 |
| R会话/加载/资源；WU-06/08 | [`session_registry.h`](../services/multisim/runtime/session/session_registry.h)、[`load_session.h`](../services/multisim/runtime/records/load_session.h) / `.cpp`、[`resource_barrier.h`](../services/multisim/runtime/resource/resource_barrier.h) | `ProcessSetMsisdnDone/ProcessIccRefresh/ProcessFileLoaded/OnAllFilesFetched`、`RadioProtocolController::ProcessResponseInfoOfEvent`；治理原真实写点，不外套一层即宣称安全；屏障需额外证据才启用 |
| C原入口、原通知/完成出口；I交付 | [`legacy_sim_facade.h`](../services/multisim/compatibility/legacy_sim_facade.h) / `.cpp`、[`legacy_completion.h`](../services/multisim/compatibility/output/legacy_completion.h) / `.cpp`、[`legacy_output_adapter.h`](../services/multisim/compatibility/output/legacy_output_adapter.h) / `.cpp` | `CoreServiceSim::SetShowNumber` 检查/队列/Transfer，`SimManager::SetShowNumber` 成功账户通知，`MultiSimMonitor::NotifySimAccountChanged`；不持业务缓存、不改公开契约 |
| H私有工厂/宿主接缝 | [`sim_module_factory.h`](../services/multisim/assembly/sim_module_factory.h) / `.cpp`、[`host_ports.h`](../services/multisim/assembly/host_ports.h) | `CoreService::OnStart`、`SimManager::OnInit/Init/InitTelExtraModule`、eSIM原装配；Core保留共享RIL、原发布顺序与单例唯一定义处，SIM只持引用/释放自身注册 |

每个端口有对应使用方/实现方/注入方：T的MSISDN/激活设备端口→`CardRuntime`，T激活Hook端口→`ActivationHookAdapter`，T账户存储→`AccountStoreAdapter`，T号码完成→`LegacyCompletion`，C原输出→`LegacyOutputAdapter`，A存储/Radio/输出→对应I/R实现；`SimModule` 构造函数展示实际注入。`LegacyIngressPort`、`CallbackHostPort`、`NumberQueryHostPort` 的实现未来留Core，由 `SimHostPorts` 注入，本批没有假造可用宿主。R结果桥经`BindResultSinks`连接窄结果接口，H注入具体用例；不引用具体应用类。真实回包仍需weak owner、epoch及在途排空，不注册第二套生产事件。

#### 6.4.3 推荐 IDE 阅读路线

1. 打开 [`compatibility/legacy_sim_facade.cpp`](../services/multisim/compatibility/legacy_sim_facade.cpp) 的 `SetShowNumber` / `RunQueuedSetShowNumber` 伪代码，再跳到 [`SetShowNumberUseCase::StartAtExecutionPoint`](../services/multisim/topology/account_profile/set_show_number_use_case.h)。原SystemApp→权限→空值→调用者槽能力的检查在C/宿主端口；受理返回不等待业务，也不保证IPC reply早于callback。
2. 查看 [`PlanShowNumberUpdate`](../services/multisim/topology/account_profile/show_number_plan.cpp)：MSISDN→DB→当前账户缓存→历史账户缓存，账户失败不反向写卡。`NumberExecutionTarget` / `MsisdnWriteRequest` 保存相关绑定、号码字段版本、会话和step/attempt。
3. 沿 [`ShowNumberRuntimePort::WriteMsisdn`](../services/multisim/topology/api/show_number_runtime_port.h) 跳到 [`CardRuntime::WriteMsisdn/AcceptMsisdnWriteCompletion`](../services/multisim/runtime/card_runtime.h)，再对照旧 [`SimFileManager::SetSimTelephoneNumber`](../services/sim/src/sim_file_manager.cpp) → [`SimFile::UpdateMsisdnNumber/ProcessSetMsisdnDone`](../services/sim/src/sim_file.cpp)。协议和卡缓存属于R；`false`包含等待超时，不能一律推断未写卡。
4. 回到 [`AcceptMsisdnResult/ContinueAccountUpdate`](../services/multisim/topology/account_profile/set_show_number_use_case.cpp)，沿 [`TopologyService::AcceptNumberAfterCardWrite`](../services/multisim/topology/topology_service.cpp) → [`AccountStore::WriteDisplayNumber`](../services/multisim/topology/api/account_store.h) → [`AccountStoreAdapter`](../services/multisim/topology/adapters/account_store_adapter.cpp)。看 `NumberAcceptance` 中DB、当前缓存、历史缓存的独立结果，及跨I/O的目标/字段预留说明。
5. 沿 [`FinishWithLegacyResult`](../services/multisim/topology/account_profile/set_show_number_use_case.h) → [`LegacyCompletion::CompleteShowNumber`](../services/multisim/compatibility/output/legacy_completion.cpp) → [`LegacyOutputAdapter`](../services/multisim/compatibility/output/legacy_output_adapter.cpp) 理解整体成功才发原账户通知，随后交付原callback；不是全局事务或全局事件顺序。晚到结果只核对，不复活旧账户步骤或旧成功回调。
6. 旁读 [`ShowNumberQuery`](../services/multisim/topology/account_profile/show_number_query.cpp)，区分独立查询回写；再看 [`ActivateSimUseCase`](../services/multisim/topology/activation/activate_sim_use_case.cpp) 与 [`AssignmentService`](../services/multisim/assignment/assignment_service.cpp)：激活属于T，Primary切换/默认Data/重试属于A。最后看 [`SimModule` 构造函数](../services/multisim/assembly/sim_module_factory.cpp) 确认端口实现、对象寿命和注入链。

#### 6.4.4 未实现、局部缺口与生产接入边界

- **实现深度**：账户双投影/字段重载、发现结果资格、激活槽进度、Primary计划、完成路由、Profile短编排、会话期望、资源状态与发布身份已有可执行纯逻辑。真实业务写入/设备、完整入口、生产工厂/deleter、平台等待与排空仍未闭合；`.cpp` 的业务步骤明确标为伪代码，不能链接或调用为可用功能。没有生产构建/设备验证，也没有G2/G3迁移完成结论。
- **号码解析/准入**：旧入口排队仅捕获slot，任务执行时定位File，账户写时又取 `GetTargetSimId`；旧查询回写执行时也重新取目标。本骨架推荐在执行接纳点固定目标/会话并拒绝迟到错误提交，属于 §9.5 / §16.2 的G3b增强。G2须保留原解析点和逐分支错误，包括旧越界返回`false`落为整数0的异常轨迹；不能把推荐骨架直接替换成“等价实现”。前移到IPC接收时固定仍属TGT-OPEN-011评审范围。
- **查询回写与存储结束**：查询立即返回；固定回写关联是新用例。DB已成功但目标过期须核对，不能仅拒绝内存发布便宣称无效果。预留覆盖整个外部调用；callFinished/效果未知不能自动解锁或重试。provider可重入/跨行事务/CAS未证实，继续沿 §9.2 / §13 限制。
- **次要流程深度**：激活只展开关键声明和普通路径伪代码，完成路由契约已有表达，真实卫星技术端口、两次缓存完整流程与通知适配待生产切片补齐；A已有进行中/参数/Radio/重试入口，但第三卡换路仍需T映射用例与窄厂商端口，`AssignmentView` 尚未补全第三卡旧翻译所需Radio投影。均为局部待实现，不恢复O中央编排，也不假造成功。
- **R及外围**：实际 `ProcessSetMsisdnDone`、所有卡型Records写点、State软件/物理输入、电话簿/STK/eUICC仍在旧生产对象；准入必须接管这些真实写点。新屏障需资源键、错误/投递失败关联、结束与恢复证据；在证据闭合前保持未启用。66250匹配服务、厂商库、Port/MEP及N槽策略继续按 §21 局部缺口处理，不从目录推出产品能力。
- **完整写权闭包**：号码资料迁移须同时处理初始化、建行、Monitor重载、用户/provider重连、卡探测、名称/IMS/标签、异步回写和其他写入口；active需普通/force/卫星/拔卡及初始化；A需全部角色列、入口/eSIM偏好、参数恢复、重试、detached自调用。完整清单仍以 §6.3/WU为准，Adapter不能创建独立缓存或整行覆盖其他Owner。
- **宿主/链接**：Core仍创建并拥有共享RIL，Core HostPorts和消费侧CMI保持原位置。`SimModule`展示附着注入，`AttachSimModuleToExisting`与最终`CreateOwnedSimServices`用途分开，真实创建/匹配deleter仍待实现，不能旁路旧装配。未来先验证原初始化时点、失败清理、单例唯一定义和回调寿命；排空失败不卸载SO，SIM不停止共享RIL，不修改公共虚表。

原骨架建立批次的历史检查（2026-10-29；本轮整合复验见评审§10）：

- 对新增 **36个头文件、15个源文件** 分别执行 `clang++ -std=c++17 -fsyntax-only -Wall -Wextra -Werror`，include根为 `services/multisim`，全部通过。没有全量链接、OHOS构建、真实初始化、设备、IPC/ABI或Feature矩阵验证；仅声明的方法仍无定义。
- 临时主机规则检查通过：加载成员重复、父成员缺失、错误结果一次消费、错误epoch/卡应用/代次/scope、同槽换卡旧结果、Refresh受影响读失效且无关scope继续、写请求不按读结果消费，以及目录缺项/未知资源不回退槽0或Modem0。临时检查只链接纯 `LoadSession` / `SlotCatalog`，未创建或替换任何生产设备/业务对象，检查文件和二进制留在`/tmp`。
- 本地include路径存在且无引用环；Domain传递依赖仅纯Domain/model；Application/ports不依赖R/I/H/C具体实现；端口实现/构造注入对应、头保护和新增源码空白检查通过。源码没有新增同步原语、万能Execute、operation/domain或生产BUILD.gn。
- 两份文档本地链接、骨架导航锚点、代码围栏与 `git diff --check` 通过；未执行Mermaid渲染，不将图形渲染列为已通过。
- 与编辑前1277个跟踪文件的SHA-256摘要对照：仅两份指定文档内容改变；全部旧生产/构建/接口/测试文件及其已有修改保持原内容。暂存区为空、HEAD未变；未提交或推送。新增51个源码文件全部位于`services/multisim/`。



<a id="consolidated-contracts"></a>

### 6.5 分支整合后的契约修订（2026-10-29）

本节以 `main@964dc569` 和 `publish@2448b42e` 为输入，合并后保留第三/四轮复审，再审视[独立评审记录](SIM_Target_Architecture_Independent_Review_2026-10-29.md)的未处理项。这里只修正目标设计、未接入骨架与纯模型；`services/sim`、Core、GN、公共接口、schema、厂商ABI不变。静态模型检查不表示生产迁移完成。分支意见的采用/取代关系见 §20.6。

本轮在远端`main@6697ff7a`基础整合本地评审实施记录，保留已有共享RIL、窄T/A端口、epoch/RequestOwner及冲突资格，未覆盖远端并行修正。原本地符号与整合后实现的采用关系、验证命令见[评审报告§10](SIM_Target_Architecture_Independent_Review_2026-10-29.md#10-阶段一main整合与复验2026-10-30)。未包含下一阶段G2a生产接入。

#### 6.5.1 账户投影和重载

AccountRegistry 是一个 Owner，但 Current/历史两种兼容投影有各自字段与版本。显示名、号码、active、label、IMS等按旧公开可见节点接纳；不要求两个投影同时成功。AccountStore读取明确投影类别、load attempt与独立结果，分别复用 QueryAllValidData/QueryAllData；当前成功/历史失败时保持历史旧值及其重试责任。角色列从同次读取拆为A片段，不由T重算或整行反灌。

号码步骤计划是Application兼容编排，移出Domain。号码写资格依赖绑定、会话和号码字段版本，不因无关显示名变化而失效。资格覆盖锁外存储全过程；DB已写但资格失效时进入核对，不以拒绝内存发布宣称无效果。同值字段重载保留字段版本，仅变名称/标签不撤销号码资格；view各自的attempt、成员基线和删除接纳防止旧列表覆盖新成员；投影删除/重建保留字段版本水位，避免旧号码资格ABA。

#### 6.5.2 发现、联合建行与跨Owner接纳

R发出不可变发现/替换/撤销观察，携带发起SessionRef及单调观察序号。T按卡应用保存已接受的观察顺序，最后提交与相关失效输入在同一受控接纳域排序；重复、乱序或旧epoch不得重新绑定当前槽。单次R校验或T同步读取一次R当前值都不能覆盖检查后的窗口。

联合发现分成四个节点：T准备身份匹配/建行上下文（可尚无SubscriptionId）→A根据产品/角色输入及角色资格生成只读计划片段→I执行原Insert/Update并返回实际身份和效果→T接纳账户/绑定、A接纳所属角色投影。多步跨Owner子请求保留父子关联；初始角色片段以A预留资格关联T建行计划，不以新建前未知simId为A规划前置，不复制第二存储缓存，不承诺provider事务。锁外I/O期间观察/角色失效时，只能核对已发生持久效果，不盲目重放。原用例在调用已结束且确认未生效时，可按原预算在同一有效观察下显式取得新尝试；重复观察本身不重试，Unknown或已写效果先核对，不强求新的R事件。

空位置也保留按绑定键连续推进的revision。移除只清内容而不清版本；同一卡拔出再插不能使旧BindingRef重新有效。若仅路由组合变化，必须明确R会话与T绑定的分别失效范围。

#### 6.5.3 激活进度、冲突资格与过程状态

T继续拥有active投影及激活进行中事实，但进行中状态以slot、原SessionRef和请求资格维护轻量transition，不放在历史AccountRecord里。旧IsSetActiveSimInProgress(slot)读其兼容投影。开始、清零和晚到处理均核对实例/请求/step；首次UpdateSubState在DB前清零仍保留，false不代表设备空闲或请求终态。已完成请求的旧Begin不能复活槽进度；旧清理必须匹配完整原会话/请求，不能清掉后继请求。Hook保留先执行再清原进度的节点。

A原子登记激活/Primary冲突资格，T再向R申请真实设备租约。A资格、T兼容进度、用例等待与R租约是四种状态，不以一次bool更新同时释放。G2仍保持旧4s有界等待轨迹；新的排队/拒绝/隔离只在对应G3c验收后启用。旧用例尚未迁移时也通过桥登记/撤销ExpectedTransition，不能因G3c晚于G3b而无条件丢掉合法切换后结果。

请求引用至少在模块epoch及用例命名空间中唯一，step/attempt和父子关系在发起时保存。各RuntimeOperationStore的局部序号不能直接当R共享资源中的全局身份；通用机制不负责重试，不形成中央业务Owner。

#### 6.5.4 Primary模式与完成出口

A的计划显式区分普通三阶段、免重启专用RIL、已为M0及第三卡换路；静态SlotCatalog只给允许连接，不提供当前Modem分配事实。R返回真实能力/观察及原协议结果，A决定参数预写或成功后保存，不让R代写A。

普通模式保留预写→协议→广播→Data→Done及失败条件恢复；免重启模式保留设备在前、成功后保存、失败也Done(true)；已为M0保留无切换协议的保存/联动；第三卡子步骤保留原早返回节点和延后执行。Done不是成功码，Data成功不是数据连接完成。

C的完成路由明确原同步int结果、旧异步callback、内部回执三类。应用向具名窄出口完成，C/宿主负责原等待和通知节点；同步等待不能占住处理结果的执行域。原返回既可能等待设备，也可能是原分支的早返回，不统一解释为整体设备终态。晚到证据不能重复完成旧调用者。

#### 6.5.5 R结果、资源与本地Profile

R只引用应用定义的窄结果sink，由H绑定具体用例，不保存具体SetShowNumberUseCase/ActivateSimUseCase/AssignmentService指针。一个生产EsimManager内部拥有普通/低优先级EsimFile；对象数量不决定真实资源域。

ResourceBarrier的契约包含初始Checking核对、全资源集合一次Acquire、MarkSent、确定NotSent撤销、正常有效完成/通道关闭后的释放及Unknown隔离；恢复必须匹配原epoch、租约/fence和持有者。读取载荷丢失与写效果未知分别判定；旧证据不得释放新租约。纯规则不生成硬件结束事实，缺设备证据时不启用生产屏障。

本地Profile请求保留原portIndex/ICCID/forceDisableProfile，将可选已证实路由另存。force目前在原路径保存而未参与该APDU编码，不据此擅自删除兼容输入。具名本地短请求在发起前登记/清理ExpectedTransition，R执行原通道/APDU并保留双结果；A在旧File返回后接纳提示，即使卡enum失败也不悄然推迟。非法槽/File缺失不写提示，也不改原enum out；内部结果明确“未赋值”。等待结束不复活旧请求，期望只能接续带因果证据的已发布新观察，到期清理不释放未知设备租约。T绑定仅接受后续带资格观察。DisableProfile槽提示归T、无后缀last_deactive_profile为旧外部输出，继续按§13.2局部缺口处理；不复制66250任务。

#### 6.5.6 附着装配与最终创建边界

原SO内的附着工厂仅借用原SIM内部对象以支持渐进委托，不是独立SO创建边界。最终SIM侧工厂由本库创建并拥有原Manager及内部对象，返回原ISimManager/IEsimManager句柄及匹配控制块/deleter；Core只持消费接口、HostPorts和共享RIL，不取得内部Controller/store引用来装配。工厂分阶段初始化和发布仍沿原时点；发布验证要求同地址且同shared_ptr控制块，eSIM先初始化/发布，之后SIM初始化/发布。原Core未检查Manager OnInit返回，结构整合不新增失败策略；创建库匹配销毁，不Stop共享RIL。

G2b的业务导出限私有工厂/匹配销毁，工具链必需符号单列；不恢复绝对空导出要求。目录引入不是拆SO强制前提。容量逐调用点差分仍仅针对已识别的推导差异产品，并记录首次取值时点；动态卸载继续单列排空验收。


---

### 6.6 最终稳态源码组织与变化隔离（2026-10-30，PROPOSED）

本节取代§5.1.5旧分层路径及§6.4骨架作为“最终目录”的解读，保留§4～6.5的事实写权与安全协议。融合裁决与唯一完整树分别在[详细设计§4.3](SIM_Multisim_Source_Architecture_Detailed_Design.md#review-decisions)、[§3.2](SIM_Multisim_Source_Architecture_Detailed_Design.md#target-tree)；不复制逐文件清单。推荐仅此一套，不等于批准业务变更。2026-10-31已局部落地accounts/account_profile命名；当前与计划能力见详细设计§3。

| 一级目录 | 稳态职责与内部原则 |
| --- | --- |
| `include/` | 仓内导出白名单及私有创建/宿主契约入口；公开API仍在原innerkits/IDL，不复制定义或暴露可写聚合 |
| `foundation/` | 平铺纯身份、关联值与只读SlotCatalog；`orchestration/`另属Application机制target，供各Owner实例化，不是Domain共同依赖或全局请求中心；不放平台客户端 |
| `topology/` | T；`api/accounts/adapters`加`discovery/activation/account_profile`；映射请求就近归accounts或实际场景场景族，TopologyService受控写入口；场景目录含计划、用例及私有端口 |
| `assignment/` | A；`api/domain/adapters`加`roles/primary/account_roles`场景族，AssignmentService受控写入口；参数/设备偏序计划属于Application |
| `runtime/` | R；`api/session/resource/state/records/phonebook/sms_storage/stk/carrier/radio/euicc`直接二级分包；`records/codec`只承载实证可独立的解码子集，不套DDD四层 |
| `compatibility/` | C；旧门面按特性拆cpp，`api/`为宿主窄接缝，`monitor/`负责登记，`output/`负责完成/通知翻译；不保存业务缓存 |
| `infrastructure/` | I；仅跨Owner技术连接、参数访问等有实际复用的客户端；允许连接/重连等技术状态，不含业务写策略，不收拢Owner专属Adapter或共享进程EXT单例 |
| `assembly/` | H；工厂、同一对象/控制块、注入、原发布时点及匹配销毁；只有组合根可连接各具体实现，不承接业务分支 |

**边界实施规则。** T/A核心不依赖R/C/I实现或对方核心；调用通过独立契约target和H注入。对外入站服务由提供方发布，出站执行/结果契约由消费方定义；跨模块所需私有端口应选取导出到所属`api/`，其余留场景内。T/A互调不要求两个实现target互依，也不把“T永远高于A”写成业务不变量。契约之间必须成DAG，避免相互包含聚合、平台DTO或具体用例。具体target/public/visibility/check_includes及静态规则见详细设计§4；namespace只是命名约束，GN也不能替代运行时接纳。

`foundation`是物理父目录，不是一个总target：`sim_values`只含纯值/只读能力；同在orchestration目录的`request_protocol`只导出CompletionPlan/RequestContext等无平台应用协议DTO，供命令/执行契约依赖；`request_mechanism`再依赖该协议，包含各Owner的可变运行表，仅用例使用。Domain不得通过总依赖间接取得等待、RIL、DataShare、FFRT或平台类型。现RequestOwner安全命名空间继续保留，不因“删公共枚举”丢失epoch/Owner/序号、step/attempt及旧清理资格；本轮不重写标识协议。

**R内部状态不能因分包重复。** CardRuntime可保留为薄的受控分派/结果绑定接缝，协议和卡缓存归各能力组件；SessionRegistry按卡应用维护唯一会话水位，LoadSession归records加载资格，ResourceBarrier按真实跨槽Modem/eUICC共享域注入。每槽对象组不代表每槽独占资源，不复制原Manager、RIL、业务重试或第二组屏障。PIN/EF/本地Profile具名应用入口分别位于state/records/euicc，不另立必经access网关；Profile仍保留原参数、期望生命周期、T/A原副作用节点和双结果。下载/OSU的服务任务边界不变。

**完整替代与仓内依赖。** State、安全、各卡型File/Records/Refresh、电话簿/PBR、卡短信、STK、运营商配置/APN/权限及eUICC均有稳态落点。SimStateTracker按配置实际职责归carrier；CardFileDetectManager按探测输入、T存储翻译和T/A接纳拆分，不整类当文件协议迁R。PdpProfileRdbHelper随carrier；共享技术句柄不是第二业务Owner。原sim/include被Core/搜网/RIL/innerkits/卫星引用，须逐头核对导出与构建；纯解码/表格可独立target供RIL与R共同消费，禁止RIL反依赖整个R再绕回RIL。MccPool有FFRT/懒初始化，PlmnFile含Parcel，VoiceMailConstants调用配置/EXT，均不能仅按名字标为纯值。EXT继续由宿主共享唯一实例。

最终验收是旧目录全部活动功能及外部依赖得到承接、全部WU写权单次交接后移除旧生产路径；迁移期间的Attach/旧实现桥不成为永久兼容模块。必要的旧API语义适配继续存在，但不保留第二套旧实现。未装配SimCacheSyncManager不为“完整迁移”自动启用。

| 变化场景 | 主要修改轴及条件性传播 | 仍须证明的边界 |
| --- | --- | --- |
| 三卡到四卡/更多逻辑端点 | SlotCatalog/装配、T绑定、A候选与冲突、R硬编码、C旧投影；配置槽与Modem栈分别变化 | 不承诺只改配置；N:M拓扑、权限/容量宏/TU求值差分及产品矩阵 |
| MEP/Port/Profile规格 | R euicc/session/resource、T绑定、C解析；A仅在候选/共享资源语义变化时联动 | Port请求不等于观察；一Port一逻辑槽未必改schema，一槽多当前绑定需独立契约 |
| 硬件初始化/激活顺序 | R state/records/radio及T activation的兼容步骤；其他能力消费既定就绪语义 | READY/LOADED、Hook、错误及部分效果不变才可缩小传播 |
| Primary硬件握手/恢复 | A primary与R radio/resource，参数意图与设备观察分开 | 免重启/三阶段/厂商映射各自偏序，结束及回滚证据 |
| 协议下沉RIL | 优先调整R对应执行Adapter；仅在入站/结果契约等值时期待T/A稳定 | 当前Radio结果含phase/session，错误、完成、取消/超时改变时A/C与测试仍需联动 |
| RIL功能上移 | R承接新增协议/状态门控，T接纳绑定，A接纳角色策略；C保留原事件时序 | 不假定现有底层已负责某回退；不未经差分统一过滤Monitor输入 |

不采用“所有潜在联动为零”或“只需重跑修改包测试”的承诺。验证按真实头依赖、共享资源、条件编译及可见效果选择；目录本身不提供设备能力、事务或协议等值证据。G2a原SO、G2b工程边界、G2c生命周期、G3写权/行为增强、G4能力扩展门禁仍依§22分别推进。

---

## 7. 部署与跨 SA 协作

### 7.1 部署配置与目标链接

FACT：4010.json 指向 telephony 中的 `libtel_core_service.z.so`；telephony_trust.json 列出 4010、66250 等共宿主配置。配置不证明设备已安装/加载，独立 SA ID 不等于独立进程。现有 CoreManagerInner 实现在 API SO，不应把其每次反查都称为当前 SIM↔Core SO 环，因为当前尚无 SIM SO。[F02](#src-02)、[F14](#src-14)、[F30](#src-30)

目标部署保持原 SA/进程配置，图中实线是目标**链接依赖**，虚线是既有服务调用；框不是新进程：

```mermaid
flowchart TB
  subgraph Host["telephony 配置宿主；设备实际加载待验证"]
    Core["4010：旧 Core SO；Core / RIL / 搜网 / 工厂"]
    Sim["候选私有 SIM SO：C + Application + Domain + R + I"]
    Api["既有 API SO / 公共 utils；含 CoreManagerInner"]
    Svc["66250 既有服务；实现不在本仓"]
    Core --> Sim
    Core --> Api
    Sim --> Api
    Sim -. "仅既有服务协作，不转办所有原语" .-> Svc
  end
```

`libtel_sim_service.z.so` 只是候选名称；GN target、符号和安装关系待 G2b 验收。Core 工厂实现 HostPorts，注入共享 ITelRilManager 能力；SIM 不反链 Core 的具体实现。API 中原门面通过注入虚接口运行时委托，不新增 API→SIM 具体符号依赖。66250 的服务 SO/内部结构不由本仓猜测。

拆库的现状约束（FACT，[F46](#src-46)）：当前 `libtel_core_service.versionscript` 不导出任何符号（`global` 为空、`local: *`），SA 由 core_service.cpp 中的全局构造 `G_REGISTER_RESULT` 在 SO 装载时注册；SIM 源直接使用定义在 Core SO 内的进程级单例、客户端与工具符号——`TELEPHONY_EXT_WRAPPER` 见于 15 个 SIM 源文件（另有 11 个搜网文件和 RIL 使用），`CoreServiceHiSysEvent`、`ImsCoreServiceClient` 及条件编译的 `SatelliteServiceClient` 亦然；单元测试把生产源编成 `core_service_test_base` 静态库，与生产打包无关。因此拆库至少要裁决：每个进程级单例只有一个定义所在 SO（由 H 经 HostPorts 注入，或下沉到双方都链接的既有低层库），绝不同时编入两个 SO；SIM SO 的业务导出只含私有工厂/匹配销毁入口，Core SO 不新增业务导出（工具链必需符号另列）；SA 注册与初始化顺序不变。单测通过不能证明这些打包性质，见 §14.2、§22。

### 7.2 一类端到端任务只有一个主编排者

| 任务类别 | 本次核实边界 | 默认主编排 / Core 责任 |
|---|---|---|
| 下载、OSU、高层 Profile 任务 | IEsimService 声明 listener、DownloadProfile、StartOsu、SwitchToProfile 等；Client 加载 66250；没有匹配服务实现 [F10](#src-10) | 保留既有服务为任务责任边界；由外仓负责人补步骤/重试/补偿证据。Core 不新建第二完整任务状态机 |
| Core eUICC 卡侧原语 | EsimManager/File 执行认证、BPP、通道、切换及双结果 [F11](#src-11)～[F13](#src-13) | R 执行原协议；应用短请求关联本地步骤，结果事实由 T（绑定）、A（偏好提示）接纳。来自服务的子请求不反向转办同一高层任务，禁止 Core→66250→Core→66250 循环 |
| 普通 SIM 激活、号码修改、Primary/Radio 切换 | Controller/RadioProtocol/File 本仓完整主要路径 [F05](#src-05)～[F08](#src-08) | Core Application 中目标事实 Owner 的用例主编排（激活、号码：T；Primary/Radio：A）；R 执行 |
| PIN/PUK、单次 IO/认证；EF/电话簿/STK | 本仓安全与卡运行能力 [F15](#src-15)、[F16](#src-16)、[F25](#src-25)～[F26](#src-26) | 应用短请求门控 + R 协议/加载状态，不要求独立 OperationAggregate，不委托 66250 |
| 名称、角色查询、状态只读 | 本仓旧接口/缓存来源 | T/A/R 对应用例直接提供；无需登记请求关联 |

原“所有 OperationCoordinator 部署于 esim_service SA，Core 只有只读投影”撤回。此裁决仅确定本仓架构，不声称已核实远端任务完成点。远端服务与本地子步骤可各有技术重传，但同一业务步骤的重试/补偿必须由其主编排者授权，不能双方超时后各自重放。

### 7.3 跨 SA 交付、重连与能力边界

近期保持现有 IPC、listener、parcel 和死亡接收路径，不向旧载荷追加 OperationId/epoch。内部关联在发起时保存已有请求/回调对象，接收时验证原归属；不能用当前 slot 反填缺失身份。跨 SA 不共享聚合实例、锁、全局顺序或内存事件总线。

现有 IEsimService 的 GetEuiccProfileInfoList/GetOsuStatus 等是特定事实查询，**没有通用 QueryOperation(OperationId)、持久结果重放或可靠取消证明**。[F10](#src-10) 连接恢复仅重建访问能力，不证明任务成功或旧执行终止。关联丢失时本地记 Unknown，按 §12 核对可达事实；不重复投递旧成功，不替服务执行补偿。

长期若要求结果不丢失/跨重启查询，再独立设计协议版本、稳定任务 ID、结果保留、幂等/重放、查询及必要持久记录；由服务/API/产品负责人确认并做断线/崩溃实验。这需要新增协议或日志时进入独立审批范围，不是 G2 拆库前置条件。详见 TGT-OPEN-001/008/009。

---

## 8. 状态所有权、Repository 与查询

### 8.1 单一事实来源不等于一张全量快照

每个状态字段组只允许对应 T/A/R 入口修改；编排内核只保存运行期请求关联，不是事实写者。Domain 检查纯规则，Application 决定旧兼容节点及 I/O 协调，I 按计划存取，C 格式化输出。T 中若带角色字段，只能是 A 发布的版本化只读投影；R 缓存卡记录，T 的号码资料不能反向覆盖 R 的设备观察。

旧 DataShare、参数、内存和设备各有真实能力，不被“已提交快照”统一替代。内存受控接纳、持久化结果、设备观察和外部完成是正交事实，§9 规定其组合。Repository 不另设可独立变更的业务缓存，也不默认拥有跨进程唯一写权。

### 8.2 查询分类

| 查询类别 | 目标承接与兼容要求 |
|---|---|
| 账户及公开卡状态的旧查询 | 读取原来源所对应的 T/R 兼容投影，保留 active 筛选、历史账户、权限裁剪及各入口同步/异步出口；不等待整个用例终态 |
| Voice / SMS 角色查询 | Controller Voice 找 active 且 isVoiceCard=MAIN 的首项，无则 INVALID_VALUE；SMS 无同类命中则 GetFirstActivedSlotId。SimManager 的两个 Slot getter 在单卡配置下直接返回 0，但 SimId getter 自身走 Controller/映射，不能合并实现 [F33](#src-33) |
| Data / Primary 查询 | 各读 lastCellularDataSlotId_ / lastPrimarySlotId_；InitMainCardSlotId 从对应参数读取，缺省字符串为 "0"。不按数据库同名布尔列、当前 active 或设备观察重新推导；外层 Data 校验见 §16.1 [F06](#src-06)、[F33](#src-33) |
| GetShowNumber | 应用查询用例读取 R 卡缓存→账户→Host IMS IMPU；需要时异步向 T 提交资料回写，查询返回不等回写完成 [F09](#src-09) |
| QueryImsSwitch | 保留按 ICCID 的存储查询和原缺省/返回语义，不能统一改为内存读取 [F09](#src-09) |
| 设备/库存观察 | 最近观察 + 来源/会话/有效性；实时查询保留该方法原有路径，不为所有查询新增 RIL 请求。Get 可能开通道/发 APDU，是否准入按实际资源冲突判断，见 §12.3；不把原实时查询静默改成缓存 |
| 内部操作查询 | 返回本地接纳、效果、等待与终态；不新增公开 Operation 查询接口或跨重启承诺 |

Readiness 分为服务可访问、卡存在/锁态、Records 加载、账户可见、Profile 启用、角色有效、Radio/网络可用。不能一个 ready 或一条成功回执代表全部。

### 8.3 组合查询

内部组合视图携带各自版本及绑定引用，读取前后核对相关输入，必要时在有界预算内重读；不依赖全局 Revision 拒绝另一槽合法变化。对外旧方法保留原字段、错误及返回点，不凭内部“协调中”新增错误码或隐藏旧可见数据。若旧接口无法表达不确定，保持原投影并记录内部来源，不能宣称它是跨聚合原子快照。需要强一致组合或新 readiness 字段时另走 API 契约评审（TGT-OPEN-012）。

---

## 9. 提交、兼容可见性与关键失败行为

### 9.1 Commit 的限定定义

Commit 是**某个 Owner 在其作用域内接纳状态的一次动作**，必须说明接纳的是何种事实。它不等于数据库、Modem、eUICC、公开回调和通知同时成功。

每次用例至少区分 `acceptance`（请求接纳）、`effect`（设备效果认知）、`legacyVisibility`（旧查询投影）、`persistence`（各存储步骤结果）、`terminal`（用例结果）、`delivery`（回调/通知出口结果）。这些维度不合并成一个成功布尔值，也不要求每次都持久化。

### 9.2 两类提交协议

1. **兼容重构协议（近期默认）**：按旧方法已核实节点，让对应 Owner 接纳每个可见步骤；保持存储、参数、缓存、设备与输出的原必要偏序。原节点前的候选不可见，原节点后即使后续失败也不能统一隐藏。存储结果与设备效果单独记录，C 无自主写权。
2. **严格持久后发布协议（有条件长期目标）**：仅对已经具有事务/条件更新能力且契约允许的状态，构造候选→锁外持久化→复核输入/处理冲突→发布快照→发对应事实通知。未证明 provider CAS/事务时不能承诺；旧行为不同的方法须单独批准行为变化，不把这一协议全局套用。

同一写权内有关候选受控排序，锁外 I/O 返回后再验证目标/版本。若存储已成功但内存接纳因新版本失效，进入核对，不能静默抛弃已发生写入，也不能盲目重复设备步骤。对无关卡不持整聚合通道等待外部调用。

**本仓冲突写的排序必须覆盖 I/O，而不只覆盖内存发布。** 应用在短提交域内为目标/字段或角色命令冲突范围预留资格，离开锁执行旧调用；结果已接纳且原写入已结束后，才准许后继冲突写。若效果或结束状态未知，资格转交核对责任方继续隔离，不能因本次等待结束就发出后继冲突写。默认卡命令涉及其他账户角色位，应按角色域排序。不同 Owner 的不相交字段补丁可并行；重载保存发起时的相关本地版本，过期字段不直接覆盖新值，按有界预算重读。不得让同字段写 W1/W2 同时发出，仅靠拒绝 W1 的迟到内存发布处理数据库已经被 W1 覆盖的问题。[F08](#src-08)、[F21](#src-21)

这里的预留是应用执行资格，不是持锁等待或 provider 事务。DataShare/Hook 同步反向调用 Core 时，查询仍按 §8.2 保留原读源；原内存查询可读已可见投影，不能为了规避重入把 QueryImsSwitch 的 DataShare 查询或 GetShowNumber 的异步回写改成纯快照。[F09](#src-09) 若查询再次访问同一 provider/服务形成同步闭环，须由对应提供方证明可重入或给出获批准的处理方式；锁外调用本身不消除该闭环。冲突写回调不得同步等待正在等待它的同一通道，只能采用该方法契约允许的排队或既有失败出口；缺证据仅阻塞相关切片，不统一新增错误或缓存回退。

### 9.3 本次核实的兼容轨迹与目标处理

表中顺序为源码单条路径的局部顺序；通知消费者到达顺序、并发查询实际交错需另做运行采集。失败验收是将来的要求，非本轮测试结果。

| 功能 / 已核实主张与证据 | 正常及部分失败节点 | 目标处理 / 必须验证的差异 |
|---|---|---|
| SIM-024 普通/force 激活 [F05](#src-05)、[F32](#src-32) | CoreServiceSim 完成原检查后先写 userPreferPrimarySlot=true，Manager/Controller 后续仍可失败；直接 CMI/初始化不走此参数节点。普通与 force 路径发 RIL 前都等待 Primary 切换结束，单次等待 4 s 超时即继续；普通激活由同一 Controller 的 activeSimMutex_ 覆盖设备等待及后续写入，静态 ctx_ 在 cv_.wait 时释放；回包无请求关联 [F43](#src-43)、[F45](#src-45)、[F47](#src-47)。非 force：RIL 等待结果成功→当前缓存→全量缓存索引/写入→UpdateSubState（含进度清零）→DB→再次缓存检查/写入→第二次 UpdateSubState→主卡检查→SetActiveSimFunc→Manager 账户通知→active Registry。全量索引失败可只改当前缓存；DB 失败保留前面缓存与第一次 Hook，不走最终 Hook/Manager 通知。普通 force 仍先执行 RIL，然后 UpdateSubState、SetActiveSimFunc；直接初始化调用不产生 Manager 的最终通知 | T 激活用例为进程所有者：T 接纳账户步骤和激活进行中标志，A 接纳入口偏好提示，R 执行。保留两次 Hook 的适用条件、失败后的进度和可见状态；Hook 是 void 不代表设备确认；不得统一延迟或去重 |
| SIM-024 卫星激活 [F05](#src-05)、[F32](#src-32) | 卫星 Controller 路径不调用普通 SetActiveSimToRil 或 SetActiveSimFunc；非 force 为 DB→当前缓存→全量缓存→UpdateSubState→主卡检查→Manager 账户通知，DB 后缓存失败可部分可见。force 只 UpdateSubState 并返回；公开卫星入口也没有普通入口的偏好参数预写 | 不能复用普通激活的“设备先于 DB”轨迹；保留卫星支持条件、参数/缓存失败和最终输出差异。Manager 卫星成功路径无普通 active Registry 调用 |
| SIM-028 Primary [F06](#src-06)、[F33](#src-33) | slot3 先走 NeedSwitchSimCard/EXT；用户设置且支持免重启则走专用 RIL。普通非 M0 路径先 SavePrimaryCardInfo 写 last/参数与开始事件，再设备协议；失败条件恢复参数→SetPrimarySlotIdDone(false)→排重试；成功主卡广播→Data 保存→同一 Done(false)。已为 M0 的分支仍 SavePrimarySlotIdInfo 并联动 Data。免重启先发设备，失败也 Done(true)，成功才保存/联动 Data。参数、广播、Data 的部分失败未统一向外传播；入口无进行中准入，并发请求在协议会话中走失败分支并提前结束进行中状态 [F44](#src-44) | A 分离意图/参数/观察；A 的切换用例持有 AssignmentTransition 与唯一重试。完成事件 setDone=true 表示阶段结束，失败路径同样发布，isUserSet 参数也不是结果码；不据事件认定设备成功。验证空/非法旧参数、两类 RIL、重试与 Primary→Data 偏序，并采集并发请求的旧轨迹后再裁决准入收紧 |
| SIM-025/026 默认角色 [F07](#src-07)、[F33](#src-33) | 先 provider 默认卡命令，再清/置当前缓存角色、更新 default*SimId，最后广播；setter 未直接同步全量缓存。provider 返回后当前缓存为空可失败；广播失败角色已变，Manager 不发成功账户通知。相同 simId 时仍先存储/当前缓存更新，仅抑制角色广播，Manager 成功账户通知仍可发生；remove 参数的 simId=0 与 getter 回退分别保留 | A 逐节点接纳，保留 current/all 的旧差异直到原重载节点；同一个 Owner 也不强制两套旧投影在同一节点同步。C 保留错误与次数；Voice/SMS 的回退不能统一 |
| SIM-027 Data [F06](#src-06)、[F33](#src-33) | Controller 先 SetParameter，再写 lastCellularDataSlotId_，再设置 defaultCellularSimId_ 并广播；参数返回和广播结果均未成为 setter 的失败返回，Controller 返回成功。Manager 的直接入口只先校验 slot；完整外层校验不在这个函数 | A 保留参数失败仍可能推进 last、广播失败仍可能返回成功；不套用 Voice/SMS 的 PUBLISH_BROADCAST_FAIL。该成功不表示数据连接/网络切换完成 |
| SIM-023.a 名称 [F34](#src-34) | SetShowName 不写卡；DB→当前缓存→全量缓存索引/写入。DB 成功后索引失败可留部分缓存；Manager 仅整体成功账户通知，CoreServiceSim 经队列回调 | T 属性用例直接承接，不登记多步关联；保存后失败按旧错误，不能把名称和号码合成同一写卡流程 |
| SIM-023.b 号码修改 [F08](#src-08)、[F15](#src-15) | 写 MSISDN 返回 false 不进 DB；该 false 包含等待超时，不能等同已确认卡未写入。卡成功回包更新 MSISDN 缓存；之后 DB 失败不撤销卡效果，DB 成功后账户缓存检查仍可能失败；Manager 仅整体成功发原账户通知 | R 卡记录、T 资料分别接纳，T 号码用例保留部分结果。保留原失败出口；超时内部记 Unknown，晚到有效写卡结果仅作原会话观察/核对，不补走已结束用例的账户写入或成功通知；后续独立 GetShowNumber 仍可按旧规则触发新的账户回写，不视为复活旧用例；不自动改回卡号码 |
| SIM-023.c 查询回写 [F09](#src-09) | 卡缓存非空可直接返回并 Submit 异步 SetShowNumberToDB；卡/账户为空再用 IMPU；闭包捕获 slot 和号码，执行时才 GetTargetSimId，查询不等待 DB | T 接管全部回写。原目标固定/陈旧拒绝须单列行为变更；可证明交错及范围见下文，不宣称已复现 |
| SIM-051 卡侧 Profile [F11](#src-11)、[F13](#src-13) | EsimManager::SwitchToProfile 外层返回成功，enumResult 承载卡侧失败/超时；随后仍写 USER_PREFER_PRIMARY_SLOT | 不把外层 int 成功当 Profile 已切换、拓扑/Radio 就绪；保持旧双结果及参数节点，缺陷修改另审 |
| SIM-031.c 映射开始 [F18](#src-18) | SimManager 先置映射标志，再 StateManager/Handle 注入人工 ABSENT；后者失败不撤销标志；旧相同状态分支及清理条件另受状态机控制 | T 标志与映射用例、R 软件原因状态、C 输出分别维护；不宣称物理拔卡/路由已完成 |

号码回写的 DERIVED 交错为：A 在 slot=s 时 GetShowNumber 读到号码 nA 并排入仅带 `(s,nA)` 的任务；任务执行前换卡/重载使 s 对应 B；旧任务 SetShowNumberToDB 此时 GetTargetSimId 得到 simIdB，随后按 B 写库/当前缓存。F09/F08 支持这些指令和捕获值，不证明本轮发生过该交错。推荐在原查询用例发起回写时保存 A 的身份/会话，执行时由 T 校验，不改查询立即返回；拒绝后不自动补写 B。它与异步公共写命令“接收时还是执行时解析”的问题不同（§16.2），仅阻塞相关 G3b 行为增强。

### 9.4 崩溃、输出失败与恢复

| 崩溃或失败位置 | 近期可承诺的处理 | 不能推断的结论 |
|---|---|---|
| 设备发出前 | 未发出关联可撤销；保持旧错误 | 受理成功不是已执行 |
| 缓存可见后、DB 失败/退出 | 当前进程保留已发生兼容投影；重启按旧存储/设备重建并核对 | 不承诺该内存步骤跨重启保留，不捏造回滚 |
| DB 写后、内存发布前 | 重读现有持久事实，按字段接纳 | 读取到目标值不证明本次 operation 身份或其他存储成功 |
| 角色已变、广播失败/未发 | 保持角色与原错误，沿旧查询/服务重连路径观察；无 outbox 不保证崩溃后补发 | Publish 返回成功也不等于所有订阅者送达 |
| 设备动作后、响应丢失 | Unknown；对应资源进入核对，见 §12 | 超时、进程退出、本地表清空不证明未执行或静止 |
| 回调已交付后收到迟到结果 | 不重复完成调用者；有效证据交资源核对 | 不复活旧操作、不借旧结果写新卡 |

### 9.5 需要单独批准的行为修正

拒绝陈旧号码回写、对未知设备或存储效果增加隔离、禁止原无证据自动重试、修正错误成功码/未检查通道关闭结果，都可能改变返回、时延、重试或后续可用性。推荐修正有风险的路径，但须记录具体方法的旧/新轨迹、故障原因、错误映射、产品影响及回退条件，完成定向故障/并发差分后才启用（相关 G3 切片）。例如 SetShowNumberToDB 越界 `return false` 出现在 int32 方法中，只能记作缺陷候选，不能在文档中宣称已经修复。[F08](#src-08) 第二轮新增、本轮保留的同类候选为：并发 Primary 请求走失败分支并提前结束进行中/SenseSwitch 状态，Radio 协议会话超时后 COMPLETE(FAIL) 不发出 [F44](#src-44)，激活迟到回包被下一次激活接收 [F45](#src-45)，`simId` 稀疏时全量缓存丢行 [F42](#src-42)。它们的修正分别改变排队/拒绝、设备交互、超时结果或列表可见性，均按本节流程单列。新增持久日志、强一致查询或可靠通知属于 G4 的独立需求。

---

## 10. 身份、版本与会话有效性

### 10.1 标识的产生者与失效范围

| 标识 | 产生/维护者 | 变化或失效条件 / 不可替代的标识 |
|---|---|---|
| simId / 内部 SubscriptionId 包装 | T 接受现有账户分配与匹配结果 | 不新造第二公共 ID；删除/重装/跨设备稳定性未承诺，不能等同 slot |
| Slot / Endpoint / Port / Profile / ApplicationRoute | 配置端点种类与允许连接关系由 H 构建的 SlotCatalog 给出；当前在位/绑定由 T 接受 R 观察，当前 Radio 分配由 A 接纳 | 位置、硬件、Profile、应用分别表达；缺 Port 证据不填默认 0，标签不作身份；槽号种类只读目录 |
| runtimeEpoch | H 每次模块实例创建分配；R 使用 | 模块重建/进程重启更换实例令牌（建议私有随机 128 位值）；不能只用从零重置的计数器，不是业务持久版本 |
| bindingGeneration | R SessionRegistry，按卡/应用作用域 | 换卡、应用替换、路由绑定失效；显示名变更不递增 |
| bindingRevision（按绑定键）/ accountVersion（按账户键） | T | 接纳有关绑定/账户变化；只复核实际依赖键。前稿的全局 TopologyRevision 不参与准入，至多作诊断或发布序号，避免以全局 Revision 拒绝所有请求 |
| LoadScope + recordsGeneration | R 每轮有关文件/依赖闭包加载 | 同卡 Refresh 只更新受影响 scope；不用 bindingGeneration 吞掉所有合法在途响应 |
| AssignmentRevision / 决策依赖集 | A | 角色/分配事实变更；记录所依赖绑定及资源版本，不要求每次 T 变化都失效 |
| OperationVersion / step / attempt | 编排内核，按单次用例请求；业务重试次数由进程所有者（T/A）决定 | 状态推进、步骤及重试；不能冒充设备取消/事务序号 |
| RIL serial / RadioProtocol sessionId / eSIM transactionId | 原 RIL / 协议 / 服务生成 | 保留各自协议作用域，serial 可复用/回绕；不能当跨重启业务身份 [F17](#src-17)、[F19](#src-19) |
| resource fence | R 每个真实共享资源域 | 已核实恢复或新租约边界推进；区分旧响应和新执行资格，不证明硬件自行停机 |

### 10.2 换卡、Refresh 与合法会话接续

换卡/移除使受影响卡应用的旧状态写权失效；R 清理对应 Records/电话簿会话，T 独立撤销或接纳绑定。无关卡和历史账户不随之全局失效。卡移除也不自动证明共享 Modem 空闲。

R 接管的是原有写入协议，不另建一套仅由 Modem 驱动的状态机：Handle::ProcessIccCardState 更新原卡/锁状态，IccFile::NotifyRegistrySimState 仍经 Manager 回写 Handle 的公开状态。SIM 路径的 LOADED 前提为 state>=READY、fileToGet_==0 且 fileQueried_，随后还受 mapping 参数抑制；OnAllFilesFetched 的调用顺序为 UpdateLoaded→Records observer→状态/Registry→CES→语音信箱加载/EXT，不能把 LOADED 改成所有可选文件均成功。其他卡型保持自己的完成路径。人工 ABSENT 走原 ProcessIccCardState，重复相同状态仍可能 needReupdate；真实上报保留切换窗口和 specifiedIccid 过滤。人工输入不证明物理移除，不得仅因重复 ABSENT 就生成新 bindingGeneration；实际换卡/绑定失效才按有关作用域推进。[F16](#src-16)、[F18](#src-18)、[F35](#src-35)

`isSimSlotsMapping_[slot]` 的软件过程标志与 File 读取的 `persist.telephony.is_simslots_mapping` 参数是两个读源，不能合成一项。前者由 T 接管；本仓定向检索未见后者写入，先作为有来源的外部参数观察，匹配厂商/产品写者未核实前不授予 T 一个新参数写入口。[F18](#src-18)、[F35](#src-35)

同卡全量 Refresh 更新有关加载 scope；局部 Refresh 按已有 EF 选择及 PBR/ANR/IAP 依赖闭包更新局部代次。LoadSession 维护必需/可选请求集合、父子关系、读写类型、Pending/Completed/Failed/Invalidated 状态；一次结果只消费一次成员资格。未受影响请求保留原 token 完成；新视图可包含不同 scope 的有效代次。原 fileToGet 计数与 LOADED/通知路径须逐卡型兼容，不把计数归零作为成功依据。[F16](#src-16)、[F25](#src-25)

`Invalidated` 撤销旧加载结果写入新 Records 的资格，不取消已发出的写卡动作。受影响的在途写请求仍保留原 RequestContext，由 R 与需要该步骤的用例进程所有者接纳效果证据并核对；不得按陈旧读取直接丢弃、释放资源或自动重写。新视图所需状态由当前关联重查获得，不能把旧写回包贴到新 LoadToken 上。PIN/单次 APDU 的短关联也不因没有 LoadSession 而失去准入保护。

合法激活/Profile 切换可能产生新会话。用例进程所有者（激活为 T，Primary/Radio 为 A）在发起前记录 `ExpectedTransition(operation, step, oldSession, affectedKeys, expectedIdentity, allowedCause, deadline, consumed)`；R 仅在范围、已知目标、协议或关联重查的因果证据、时限均匹配时签发新 SessionRef。只允许该操作的后续观察步骤接续，旧 EF/旧 attempt 仍无写权。相同 slot 或任意 READY 不够；证据不足进入核对，不无条件接纳新会话。期望本身不是事实。现有 Primary 切换期间 SimStateHandle 进入 SenseSwitch 阶段，暂停 ObtainIccStatus 并丢弃卡状态回包，Monitor 也按切换进行中把 UNKNOWN 归为切换所致 [F43](#src-43)；这是同一机制的旧形态，目标改由 A 的切换用例登记与撤销，避免并发请求提前结束它（§4.4 场景 2）。

### 10.3 决策复核

发出设备步骤前及接纳结果前分别核对目标、有关绑定、资源租约和实际依赖版本；无关属性变更不拒绝合法结果。若目标已变而设备步骤已发，撤销的是新状态写权，不是假定设备没执行；转用例进程所有者与 R 的效果核对。运行期计数器不能恢复旧 operation；已启用新屏障的资源重启后先进入 Checking，其重新准入依据及未启用边界见 §12。

---

## 11. 回调准入、关联与幂等

### 11.1 发起时关联，禁止到达时补造

发请求时把目标类别/身份中已知部分、SessionRef、LoadToken 或 operation/step/attempt、resource fence、原 callback/InnerEvent/RIL serial 关联存入不可变请求上下文。RIL 已有静态 nextSerialId_/requestMap_ 及共享 requestLock_，按 serial 取回删除并跨这些 RIL 实例共用，不能描述为毫无关联；该锁保护映射，不串行化所有设备执行，也不是按 Modem/eUICC 的资源租约。应复用传输关联而非另发设备请求。[F17](#src-17)

号码路径还需注意：ProcessSetMsisdnDone 在无异常分支用共享 lastMsisdn_/lastMsisdnTag_ 更新缓存，并非从回包读回实际号码；waitResult_ 的 false 同时含失败等待和超时。R 的内部结果端口必须保留原请求、暂存输入和可证明的失败/超时来源；证据不足记 Unknown，不能按旧布尔值补造 ConfirmedNotApplied。该增强与旧公开错误分开验收。[F08](#src-08)

适配层可补存本地元数据，但只能从发起上下文补足，**不能响应到达后读取当前 bindingGeneration 并贴到旧响应上**。底层不携带身份/代次的 unsolicited 通知仅视为带来源的观察；无法分辨新旧时使用可达重查和现有过滤，不虚构因果或 OperationId。请求回执无匹配发起关联不写当前卡数据；无请求上报按 R 的观察入口及原协议原生关联（如 RadioProtocol session）处理，不能因没有本地 OperationId 一概丢弃。

现有 serial 查表只证明 RIL 层找回了一条请求，不证明上层已完成身份、阶段、去重或交付校验。`TelRilBase::Response` 在错误时经 ErrorResponse 发送 RadioResponseInfo，成功时发送具体载荷；上层未必同时收到完整原请求上下文。FindTelRilRequest 在后续投递前已删表，Owner 不存在或事件发送失败也不能据此认定业务已接纳。[F17](#src-17) 私有适配路径须分别证明成功、错误、发送失败及迟到证据如何找到发送时上下文；复用现有传输表，不新发一份设备请求，也不假定删除后还能从该表恢复历史关联。

### 11.2 最终准入与状态转换同一受控边界

格式/权限预校验 → 定位原 Owner/请求 → 在该执行域内复核 epoch、目标、scope/attempt、成员资格及终态 → 原子消费结果资格并推进状态 → 锁外必要 I/O → 回到同域复核并接纳结果 → 按原 completionPlan 输出。预校验或一次取快照不能替代最后复核。跨 Owner 使用不可变输入及正式应用入口，不直接回写其他聚合。

**错误结果也必须经过相同归属校验。** 当前 RadioProtocolController::ProcessResponseInfoOfEvent 对非 NONE 的 RadioResponseInfo 先置 communicationFailed 并返回 true，只有另一条 RadioProtocol 分支比较 sessionId；SendRadioProtocolEvent 也未将该 session 写入 InnerEvent param，不能直接把 ErrorResponse 的 flag 当成已保存的 session。[F17](#src-17)、[F19](#src-19) “存在 session 检查”不等于所有失败回包均受保护。旧轮错误影响新轮失败标志/计数是 DERIVED 风险，未作为已复现故障。目标按发起时请求身份、slot、phase、session、attempt 和成员资格准入，成功/错误均只能消费对应槽/请求在该阶段的资格一次；旧轮或重复输入不得先修改新轮标志或计数。该修正单列 G3c 行为验证，不改变错误码或公开协议。

激活设备步骤有同类缺口：普通入口的 `SetActiveCommonSim` 持实例级 `activeSimMutex_` 覆盖设备等待及后续写入；`SetActiveSimToRil` 的类静态 `ctx_` 在 `cv_.wait` 时释放，单靠静态锁不能证明进程级执行串行。[F47](#src-47) 当前装配共用一个 Controller，故普通路径可串行，但不能据此推导多个实例或真实硬件域的互斥。同一把锁还反向耦合 Primary：`SetPrimarySlotIdDone` 也取 `activeSimMutex_` 来清除切换进行中标志 [F47](#src-47)，Primary 若在激活持锁做设备等待与写库期间到达结束步骤，会被阻塞到激活返回（FACT；实际时长未测）。目标准入须保持或显式变更这一旧时序。结果仍写实例级单一 `activeResponse_`/`responseReady_`，超时事件无请求键，回包不核对归属 [F45](#src-45)。一次超时退出后，下一请求进入等待，旧回包仍可完成新等待（DERIVED，未复现）。目标在发起时关联 slot/attempt/原事件，超时事件也携带同一请求键；结果与超时均由原 Owner 原子消费资格。G3c 保留必要的普通入口排序并单独验证准入收紧，不把 ctx_ 搬成资源锁就认为问题已解决。

### 11.3 重复、迟到与取消

- 内部同一请求完成资格最多消费一次，不额外删去旧协议允许的外部通知重复。两个参数相同的公开调用仍是两个请求；旧接口无幂等键时，不能按目标/参数或时间窗口合并、复用结果。请求回执去重、调用方重试识别、公开通知次数是三份不同约束。
- 等待状态 `Waiting / Delivered / TimedOut / CancelledWait / CallerDied` 与业务终态、设备效果分别保存。取消等待不会停止硬件，也不等于 IEsimService::CancelSession 的卡/服务协议语义。
- 调用者超时后，归属有效的设备结果仍可送用例进程所有者与 R 核对；已终结操作不复活，R 持最小恢复记录接纳旧关联证据，不再回调原调用者。
- 注销 callback/observer 不保证已入队工作消失；weak owner + epoch + 在途计数 + 排空共同保护对象生命周期。关联表到期不能顺便解除资源隔离。
- 密码、认证材料、完整 BPP 不进入长期日志或恢复记录；仅保留必要的合成标识/受控引用与效果证据。

---

## 12. 本地操作、Profile 步骤与资源恢复

### 12.1 操作状态与成功范围

本地多步骤用例的运行期请求状态（由编排内核记录、进程所有者推进）采用 `Accepted → Executing → Reconciling? → Succeeded / Failed / Unresolved`。它不是业务事实：A 的 AssignmentTransition、T 的激活进行中兼容投影由原 Owner 按方法节点维护，不与请求状态逐值对应。取消、补偿仅在该方法存在实际能力和契约时增加，不能用统一 COMPENSATED 状态暗示可撤销所有设备动作。效果认知与等待状态见 §6 / §11；PIN 等单请求无须完整业务状态图。

成功范围逐接口保持：接收返回、卡业务结果、拓扑重新发现、角色/Radio 协调、网络驻网分别判断。EsimManager 外层成功不能覆盖 enumResult，66250 listener 的完整保证需匹配服务契约；不默认把网络就绪加入成功条件。TGT-OPEN-008 给出当前默认边界。

本地操作终态不等于硬件终态：确认未发出的设备步骤可 Failed；设备步骤已发出但超时/关联丢失时先记 Unknown 并进入 Reconciling，预算耗尽可 Unresolved，设备恢复责任转 R。晚到可信证据可以更新 R 的效果认知；不能把已向旧调用者完成的操作改成再次成功，也不自动发下一业务步骤。

O 内核的记录由所属 T/A 用例在同一受控执行域推进；它不另开一条异步队列独立更新进行中投影。开始/完成均校验 `(runtimeEpoch, request, step/attempt)`，旧请求不得清除后继请求的资格；R 的租约/fence 用同一关联交接，但仍由 R 唯一维护。兼容投影可以早于用例终态清零：普通激活在第一次 UpdateSubState 后仍会写 DB [F47](#src-47)，因此不得用 `IsSetActiveSimInProgress()==false` 作为资源空闲或允许新冲突写的依据。超时/调用者死亡也不能由 C 或内核直接清业务标志及租约；Owner 按旧输出节点结束等待，未决执行责任显式移交 R。该约束补齐 O 降级后的状态一致性，不新增持久日志。


### 12.2 运行期操作记录与跨重启边界

近期 RuntimeOperationStore 只保存内存关联、进度及有限终态，不存在已核实的持久 OperationRepository。重启可丢失本地 OperationId；按旧 DataShare/参数/设备事实重建，并在**已获准启用新屏障的切片**为相关共享资源进入 Checking。不宣称可重放原请求、查询同 ID 结果或恰好一次执行。

进程重启会丢失运行期租约、fence 与旧请求归属，新的随机 runtimeEpoch 仅防本地对象误接旧回调，不能使设备旧命令失效。启动准入必须有该产品可验证的链条：受影响资源键→旧执行经设备重置/协议失效或明确终止→路由/通道可重用→所需业务事实重读。仅重连 RIL、建立新 serial 表、读取一次 READY/库存或参数均不够。本轮未取得该跨重启终止依据，因此不能承诺自动 Checking→Available；相关新屏障/恢复切片维持未启用，原启动行为留在旧实现。若要求在丢失关联时无条件恢复服务，需产品选择并批准可证明的设备恢复机制或必要持久方案（001/007/008），不能用无限等待掩盖缺口。

只有产品明确要求跨重启同 ID 追踪/可靠结果交付时，才另行引入持久受理/效果日志、保留期限、协议关联及隐私规则，并验证“设备已做但日志未写”的窗口。新增 schema、provider 或服务协议需独立授权。设备副作用存在是需要核对的理由，不是本轮自动新建日志的授权。

### 12.3 真实共享资源屏障（PROPOSED）

R 按经证实的 Modem/eUICC/通道共享域维护一个资源准入者；普通和低优先级 EsimFile 的对象锁不能各自证明设备独占。[F12](#src-12) 资源键来源不齐时采用已知保守共享域，不把逻辑 slot 数当并发能力。A 决定分配策略，用例进程所有者或 R 的实际运行请求申请执行资格，R 统一管理租约；短请求不为申请资源登记多步关联，三者不是重复 arbiter。当前普通激活的实例锁与 Radio 协议的 `isCommunicating_` 分别排序各自路径 [F47](#src-47)、[F44](#src-44)，没有共同设备租约；两类操作的冲突集合须统一登记到 R，不能按旧锁各建一个互不相知的屏障。未知独立性时使用同一保守资源域是目标建议，仍须证明结束判据并验收行为变化。

准入范围按实际设备动作判断，包含有冲突的查询。当前 EsimFile::GetEuiccProfileInfoList 会开通道、发送请求、等待并关闭；ObtainEid 在缓存未命中时也走卡侧通道。[F12](#src-12) API 名称为 Get、业务上只读，不等于无硬件占用。缓存命中及不触及隔离资源的原查询可继续；需开通道/APDU 的查询按同一资源域准入，不能借“只读”绕过 Checking/Quarantined，也不能静默改用旧缓存伪装原实时结果。

| 状态 | 准入及唯一责任 | 离开条件 |
|---|---|---|
| Checking | 启动/重连由 R 核对；不触及受限资源的原查询继续；冲突设备请求（含查询）等待或按该方法既有失败出口返回 | 证实原执行已结束/被可靠协议失效，路由和资源可重用 |
| Available | 无未释放租约；Acquire 在同一短准入域按稳定顺序一次预留全资源集合并转 InFlight，然后锁外发出；不逐把持锁等待 | Acquire 返回前已转 InFlight；发出前失败且确知 NotSent 才撤销 |
| InFlight | 用例步骤或 R 请求持唯一执行资格，含预留未发送阶段；R 记录 NotSent/Sent 及原关联，等待取消不释放 | 有效完成及所需通道关闭证据后释放；超时/失联/关闭不明转 Quarantined |
| Quarantined | R 持 key/fence/旧请求归属/可能效果/证据/预算；原调用可结束，用例可终结为 Unresolved | 满足该命令的效果核对要求并证明执行静止/资源可重用；推进 fence 后再准入，不要求旧调用变为成功 |

恢复查询由同一 R 协调入口签发，不能为拿被隔离租约而死锁，也不能以“查询”名义随意开新通道。对于未改变业务配置的查询，若协议证据已确认该请求结束且通道可复用，不因缺少旧查询载荷永久占用资源；旧调用仍保留原失败结果。涉及写效果的步骤另核对相关事实，不能仅凭通道关闭宣称整个任务成功。预算用尽停止主动轮询，保留隔离与诊断，等待有根据的新证据或已批准恢复动作；无关资源继续服务。不以无限等待保留调用者、密码或 BPP。

对设备型查询新增等待/拒绝也会改变原返回或时延，须与写命令一样记录方法级差异、既有错误映射及 G3 准入证据；不能只因它是 Get 就跳过兼容验收。

### 12.4 可达核对输入与证明缺口

| 操作族 / 本仓依据 | 可采用的核对 | 解除隔离仍需什么 |
|---|---|---|
| 激活；SetActive 结果、卡状态查询 [F05](#src-05)、[F19](#src-19) | 关联原结果和正式卡状态观察 | READY/在位不等于 active 已生效且无在途；设备负责人给结束/失效判据 |
| Primary/Radio；GetRadioProtocol、原 session/check/update/complete [F19](#src-19) | 按受影响槽/Modem 查询当前协议，核对旧 session 完成 | 当前分配相符不保证旧命令不会继续；缓存 getter、参数回滚、超时 CleanUp 均不足。现有 45 s 会话超时路径先清空新旧协议向量，随后构建 COMPLETE(FAIL) 失败而不会发出 [F44](#src-44)（DERIVED 缺陷候选），故超时后不能假定 Modem 收到失败完成阶段；挂载 RIL 的 session/关闭转发也不证明硬件终止 [F38](#src-38) |
| MSISDN；写响应、EF 读取 [F08](#src-08) | 原会话下有因果的读回及写响应 | 缓存可能旧；读到目标值仍需旧写已结束证据；不盲目重写/反向补偿 |
| Profile；enumResult、GetEuiccProfileInfoList [F11](#src-11)～[F13](#src-13) | 核对库存中目标状态；服务任务由 66250 确认 | 库存不是通用任务查询；任务终止及通道静止另验，不能 Core 重放远端下载 |
| 逻辑通道；open/close 回调 [F20](#src-20) | 保留真实关闭结果及原请求归属 | 当前 SyncCloseChannel 重试后清本地值，CloseDone 未检查载荷即清零；本地 channel=0 不证明设备关闭。通道未知时不能再开通道来“验证空闲” |

每个迁移用例提交“可达接口→结果含义→因果关联→静止判据→错误映射→预算来源”的验收证据；未具备可靠判据的资源用例保持未切换，旧实现作为独立构建继续使用。不得新增 ResetMemory/擦卡/硬件复位来替代缺失证明。新屏障可能改变旧自动重试和可用性，属于显式 G3 行为差异，不能夹带入 G2；缺口仅阻塞对应切片（§21.2）。

### 12.5 重试与补偿的唯一责任

近期结构切片保留逐方法已有次数、间隔、触发条件、错误及完成节点，不统一改成指数退避。用例进程所有者接管前必须关闭旧 Controller 的同层业务重试发令分支（含经 CMI 自调用 SetPrimarySlotId 的 detached 线程）；R 保留协议明确定义的分片/传输机制，两者需登记实际层级，避免业务层与 R 同时重放同一设备动作。现有 Primary 重试事件有明确剩余次数与延时，但其存在不证明超时后的重发总是安全：`SetRadioProtocol` 本地最长等待 60 s 后即按失败恢复参数并排重试，而协议会话自身超时为 45 s，二者都不证明设备侧已结束。[F06](#src-06)、[F44](#src-44)

新增重试只在旧步骤已证实终止或可靠失效，并确认该命令允许重复执行时准入；已知部分效果和 Unknown 先按 §12.4 核对。存储、参数、厂商调用也按各自效果判断，不因未发 RIL 就盲重试。反向补偿须有独立业务规则、固定目标和设备依据，不能用通用 COMPENSATED 状态承诺回到原状。抑制旧重试或增加隔离属于 §9.5 的行为变化，须通过相关 G3 切片验收。

---

## 13. 存储、字段所有权与恢复依据

### 13.1 已核实能力与没有证明的能力

SimRdbHelper 的 Insert/Query/Update/Delete 调用 DataShareHelper；UpdateDataBySimId/ByIccId 按 ID 构造谓词并提交 ValuesBucket，没有版本 CAS 谓词。Voice/SMS 使用专门 default-set URI 和 SIM_ID/CARD_TYPE 命令。Primary/Data 使用系统参数和 last 缓存，helper Release/游标 Close 是资源管理，不是事务提交。[F21](#src-21)、[F22](#src-22)、[F06](#src-06)、[F07](#src-07)

这证明本仓调用能力，不证明 provider 无事务，也不证明有事务。已挂载上游 provider 的 SetCard 要求 SLOT_INDEX/CARD_TYPE，而本 fork 发送 SIM_ID/CARD_TYPE；其建表还缺当前 fork 所用 IS_ESIM、SIM_LABEL_INDEX、OPKEY 等列。[F37](#src-37) 上游 SetDefaultCardByType 确有 BeginTransaction/Commit，但部分失败路径与返回覆盖仍须核实，不能把它晋升为本 fork 可用的原子命令。缺的是匹配 fork 的 provider、默认卡多行/失败返回/通知和外部直写协议证据，并非未找到任何 provider。不能用本地锁、队列、Repository 或一次 Update 证明跨连接/跨参数/跨设备原子性。本仓未建立第二条直接 RDB 写通道。

### 13.2 近期可实施方案：保留协议，按字段写

| 数据组 | 规划/接纳 Owner | 当前持久化边界及目标约束 |
|---|---|---|
| 身份/绑定、名称/号码/标签/IMS/探测资料、active 账户投影 | T | 现有 DataShare URI/列/占位语义；属性补丁，不能夹带角色字段 |
| Voice/SMS 等角色 | A | 保留 provider 默认卡命令；初始化角色字段亦由 A 规划，不假定该命令跨存储原子 |
| Primary/Data 的参数与 last、广播去重值及 userPreferPrimarySlot 提示 | A | 保留 MainSlotId/MainCard.Iccid/MainCellularDataSlotId 的原键、启动 last、预写/条件恢复和未传播错误；CoreServiceSim::SetPrimarySlotId/SetActiveSim、EsimManager::SwitchToProfile 的提示写均入 A，保留各自原节点 [F32](#src-32)、[F33](#src-33) |
| 卡/Records/Profile 库存与 enabledProfileNum 等 | R 对应运行事实 | 保留现有缓存及 TelProfileUtil 键的原含义，不升级为永久账户目录 |
| 运行期请求关联（原本地 OperationContext） | 编排内核，按用例请求；进行中标志归 T/A | 内存关联；不持久化虚构提交版本或同 ID 恢复能力 |

Profile 关闭还有三个既有参数键：EsimFile::DisableProfile 在开通道前按槽 0/1 写 `persist.telephony.last_deactive_profile_slot0/slot1`，Controller 读取它们恢复标签；EsimManager 在卡侧返回后再解析当时 simId 写 `persist.telephony.last_deactive_profile`，不以 enumResult 成功为前提 [F50](#src-50)。两个按槽键在仓内的唯一读者是 T 的标签逻辑，因而是 T 的标签恢复提示，由 I 持久化。无后缀键在本仓没有读者（相关单测读取的是另一个 `last_deactive_profile0` 键，不能作为它的旧轨迹 Oracle，G1 须单独采集），消费者与语义属于 OPEN-EXTERNAL：目标只保证按原节点、原取值继续写入（值取自 T 的 simId 视图），把它作为旧外部输出登记，不升级为 T 的业务规则。三键都不能留给 R 经 CMI 反查 simId 后自行写参数，也不能当“关闭成功”或 Port 绑定证据。G3a 经应用桥保留各键、取值时点和失败可见性，G3d 再迁外围调用；固定目标或改写入条件须独立行为裁决。

**禁止 T/A 各自整行 Save 同一账户。** I 接受带字段所有者和目标的窄补丁/现有命令，拒绝越权字段；建行由 T 与 A 形成一次明确写计划，重载把字段分别交相应 Owner，再生成只读旧 DTO。共享数据库行不意味着共享领域可写引用。

本仓存储协调只排序本仓受控写请求；跨进程直接写者清单由 provider 负责人提供，确认前假定可能存在。对会覆盖未知字段的计划不实施，不能用“先读后整行写”消除竞争。provider 更改通知、服务重连和启动重读仍按旧路径；重读也要防止旧读取覆盖本地新版本，但无法从无版本 provider 结果证明全局新鲜度。严格跨进程保证留待独立协议。

### 13.3 长期增强及独立前提

若需要一次 Assignment 完整持久提交、跨账户条件更新或强重启恢复，应由存储/provider 负责人证明可用事务与条件更新、外部写者协议、失败返回及崩溃行为；再选择复用 provider 事务或增量协议。需要新 schema、日志、公开 API 时进入独立需求，不能仅凭抽象接口先行承诺。ICCID 不可用在内部显式表达；写 NULL、删除历史行或改变占位值须核实 schema/旧读者/回退，不在本轮默认执行。

推荐不默认采用 Event Sourcing，也不为内部事件必配 Outbox。当前收益不足以覆盖旧 schema、外部写者和设备非原子性带来的复杂度；持久日志仅由具体恢复/审计需求驱动。TGT-OPEN-004/005/009 分别处理存储能力、技术选型与可见性。

### 13.4 故障恢复

存储失败保留每条调用的已确认失败、已生效或结果未知。未发设备命令只说明没有本次设备效果，不能证明 DataShare/参数写入未生效或可重复；provider 返回错误、调用中断和通知失败分别处理。仅在原调用语义及效果证据支持重复时重试该存储步骤，不重跑已经完成的写卡步骤。设备已改变时走 §12，不用旧快照“回滚现实”；持久写已成功但内存未发布时走字段重查，读取值相同也不证明本次操作身份。

存储结果未知由 T/A 应用写者保留相应字段/角色冲突资格，经 I 的既有访问路径核对；设备在途由 R 独立接管。只有原存储调用已结束或可靠失效，并完成该方法所需的字段重读/冲突处理，才重新准入相关写；无法确认时保留局部阻塞与诊断，不增加无界轮询、自动重写或虚构 provider 查询。未知效果隔离造成的可用性变化按 §9.5 单列，未完成验收的切片继续保留旧路径。

重启按现有参数/DataShare 和可达设备事实重建，不恢复不存在的操作日志。验证至少包含跨 Owner 字段并发、初始化/重载竞争、provider 更新失败、参数部分写和外部写者；未取得 provider 的切片只验本仓计划与兼容，不宣称全局事务通过。

---

## 14. 线程、生命周期与跨 SO 所有权

### 14.1 执行模型

Domain 无线程/FFRT 依赖；Application 按事实作用域序列化相关提交。锁只保护短时内存操作，锁内不做 DB/RIL/APDU/IPC/厂商回调。异步 I/O 返回同一 Owner 执行域复核，不能指望线程池自然有序；无关卡工作不被一个长操作占住全局通道。

新增生产同步采用 FFRT 原语，这是仓库约束；现有代码仍有 std 与 FFRT 混用，不宣称已经全量迁移。Core 的通用/PIN 队列及各协议完成线程是旧可观察路径的一部分；拆库不擅自合并队列。[F15](#src-15)、[F25](#src-25) 明确队列上限、超时/取消、背压、重入和公平性；沿用已验证方法预算，没有证据不指定统一秒数。

### 14.2 对象与 RIL 所有权

ISimManager/IEsimManager 无虚析构；不向公开虚表追加析构、不裸 `delete interface*`。Core 通过具体类型创建的 shared_ptr 控制块或创建库提供的匹配 deleter 销毁，工厂失败和半初始化路径同样受控。[F01](#src-01)、[F30](#src-30) 私有工厂/端口不变成新的公共 InnerKit ABI。

工厂抽取保持原装配/发布时点：Core 先注册共享 RIL，再 eSIM OnInit（其中会向 66250 查询 IsSupported），随后 SIM OnInit/注入 CoreServiceSim，最后搜网及 CMI OnInit。SimManager 先用当时的 multiSimController_ 构造 CardFileDetectManager，再创建 Controller；其 weak_ptr 失效风险是独立修正，不能在 G2 以“补注入”悄然改变行为。缺少匹配服务调用链时不从上述反向调用推定实际死锁。[F01](#src-01)、[F03](#src-03)、[F12](#src-12)、[F27](#src-27)

Core 拥有共享 RIL 生命周期，SIM 仅释放自身注册和引用，不可停止供搜网等模块使用的 RIL。HostPorts、callback、observer、线程任务及 deleter 的代码所在 SO 必须活到最后使用结束；不能只看对象引用计数就卸载含回调代码的库。

进程级单例与符号的唯一定义处按 §7.1 裁决，此外还有两类静态状态：RadioProtocolController 的类静态锁/条件变量只能存在于 SIM SO；头文件匿名命名空间中的容量缓存按编译单元定义，模板实例的链接/内联选择还可能影响实际使用；移动同一批编译单元并不必然增加副本数，但改变 SO 边界可能改变取值。按 [F40](#src-40) 两个推导函数的代码，二者只在虚拟 Modem 开关（`const.booster.virtual_modem_switch`）为真、配置槽数（`const.telephony.slotCount`）小于 8，且某编译单元首次取值时分布式多设备参数（`persist.distributedmodem.multidevice.enable`）为 true 时才给出不同结果。前两个是 const 参数，最后一个是可在运行期改变的 persist 参数，因此该组合下不仅首个调用者，首次调用时点也会影响缓存值。G2b 先按两个 const 参数判定产品：前两个条件不同时成立的产品，两种推导恒等，只记录这一等值，不做逐调用点差分；两者都成立的产品，除非产品确认多设备参数不会为 true，须比较拆分前后各调用点的实际取值、首次调用时点与初始化次序，差异未裁决则不切换。这一检查不以可选的 SlotCatalog 为前提；目录也无法同时等值于互相冲突的旧值。Controller 现有 5 处 detached 线程经 CMI 回调 SetPrimarySlotId [F44](#src-44)；在 G3c 关闭这些自调用之前，G2c 的排空必须把它们计为在途工作。[F46](#src-46)

### 14.3 新增 StopAndDrain 协议与机械拆分分开

当前 CoreService::OnStop 只见 IMS、搜网、RIL 和 EXT 清理，未证明完整 SIM/eSIM 排空。[F01](#src-01) 目标私有关闭协议为：停止新受理 → 失效实例准入 → 反注册外部订阅 → 结束等待但保留未知设备效果 → 排空已排队/在途回调和 Hook → 确认代码不再执行 → 匹配销毁。排空失败不得卸载 SO；设备隔离记录与对象安全分开处理。

常驻 SO 的 G2b 可保持旧生命周期并如实列出限制，不要求先实现所有长期关闭增强；产品有动态卸载、模块重建或初始化失败回收要求时，相关 G2c 能力必须先验收。跨 SO 还需导出符号/版本脚本、C++ 类型布局/RTTI、分配释放、跨 DSO CFI、PAC、LTO、Feature 宏和依赖安装验证。[F02](#src-02) 编译成功不能替代生命周期或行为兼容证明。

### 14.4 通知与锁

订阅登记由 C/I 各自维护，回调锁外调用；异常/慢消费者不持有领域锁。偏序按用例定义，不承诺所有 Context/进程/订阅者全局有序。重复通知和注销后入队回调按原契约及 epoch 门控处理。

---

## 15. 事件与集成通知

### 15.1 语义分开，不默认统一总线

| 类别 | 用途 / 边界 |
|---|---|
| 原协议消息 | RIL RadioEvent、EF/APDU 回应、协议 session；由 R 处理，未必是已接受业务事实 |
| 内部 Domain Event | Owner 已接受事实的内部协作；可用直接应用调用或轻量分发，不强制所有路径经过总线 |
| Integration Notification | 既有跨 SA listener/IPC；不等同内存事件，恢复能力见 §7 |
| Published External Event / Callback | Registry、CES、Hub、账户回调、eSIM 回执；受旧契约约束，不能统一重排或去重 |

### 15.2 候选目录与发布条件

TopologyChanged、SubscriptionDiscovered/Removed、ActivationTransitionChanged、AssignmentTransitionChanged、RoleAssignmentChanged、ResourceAssignmentChanged 可作内部事件名；前稿的 OperationStateChanged 改由这两个进行中事实事件表达，请求过程状态不作领域事件。不固定十类事件或新增十个 CES action。载荷带事实作用域、来源版本与所需最小数据；卡存在观察可无 simId，不携带多余敏感身份。

事件只声明其对应事实已被接纳，不宣称设备、存储和通知同时完成。接收方通过自己的入口修改状态，不直接回写发布者。重复/遗漏/乱序需按实际投递能力处理；内部幂等不等于擅自去掉旧输出重复。

### 15.3 通知恢复的真实界限

无持久发送/结果记录时，内存队列不能保证崩溃后必达。近期保留原查询和重连重发能力，承认没有通用可靠重放；产品要求必达时另审结果查询/持久通知方案，不能只命名为 EventBus 就宣称解决。Event Sourcing 与可靠投递是独立问题，默认均不引入新机制（TGT-OPEN-005/009）。

---

## 16. API 与兼容协议

### 16.1 冻结范围及验收维度

保留既有公开 Framework、ICoreService、ISimManager/IEsimManager、IEsimService、CoreManagerInner 消费侧契约；不新增或重排 IPC，不改 IDL、公开枚举、schema、厂商 ABI、SA 或进程。本轮仅文档设计，不授权生产重构。接口明细继续维护于[接口说明](SIM管理对外接口说明_源码核实版.md)。

逐方法验收签名/二进制兼容、权限及错误优先级、slot capability、敏感字段裁剪、同步接收返回、异步业务结果与 out 参数、查询来源、可见中间态、通知次数/首帧/必要偏序、超时与重试影响。CoreServiceSim::SetShowNumber / UnlockPin 中调用线程的检查与队列任务回包分开，[F15](#src-15)不能把检查移到异步后再用服务身份查询权限。`CoreServiceSim::GetSimState` 同样返回投递路径的结果，由 parcel callback 携带业务结果及可选状态；PostTask 返回值当前未检查，外层 SUCCESS 也不证明任务可靠排入/完成。CoreServiceClient::GetSimState 再同步等待该 callback，超时映射 IPC_CONNECT_STUB_FAIL，成功时才改 out state；CMI 的同名方法则直接委托 Manager。目标分别保持这三个边界，不能把服务端异步概括成所有调用方均异步。[F15](#src-15)、[F36](#src-36)

入口校验也不统一：SetShowNumber 的 system app→SET 权限→Manager/callback→capability 在调用线程执行；GetSimState 服务入口仅检查对象/callback 与 capability，不新增同样的 SET 权限。CoreServiceSim::SetActiveSim 在这些检查后、Manager 前预写偏好提示；Primary 另有 HasSim/active 检查。CMI 是既有直接委托，不能搬入 IPC 入口检查使其内部调用结果改变。已挂载 Data 服务的公共 setter 先检查 system app/SET 权限、HasSim、active，再调用 CMI；SMS Misc 同样经 CMI 委托，通话选槽在 Voice 不可用时可回退 Data。该外仓证据说明消费者和调用层差异，不证明其与当前 fork 已联调；G2a/b 需保持门面和逐入口契约，不能让 A 再自行发明统一外层校验。[F32](#src-32)、[F36](#src-36)、[F39](#src-39)

通知登记同样逐种保持：账户 callback 注册按远端对象判重，重复注册成功但不主动发首帧；NotifySimAccountChanged 先取登记快照、锁外逐个 callback，再 UpdateIccAccount。账户 LOADED observer 登记在已 loaded/VSIM 条件下有立即通知；这些不是所有订阅统一的“注册即首帧”。普通激活的 Manager 账户通知后才 UpdateSimActiveState，卫星 Manager 路径只有账户通知。以上是调用顺序，非消费者实际收到的全局次序；注销也不保证已复制的登记快照立即消失。[F23](#src-23)、[F32](#src-32)、[F34](#src-34)

### 16.2 旧写命令目标绑定

当前部分入口在接收时仅捕获 slot，目标查找在异步任务执行时发生。[F15](#src-15) 因此原提案“统一在命令接纳时绑定 Subscription 身份”并非已验证的兼容事实。

近期结构切片保留方法原解析时点及 Slot/CardApplication/Subscription 分类，不让 PIN、空槽和未识别卡因缺 simId 被新模型拒绝。增强建议是在**该方法明确的执行接纳点、发设备步骤或后台回写之前**固定目标/会话，并在结果阶段复核；不能旧 slot 被复用后静默重定向新身份。若要求前移至 IPC 接收点、禁止原有重定向或改变等待/返回，作为 TGT-OPEN-011 的行为变化评审；比较接收→排队→换卡→执行→回调的轨迹和非目标卡影响。

### 16.3 新 Subscription API

近期不新增第二公共 SubscriptionId 或 OperationId。内部强类型包装既有 simId 不改变其分配、恢复和失效含义。长期新 API 若有独立需求，应明确同步接纳/异步终态、权限、幂等、取消、结果保留与跨重启含义；不得通过修改旧签名、错误码或返回字段暗中引入。身份政策见 TGT-OPEN-006，强组合查询见 TGT-OPEN-012。

---

## 17. PSIM/eSIM 与厂商差异隔离

### 17.1 按实际能力设端口

统一业务语义，不统一全部协议。近期不创建覆盖 EF、APDU、STK、电话簿、安全和全部 eSIM 任务的万能 ICardChannel；保留现有 IccFileController/卡型选择/EsimFile，按具体用例提取窄执行与结果端口。协议解析与数据分片保留在 R；只有纯可复用规则抽出。TGT-OPEN-002 已形成该推荐，不再以抽象范围悬置全部迁移。

能力区分硬件接入点、可路由 Slot、已装配实例、对外可见数量、每 eUICC 的 Port/可启用 Profile、Radio 并发和调用者权限。eSIM 默认宏关闭但产品可启用；专有槽排除、协议初始化的双槽限制等需逐产品核验。[F02](#src-02)、[F12](#src-12)、[F19](#src-19)、[F29](#src-29) 扩数组不能证明四槽/MEP，当前接口含 portIndex 也不证明完整持久映射和设备支持。

**SlotCatalog（PROPOSED）。** 其中启动期能力与允许连接关系落到一个由 H 在启动时构建、之后只读的目录值对象，而不是新的 Manager 或公开 API：

- 内容：逐槽索引的端点种类（实体卡、可承载 eUICC、VSIM、受限第三卡、分布式虚拟槽等）、到 RIL 端点及 Radio 域的**允许连接关系/固定接线**、对外可见性与调用者能力类别、是否可运行期加入；配置容量、可路由能力与 Radio 栈能力分别表达。当前装配数/在位/绑定是 T 的运行清单，当前 Radio 分配由 A 接纳 R 观察；C 用目录与运行视图计算该入口的旧可见投影。
- 来源：现有参数和能力输入（`const.telephony.slotCount`、设备类型与虚拟 Modem 开关、分布式多设备开关、VSIM Modem 数、eSIM 支持与默认槽、MEP 参数）及厂商查询（如 VSIM 槽号）；仅已证明稳定的输入进入启动只读部分；运行期可变能力保留原观察/刷新路径，不能因来自参数就冻结。来源未知的维度显式为 Unknown，不填默认值。[F40](#src-40)、[F41](#src-41)
- 消费：T 按目录项建立槽运行清单，A 结合允许资源域与当前分配观察计算 Primary 与切换策略，C 按能力类别做旧可见性投影，R 按目录实例化运行对象。Core 内的 RIL 与搜网可由 H 注入同一实例以替换各自的 `IsValidSlotId`；这是后续切片，不是 SIM 拆库前提。
- 引入方式：首个切片只复现旧计算，对每个产品参数组合，将目录值与 `SIM_SLOT_COUNT`、`_MD`、`_REAL` 等旧函数在各调用点的取值做等值差分；旧值因调用顺序或编译单元而不一致的组合先记为 LEGACY，由产品裁决，不借目录悄然统一。
- 不做的事：不据目录推出四槽、MEP 或 Radio 并发能力；新增端点种类与 N 槽策略仍需 G4 的产品、协议与设备证据。

目录不能保存 SwapM0M2SimCards 后的“当前路由”：该调用返回重映射目标，RadioProtocol 也会更新 slot→modem 观察 [F48](#src-48)。固定能力由 H 配置，T/A 分别接纳带版本的绑定/分配，发令前 R 核对有关会话与实际路由；不能新建另一个可写全局 RoutingManager。枚举项可由目录共享，但 C 的权限/旧翻译规则及 T/A 的产品策略仍是显式模块；允许具名旧策略集中承接特殊槽规则，验收看重复决策是否消除，不以消灭所有数字字面量代替架构验证。

Port 证据必须区分请求与观察：GetEsimPortIndex 返回可被多个请求赋值的 `esimProfile_.portIndex`，不是硬件绑定查询；库存解析在有 TAG_ESIM_PORT 时能给每个 Profile 的 Port [F49](#src-49)。后者仍需匹配设备/66250 的 eUICC 标识与 Port→逻辑槽契约，才能被 T 接纳为绑定。先验证“一 Port 一逻辑槽”与“多个 Port 共用槽”两种映射、缺标签及失败请求后读回；证据不足保持 Unknown。MEP 能力本身也缺少统一口径：`const.ril.sim.esim_support_mep` 在 3 个文件中有 8 处读取，6 处缺省 false，2 处缺省 true [F51](#src-51)。其中 `IsESimUpdateStatus` 只在 ForgetAllData 已按缺省 false 的成员值判定支持 MEP 后才被调用，它的缺省值不影响结果；实际不一致在 `ProcessSwitchToProfile`：参数未设置时，切换报文按 MEP 编码 Port 与刷新标志，而其余读取点按非 MEP 处理（代码结构为 FACT，卡侧/产品行为未验证）。该能力纳入 SlotCatalog 前，须先采集各产品的参数值，缺省值差异按 LEGACY 由产品裁决，不能借目录统一悄然改变某一路径。保留现有 MEP 分支，不因未证明永久映射就强迫新增 schema，也不宣称该产品已支持 MEP。


### 17.2 厂商调用与命令入口

厂商正式业务请求从应用入口进入；Core 向厂商的调用按真实语义分为能力/策略查询、有副作用执行、兼容通知。入站命令与出站执行不能共用一个含糊的“命令端口”来转办。核心策略保留在对应 Owner，厂商不能得到可写聚合引用或独立存储入口。

`SwapM0M2SimCards` 是转调动态符号并返回结果的执行路径；`SetActiveSimFunc` 转调 void 函数，不能从函数返回推定设备完成。`UpdateSubState` 的同步旧节点也不自动等于纯通知。厂商内部实现未提供，副作用和结束保证仍待确认。[F05](#src-05)、[F26](#src-26)、[F28](#src-28) 有副作用执行由所属应用用例（进程所有者）经 I 调用并记录效果，缺证据时沿用旧行为；不得在超时后盲目 fallback、并行重发或卸载库。通知按原步骤锁外调用，旧返回、缺符号分支和重入语义分别保持，不统一推迟到最终提交后。

### 17.3 动态库与 ABI

近期适配现有 dlopen/dlsym 函数指针，不新建厂商 ABI。接口版本/能力协商是长期联合演进建议，须厂商实现及匹配头文件证据。验证类型布局、工具链、所有权与释放方、缺库/缺符号、回调线程/重入及卸载前在途调用；POD 本身不能证明 ABI 稳定。厂商同进程函数超时不等于它停止执行，不据此释放对象或卸载库（TGT-OPEN-003）。

---

## 18. 架构规则与验证方式

保留原 ARCH-RULE 编号，作为本文内部评审规则，不冒充缺失上游规则的批准记录。

| 规则 | 修订后的约束 | 主要验证方式 |
|---|---|---|
| ARCH-RULE-001 | 身份、位置与会话分开；沿用既有 ID 契约 | 固定源码及身份/换卡场景，不断言永久稳定 |
| ARCH-RULE-002 | 每字段组唯一写入口；初始化/重载/恢复同样接管 | 赋值及引用扫描、WU 清单、并发负控 |
| ARCH-RULE-003 | 决策复核实际依赖版本，不使用全局 Revision 一刀切 | 无关槽变化、旧绑定决策测试 |
| ARCH-RULE-004 | 发起时关联，成功/错误均先准入再改状态；失效写请求仍需核对效果 | 换卡、Refresh、旧 session 错误、阶段/槽成员重复、投递失败与迟到测试 |
| ARCH-RULE-005 | C 无业务写权；允许保留查询用例的异步回写和存储读取 | 查询来源与后台写者检查 |
| ARCH-RULE-006 | Domain 纯规则；用例编排在 Application，每类多步用例只有一个进程所有者且即目标事实 Owner；编排内核不拥有业务事实；R 单列 | 编译依赖/头文件/平台类型检查；主/子请求、跨 Owner 等待图、进行中归属与重试发令点扫描 |
| ARCH-RULE-007 | 厂商查询、执行、输出与入站命令分别经私有边界；保留原 ABI | Hook 节点、副作用/结束证据、缺符号、ABI/重入验证 |
| ARCH-RULE-008 | 领域锁内无外部 I/O；相关提交有序且无关资源可前进 | 阻塞、饥饿、重入与竞争测试 |
| ARCH-RULE-009 | 事实只在对应接纳点可见；旧中间可见性保持，可靠通知能力不虚构 | §9 部分失败轨迹、崩溃/输出失败与恢复证据 |
| ARCH-RULE-010 | 容量、路由、权限和真实共享资源分开；槽种类与容量口径只经 SlotCatalog 解释；静态目录不存当前绑定/分配，设备查询按实际占用准入 | Feature 矩阵、多拓扑、Get 开关通道/缓存分支及设备验证；特殊槽号字面量扫描与目录/旧函数等值差分 |

规则通过代码/结构与运行证据共同验收。静态依赖、链接通过、Linux host PASS 和真实设备 PASS 分开；本轮只运行文档检查。

---

## 19. Linux 测试防护网与 DDD 复用

### 19.1 既有工程及可信范围

继续以 [test/systemtest](../test/systemtest/README.md) 的独立 GN/Clang/GoogleTest 工程及其[场景目录](../test/systemtest/docs/cases/README.md)、[Oracle](../test/systemtest/docs/oracle_catalog.md)为详细来源，不在本文复制用例全集。原文“不得依赖现有 test 的任何 Mock/清单”过宽：应禁止借用能替换被测业务的旧单测替身及未知测试静态库；可复用已核实的独立 host 边界设施、依赖锁定和真实生产源清单，保持执行审计。

历史记录只有容量切片、有限设施自检的运行证据；无卡/Ready/账户/PIN/双卡及完整初始化闭包不能由容量 PASS 推出。初始化审计记录 45 个 object 编译、14 个 fork 存储字段缺口及链接/运行未闭合，均是其记录时的状态，**本轮未重跑，不当成当前重新测得的数字**。前轮所述 test/systemtest 的工作区修改与 test/linux_sim_guard 设施骨架已于 489c88fe 提交；提交只说明设施存在，本轮未构建或运行，最新执行记录仍是 2026-10-08 的容量切片 PASS 与初始化审计 BLOCKED。

### 19.2 测试分层与最小下一切片

| 层次 | 验收对象 | 不能替代 |
|---|---|---|
| 纯 Domain 测试 | 真实规则与状态转换，输入为纯值 | 原协议/公开接口兼容 |
| Linux 组件边界测试 | 未修改生产 SIM，正式 RIL 响应/事件与受控 DataShare/参数/CES 边界 | 真实 SA、IPC、权限、Modem/eUICC |
| 已发布接口差分 | 旧返回、回调、查询来源/中间态、外部请求及输出 | 仅内部方法调用次数不能证明业务 |
| 架构/链接适应度 | 依赖、writer、工厂销毁、符号、Feature/CFI | 编译成功不证明并发及设备恢复 |
| 产品/设备实验 | 真实协议、外仓服务、资源静止、重启与厂商组合 | host Fake 不证明这些能力 |

推荐最小业务运行闭包仍是 `ISimManager::OnInit(1)` + 正式 RIL 无卡输入 + 已核实平台边界，再观察状态/账户查询；真实生产 State/File/Monitor/Controller 不被桩改写。未提供的平台能力明确失败/BLOCKED，不能用未初始化对象或空 Mock 账户冒充无卡场景。先补匹配存储协议及链接/运行设施，再采集业务 Oracle；不要求设备作为所有 host 工作前提（TGT-OPEN-010）。

### 19.3 跨重构稳定项与验收重点

业务场景、稳定功能 ID、已确认契约/不变量、目标/非目标影响保持；允许入口绑定、装配、边界替身和调度设施变化。内部类名、Repository 方法或事件数量不是长期业务断言。Legacy/Split 以分开构建运行采集相同输入和外部轨迹，不双发设备命令；只读 shadow 不获写权。

优先覆盖 §9.3 的各类部分失败、同卡局部 Refresh 与另一 scope 合法响应/已发写卡、换卡/重建的旧结果、旧 session 错误及同阶段重复输入、RIL 取回后投递失败、合法 ExpectedTransition、未知效果后第二请求、普通/低优先级 eUICC 的写与通道型查询竞争、provider 字段互不覆盖及重入存储查询、通知失败、排空失败及有条件重启恢复。第二轮提出、本轮保留的四类先采集旧轨迹再裁决场景：Primary 切换进行中的第二个请求（IPC 与内部自调用两种入口）、激活超时后迟到回包与下一次激活交错、Radio 协议会话超时后的 Modem 阶段、稀疏 `simId` 下的账户列表；另对每个产品参数组合做 SlotCatalog 与旧容量函数的等值检查。恢复测试只能断言已有/已批准的接口能力，不能把本轮建议作为已实现不变量。

### 19.4 Oracle 分类

继续区分 CONFIRMED_CONTRACT、VERIFIED_INVARIANT、LEGACY_BEHAVIOR、OPEN_QUESTION。源码静态候选需运行采集后才能标为运行观察；运行观察仍不自动成为永久业务规则。旧 Oracle 对 capability 的解释如与当前实现不符，以 [F29](#src-29) 定向复核，而非复制旧 BLOCKED 理由；更新测试文件需另次任务。本轮无 hb、gTest、fuzzer、设备、真实双版本或 LSP 通过声明。

---

## 20. 冲突裁决与可追溯证据

### 20.1 主张—证据—判断—修改

| 原主张或冲突 | 证据 | 判断及正文落位 |
|---|---|---|
| 四业务上下文；Coordinator/运行机制入 Domain | Controller 混合职责、卡运行装配 F03～F09；既有 DDD/类设计原文 | C 是兼容边界；Coordinator 在 Application，R 单列，§4～6。早期据此形成 T/A/O，第二轮再裁 O，见本表末四行 |
| Operation 唯一部署 66250，Core 无本地操作事实 | F10～F13 的双通道；F05/F06/F08 普通本地用例 | 拆分本地编排与远端任务，禁止双编排/循环委托，§7 |
| 所有候选持久化后可见，查询只读 | F05～F09/F11/F15 的中间态与双结果 | Owner 分步骤接纳，保留副作用查询，§8～9；严格新协议另受门禁 |
| Repository 和本地串行即原子存储/重启日志 | F21/F22 的现有 DataShare 请求、F06 参数写；未发现目标操作日志实现 | 窄字段计划、保留当前数据源、运行期操作记录，§12～13 |
| 一个 Revision 保护所有结果，等待完即资源释放 | F16/F17/F19/F20 的加载、serial、协议 session 及清理 | 按作用域门控，合法会话接续，资源核对需静止证据，§10～12 |
| 按现有类搬迁，切一个 API 即完成写权迁移 | F04/F23/F24/F27 的重载/配置/后台路径 | 原功能与 WU 全入口切换，§6；SimAccountManager 不整体入账户域 |
| 所有增强关闭前不能结构拆分 | F01/F02/F30 当前生命周期与 ABI；旧迁移材料 | G2a 依赖、G2b 机械链接、G2c 生命周期及 G3 行为分别验收，§14/22 |
| 旧兼容摘要未覆盖入口提示、普通/卫星链及 Data/完成事件差异 | F05～F09、F32～F36 的完整入口/写入/输出链 | 补方法级轨迹与真实查询来源，A 接管偏好提示；服务端投递、Client 等待、CMI 直调分开，§6/8/9/16；保持旧行为的 G2 与修正 G3 分离 |
| WU 摘要把 loaded/参数入口留到较晚外围/编排阶段 | F04/F23、F32/F33 及原 WU 字段定义 | §6.3 逐 WU 定义先接管/后迁编排，§22 同步；账户与 R loaded 分别随 G3a/b 转移，参数子单元 G3c 关闭全部直接写 |
| 所有外仓均无输入，或以找到上游 provider 直接认定本 fork 有事务 | F37～F39；上游 SLOT_INDEX 对本 fork SIM_ID，表列不同 | §1.4 独立基线，§13 改为匹配版本缺口；保留现有协议与外部写者未知，不据上游事务关闭 004 |
| 资源 Available 预留/发出间隙不清；重启只写 Checking 未说明失去关联如何恢复 | F17/F19/F20/F38；无匹配硬件重启终止证据 | §12 在 Acquire 内原子占用；明确重启证据链与未满足时不启用新屏障，§21/22 仅阻塞相应用例，非 G2 总门禁 |
| O（Local Operation）是第三个业务上下文，拥有步骤、重试与终态 | F43 的进行中标志被搜网、运营商配置、Monitor 与 R 消费；F44 的 Primary 参数预写/恢复与重试属于 A 的事实；O 自身状态不持久、不被查询 | 第二轮：O 无独立业务不变量，降为应用编排内核；进程所有者为目标事实 Owner，进行中状态归 T/A，§4.1/4.2/6/22 |
| 目标设计未解释双卡到三卡为何跨模块联动 | F40～F42：特殊槽号、容量口径、双槽策略与账户下标散布于 21 个生产文件 | 第二轮：新增 §3.3 热点表与 H 构建的只读 SlotCatalog，§17.1、ARCH-RULE-010 |
| 前稿未覆盖 Primary 入口准入、激活回包关联与协议超时后的完成阶段 | F44：Primary 入口无准入、超时后 COMPLETE(FAIL) 不发出；F45：激活回包单槽承接、无请求关联 | 第二轮：A 的 AssignmentTransition 准入、激活请求关联；均为 DERIVED 风险，收紧行为按 §9.5、G3c 单列 |
| 前稿把机械拆库验收概括为 GN/符号/CFI 检查 | F46：Core SO 空导出、SA 全局构造注册、SIM 使用 Core 内单例、单测不经生产打包 | 第二轮：单例唯一定义处与最小导出面写入 §7.1/14.2，G2a/G2b 门禁细化，§22 |

第三轮在上述历史裁决基础上的独立结论如下。CONFIRMED 限于静态事实/文档矛盾，DERIVED 不表示生产故障；建议均保持 PROPOSED。

| 级别 / 输入主张 | 独立证据与反例 | 裁决、代价与实施时点 |
|---|---|---|
| P0 / T/A 跨 Owner 只规定应用入口协作，AssignmentTransition 接口仅列 Radio 域 | F04/F06/F43/F47：发现/激活会触发主卡复核，切换又需 T 映射；Primary/Data 为共享参数；旧 active 投影在 DB 前清零。简单跨入口调用或独立队列可能形成等待环、TOCTOU 与旧完成清新资格（DERIVED） | 保留边界，补父/子请求、冲突集合与同 Owner 受控推进，§4.2/4.3/12.1；不新增协调服务。对应 G3b 桥/G3c 编码前落实，G2 不改变原调度 |
| P1 / 只读目录统一路由即可局部扩卡 | F48：运行期换路/分配更新与启动只读冲突（CONFIRMED）；双槽反转不使用目标参数，四栈不能只扩数组 | 目录限固定能力，动态绑定/分配归 T/A；按端点/Radio 栈分别列修改范围，§3.3/17.1。目录切片前澄清读源，N 槽协议留 G4 |
| P1 / ctx_ 已提供全进程激活串行 | F47：cv_.wait 释放 ctx_，普通排序来自外层实例锁（CONFIRMED）；旧回包仍可完成后继等待（DERIVED） | 纠正锁归因，关联结果与超时、统一真实冲突域，§11.2/12.3；不据该锁放宽并行或复制屏障。G3c 验收 |
| P1 / eUICC 卡侧只需向 A 交还启用偏好；每槽一账户说明 MEP 无法表示 | F49/F50：关闭还有 T 标签历史参数，Port getter 是请求暂存，库存另有可选 Port（CONFIRMED）；实际映射仍 OPEN-EXTERNAL | WU-05/G3a 先收口提示写；撤回“一槽一账户必然阻断 MEP/必须改 schema”的推断，保留现有 MEP 分支与外部映射验证；§6/13/17 |
| P1/P2 / 拆库增加缓存副本且可由可选目录解决；C 不持翻译规则 | F40/F46：头内静态缓存/模板与 SO 链接边界；§5.4 又要求 C 保留旧翻译（CONFIRMED 文档冲突，拆库取值变化为 DERIVED） | G2b 独立验容量/初始化及启用产品分支，不强制前置目录；统一 C 仅持旧输出规则，补代码责任落点；§4～7/14/22 |

第四轮复核 PR #3（输入 c70894f8）的逐项裁决如下。第三轮表中的 CONFIRMED 按 §1.3 读作 FACT；本表的建议仍为 PROPOSED。

| PR #3 变更 | 第四轮核实 | 裁决与处理 |
|---|---|---|
| T/A 跨 Owner 协作：父子请求、不嵌套等待、准入原子登记、旧完成不清后继资格、兼容投影早于终态清零 | 激活在 `activeSimMutex_` 下等待切换标志清除，4 s 超时后照常发令；Primary 的两个入口置标志时都不取该锁，激活检查之后仍可开始切换（代码结构为 FACT，实际交错为 DERIVED）；缓存更新中的首次 UpdateSubState 在写库前即清激活进行中（F47，FACT）；激活后的主卡复核经 detached 线程发起，激活持锁时不同步等待 Primary（F44，FACT），嵌套同步等待是目标实现改为同步调用时的风险（DERIVED） | **ACCEPT，表述 MODIFY**：§4.2 收为四条规则，补“T/A 只经对方窄应用接口互调、由 H 注入”，避免两个应用模块形成编译环；删去缺乏证据的“父流程需激活”假设；§12.1 原样保留 |
| 激活锁归因与回包关联 | `ctx_` 在 `cv_.wait` 时释放，排序来自实例级 `activeSimMutex_`，唯一调用者为 SetActiveCommonSim（F47，FACT） | **ACCEPT**，并补 FACT：SetPrimarySlotIdDone 取同一把锁清切换标志，Primary 若在激活持锁期间到达结束步骤会被阻塞（§11.2） |
| SlotCatalog 限静态能力；四卡修改范围；双槽反转算法 | `SetRadioProtocol` 不使用 slotId，CHECK 阶段按 `slotCount_ - 1 - i` 反转；`UpdateRadioProtocol` 更新运行期分配（F48，FACT） | **ACCEPT**，并补 MEP 能力参数 8 处读取、缺省值不一致（F51），作为目录引入前须裁决的 LEGACY 差异（§17.1） |
| eSIM 关闭参数与 MEP 推断 | 按槽两键仅由 T 的标签逻辑读取；无后缀键在本仓无读者，相关单测读的是另一个键（F50，FACT）；GetEsimPortIndex 返回请求暂存值（F49，FACT） | 撤回 MEP 过度推断：**ACCEPT**。参数归属 **MODIFY**：按槽两键归 T（WU-05），无后缀键只作旧外部输出按原节点保留，语义 **UNVERIFIED**（§13.2） |
| G2b 容量门禁、导出面与冒烟矩阵 | 两种推导只在虚拟 Modem 开关为真、槽数小于 8 且首次取值时多设备参数为 true 时不同；该参数为 persist，可在运行期改变（F40 代码，FACT） | 导出面与冒烟矩阵 **ACCEPT**；逐调用点容量差分 **MODIFY** 为只对虚拟 Modem 开关为真且槽数小于 8 的产品执行，并纳入首次调用时点；其余产品记录两推导等值（§14.2、G2b） |
| 新增状态标记、重复裁决段、§24 中的轮次结论 | CONFIRMED 与 §19.4 的 CONFIRMED_CONTRACT 易混；HYPOTHESIS 未使用；DESIGN-DECISION 与 PROPOSED 重复；§4.1 出现两段裁决 | **MODIFY**：三个标记并回 FACT/DERIVED/PROPOSED，两段裁决合并，§24 改为与轮次无关的实施建议；历史记录原文保留 |

### 20.2 固定源码证据索引

F01～F31 保留固定提交 `6017003eff50c1bc8c5dfa72336a4bfe42e948b0` 的既有链接，F32～F36 固定到 `67c1b318e4f165c1671b0daac8ede6e38d2117cc`，二者的生产文件与后续各轮输入（3fd0f815、eeb9ec0e、c70894f8）相同。F37～F39 按 §1.4 外仓独立 SHA，路径从各仓根计算，第二轮起未重读。F40～F46 为第二轮新增，固定到 3fd0f815；F47～F50 为第三轮新增，F51 为第四轮新增，均固定到 eeb9ec0e。第四轮对全部仓内链接以 `git show <commit>:<path>` 复核路径、行号与符号，并对第三轮新增证据重读实现；不以符号命中证明全部行为，也不把历史证据全部视为本轮语义复审。F 编号只用于本文静态证据，不替代 SIM 功能 ID，不证明全部调用者时序、部署匹配或设备结果。

| 证据 | 固定路径、行号与关键符号 | 支持的事实 / 限度 |
|---|---|---|
| <a id="src-01"></a>F01 | [services/core/src/core_service.cpp:92](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service.cpp#L92)；`bool CoreService::Init()`<br>[services/core/src/core_service.cpp:144](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service.cpp#L144)；`void CoreService::OnStop()` | Core 装配共享 RIL、SIM、条件 eSIM 和搜网；OnStop 不能证明完整 SIM 排空。 |
| <a id="src-02"></a>F02 | [BUILD.gn:30](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/BUILD.gn#L30)；`ohos_shared_library("tel_core_service")`<br>[BUILD.gn:139](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/BUILD.gn#L139)；`if (core_service_support_esim) {`<br>[telephony_core_service.gni:21](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/telephony_core_service.gni#L21)；`core_service_support_esim = false` | Core/SIM/RIL 根共享库、CFI/PAC、eSIM 条件源及产品宏；不是设备实际配置。 |
| <a id="src-03"></a>F03 | [services/sim/src/sim_manager.cpp:41](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_manager.cpp#L41)；`bool SimManager::OnInit`<br>[services/sim/src/sim_manager.cpp:53](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_manager.cpp#L53)；`void SimManager::InitMultiSimObject`<br>[services/sim/src/sim_manager.cpp:133](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_manager.cpp#L133)；`void SimManager::InitSingleSimObject` | 按槽装配 State/File/Account/SMS/电话簿/STK；随后 Controller/Monitor；目标工厂尚未实现。 |
| <a id="src-04"></a>F04 | [services/sim/src/multi_sim_controller.cpp:231](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L231)；`bool MultiSimController::InitData`<br>[services/sim/src/multi_sim_controller.cpp:421](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L421)；`bool MultiSimController::InitIccId`<br>[services/sim/src/multi_sim_controller.cpp:733](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L733)；`bool MultiSimController::GetListFromDataBase`<br>[services/sim/src/multi_sim_controller.cpp:759](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L759)；`bool MultiSimController::GetAllListFromDataBase` | 账户匹配、建行、force 激活与主卡后续混合；缓存重载替换容器，是写权切换入口。 |
| <a id="src-05"></a>F05 | [services/sim/src/multi_sim_controller.cpp:956](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L956)；`int32_t MultiSimController::UpdataCacheSetActiveState`<br>[services/sim/src/multi_sim_controller.cpp:976](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L976)；`int32_t MultiSimController::SetActiveCommonSim`<br>[services/sim/src/multi_sim_controller.cpp:1041](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1041)；`int32_t MultiSimController::SetActiveSimSatellite` | 普通激活的缓存/UpdateSubState 先于 DB；force 路径不同；失败不等于无效果。 |
| <a id="src-06"></a>F06 | [services/sim/src/multi_sim_controller.cpp:1483](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1483)；`int32_t MultiSimController::SetPrimarySlotId(`<br>[services/sim/src/multi_sim_controller.cpp:1567](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1567)；`void MultiSimController::ResumePrimaryCardInfo`<br>[services/sim/src/multi_sim_controller.cpp:1701](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1701)；`void MultiSimController::SaveDefaultCellularDataSlotIdInfo`<br>[services/sim/src/multi_sim_controller.cpp:2071](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L2071)；`int32_t MultiSimController::SetPrimarySlotIdWithoutModemReboot`<br>[services/sim/src/multi_sim_controller.cpp:1535](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1535)；`void MultiSimController::RetrySetPrimarySlotId` | 主卡普通路径参数预写/条件恢复/重试/默认 Data 后续；免重启分支先设备后保存。 |
| <a id="src-07"></a>F07 | [services/sim/src/multi_sim_controller.cpp:1280](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1280)；`int32_t MultiSimController::SetDefaultVoiceSlotId`<br>[services/sim/src/multi_sim_controller.cpp:1339](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1339)；`int32_t MultiSimController::SetDefaultSmsSlotId` | Voice/SMS 存储和缓存更新后广播可失败；不是全部回滚。 |
| <a id="src-08"></a>F08 | [services/sim/src/multi_sim_controller.cpp:1757](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1757)；`int32_t MultiSimController::SetShowNumber(`<br>[services/sim/src/multi_sim_controller.cpp:1774](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1774)；`int32_t MultiSimController::SetShowNumberToDB`<br>[services/sim/src/sim_file.cpp:150](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_file.cpp#L150)；`bool SimFile::UpdateMsisdnNumber`<br>[services/sim/src/sim_manager.cpp:432](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_manager.cpp#L432)；`int32_t SimManager::SetShowNumber(`<br>[services/sim/src/sim_file.cpp:688](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_file.cpp#L688)；`bool SimFile::ProcessSetMsisdnDone` | 号码写卡、DB、缓存和成功通知分节点；卡等待/DB 失败与 int32 返回 false 的缺陷候选。 |
| <a id="src-09"></a>F09 | [services/sim/src/multi_sim_controller.cpp:1720](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1720)；`int32_t MultiSimController::GetShowNumber`<br>[services/sim/src/multi_sim_controller.cpp:1861](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1861)；`int32_t MultiSimController::GetSimTelephoneNumber`<br>[services/sim/src/multi_sim_controller.cpp:1945](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1945)；`int32_t MultiSimController::SaveImsSwitch`<br>[services/sim/src/multi_sim_controller.cpp:1959](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1959)；`int32_t MultiSimController::QueryImsSwitch` | GetShowNumber/IMPU 可异步回写；IMS 开关按 ICCID 存取；非全量只读内存查询。 |
| <a id="src-10"></a>F10 | [frameworks/native/src/esim_service_client.cpp:29](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/frameworks/native/src/esim_service_client.cpp#L29)；`constexpr int32_t TELEPHONY_ESIM_SERVICE_SYS_ABILITY_ID`<br>[frameworks/native/src/esim_service_client.cpp:65](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/frameworks/native/src/esim_service_client.cpp#L65)；`sptr<IEsimService> EsimServiceClient::GetProxy`<br>[frameworks/native/src/esim_service_client.cpp:118](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/frameworks/native/src/esim_service_client.cpp#L118)；`void EsimServiceClient::OnRemoteDied`<br>[interfaces/innerkits/IEsimService.idl:25](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/interfaces/innerkits/IEsimService.idl#L25)；`interface OHOS.Telephony.IEsimService {` | 66250 加载、死亡接收、既有任务 IDL；无通用 OperationId 查询/重放定义，服务实现未提供。 |
| <a id="src-11"></a>F11 | [services/sim/src/esim_manager.cpp:318](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_manager.cpp#L318)；`int32_t EsimManager::SwitchToProfile`<br>[services/sim/src/esim_manager.cpp:85](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_manager.cpp#L85)；`int32_t EsimManager::GetEuiccProfileInfoList` | Core 本地 Profile 双结果，外层成功与 enumResult 不等同；偏好参数仍写。 |
| <a id="src-12"></a>F12 | [services/sim/src/esim_manager.cpp:36](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_manager.cpp#L36)；`bool EsimManager::OnInit`<br>[services/sim/src/esim_file.cpp:77](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L77)；`ResultInnerCode EsimFile::ObtainChannelSuccessExclusive`<br>[services/sim/src/esim_file.cpp:155](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L155)；`std::string EsimFile::ObtainEid()`<br>[services/sim/src/esim_file.cpp:183](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L183)；`GetEuiccProfileInfoListInnerResult EsimFile::GetEuiccProfileInfoList()` | 同 slot 普通/低优先级实例不证明设备独立；EID 缓存未命中/Profile 列表查询也有开通道、请求与关闭。 |
| <a id="src-13"></a>F13 | [services/sim/src/esim_file.cpp:2728](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L2728)；`int32_t EsimFile::SwitchToProfile`<br>[services/sim/src/esim_file.cpp:3030](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L3030)；`bool EsimFile::ProcessAuthenticateServer(`<br>[services/sim/src/esim_file.cpp:2108](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L2108)；`bool EsimFile::ProcessLoadBoundProfilePackage(` | 卡侧 Profile 切换/认证/BPP 原语存在；不证明远端端到端任务实现。 |
| <a id="src-14"></a>F14 | [sa_profile/4010.json:6](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/sa_profile/4010.json#L6)；`"libpath"`<br>[services/etc/init/telephony_trust.json:2](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/etc/init/telephony_trust.json#L2)；`"process"` | 4010 Core SO 与 telephony 配置；trust 列出 66250，不证明实际 PID/装载。 |
| <a id="src-15"></a>F15 | [services/core/src/core_service_sim.cpp:502](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service_sim.cpp#L502)；`int32_t CoreServiceSim::SetShowNumber`<br>[services/core/src/core_service_sim.cpp:688](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service_sim.cpp#L688)；`int32_t CoreServiceSim::UnlockPin(`<br>[services/core/src/core_service_sim.cpp:91](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service_sim.cpp#L91)；`int32_t CoreServiceSim::GetSimState` | 权限/空对象/capability 先检查，捕获 slot 排队，接收返回与业务 callback 分离。 |
| <a id="src-16"></a>F16 | [services/sim/src/sim_file.cpp:197](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_file.cpp#L197)；`void SimFile::ProcessIccRefresh`<br>[services/sim/src/sim_file.cpp:226](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_file.cpp#L226)；`void SimFile::ProcessFileLoaded`<br>[services/sim/src/icc_file.cpp:775](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/icc_file.cpp#L775)；`void IccFile::NotifyRegistrySimState` | 局部 Refresh 与完整重载、计数及 LOADED 回写；新增 scope 机制为目标建议。 |
| <a id="src-17"></a>F17 | [services/tel_ril/src/tel_ril_base.cpp:47](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/tel_ril/src/tel_ril_base.cpp#L47)；`std::shared_ptr<TelRilRequest> TelRilBase::CreateTelRilRequest`<br>[services/tel_ril/src/tel_ril_base.cpp:69](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/tel_ril/src/tel_ril_base.cpp#L69)；`std::shared_ptr<TelRilRequest> TelRilBase::FindTelRilRequest`<br>[services/tel_ril/include/tel_ril_base.h:199](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/tel_ril/include/tel_ril_base.h#L199)；`inline int32_t TelRilBase::Response(`<br>[services/tel_ril/src/tel_ril_base.cpp:135](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/tel_ril/src/tel_ril_base.cpp#L135)；`int32_t TelRilBase::ErrorResponse(`<br>[services/tel_ril/src/tel_ril_sim.cpp:258](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/tel_ril/src/tel_ril_sim.cpp#L258)；`int32_t TelRilSim::SetRadioProtocolResponse(` | serial 查表后删除；成功/错误载荷经不同路径交付，业务准入与投递成功另验；回绕序号不等于运行实例/业务身份。 |
| <a id="src-18"></a>F18 | [services/sim/src/sim_manager.cpp:1471](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_manager.cpp#L1471)；`int32_t SimManager::NotifySimSlotsMapping`<br>[services/sim/src/sim_state_manager.cpp:630](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_state_manager.cpp#L630)；`int32_t SimStateManager::NotifySimSlotsMapping`<br>[services/sim/src/sim_state_handle.cpp:1184](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_state_handle.cpp#L1184)；`int32_t SimStateHandle::SetIccCardState` | 映射标志先置，StateManager 注入人工 ABSENT；不证明物理缺卡或映射成功。 |
| <a id="src-19"></a>F19 | [services/sim/src/radio_protocol_controller.cpp:32](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L32)；`void RadioProtocolController::Init()`<br>[services/sim/src/radio_protocol_controller.cpp:87](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L87)；`void RadioProtocolController::GetRadioProtocol(`<br>[services/sim/src/radio_protocol_controller.cpp:250](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L250)；`void RadioProtocolController::ProcessSetRadioProtocolTimeout`<br>[services/sim/src/radio_protocol_controller.cpp:359](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L359)；`void RadioProtocolController::SendRadioProtocolEvent(`<br>[services/sim/src/radio_protocol_controller.cpp:403](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L403)；`bool RadioProtocolController::ProcessResponseInfoOfEvent(` | 已有 session/三阶段与真实查询，但错误载荷分支不比较 session；事件未保存 session param，存在校验不等于完整门控。 |
| <a id="src-20"></a>F20 | [services/sim/src/esim_file.cpp:133](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L133)；`void EsimFile::SyncCloseChannel`<br>[services/sim/src/esim_file.cpp:437](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/esim_file.cpp#L437)；`bool EsimFile::ProcessEsimCloseChannelDone` | 关闭超时重试后本地清零；CloseDone 未检查返回载荷，不能宣称关闭成功。 |
| <a id="src-21"></a>F21 | [services/sim/src/sim_rdb_helper.cpp:94](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L94)；`int SimRdbHelper::Insert(`<br>[services/sim/src/sim_rdb_helper.cpp:117](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L117)；`int SimRdbHelper::Update(`<br>[services/sim/src/sim_rdb_helper.cpp:287](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L287)；`int32_t SimRdbHelper::SetDefaultVoiceCard`<br>[services/sim/src/sim_rdb_helper.cpp:307](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L307)；`int32_t SimRdbHelper::SetDefaultMessageCard` | 实际 DataShare CRUD、default-set 命令；客户端没有证明 provider 跨行事务。 |
| <a id="src-22"></a>F22 | [services/sim/src/sim_rdb_helper.cpp:523](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L523)；`int32_t SimRdbHelper::UpdateDataBySimId`<br>[services/sim/src/sim_rdb_helper.cpp:538](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L538)；`int32_t SimRdbHelper::UpdateDataByIccId`<br>[services/sim/src/sim_rdb_helper.cpp:585](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L585)；`int32_t SimRdbHelper::ForgetAllData(int32_t` | ID 谓词更新无版本 CAS；ForgetAllData 的 active/标签条件保留；不证明不存在外部写者。 |
| <a id="src-23"></a>F23 | [services/sim/src/multi_sim_monitor.cpp:281](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L281)；`void MultiSimMonitor::InitData(`<br>[services/sim/src/multi_sim_monitor.cpp:345](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L345)；`void MultiSimMonitor::RefreshData`<br>[services/sim/src/multi_sim_monitor.cpp:583](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L583)；`void MultiSimMonitor::OnUserSwitched(` | Monitor 初始化重试、缺卡/Unknown 清理重载、通知/加载及用户切换路径。 |
| <a id="src-24"></a>F24 | [services/sim/src/sim_account_manager.cpp:42](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_account_manager.cpp#L42)；`void SimAccountManager::Init(`<br>[services/sim/src/pdp_profile_rdb_helper.cpp:42](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/pdp_profile_rdb_helper.cpp#L42)；`bool PdpProfileRdbHelper::notifyInitApnConfigs` | SimAccountManager 装配运营商 cache/tracker；APN 初始化通过现有 provider URI。 |
| <a id="src-25"></a>F25 | [services/sim/src/sim_sms_controller.cpp:197](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_sms_controller.cpp#L197)；`int32_t SimSmsController::AddSmsToIcc`<br>[services/sim/src/icc_dialling_numbers_manager.cpp:301](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/icc_dialling_numbers_manager.cpp#L301)；`int32_t IccDiallingNumbersManager::QueryIccDiallingNumbers(`<br>[services/sim/src/usim_dialling_numbers_service.cpp:130](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/usim_dialling_numbers_service.cpp#L130)；`void UsimDiallingNumbersService::StartLoadByPbrFiles` | 卡短信读写、电话簿类型/安全/等待及 PBR 多文件生产加载，不是账户聚合。 |
| <a id="src-26"></a>F26 | [services/sim/src/stk_manager.cpp:63](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/stk_manager.cpp#L63)；`int32_t StkManager::SendTerminalResponseCmd`<br>[services/sim/src/stk_controller.cpp:372](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/stk_controller.cpp#L372)；`void StkController::OnSendRilSessionEnd` | STK 命令含 EXT 特殊分支，SessionEnd 走既有 CES 输出。 |
| <a id="src-27"></a>F27 | [services/sim/src/card_file_detect_manager.cpp:127](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/card_file_detect_manager.cpp#L127)；`int32_t CardFileDetectManager::SaveCardFileDetectData` | 卡探测存储后尝试刷新全账户缓存，亦是需接管的写/重载入口。 |
| <a id="src-28"></a>F28 | [services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp:46](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp#L46)；`void TelephonyExtWrapper::InitTelephonyExtWrapper()`<br>[services/sim/src/sim_cache_sync_manager.cpp:45](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/sim/src/sim_cache_sync_manager.cpp#L45)；`int32_t SimCacheSyncManager::SyncCacheOnUserSwitch`<br>[services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp:408](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp#L408)；`void TelephonyExtWrapper::SetActiveSimFunc`<br>[services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp:437](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp#L437)；`int32_t TelephonyExtWrapper::SwapM0M2SimCards` | 厂商库动态加载及函数指针；CacheSync 源存在但根 BUILD/生产装配检索未命中。 |
| <a id="src-29"></a>F29 | [common/capability_mgr/src/multi_sims_capability_manager.cpp:28](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/common/capability_mgr/src/multi_sims_capability_manager.cpp#L28)；`bool MultiSimsCapabilityMgr::IsMultiSimsCapabilitySupported` | 非 SLOT_3_INDEX 直接 true；受限槽检查 native/shell/system app，不能沿用旧文档的反向结论。 |
| <a id="src-30"></a>F30 | [interfaces/innerkits/include/i_sim_manager.h:29](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/interfaces/innerkits/include/i_sim_manager.h#L29)；`class ISimManager {`<br>[interfaces/innerkits/include/i_esim_manager.h:32](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/interfaces/innerkits/include/i_esim_manager.h#L32)；`class IEsimManager {`<br>[interfaces/innerkits/BUILD.gn:65](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/interfaces/innerkits/BUILD.gn#L65)；`ohos_shared_library("tel_core_service_api")`<br>[frameworks/native/src/core_manager_inner.cpp:40](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/frameworks/native/src/core_manager_inner.cpp#L40)；`void CoreManagerInner::OnInit` | 接口声明无虚析构；CoreManagerInner 编入 API SO；具体类型共享控制块需保持。 |
| <a id="src-31"></a>F31 | [services/core/src/core_service.cpp:1017](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/core/src/core_service.cpp#L1017)；`int32_t CoreService::FactoryReset`<br>[services/network_search/src/network_search_manager.cpp:1149](https://github.com/zjuluowu/multisim/blob/6017003eff50c1bc8c5dfa72336a4bfe42e948b0/telephony_core_service/services/network_search/src/network_search_manager.cpp#L1149)；`int32_t NetworkSearchManager::FactoryReset` | Core FactoryReset 委托搜网，不能据方法名新增 eUICC 擦除。 |
| <a id="src-32"></a>F32 | [telephony_core_service/services/core/src/core_service_sim.cpp:1051](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/core/src/core_service_sim.cpp#L1051)；`int32_t CoreServiceSim::SetActiveSim(`<br>[telephony_core_service/services/core/src/core_service_sim.cpp:456](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/core/src/core_service_sim.cpp#L456)；`int32_t CoreServiceSim::SetPrimarySlotId(`<br>[telephony_core_service/services/sim/src/sim_manager.cpp:318](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_manager.cpp#L318)；`int32_t SimManager::SetActiveSim(`<br>[telephony_core_service/services/sim/src/sim_manager.cpp:332](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_manager.cpp#L332)；`int32_t SimManager::SetActiveSimSatellite(`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:928](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L928)；`void MultiSimController::UpdateSubState(` | 公开入口预写用户偏好；普通/force/卫星及直接入口分支的 Hook、进度和最终通知不同；局部调用顺序不是消费者到达顺序。 |
| <a id="src-33"></a>F33 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:1237](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1237)；`int32_t MultiSimController::GetDefaultVoiceSlotId()`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1323](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1323)；`int32_t MultiSimController::GetDefaultSmsSlotId()`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1401](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1401)；`int32_t MultiSimController::GetDefaultCellularDataSlotId()`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1406](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1406)；`int32_t MultiSimController::SetDefaultCellularDataSlotId(`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1418](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1418)；`void MultiSimController::SetPrimarySlotIdDone(`<br>[telephony_core_service/services/sim/src/sim_manager.cpp:458](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_manager.cpp#L458)；`int32_t SimManager::GetDefaultVoiceSlotId()`<br>[telephony_core_service/services/sim/src/sim_manager.cpp:493](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_manager.cpp#L493)；`int32_t SimManager::GetDefaultSmsSlotId()` | Voice/SMS 回退不同；Data 返回未传播参数/广播失败；Primary Done 在失败路径也调用；单卡 Slot getter 和 SimId getter 不可统一。 |
| <a id="src-34"></a>F34 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:1825](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1825)；`int32_t MultiSimController::SetShowName(`<br>[telephony_core_service/services/sim/src/multi_sim_monitor.cpp:661](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L661)；`int32_t MultiSimMonitor::RegisterSimAccountCallback(`<br>[telephony_core_service/services/sim/src/multi_sim_monitor.cpp:735](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L735)；`void MultiSimMonitor::NotifySimAccountChanged()` | 名称不写卡且可出现 DB/当前/全量部分更新；账户 callback 登记无主动首帧，通知先 callback 后 Registry；observer loaded 首帧见 F23。 |
| <a id="src-35"></a>F35 | [telephony_core_service/services/sim/src/icc_file.cpp:227](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/icc_file.cpp#L227)；`bool IccFile::ObtainFilesFetched()`<br>[telephony_core_service/services/sim/src/sim_file.cpp:244](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_file.cpp#L244)；`void SimFile::OnAllFilesFetched()`<br>[telephony_core_service/services/sim/src/sim_state_handle.cpp:403](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_state_handle.cpp#L403)；`void SimStateHandle::ProcessIccCardState(`<br>[telephony_core_service/services/sim/src/sim_state_handle.cpp:634](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/sim/src/sim_state_handle.cpp#L634)；`void SimStateHandle::GetSimCardData(` | LOADED 实际判据、mapping 抑制和输出顺序；原状态及文件回写、相同状态/人工输入和切换/指定 ICCID 过滤；不是新目标状态机实现。 |
| <a id="src-36"></a>F36 | [telephony_core_service/services/core/src/core_service_sim.cpp:27](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/services/core/src/core_service_sim.cpp#L27)；`void CoreServiceSim::AsyncSimGeneralExecute(`<br>[telephony_core_service/frameworks/native/src/core_service_client.cpp:364](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/frameworks/native/src/core_service_client.cpp#L364)；`int32_t CoreServiceClient::GetSimState(`<br>[telephony_core_service/frameworks/native/src/core_service_proxy.cpp:604](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/frameworks/native/src/core_service_proxy.cpp#L604)；`int32_t CoreServiceProxy::GetSimState(`<br>[telephony_core_service/frameworks/native/src/core_manager_inner.cpp:2413](https://github.com/zjuluowu/multisim/blob/67c1b318e4f165c1671b0daac8ede6e38d2117cc/telephony_core_service/frameworks/native/src/core_manager_inner.cpp#L2413)；`int32_t CoreManagerInner::GetSimState(` | PostTask 结果未检查；服务端不等待业务、Client 等回调并映射超时、CMI 直接委托；未证明 reply/回调全局先后。 |
| <a id="src-37"></a>F37 | [sim/src/sim_ability.cpp:200](https://github.com/openharmony/telephony_data_storage/blob/843a8be7f8d1dc304870a3999b754a03c6cce57d/sim/src/sim_ability.cpp#L200)；`int SimAbility::SetCard(`<br>[sim/src/rdb_sim_helper.cpp:50](https://github.com/openharmony/telephony_data_storage/blob/843a8be7f8d1dc304870a3999b754a03c6cce57d/sim/src/rdb_sim_helper.cpp#L50)；`void RdbSimHelper::CreateSimInfoTableStr(`<br>[sim/src/rdb_sim_helper.cpp:74](https://github.com/openharmony/telephony_data_storage/blob/843a8be7f8d1dc304870a3999b754a03c6cce57d/sim/src/rdb_sim_helper.cpp#L74)；`int32_t RdbSimHelper::SetDefaultCardByType(` | 上游 provider 已挂载且存在事务调用；SLOT_INDEX 命令、schema 与当前 fork 所需协议不匹配；不能作为当前部署或可靠事务的证明。 |
| <a id="src-38"></a>F38 | [services/hril/src/hril_sim.cpp:314](https://github.com/openharmony/telephony_ril_adapter/blob/d039c7ce8518487bfcec54c9777e6172c0c9f684/services/hril/src/hril_sim.cpp#L314)；`int32_t HRilSim::SetRadioProtocol(`<br>[services/hril/src/hril_sim.cpp:352](https://github.com/openharmony/telephony_ril_adapter/blob/d039c7ce8518487bfcec54c9777e6172c0c9f684/services/hril/src/hril_sim.cpp#L352)；`int32_t HRilSim::SimCloseLogicalChannel(`<br>[services/hril/src/hril_sim.cpp:629](https://github.com/openharmony/telephony_ril_adapter/blob/d039c7ce8518487bfcec54c9777e6172c0c9f684/services/hril/src/hril_sim.cpp#L629)；`int32_t HRilSim::SimCloseLogicalChannelResponse(` | 挂载 RIL 转发 serial、协议 session 及 channel 请求/响应；既有相关标识必须保留；这几条转发路径不提供跨重启硬件终止判据。 |
| <a id="src-39"></a>F39 | [services/src/cellular_data_service.cpp:490](https://github.com/openharmony/telephony_cellular_data/blob/29d6fa9c6105c6e39a9a765048868e4631ffbb8f/services/src/cellular_data_service.cpp#L490)；`int32_t CellularDataService::SetDefaultCellularDataSlotId(`<br>[services/sms/sms_misc_manager.cpp:569](https://github.com/openharmony/telephony_sms_mms/blob/2336a3c3ba9a5cc53e95535cf25a6cfe15c74de7/services/sms/sms_misc_manager.cpp#L569)；`int32_t SmsMiscManager::SetDefaultSmsSlotId(`<br>[utils/src/call_number_utils.cpp:240](https://github.com/openharmony/telephony_call_manager/blob/3cba608593a8a62d1d0d88629109f96c7a43f3f9/utils/src/call_number_utils.cpp#L240)；`bool CallNumberUtils::SelectAccountId(` | 外仓 Data 校验与 CMI 直接 setter 分层、SMS 委托、Voice→Data 消费回退；仅证明这些固定版本路径，不证明本 fork 产品联调。 |
| <a id="src-40"></a>F40 | [telephony_core_service/interfaces/innerkits/include/telephony_types.h:30](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/interfaces/innerkits/include/telephony_types.h#L30)；`#define SIM_SLOT_COUNT GetMaxSlotCount<int32_t>()`<br>[telephony_core_service/interfaces/innerkits/include/telephony_types.h:57](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/interfaces/innerkits/include/telephony_types.h#L57)；`int32_t maxSlotCount_ = 0;`<br>[telephony_core_service/interfaces/innerkits/include/telephony_types.h:100](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/interfaces/innerkits/include/telephony_types.h#L100)；`T GetMaxSlotCount()`<br>[telephony_core_service/interfaces/innerkits/include/telephony_types.h:123](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/interfaces/innerkits/include/telephony_types.h#L123)；`T GetMaxSlotCountMd()`<br>[telephony_core_service/services/tel_ril/src/tel_ril_utils.cpp:22](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/tel_ril/src/tel_ril_utils.cpp#L22)；`bool TelRilUtils::IsValidSlotId(int32_t slotId)`<br>[telephony_core_service/services/network_search/src/network_utils.cpp:214](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/network_search/src/network_utils.cpp#L214)；`bool NetworkUtils::IsValidSlotId(int32_t slotId)`<br>[telephony_core_service/services/telephony_ext_wrapper/include/telephony_ext_wrapper.h:39](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/telephony_ext_wrapper/include/telephony_ext_wrapper.h#L39)；`VSIM_SLOT_ID = 2,`<br>[telephony_core_service/services/sim/src/esim_manager.cpp:48](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/esim_manager.cpp#L48)；`// slot2 保留给天际通`<br>[telephony_core_service/common/capability_mgr/include/multi_sims_capability_manager.h:23](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/common/capability_mgr/include/multi_sims_capability_manager.h#L23)；`static constexpr int32_t SLOT_3_INDEX = 3;` | 槽号编码 VSIM（2）与受限第三卡（3）；RIL 与搜网各有一份相同的槽合法性判断；两个容量宏共享头文件匿名命名空间中的同一缓存而推导不同。“21 个文件、49 行”为第二轮检索计数（模式见 §20.3），不证明每处都影响运行行为。 |
| <a id="src-41"></a>F41 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:1156](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1156)；`int32_t MultiSimController::GetDefaultMainSlotByIccId()`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:40](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L40)；`slotCount_ = SIM_SLOT_COUNT_REAL >= DUAL_SLOT_COUNT ? DUAL_SLOT_COUNT : SIM_SLOT_COUNT_REAL;`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:67](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L67)；`int32_t RadioProtocolController::GetRadioProtocolModemId(int32_t slotId)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:517](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L517)；`int32_t MultiSimController::InsertData(int slotId, const std::string &newIccId)`<br>[telephony_core_service/services/sim/src/multi_sim_helper.cpp:209](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_helper.cpp#L209)；`int32_t MultiSimHelper::GetPsimLabelIndex(int slotId)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:507](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L507)；`int32_t MultiSimController::NeedSwitchSimCard(int32_t slotId)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:368](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L368)；`int32_t MultiSimController::SetTargetPrimarySlotId(bool isDualCard, int32_t primarySlotId)`<br>[telephony_core_service/services/core/src/core_service_sim.cpp:415](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/core/src/core_service_sim.cpp#L415)；`int32_t CoreServiceSim::GetDefaultVoiceSlotId()`<br>[telephony_core_service/services/sim/src/sim_manager.cpp:92](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/sim_manager.cpp#L92)；`int32_t SimManager::InitTelExtraModule(int32_t slotId)` | 主卡记忆只比较槽 0/1；协议模型至多两槽，越界或索引 2 的 modemId 为 0；标签按槽 0/1 与 `persist.ril.sim_switch` 推导；槽 3 经厂商换路后延迟切换；IPC 入口按调用者能力翻译槽 3；VSIM 槽运行期追加。不证明三卡产品的实际配置或厂商换路效果。 |
| <a id="src-42"></a>F42 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:786](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L786)；`void MultiSimController::SortCache()`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:817](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L817)；`void MultiSimController::SortAllCache()`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:970](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L970)；`allLocalCacheInfo_[curSimId - 1].isActive = enable;`<br>[telephony_core_service/services/sim/src/sim_rdb_helper.cpp:489](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L489)；`int32_t SimRdbHelper::QueryAllValidData(std::vector<SimRdbInfo> &vec)`<br>[telephony_core_service/services/sim/src/sim_rdb_helper.cpp:585](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/sim_rdb_helper.cpp#L585)；`int32_t SimRdbHelper::ForgetAllData(int32_t slotId, bool isNeedUpdateSimLabel, bool isUpdateActiveState)` | 当前缓存以槽号为下标；全量缓存以 `simId - 1` 为下标并丢弃越界行；当前账户即 `SLOT_INDEX > -1`；按槽解绑在同一 Update 中写槽号，并按条件写标签和 active。产品中是否出现稀疏 simId 取决于外部 provider，未核实。 |
| <a id="src-43"></a>F43 | [telephony_core_service/interfaces/innerkits/include/i_sim_manager.h:146](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/interfaces/innerkits/include/i_sim_manager.h#L146)；`virtual bool IsSetActiveSimInProgress(int32_t slotId) = 0;`<br>[telephony_core_service/services/network_search/src/network_search_handler.cpp:600](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/network_search/src/network_search_handler.cpp#L600)；`&& !simManager->IsSetActiveSimInProgress(slotId_) && !simManager->IsSimActive(slotId_)) {`<br>[telephony_core_service/services/sim/src/operator_config_cache.cpp:508](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/operator_config_cache.cpp#L508)；`bool isSetPrimarySlotIdInProgress = CoreManagerInner::GetInstance().IsSetPrimarySlotIdInProgress();`<br>[telephony_core_service/services/sim/src/multi_sim_monitor.cpp:368](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_monitor.cpp#L368)；`} else if (simStateManager_[slotId]->GetSimState() == SimState::SIM_STATE_UNKNOWN &&`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:980](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L980)；`while (isSetPrimarySlotIdInProgress_) {`<br>[telephony_core_service/services/sim/src/sim_state_handle.cpp:532](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/sim_state_handle.cpp#L532)；`void SimStateHandle::ObtainIccStatus(int32_t slotId)` | 激活与 Primary 进行中标志经公开 InnerKit 接口发布，被搜网 Radio 开关、运营商配置、Monitor 重载和激活等待消费；SenseSwitch 阶段暂停卡状态查询并丢弃回包（GetSimCardData 见 F35）。只证明调用关系，不证明消费者在交错下的行为。 |
| <a id="src-44"></a>F44 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:1483](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1483)；`int32_t MultiSimController::SetPrimarySlotId(int32_t slotId, bool isUserSet)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1510](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1510)；`isSetPrimarySlotIdInProgress_ = true;`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1418](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1418)；`void MultiSimController::SetPrimarySlotIdDone(bool isUserSet)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1104](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1104)；`void MultiSimController::CheckIfNeedSwitchMainSlotId(bool isUserSet)`<br>[telephony_core_service/services/sim/src/multi_sim_controller.cpp:1613](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1613)；`std::thread initDataTask([&, primarySlotId = primarySlotId]() {`<br>[telephony_core_service/frameworks/native/src/core_manager_inner.cpp:2044](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/frameworks/native/src/core_manager_inner.cpp#L2044)；`int32_t CoreManagerInner::SetPrimarySlotId(int32_t slotId, bool isUserSet)`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:103](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L103)；`bool RadioProtocolController::SetRadioProtocol(int32_t slotId)`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:263](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L263)；`CleanUpCommunication();`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:337](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L337)；`static_cast<int32_t>(oldRadioProtocol_.size()) < slotCount_` | 入口置进行中标志前无检查；失败分支结束进行中/SenseSwitch 并排重试；CheckIfNeedSwitchMainSlotId（含其调用的 IsNeedSetTargetPrimarySlotId）与重试事件共 5 处 detached 线程（1093、1121、1129、1148、1613 行）经 CMI 自调用；协议控制器遇已有会话立即返回 false，本地最长等待 60 s；45 s 会话超时先清空新旧协议向量，随后 COMPLETE 阶段构建因长度不足失败而不发送。并发交错与 Modem 实际反应未复现。 |
| <a id="src-45"></a>F45 | [telephony_core_service/services/sim/src/multi_sim_controller.cpp:1190](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1190)；`bool MultiSimController::SetActiveSimToRil(int32_t slotId, int32_t type, int32_t enable)`<br>[telephony_core_service/services/sim/include/radio_protocol_controller.h:46](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/include/radio_protocol_controller.h#L46)；`static ffrt::mutex ctx_;`<br>[telephony_core_service/services/sim/include/radio_protocol_controller.h:88](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/include/radio_protocol_controller.h#L88)；`int32_t activeResponse_ = 0;`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:471](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L471)；`bool RadioProtocolController::SetActiveSimToRil(int32_t slotId, int32_t type, int32_t enable)`<br>[telephony_core_service/services/sim/src/radio_protocol_controller.cpp:492](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L492)；`void RadioProtocolController::ProcessActiveSimToRilResponse(const AppExecFwk::InnerEvent::Pointer &event)` | 激活等待使用类静态锁/条件变量，wait 期间释放锁；普通入口排序另见 F47。结果写入实例级单值，由单一 10 s 超时事件兜底，回包处理不核对请求归属。迟到回包错配为 DERIVED，未复现。 |
| <a id="src-46"></a>F46 | [telephony_core_service/BUILD.gn:41](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/BUILD.gn#L41)；`"$TELEPHONY_EXT_WRAPPER_ROOT/src/telephony_ext_wrapper.cpp",`<br>[telephony_core_service/BUILD.gn:132](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/BUILD.gn#L132)；`"services/core/src/core_service_hisysevent.cpp",`<br>[telephony_core_service/libtel_core_service.versionscript:14](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/libtel_core_service.versionscript#L14)；`1.0 {`<br>[telephony_core_service/services/core/src/core_service.cpp:49](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/services/core/src/core_service.cpp#L49)；`const bool G_REGISTER_RESULT =`<br>[telephony_core_service/test/BUILD.gn:34](https://github.com/zjuluowu/multisim/blob/3fd0f815991fe6d5f9ba807dbc9d034c8594cb83/telephony_core_service/test/BUILD.gn#L34)；`ohos_static_library("core_service_test_base") {` | EXT 包装、HiSysEvent 等与 SIM 同编入 Core SO；版本脚本 global 为空、`local: *`；SA 经全局构造注册；单测从源码静态库构建。单例使用文件数为第二轮检索计数，不证明拆分后的实际符号图。 |
| <a id="src-47"></a>F47 | [services/sim/src/multi_sim_controller.cpp:976](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L976)；`int32_t MultiSimController::SetActiveCommonSim(`<br>[services/sim/src/multi_sim_controller.cpp:1204](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1204)；`radioProtocolController_->cv_.wait(lck);`<br>[services/sim/src/multi_sim_controller.cpp:928](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L928)；`void MultiSimController::UpdateSubState(`<br>[services/sim/src/sim_manager.cpp:133](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/sim_manager.cpp#L133)；`void SimManager::InitSingleSimObject()`<br>[services/sim/src/multi_sim_controller.cpp:1421](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L1421)；`std::unique_lock<ffrt::mutex> lock(activeSimMutex_);` | 普通激活的 activeSimMutex_ 覆盖设备等待及后续写入；等待 Primary 时的 activeSimConn_ 与内层 cv_ 均有解锁等待，不能把 ctx_ 当设备执行租约。当前装配一个 Controller；UpdateSubState 在 DB 前就清激活进行中投影；SetPrimarySlotIdDone 取同一 activeSimMutex_ 清切换标志（第四轮补充）。调用/锁范围及旧节点为 FACT，迟到交错仍为 DERIVED。 |
| <a id="src-48"></a>F48 | [services/sim/src/multi_sim_controller.cpp:507](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L507)；`int32_t MultiSimController::NeedSwitchSimCard(`<br>[services/sim/src/radio_protocol_controller.cpp:103](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L103)；`bool RadioProtocolController::SetRadioProtocol(`<br>[services/sim/src/radio_protocol_controller.cpp:330](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L330)；`newRadioProtocol_[i].modemId = radioProtocol_[slotCount_ - 1 - i].modemId;`<br>[services/sim/src/radio_protocol_controller.cpp:390](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/radio_protocol_controller.cpp#L390)；`void RadioProtocolController::UpdateRadioProtocol(`<br>[test/unittest/sim_gtest/multi_sim_controller_gtest.cpp:64](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/test/unittest/sim_gtest/multi_sim_controller_gtest.cpp#L64)；`HWTEST_F(MultiSimControllerTest, MultiSimControllerTest_Init_001,`<br>[test/systemtest/tests/capacity_test.cpp:8](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/test/systemtest/tests/capacity_test.cpp#L8)；`TEST(SimComponent, CapacityReadOnly)` | 厂商返回值用于目标槽；协议观察可更新 slot→modem。SetRadioProtocol 不使用 slotId，构造采用反转双槽算法，扩数组不产生 N 槽目标计划。示例单测使用两个空对象，容量切片参数固定 1；未运行这些测试，也不由其存在认定覆盖多卡。 |
| <a id="src-49"></a>F49 | [services/sim/src/esim_file.cpp:737](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L737)；`int32_t EsimFile::GetEsimPortIndex()`<br>[services/sim/src/esim_file.cpp:743](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L743)；`void EsimFile::BuildAdvancedProfileInfo(`<br>[services/sim/src/esim_file.cpp:2728](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L2728)；`int32_t EsimFile::SwitchToProfile(`<br>[services/sim/src/esim_file.cpp:2853](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L2853)；`bool EsimFile::ProcessSwitchToProfile(` | GetEsimPortIndex 返回请求暂存 portIndex；Switch 在开通道前赋值，失败亦可留下输入。库存解析另可读 TAG_ESIM_PORT；MEP 分支有条件编码 Port。不证明产品 Port→逻辑槽映射、Profile 启用成功或持久绑定，不据每槽一账户判定 MEP 必须改 schema。 |
| <a id="src-50"></a>F50 | [services/sim/src/esim_file.cpp:826](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L826)；`int32_t EsimFile::DisableProfile(`<br>[services/sim/src/esim_manager.cpp:115](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_manager.cpp#L115)；`int32_t EsimManager::DisableProfile(`<br>[services/sim/src/multi_sim_controller.cpp:638](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L638)；`void MultiSimController::GetSimLabelIdxFromAllLocalCache(`<br>[test/unittest/esim_gtest/esim_manager_test.cpp:137](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/test/unittest/esim_gtest/esim_manager_test.cpp#L137)；`GetParameter("persist.telephony.last_deactive_profile0", "0")` | File 在执行前按槽写 last_deactive_profile_slot0/slot1，Controller 读取用于标签；Manager 在返回后写不同的 last_deactive_profile 键，该键在本仓生产代码与测试中都没有读者，唯一相关单测读取的是另一个 `last_deactive_profile0` 键（第四轮核实）。各处当时经 CMI 解析 simId，提示不证明执行成功。按槽两键的目标 Owner 为 T（WU-05），无后缀键作旧外部输出按原节点保留，消费者 OPEN-EXTERNAL；G3a 起关闭 R 参数直写，保留旧取值/写入节点。 |
| <a id="src-51"></a>F51 | [services/sim/src/esim_file.cpp:2871](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/esim_file.cpp#L2871)；`bool isSupportEsimMep = OHOS::system::GetBoolParameter(SUPPORT_ESIM_MEP, true);`<br>[services/sim/src/multi_sim_controller.cpp:178](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L178)；`bool isSupportMep = OHOS::system::GetBoolParameter("const.ril.sim.esim_support_mep", true);`<br>[services/sim/src/multi_sim_controller.cpp:129](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L129)；`isSupportEsimMep_ = OHOS::system::GetBoolParameter(SUPPORT_ESIM_MEP, false);`<br>[services/sim/src/multi_sim_controller.cpp:164](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/multi_sim_controller.cpp#L164)；`isNeedUpdateSimLabel = !IsSimSlotsMapping() && !IsESimUpdateStatus(slotId)`<br>[services/sim/src/sim_state_handle.cpp:1237](https://github.com/zjuluowu/multisim/blob/eeb9ec0e29aef9d5a8ef6b132d75a3301976cdc0/telephony_core_service/services/sim/src/sim_state_handle.cpp#L1237)；`bool isSupportEsimMep = OHOS::system::GetBoolParameter(SUPPORT_ESIM_MEP, false);` | 同一 MEP 能力参数在 esim_file.cpp（5 处）、multi_sim_controller.cpp（2 处）、sim_state_handle.cpp（1 处）读取，6 处缺省 false，2 处缺省 true（第四轮检索计数）。IsESimUpdateStatus 的唯一调用点位于 ForgetAllData 的 `if (isSupportEsimMep_)` 分支内，该成员按缺省 false 读取，故实际生效的不一致只在切换报文是否编码 Port。只证明代码中的缺省值；参数未设置时的卡侧/产品行为未在设备上验证。 |

### 20.3 分轮检查记录（历史结果不继承）

2026-10-29 分支整合修订（revision consolidated-05）：实际执行64个骨架头/源（43h/21cpp）的 `clang++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only`，全部通过；仓内新增[纯规则入口](../test/multisim_architecture_rules/README.md)，topology/resource/assignment三套实际编译并运行通过，包含字段/会话/ABA、51项资源断言、29项分配/请求/完成及资格负控。它们不构建生产对象，不证明设备执行或外仓协议。

本轮检查本地链接、显式锚点、表格列数、代码围栏、include依赖及空白；Mermaid 11.4.1在jsdom下31幅语法解析通过且错误语法负控被拒绝，PlantUML 4幅 `-checkonly` 通过，未做图形视觉验收。合入的56页PPTX与publish分支原字节一致，ZIP CRC与251个XML/关系部件解析通过；技能完整验证脚本因缺少defusedxml未完成，不继承或宣称新视觉验收。未运行hb、OHOS gTest/fuzzer、生产初始化/完整链接、IPC/ABI、设备或产品联调。当前声明/伪代码的业务I/O、工厂与旧用例桥仍待相应切片实现。


2026-10-29 第四轮复核（输入 PR #3 head c70894f8，base main@eeb9ec0e；revision 2026-10-29-independent-04；本轮结果）：经 GitHub API 与 `git fetch` 确认 PR #3 只有 c70894f8 一个提交、只改本文、无评论或评审；读取 PR 描述与完整差异（+136/−68），逐项裁决见 §20.1 末表。按入口→锁/标志→设备→回写的顺序重读：multi_sim_controller.cpp 的激活缓存更新与 SetActiveCommonSim（956–1020）、SetActiveSimToRil（1190–1207）、两个 Primary 入口（1483–1510、2071–2089）、SetPrimarySlotIdDone（1418–1424）、CheckIfNeedSwitchMainSlotId 的 detached 线程（1104 起）、ForgetAllData/IsESimUpdateStatus（150–188）与标签读参（638–645）；radio_protocol_controller.cpp 的 103、308–340、390；esim_file.cpp 的 737、783–789、826–861、2728、2853–2878；esim_manager.cpp 115–126；telephony_types.h 53–151。另在全仓（排除文档）检索 `last_deactive_profile` 与 `const.ril.sim.esim_support_mep` 的全部读写点。均为静态阅读，未复现任何交错。

自写脚本实际核对：24 章与 5 个代码围栏；38 张 Markdown 表（328 行）列数一致；33 个相对链接与 217 个文内锚点有效；191 处本仓固定链接逐条以 `git show <commit>:<path>` 核对路径、行号与所列符号；F01～F51 均有定义和引用；62 个主功能、WU-01～09、ARCH-RULE-001～010、TGT-OPEN-001～013 保留，编号集合与 c70894f8 相同，新增行无 ACCEPTED。11 个外部链接（9 个外仓固定链接与 PR #2/#3）只计数，本轮未访问、未重读。四个临时损坏副本（错误源码符号、错误锚点、多一列的表行、未定义证据）均被检查器拒绝，副本已删除。4 幅 PlantUML 与 1 幅 Mermaid 的围栏内容与 c70894f8 逐字节相同；PlantUML 1.2025.2 / JRE 21 `-checkonly` 4/4 通过，Mermaid 11.4.1 / jsdom 解析 1/1 通过。`git diff --check` 通过，仅本文改变且保持 LF。main 工作区未编辑，既有 926 项修改数量不变；已有的第三轮 worktree 保持干净。**NOT RUN**：图示 SVG/PNG 渲染（图未改）、Mermaid 浏览器渲染、外部链接访问、hb/生产构建、gTest、fuzzer、Linux systemtest、Legacy/Split 差分、设备/外仓联调、竞态复现及 LSP；不声称任何生产实现、业务测试或设备行为通过。

以下为历史记录，只说明各次输入及当时检查，不作为本轮 PASS：

2026-10-29 第三轮独立审计（输入 eeb9ec0e，Claude 修订 1afffa4e；revision 2026-10-29-independent-03；历史记录，其中“本轮”指第三轮）：读取 PR #2/提交差异并定向重读固定源码，按 §4.4 五类场景作静态反证，结论见 §20.1；未将推演称为故障复现。自写脚本实际核对：24 章、37 张 Markdown 表及围栏，246 处本地链接/锚点，184 处本仓与 9 处外仓固定源码引用的路径/行号/所列符号（84 个仓库/commit/文件组合）；62 个主功能覆盖、既有 WU-01～09、ARCH-RULE-001～010、TGT-OPEN-001～013 及原显式功能子项标识保留，F01～F50 均有定义和引用。三个临时损坏副本的错误锚点、源码符号、多列表格均被检查器拒绝。外仓仅定位核对，不据此完成匹配产品语义验证。

PlantUML 1.2025.2 / JRE 21 的 4 幅图语法检查及 SVG 渲染通过，修改的目标图以临时 Noto Sans CJK SC 字体渲染 PNG 并目视检查；Mermaid 11.4.1 / jsdom 解析 1 幅通过，两类错误语法负控均被拒绝。未运行 Mermaid 浏览器渲染。人工复核 Owner→接口/用例→WU→G 阶段及规则，修正表格断行与历史轮次标记；`git diff --check` 通过、仅本文改变且保持 LF。原 main 工作区 1277 个跟踪文件的前后 SHA-256 相同，926 项既有修改保留。临时工具与预览仅在 `/tmp`。**NOT RUN**：hb/生产构建、gTest、fuzzer、Linux systemtest、真实 Legacy/Split 差分、设备/外仓联调、竞态复现及 LSP；本次只修改设计文档，当前无匹配产品构建/设备证据，不声称任何生产实现或业务测试通过。

2026-10-29 独立架构评审（输入 3fd0f815；revision 2026-10-29-independent-02；历史记录）：完整通读本文，按 §20.1 末四行的问题从入口追踪源码并新增 F40～F46。实际执行的检查如下。（1）自写静态检查脚本：24 章与各节编号连续；34 张表 300 行列数一致；5 个代码围栏闭合；33 个相对链接及其锚点、200 个文内证据锚点有效；167 个 zjuluowu/multisim 固定提交链接逐条以 `git show <commit>:<path>` 核对路径、行号及所列符号；46 个证据锚点均被引用（顺带补上前稿未引用的 F31）；62 个主功能、WU-01～09、ARCH-RULE-001～010、TGT-OPEN-001～013 均在。对脚本注入错误符号、错误锚点、未定义证据和多列表格四类故障的负控均被检出。（2）PlantUML 1.2025.2（Temurin JRE 21）`-checkonly` 4/4 通过并渲染 SVG；本轮改动的目标架构图与 SetShowNumber 时序图另用 Noto Sans CJK SC 渲染 PNG，人工查看中文与布局；错误语法负控失败。（3）Mermaid 11.4.1 在 jsdom 下 `parse` 1/1 通过，负控失败；未做浏览器渲染。（4）9 个外仓固定链接与 3 个本仓各固定提交抽样链接 HTTP 200，仅证明可访问，本轮未重读外仓内容。（5）`git diff --check` 通过，差异仅本文。§3.3 的“21 个文件、49 行”来自对 services、common、frameworks/native、interfaces/innerkits（排除 test）检索 `SIM_SLOT_2\b`、`SLOT_ID_2\b`、`SLOT_3_INDEX`、`SIM_SLOT_3\b`、`SLOT_ID_3\b`、`DUAL_SLOT_COUNT`、`THREE_CARD_COUNT`、`VSIM_SLOT_ID`、`THREE_MODEMS`；§7.1 的单例使用文件数来自对 services 下各模块 src 的同类检索。工具、字体与预览仅在 `/tmp`。未运行 hb、业务 gTest、fuzzer、Linux systemtest、设备/外仓联调、交错复现、Legacy/Split 或 LSP；本轮 DERIVED 风险均未复现，不声称生产修复或业务测试 PASS。

2026-10-26 独立源码复审（输入 67c1b318；revision 2026-10-26-independent-01；历史记录）：完整通读本文并按 §20.1 的问题沿源码核查；当轮文档检查通过 281 个链接（128 处固定源码、153 处本地导航）、46 个仓库/路径的 54 份固定版本内容及路径/行号/符号与工作树换行归一一致性、29 张表格、围栏/相对链接/锚点、24 章、62 个主功能与 8 子项、WU-01～09、10 条规则及 13 个原决策主记录。所引外仓提交各自固定，不晋升为匹配产品契约。4 幅 UML 经 PlantUML 1.2025.2 语法检查、SVG/PNG 渲染及中文可读性检查；1 幅 Mermaid 经 11.4.1 解析，当轮未做其浏览器渲染。临时工具、字体、摘要和预览仅在 `/tmp`。1274 个输入文件前后摘要确认仅本文改变，另外 1273 个文件保持原始字节；目标 `git diff --check` 通过。原工作区全量 `git diff --check` 返回 2，换行/空白诊断均位于本文之外的既有修改，未扩大范围修复；隔离检出只应用本文后再次检查，避免把这些改动带入提交。人工复查功能→Owner→原执行链→WU/阶段→失败 Oracle，并保持目标方案 pending。未运行 hb、业务 gTest、fuzzer、设备/外仓联调、交错复现、Legacy/Split 或 LSP，不声称生产修复或业务测试 PASS。当轮远端 fetch 受环境/网络阻断，未核实远端并发变化或完成推送。

前轮复审（3476c44a，2026-10-23～2026-10-26）完成定向源码复核与文档自检。当时静态检查结果：195 个链接（84 处固定提交源码定位、111 处本地导航）、36 个所引源码/配置文件的路径/行号/符号与工作树换行归一内容一致；25 张 Markdown 表格、代码围栏、24 个章序、62 个主功能覆盖、10 条 ARCH-RULE、13 个 TGT-OPEN 主记录检查通过。复审提交中的 5 张图曾经 Mermaid 11.4.1 解析通过；该记录针对当时图形版本，未做图形渲染或设备部署验证。人工复核了上下文/写权、编译与运行图义、部分失败、恢复能力、决策状态和门禁之间的一致性。

该轮编辑前文件摘要对比确认仅本文变化，另外 1272 个已纳入检查的工作区文件保持原始字节；`git diff --check` 通过。核验工具及临时依赖仅置于 `/tmp`，不作为仓库交付物。未运行 hb、业务 gTest、fuzzer、设备/跨 SA、真实 Legacy/Split 或 LSP 验证；历史测试记录不作为本轮运行结果。

2026-10-26 分层图表达优化（9501c2b5）：第 5 章 4 幅文本图的框线显示列宽检查通过；保留的第 7 章 1 幅 Mermaid 图重新解析通过。复查上述链接、表格、编号及覆盖关系，并确认该次修改仅涉及第 5 章、检查记录与修订记录；未重新运行业务测试或进行设备验证。

2026-10-26 三稿对照复核（15fcd7a6）：读取两份参考稿正文并校验其 SHA-256，定向重读角色、激活、号码等待/回包、异步查询入口、DataShare 和厂商转调路径；补充的源码依据继续固定到 6017003e。当时检查 205 个链接（89 处固定源码、116 处本地导航）、36 个引用文件的路径/行号/符号及换行归一内容、26 张表格、24 章顺序、62 个主功能、10 条规则及 13 个原决策 ID，均通过。第 5 章前三幅文本图与该次输入逐字一致，第四幅保持原框图形式并增加超时分支，4 图框线列宽检查通过；第 7 章图经 Mermaid 11.4.1 再次解析通过。1275 个文件的前后摘要中仅本文变化，另外 1274 个文件（含两稿）保持原始字节；`git diff --check` 通过。该次仅静态与文档检查，未运行业务构建/测试、图形渲染、设备或外仓验证，不新增批准或业务 PASS 声明。

2026-10-26 历史与关联设计复核（8c369322，历史记录）：读取四次主文档提交全文及关键差异，重读分层设计 §3.2/3.5/5/6.2/11.2、V3 SAD §9、类设计 §12/13.3 与重构设计 §4.6/4.7，并追踪 RIL 成功/错误交付、RadioProtocol 错误准入及 eUICC 查询通道路径。检查 217 个链接（96 处固定源码、121 处本地导航）、38 个引用文件的路径/行号/符号及换行归一内容、27 张表格、24 章、62 个主功能、10 条规则和 13 个原决策 ID，均通过。全部 4 幅文本图及 1 幅 Mermaid 图与 15fcd7a6 输入的围栏内容逐字一致，文本框列宽和 Mermaid 11.4.1 解析检查通过。1275 个文件的前后摘要确认仅本文变化，另外 1274 个文件保持原始字节；`git diff --check` 通过。未运行交错复现、业务构建/测试、设备或外仓实验，未将静态风险记为已发生故障或已修复缺陷。

2026-10-26 UML 图示转换（67c1b318，历史记录）：以 8c369322 为输入，将第 5 章四幅文本图转换为三幅 UML 组件/包图和一幅 UML 时序图，采用 PlantUML 记法。逐项核对当前/目标图 42/58 项文字、8 条编译依赖和明确失败/等待超时/成功三个时序分支，原组件、职责、关系、说明和失败语义保留；除图示、读图说明及检查/修订记录外，正文保持一致，第 7 章图未改。PlantUML 1.2025.2 语法检查、本地 SVG/PNG 渲染及中文显示检查通过；217 个链接、27 张表格、功能/规则/决策编号与原有源码定位检查通过。1275 个文件的前后摘要确认仅本文变化，其余 1274 个文件保持原始字节；`git diff --check` 通过。渲染工具、字体及预览仅置于 `/tmp`，不作为仓库交付物；未运行业务或设备测试。前轮的图示形式与逐字一致性记录均描述各自历史提交。

### 20.4 历史两份参考稿的采用与裁决

前轮输入记录（本轮仅复验摘要，不再次以两稿证明源码）：v1.16 的 SHA-256 为 `8c6a2a53c36f893b1d04676ca3987a3854f8e4fec8cd4b897d842a2b6f017e00`；v1.17 为 `61fd72597468e3bbf9a49586bdea23bd0cdf41370b77e38d27849bffdc4e02db`。参考稿自述的检查结果不继承为本文本次结果。

| 参考内容与原主文档问题 | 复核判断及落位 | 决策归并 / 后续验收 |
|---|---|---|
| v1.16 §4～13 的本地 O、R 保留、分类目标和旧可见性；v1.17 §4/7/9/13 的全 Operation 归远端、统一持久后发布/恢复 | 前者多数已被 v1.2 吸收；后者与 F05～F13/F21 的当前边界不符，继续作为受条件约束的历史提案，不覆盖 §4/7/9/12/13 | 原 001/004/008/009；无批准记录不改 ACCEPTED |
| v1.16 §9.3、v1.17 §9.2/14.2 提醒锁外 I/O 仍需有序；原文仅说候选排序，易被实现为只排序内存发布 | F08/F21 无客户端 CAS；§9.2 明确冲突资格覆盖 I/O 与未知结果核对，处理重载过期及反向调用 | 007/009；W1/W2 完成逆序、重载竞争、同步重入负控，不能仅靠拒绝旧快照 |
| v1.16 §11.2 区分三种去重；v1.17 §11/19 的重复请求要求未限定幂等键 | 保留传输 serial 机制 F17；§11.3 明确同参调用独立，§12.5 区分业务/协议重试；不许推出端到端 exactly-once | 008/011；重复回执、独立同参请求、调用者重试和旧通知分别验收 |
| 两稿均提示设备效果与等待分开；原 v1.2 号码失败行/图和 §13.4 的重试前提过于简化 | F08 的写卡等待可超时，F21 仅证明转调 provider；§5.3/9.3/12/13.4 区分 Unknown、晚到卡观察和已结束调用，未发 RIL 也不能证明存储可重试 | 008/009/011；丢回包、晚到成功、卡成功而 DB 失败、存储返回不明，设备结论仍需外部证据 |
| v1.16 §4.3 的角色范围与 §17.2 的副作用执行；原 v1.2 按共享资源域描述 A，厂商仅列查询/通知/命令 | F07 的角色选择跨槽，F28 存在动态执行入口；§4.2/4.3 以角色不变量定域，§17.2 分清厂商入站命令和出站执行 | 003/007；跨槽角色竞争、厂商缺符号/慢调用/效果未知及重入；未知厂商效果不推定已完成 |
| v1.16 §24 的逐功能开发问题、v1.17 的门禁和测试分工 | §24 补充开发定位顺序；保留现有 §6/19/22 的功能、测试与分阶段职责，不复制完整迁移/用例清单，不恢复全部 OPEN 的总门禁 | 010/013；文档核查只证明表达与追溯，不晋升为业务或设备 PASS |

### 20.5 历史复核与本轮独立裁决

下表按本地可解析的提交及差异追溯，不将提交作者、文档 revision 或多稿重复视为批准。8c369322 一轮追溯的四次提交相对 6017003e 仅变更本文，下表前六行是该轮的历史裁决；67c1b318 一轮从完整 UML 版继续复核；末三行为第二轮（输入 3fd0f815）、第三轮（输入 eeb9ec0e）与第四轮（输入 c70894f8）裁决，不能把历史图示记录误读为本轮形式要求。

| 历史节点 / 关联正文 | 本次核实与保留 | 当前不足及修正落位 |
|---|---|---|
| 6017003e 初次入库；原稿 §4/9/13/22 | 原稿将 Operation 放 66250、采用理想提交/恢复和总门禁；无新批准证据，后续源码裁决继续保留 | 不恢复旧草案的全局方案；§4/7/9/12/22 仍按已核实本地路径与局部前提执行 |
| 3476c44a 源码复审；分层设计 §5/6.2、V3 SAD §9、类设计 §12/13.3 | T/A/O 与 R 分工、旧可见性、运行期操作记录有依据；serial/session 的存在属事实 | F17/F19 表明传输匹配不等于上层成功/错误均准入；§11.1/11.2 补错误分支、原阶段/槽成员与投递失败的边界 |
| 9501c2b5 图示恢复 | 第 5 章纵向分层与组件框便于评审；该历史轮次保持四幅文本图原字节 | 后续 67c1b318 已转 UML；本轮保持 UML 形式，仅同步受理/回调说明与正文 |
| 15fcd7a6 三稿对照；分层设计 §3.5、类设计 §12.4 | 冲突资格覆盖锁外写入、独立请求不合并、厂商执行边界继续保留 | “重入查询走投影”过宽：F09 证明存在存储查询与查询回写；§9.2 保留读源并显式处理 provider 同步闭环，不发明缓存回退 |
| 3476c44a 引入的资源屏障；重构设计 §4.7 与分层设计 §6.2 | 超时不代表设备未执行/静止；隔离与恢复必须有证据 | F12 的 Get 也开关通道；§8.2/12.3 明确查询冲突、R 短请求租约和命令所需释放证据；不要求旧查询恢复成功才可释放已证实请求结束且可复用的资源 |
| 会话/加载设计：重构设计 §4.6、类设计末尾卡运行补充 | Refresh 按 scope 更新，未受影响读取继续有效，成熟协议继续复用 | 撤销加载成员不撤销设备写；§10.2 保留在途写的效果核对，§9.3 区分旧用例结束与后续新查询回写；§6.1 限定 Domain 仅承载纯状态部分 |
| 3fd0f815 独立架构评审（第二轮）；本文 §4～7、§12、§22 | T、A 边界，纯 Domain 与应用协调，兼容提交、作用域关联、资源屏障与分阶段门禁均保留 | O 无独立业务不变量，降为编排内核，进行中状态归 T/A（§4.1/4.2）；扩卡热点与 SlotCatalog（§3.3/17.1）；Primary 准入、激活回包关联、协议超时完成阶段（§4.4/11.2/12.4）；拆库单例与导出面（§7.1/14.2/22） |
| eeb9ec0e 第三轮独立审计（PR #3，Claude 修订 1afffa4e 已合并） | 保留 T/A、O 应用机制、原接口/兼容节点、会话门控、有限恢复与独立 SO/分阶段迁移 | 补跨 Owner 冲突/完成归属与 G3b 桥；纠正静态锁、目录动态事实和 MEP 推断；接管 DisableProfile 历史提示；补 G2b 容量差分。证据 F47～F50；裁决与反例见 §20.1/4.4 |
| c70894f8 第四轮复核（本轮，PR #3 第三轮修订） | 保留第三轮的跨 Owner 规则实质、激活锁归因、目录静态化与扩卡修改范围、MEP 推断撤回、C 旧翻译规则、G2b 导出面与冒烟矩阵 | §4.2 规则收为四条并补窄接口/H 注入以防编译环；按槽关闭提示归 T、无后缀键作旧外部输出（§13.2）；G2b 容量差分限于两推导可能不同的产品（§14.2）；补 Primary 结束与激活同锁（§11.2）及 MEP 缺省值不一致（F51，§17.1）；三个新增状态标记并回。裁决见 §20.1 末表 |

错误回包可能影响新轮是由静态路径推导的风险，尚未做交错复现；通道型查询不应被拟议屏障排除，是本次对目标设计的修正，当前生产屏障尚未实现。推荐防护仍为 PROPOSED。涉及返回/时延/重试变化时按 §9.5、§21 和对应 G3 切片验收，不能以本次文档修改宣称生产缺陷已修复。

---


### 20.6 远端分支意见审视与本轮处理

分支快照以 origin/main `964dc569`、publish `2448b42e`、独立评审 `1afffa4e`、round3/4 `f12ebe1e`、copilot `25ff3032` 为输入。清理范围仅为当前fork的origin；upstream保持只读。删除条件是各分支tip已在最终main历史中、有效意见有明确处置且main已成功推送；任何远端并发新提交均需重新比较，不强行覆盖。

| 分支/意见 | 审视判断 | 处理方式 |
| --- | --- | --- |
| copilot/research-architecture-comparison | tip已是main祖先，无独有提交 | 保留其端口归属、ATR/身份分离、66250/厂商边界更正；中央Coordinator、强制总线/统一快照已被后续裁决取代，不恢复 |
| docs/sim-target-arch-v1.2-independent-review | PR #2已合入 | 保留T/A裁决及证据；本轮同步此前未对齐的活动功能/分层/索引/AGENTS口径 |
| docs/sim-target-v1.2-audit-round3 | PR #3已合入 | 保留父子关联/无嵌套等待、旧用例ExpectedTransition桥、锁归因、静态目录、MEP限定、Disable提示、G2b容量/导出门禁 |
| docs/publish-local-doc-updates-20261029 | 初版骨架及详细设计尚未合入；报告已有main版本 | 三方合并骨架/文档，保留main原报告固定基线引用和全部复审修订，不用分支旧稿覆盖main |
| 报告P1-1：current/all投影 | 采纳 | §6.5.1；独立读取结果、必要投影片段与字段版本；生产两次读源仍待对应WU交接 |
| P1-2：联合建行/重载 | 采纳 | §6.5.2；新身份前规划角色、一次原存储计划、分别接纳及失效核对，不引入provider事务承诺 |
| P1-3：跨R/T发现及空位置版本 | 采纳 | §6.5.2；带资格观察和持续revision；纯模型可验证，实际事件桥未接生产 |
| P1-4：激活进度键与清理 | 采纳，保留第四轮对早清零的限定 | §6.5.3；T按slot/session/request维护，false不释放A/R资格 |
| P1-5：Primary模式 | 采纳 | §6.5.4；两类RIL、已M0和第三卡分支分别表达，旧完成点不统一 |
| P1-6：同步完成 | 采纳 | §6.5.4；同步结果、callback、内部回执分路，由C/宿主保持原预算/节点 |
| P1-7：工厂边界 | 采纳，限定G2b | §6.5.6；保留G2a附着用途，另列本库创建/拥有及匹配销毁，不强制动态卸载 |
| P1-8：本地Profile | 采纳参数和责任缺口 | §6.5.5；原port/force与验证证据分离、期望登记及原节点提示；不夸大force当前APDU作用 |
| P2-1～5 | 采纳最小修正 | 字段依赖、窄sink、资源正常出口、计划移Application、一个Manager内两套File；不借机复制生产算法 |
| 报告中的激活锁/MEP/G2b概括 | 采用第三/四轮证据作限定 | ctx_在wait时释放，排序主要来自activeSimMutex_；每槽一账户不必然否定MEP，不预断必改schema；工具链导出单列，容量门禁保持产品范围 |

以上“处理”指设计与骨架表达的同步，不代表生产写权、并发策略、设备恢复、ABI或动态卸载已完成。外部缺口仍按§21/§22仅阻塞对应切片。历史评审正文保留原基线，当前实现说明与检查以§6.5及源码详细设计为准。

### 20.7 双模型目录意见融合与本轮核验（2026-10-30）

完整读取Opus与Gemini两份只读评审，以本地`e295540f`工作树重新取证。共识、分歧、存疑及遗漏的逐项裁决统一在[详细设计§4.3](SIM_Multisim_Source_Architecture_Detailed_Design.md#review-decisions)；本文§5.1.5/§6.6维护上位边界。采纳Owner/变化轴内聚、R完整能力分包、专属Adapter回归及导出约束；调整公共依赖层、CardRuntime、GN粒度与Monitor方案；不采纳固定T/A高低、公共平台底座、无条件零传播等强主张。保留T/A/O/R及§6.5全部异步、字段、会话、资源和完成不变量。

本轮只改两篇指定设计；未改源、GN、评审或执行状态。现状树/逐文件附录与73个真实文件逐项比对，生产承接表覆盖61头/55源并区分GN54源、测试及未装配源；本地链接/章节锚点、表格、围栏、图示和差异检查见详细设计§20。未运行OHOS、业务测试或设备验证；历史构建/PASS保留原基线。目录/target/namespace为推荐，尚未实施，draft/pending不变。

## 21. 逐项决策与外部缺口

### 21.1 原 TGT-OPEN-001～013 的收敛

下表的“推荐明确”不等于已批准。除事实项外，目标方案仍为 PROPOSED；所有责任角色均待对应团队确认，未虚构姓名/审批日期。验收栏是未来实施验收条件，不是本轮测试报告。

| 原 ID / 问题及影响 | 已核实证据 | 推荐与取舍 / 正文落位 | 验收方式 | 状态、责任角色与剩余阻塞范围 |
|---|---|---|---|---|
| TGT-OPEN-001 跨 SA 传输、结果查询/恢复；易双编排 | F10：66250 加载/死亡接收与任务 IDL；F11～F13：Core 卡侧原语；无通用操作查询 | 保留既有 IPC/listener；本地多步用例由 Core 中目标事实 Owner 编排，高层任务边界留服务；不添 OperationId/可靠重放承诺，§7/12 | 匹配服务调用链，逐任务核实主编排、重试/补偿、丢回调/重连；有协议增强才做持久交付实验 | OPEN-EXTERNAL；eSIM 服务/API 负责人；只阻塞依赖远端任务终止/持久恢复的新用例，不阻塞 Core 普通功能及机械拆库 |
| TGT-OPEN-002 ICardChannel 是否包揽 PSIM/eSIM | F03/F08/F12/F16/F25/F26：EF/APDU/安全/电话簿/STK 各有真实协议 | 不建万能通道；保留协议，按用例抽窄执行端口，共享设备域由 R 屏障表达，§6/17 | 每个端口有实际调用者、数据/错误/生命周期；Get 的实际资源占用有归属，无跨层平台类型泄漏 | CLOSED-RECOMMENDATION；SIM 架构/Runtime 负责人；不再悬置全部接入，具体端口仍随切片评审 |
| TGT-OPEN-003 厂商端口、ABI/版本 | F28 的 dlopen/dlsym；F06/F26 同步 Hook 与特殊分支 | 私有适配现有 ABI，出站查询/执行/通知与入站命令分责；不擅改签名或统一延迟 Hook，§17 | 匹配厂商头/库，区分查询/副作用执行/通知和入站命令；符号、缺库、重入、执行结束/在途卸载及 Feature 组合 | OPEN-EXTERNAL；厂商集成/构建负责人；只阻塞相关产品分支与新 ABI，不阻塞不含该 Hook 的切片 |
| TGT-OPEN-004 Assignment 原子存储 | F06/F07/F21/F22：参数、缓存及默认卡 URI 分离；无客户端 CAS；F37 已挂 provider 有事务调用但命令/schema 不匹配 | 近期保留旧协议并按字段计划；不承诺统一原子性。长期先证 provider 事务/写者再定方案，§13 | 匹配 provider 的命令/schema、默认卡跨行故障/崩溃；字段并发、参数部分失败、外部写者；重启观察 | OPEN-EXTERNAL；数据提供方/角色负责人；阻塞强原子持久增强，不阻塞保留旧行为的 A 入口整理 |
| TGT-OPEN-005 Event Sourcing | F21/F22 现有存储路径，F10 无通用重放契约；未发现该目标实现或独立业务需求 | 默认不采用；操作日志和可靠通知按具体需求独立论证，避免无收益的 schema/协议扩张，§12/13/15 | 无新增日志/schema/事件重建依赖；未来有审计/恢复需求时给替代方案与成本证据 | CLOSED-RECOMMENDATION；架构负责人；关闭为当前设计取舍，不是批准。不阻塞任何近期结构切片，可靠通知缺口归 001/009 |
| TGT-OPEN-006 Subscription 身份生成/恢复 | F04/F22：ICCID 匹配、既有 simId/绑定路径；永久身份批准缺失 | 沿用不透明 simId，强类型不改变政策；不引入第二公共 ID、NULL 迁移或永久稳定承诺，§3/10/16 | 插拔/重启/停用重启用/删除重建的旧行为采集；产品确认身份保留/迁移 | OPEN-EXTERNAL；产品/领域/存储负责人；只阻塞新身份政策与 schema，不阻塞沿用旧 ID 的解耦 |
| TGT-OPEN-007 Topology 事务范围 | F04/F23：全量缓存重载；F16：局部加载；F21/F22 无全局事务证明 | T 统一入口和短提交，按账户/绑定键预留/复核；A 按角色约束域与真实资源冲突裁决，并以 AssignmentTransition 对同一 Radio 域的切换准入；运行期请求状态按请求；不把设备等待锁进设备级大聚合，§4.3/9.2/10 | 无关卡可前进、同绑定冲突、跨键绑定迁移与跨槽默认角色唯一性；无全局版本误拒绝；并发 Primary 请求的旧轨迹与收紧后差异 | PROPOSED，推荐明确；SIM 架构负责人；阻塞实际写权迁移的并发设计验收，不阻塞源码依赖收口 |
| TGT-OPEN-008 成功是否含拓扑/Radio 收敛 | F05～F08/F11/F15：不同返回、业务结果、部分效果与后续动作 | 逐旧方法保持成功范围；新增内部 Unknown 不加公开码；远端 listener 范围待契约，不默认含驻网，§9/12/16 | 正常/部分失败轨迹与返回/out/callback/查询对照；服务与产品确认长期收敛语义 | PROPOSED + OPEN-EXTERNAL；API/产品/eSIM 负责人；只阻塞改变成功保证的接口或远端依赖切片 |
| TGT-OPEN-009 Commit/可见性/通知 | F05～F09/F11 直接反例；F21 无跨存储事务证据 | 近期按旧节点接纳，严格持久后发布与可靠投递另设前提；通知失败不虚构回滚，§8/9/13/15 | §9.3 方法级失败轨迹（含写卡超时与晚到、Data 忽略错误与 Primary 失败完成事件）、查询中间态、同字段 W1/W2 锁外写排序、通知次数/偏序；新可靠投递做真实崩溃窗口实验 | PROPOSED；SIM/API 负责人，可靠投递需 provider/服务；仅阻塞所改方法的行为切换，长期增强不阻塞 G2 |
| TGT-OPEN-010 Linux 最小生产闭包 | F03 装配；systemtest 原文和工作树差异；F29 当前 capability 与旧 Oracle 不一致 | 复用独立 host 工程，从真实 OnInit+无卡 RIL 输入切片起；先补匹配平台/存储契约，不能以容量 PASS 代替，§19 | 真实链接/生产函数执行审计、合法边界输入、负控、清理和 XML/JSON；未知依赖明确 BLOCKED | PROPOSED；测试/平台适配负责人；事实范围已明确，完整闭包仍未运行，只阻塞对应业务自动化门禁 |
| TGT-OPEN-011 Slot 写命令绑定时点 | F15：接收只捕获 slot，业务在队列执行；F09 后台任务回写 | G2 保留原解析点；G3 在实际执行接纳/发起处固定目标，前移至 IPC 接收或禁止旧重定向须独立行为评审，§10/11/16 | 接收后排队期间换卡、后台回写迟到、Refresh 期间在途写、旧 session 错误、无 simId PIN、非目标不受影响及原错误/时延 | PROPOSED + OPEN-EXTERNAL；API/产品负责人；只阻塞受影响异步方法的增强，非所有查询/拆库 |
| TGT-OPEN-012 多版本组合查询 | F07/F09/F23：不同来源独立更新，无原子组合证明 | 内部携带版本/来源、有界重读；旧外部输出不加新字段/错误，不宣称原子快照；强组合另审，§8.3/16 | 并发 T/A/R 变化、原查询来源和错误；新接口明确一致性及预算 | PROPOSED；查询/API 负责人；不阻塞旧查询适配，阻塞新增强一致保证 |
| TGT-OPEN-013 当前图真实组件/调用/SA | F01～F03/F10/F14/F24/F28/F30 的装配、GN、配置及接口 | 已用固定代码重画当前图；区分 Runtime/任务、配置/实测、源码存在/装配，§5.1/7/20 | 固定 URL/符号/行号复核，GN 条件与实例构造一致；设备装载另由产品验证 | RESOLVED-FACT；本仓静态问题关闭；设备真实进程/加载未验证，仅阻塞设备部署结论 |

### 21.2 正文隐含决策的归并与剩余边界

不另建竞争决策编号。以下缺口归入原 ID，并补齐可执行默认边界：

| 隐含问题 / 原 ID 去向 | 默认方案及责任角色 | 仍需证据、验收与仅阻塞范围 |
|---|---|---|
| 单主编排的本地/远端划分 → 001/008 | Core 本地用例由目标事实 Owner（T/A）唯一编排，服务高层任务不复制；eSIM 服务负责人 | 匹配服务仓及逐任务时序/补偿；只阻塞该远端任务重构 |
| 进行中事实、切换准入与激活回包关联 → 007/011 | A 拥有 AssignmentTransition，T 拥有激活进行中标志；G2 保留旧入口、旧有界等待与旧回包处理；SIM 架构/Runtime 负责人 | 并发 Primary/激活、父子请求等待、结果/超时归属与兼容投影提前清零（F43～F47）的旧轨迹及 G3c 冲突政策；G3b 先闭合旧用例的 ExpectedTransition 桥。只阻塞对应迁移/行为收紧 |
| 超时后静止、共享设备资源键/恢复预算 → 001/007/008/011 | R 保守共享域，Unknown 不当可用；Runtime/RIL/设备负责人 | 实际 Modem/eUICC 映射、含开关通道查询的冲突集合、可达核对、运行期及重启丢关联后的结束/失效判据、旧错误映射和预算；没有证据不启用相关设备用例的新屏障/重试政策，其他资源可推进 |
| provider 直接写者、T/A 整行覆盖、存储效果未知、反查与 NULL → 004/006/009 | 字段级计划，T/A 持冲突资格并经 I 核对；Profile 关闭的按槽标签恢复提示随 WU-05/G3a 委托 T，无仓内读者的 `last_deactive_profile` 按原节点作旧外部输出；重入查询保留原读源，保持旧 schema/占位；provider/字段负责人 | 匹配 fork 实现（已挂上游 provider 的 SLOT_INDEX/SIM_ID 与 schema 差异须先解决）、写者清单/权限、调用结束/错误含义、反查重入、事务/条件更新、旧读者及恢复实验；`last_deactive_profile` 的外部读者与语义（OPEN-EXTERNAL）；只阻塞依赖该重入路径、新隔离/强一致/数据迁移或改变该参数写入节点/取值的切片，不阻塞保留既有 CRUD |
| 新操作日志、Outbox、可靠结果 → 001/005/009 | 内存关联、不承诺同 ID 重启；服务/存储/API 负责人 | 产品需求、协议及持久格式审批、崩溃验证；只阻塞新增恢复承诺 |
| 合法会话接续/Refresh scope → 007/011 | R 产代次，T 产绑定版本；ExpectedTransition 仅有证据接续 | Runtime 负责人核对卡型依赖集合、在途读/写差异与因果输入；只阻塞相关 G3b 会话迁移，不阻塞纯规则整理 |
| 新增关闭协议与 ABI → 003/013 | 具体类型销毁、共享 RIL 归宿主、失败不卸载；构建/宿主负责人 | 工厂/deleter、在途回调、CFI/Feature、实际链接/生命周期实验；常驻机械拆库与动态卸载门禁分开 |
| 四槽/MEP/新公共 Subscription API、槽种类与容量口径 → 006/012/013 | 保持原产品/公开能力；SlotCatalog 先以旧函数等值方式承载配置差异，N 槽新策略与新增 Port 映射属 G4，保留已有 MEP 分支，MEP 能力参数各读取点的缺省值差异（F51）按 LEGACY 保留；产品/API/RIL 负责人 | 匹配硬件/权限/配置、端到端契约及设备矩阵；逐产品参数的目录与旧函数等值差分；各产品 `const.ril.sim.esim_support_mep` 实际取值及缺省值差异的产品裁决；容量等值与 MEP 口径裁决只阻塞把相应能力纳入目录的切片；新增能力仅阻塞 G4 |
| 上游 ID/批准来源缺失与主规范重叠 → 006/008/013 | 本文承接目标设计，原清单/迁移/测试保留职责；文档/领域负责人 | 补领域契约和审批记录、确认跨文档权威关系；不以更新日期裁决，不授权改变业务不变量。第二轮 O 降级后，重构设计、分层与迁移设计、文档索引及 AGENTS 中“T/A/O 三业务上下文”与 `O/...` 映射待按 §4.1/4.2 同步；同步前以本文裁决和功能/WU 编号追溯 |

---

## 22. 分阶段实施门禁

### 22.1 按依赖切片，不设置一刀切总门禁

沿用既有 [G 阶段迁移设计](SIM_Architecture_Refactor_Design.md#migration-stages)。下表只列目标架构的准入/退出要求；本文修改不表示生产实施已获授权。

| 阶段 | 允许推进的内容 | 必须满足的本切片门禁 | 不应被无关缺口阻塞 |
|---|---|---|---|
| G0/G1 基线与护栏 | 固定代码/产品配置、文档来源、原接口轨迹、依赖清单及 host 设施 | 区分契约/静态候选/运行记录；受影响功能有明确边界与 Oracle | 远端日志/新身份未定不妨碍只读核查、纯规则测试或设施建设 |
| G2a 原 SO 内依赖/所有权收口 | 私有 HostPorts、委托、工厂/deleter 设计；保持原 writer 和原协议；盘点 SIM 对 Core 内进程级单例（EXT、HiSysEvent、IMS/卫星客户端）与 CMI 自调用的依赖并裁决唯一定义处；可选以旧函数等值方式引入 SlotCatalog | 消费侧 ABI、初始化/失败清理、共享 RIL 所有权明确；单例归属表与跨边界符号清单完成；若引入目录，逐产品参数与旧容量函数等值；定向差分 | 不要求先迁所有聚合或新增持久日志 |
| G2b 机械链接拆分 | Core→SIM→API/utils；同进程常驻 SO | 产物级检查：Core 不新增业务导出、SIM 业务导出仅私有工厂/匹配销毁；工具链必需符号单列；NEEDED 与安装路径正确；进程级单例和静态状态无重复定义；SA 全局构造注册与初始化顺序不变；CFI/PAC/LTO/Feature 组合构建；两种容量推导可能不同的产品（虚拟 Modem 开关为真且配置槽数小于 8，§14.2）另做各容量调用点/首次调用时点/初始化次序差分，其余产品只记录两推导等值，均不以 SlotCatalog 为前提。行为差分用同一产品镜像拆分前后的启动与冒烟矩阵（无卡/单卡/双卡、PIN、激活、默认 Data、Primary、eSIM 开/关，以及该产品启用的三卡/VSIM/卫星分支）及已可运行的 host 切片；单测由源码静态库构建，不证明打包 | 不把全部长期恢复/原子存储增强当必备；Linux 初始化闭包未打通不阻塞本阶段，但不得宣称业务全量差分；编译本身不足验收 |
| G2c 生命周期增强 | 私有 StopAndDrain、重建/卸载/半初始化回收 | 新任务停止、注销、迟到准入、排空、匹配销毁和不可排空时不卸载 | 按产品需要前置；有动态卸载/重建需求的产品不能跳过，常驻拆分不假装已增强 |
| G3a 账户/字段写权 | WU-01/02/03/05、WU-04 的角色片段及 WU-09 的 T loaded 按 §6.3 同步切换；A 旧策略桥提供角色片段 | 所有初始化/重载/后台/恢复/分支 writer 收口；冲突写资格覆盖锁外 I/O，字段互不覆盖，角色域唯一；重入仍保留原查询源，§9 旧可见性；稀疏/越界 simId 的旧可见行为已采集并裁决（F42） | 严格 provider 原子性未证可保留旧协议；不能声称已获得全局一致性 |
| G3b 会话与加载 | WU-06 与 WU-09 的 R loaded，runtimeEpoch/局部 LoadScope/ExpectedTransition；号码固定回写目标独立验收 | 换卡、局部 Refresh 的在途读/写、合法新会话与后台迟到；G3c 未迁用例的 ExpectedTransition 桥须闭合；行为差异独立确认 | 无关 eSIM 任务日志不阻塞有完整证据的卡型 |
| G3c Owner 用例编排与角色 | WU-04 参数/last/偏好/去重值、WU-07 的进行中标志/重试/自调用；A 的切换用例与 T 的激活用例成为唯一进程所有者，部分失败与资源核对 | 关闭旧同层重复重试发起及 detached 线程自调用；成功/错误先校验再更新阶段/槽计数；独立同参调用不合并；并发 Primary 请求与迟到激活回包的准入收紧作为单列行为变化获批并差分（F44/F45）；跨 Owner 不嵌套等待/重复准入、角色与资源联合冲突集合、请求/进行中投影/租约归属按 §4.2/12.1 验收；运行期和重启丢关联后的效果/静止判据、预算和原错误映射明确；保留 Data/Primary 各自错误及完成含义 | 无证据资源保留旧构建，其余已闭合用例独立推进 |
| G3d 外围/eUICC/订阅 | WU-08 与 WU-09 的外围订阅；已于 G3a/b 转移的 loaded 标志不得仍由旧壳直写 | 完整相关入口接管、匹配服务/厂商边界；Get 通道竞争、缓存命中、恢复查询准入、通道静止/销毁及回调验证 | 未装配源码不自动激活，不把外围功能遗漏为“后续再说” |
| G4 独立新行为 | 新身份/NULL/schema、日志/可靠交付、新 API、四槽/MEP、N 槽 Primary/标签策略与 Port 感知绑定 | 明确需求和批准、外仓支持、数据回退、产品设备验证 | 不反向作为所有机械优化的门槛 |

### 22.2 每次切片的验收与回退

确认受影响 SIM 功能、WU 字段与调用者；正常、部分失败、超时、迟到、重连/重启分别说明可保证范围。结构检查和 Legacy/Split 运行差分分别出证据，保持未覆盖/不可比较状态，不通过改测试期望掩盖行为变化。

回退不是只执行 git revert：先停止相关受理，确认在途设备效果及代码/回调生命周期，核对旧产物可读的数据/参数，再切回；未知效果保持隔离，不将旧快照强写为现实。无 schema 变化的切片不发明数据迁移，确有新格式的 G4 必须有配对迁移/回退方案。长期增强缺口只能阻塞它实际依赖的功能和产品组合。

回退触发条件按阶段写入切片验收单：G2a/G2b 出现产物检查失败（导出面、NEEDED、单例重复定义、CFI）、冒烟矩阵差异或启动/SA 注册异常时，恢复单 SO 构建，无数据迁移；G3 出现未获批准的旧轨迹差异、写者扫描遗漏或并发负控失败时，切回旧构建，并按本节先核对在途设备与存储效果。

### 22.3 不构成完成的证据

移动文件、增加层/Manager、编译通过、仅正常返回相同、设施 Mock PASS、当前配置中列出 SA、清空 channel/operation 变量，都不能单独证明行为兼容、设备静止或领域写权迁移完成。

---

## 23. 修订记录

| 版本 | 日期 | 变更 |
|---|---|---|
| 1.0 | 2026-09-04 | 初始架构草案 |
| 1.1 | 2026-10-23 | 明确决策状态和权威范围；消除操作聚合部署歧义；补充提交、可见性、版本、回调、跨 SA、恢复和兼容协议；补充 DDD 复用测试边界 |
| 1.2 | 2026-10-23 | 原修订：合并当前与目标逻辑图；当时未重新核验代码，保留原所有权及一致性提案 |
| 1.2（本轮复审） | 2026-10-26 | 固定 6017003e 基线，核实文档关系与源码；纠正全部 Operation 归 66250、Coordinator 入 Domain、统一持久后可见及查询只读；补全状态写权/功能映射、局部会话与资源恢复、存储能力/跨 SO 边界；逐项收敛 13 个 OPEN 和隐含决策；区分分阶段门禁、外部缺口及本轮静态检查，保持 draft/pending |
| 1.2（分层图表达优化） | 2026-10-26 | 第 5 章恢复原稿的文本分层框、层内组件框与箭头说明；优化当前/目标对照，明确 Application、Domain、R/I 与 H 的关系；编译依赖和运行示例同步采用文本图，保留已核实的设计结论与固定源码基线 |
| 1.2（三稿对照复核） | 2026-10-26 | 以当前 9501c2b5 的 v1.2 为基础读取 v1.16/v1.17，保留主路径、ID、分层图形式及 6017003e 生产基线；补输入来源与采用取舍；修正角色约束域、冲突写跨 I/O 排序、号码超时/晚到、独立请求去重、重试与厂商执行边界，同步决策和切片验收；保持 draft/pending |
| 1.2（历史与关联设计复核） | 2026-10-26 | 从 15fcd7a6 追溯四次提交和关联设计，重读实际 RIL 成功/错误交付、RadioProtocol 准入及 eUICC 查询占用；修正重入查询源、查询资源屏障、R 短请求租约与释放条件，补 Refresh 在途写、错误/重复阶段准入和 Domain 数据边界；保留四幅文本图、生产基线和全部原决策 ID，风险不冒充已复现/已修复 |
| 1.2（UML 图示转换） | 2026-10-26 | 按要求仅转换第 5 章图示形式：当前架构、目标分层及编译依赖采用 UML 组件/包图，SetShowNumber 采用 UML 时序图；保留全部原文字、关系和失败分支，更新读图说明与检查记录；不改变架构设计内容，保持 document_id、生产基线和 draft/pending |
| 1.2（独立源码复审；revision 2026-10-26-independent-01） | 2026-10-26 | 从实际 HEAD 67c1b318 草案独立通读与取证，保持 v1.2/原 ID/UML 形式及 pending；补普通/force/卫星激活全链、角色读源与 Data/Primary 失败含义、名称/查询回写和 LOADED 原语义；明确各 WU 的先接管/后编排及入口偏好 Owner；按外仓各 SHA 修正“未提供”与匹配 provider 缺口；收敛资源预留原子边界和重启丢关联的真实准入前提。历史检查不晋升本轮结果；远端获取受网络限制。 |
| 1.2（独立架构评审；revision 2026-10-29-independent-02） | 2026-10-29 | 以 origin/main 3fd0f815 为输入独立评审上下文划分：保留 T、A；O 无独立业务不变量，降为应用编排内核，用例的唯一进程所有者为目标事实 Owner，进行中状态归 T/A；新增因果链、候选划分比较、进程所有者表和五类关键场景推演；新增 slotId 多重语义与扩卡热点（§3.3）及 H 构建的只读 SlotCatalog；补 Primary 入口准入、激活回包关联、协议超时完成阶段等 DERIVED 风险；补拆库的单例归属、导出面与 G2b 产物级门禁及回退触发条件；同步图示、功能/WU 映射、规则、决策与门禁，新增证据 F40～F46。功能/WU/决策编号不变，保持 draft/pending |
| 1.2（第三轮独立审计；revision 2026-10-29-independent-03） | 2026-10-29 | 输入为已合并 Claude PR #2 的 eeb9ec0e，未退回旧稿；保留 T/A/O 定位与 SO 迁移方向，补跨 Owner 冲突资格/完成归属及 G3b 旧用例桥，纠正激活锁归因、SlotCatalog 静态/动态边界和 MEP 推断，收口 Profile 关闭提示写权，补 N 槽修改范围与 G2b 容量门禁。新增 F47～F50；既有功能/WU/决策编号不变，draft/pending，不改生产代码 |
| 1.2（第四轮复核 PR #3；revision 2026-10-29-independent-04） | 2026-10-29 | 复核 PR #3 的 c70894f8：接受跨 Owner 规则实质、激活锁归因、SlotCatalog 静态化与扩卡范围、MEP 推断撤回、G2b 导出面与冒烟矩阵；§4.2 规则收为四条，补 T/A 只经 H 注入的窄应用接口互调；DisableProfile 按槽提示归 T，无后缀键作旧外部输出（OPEN-EXTERNAL）；G2b 容量差分限于两推导可能不同的产品；补 Primary 结束与激活共用 activeSimMutex_、MEP 缺省值不一致（新增 F51）；三个新增状态标记并回，§4.1 两段裁决合并，§24 改为与轮次无关的实施建议。功能/WU/决策编号不变，draft/pending，不改生产代码 |
| 1.2（目标源码骨架说明） | 2026-10-29 | 按用户授权在services/multisim建立未接入骨架；§6.4补源码映射、IDE路线、局部缺口与实际静态检查；同步流程对照文档中的T/A主编排及开发视图。未改旧生产实现/构建/公开契约，未转移写权，不改变draft/pending或各轮历史证据基线 |
| 1.2（分支整合与骨架协议修订；revision 2026-10-29-consolidated-05） | 2026-10-29 | 合并已审分支差异，保留第三/四轮修订；审视报告P1/P2并同步账户投影、发现/建行、激活进度、Primary模式、完成出口、结果sink、资源生命周期及附着/创建工厂；统一活动文档T/A与Application口径。生产实现/构建/公开契约未切换；验证与局部缺口见§6.5/§20.6 |
| 1.2（双模型目录裁决；revision 2026-10-30-directory-adjudication-07） | 2026-10-30 | 统一multisim完全替代的稳态目录原则、场景内聚、R能力与唯一治理、公共值/编排分target、导出及GN边界；纠正§5.1.5旧路径，§6.4保留现状、§6.6定义目标并链接源码详细设计。只修改两篇设计，未实施目录迁移或审批业务变更 |

---

## 24. 目标设计的边界

实施建议：评审后可进入 G0/G1、G2a；G2b 以产物检查和同一镜像的冒烟差分为门禁，容量逐调用点差分只对两种推导可能不同的产品执行；G3 各切片在对应冲突政策、桥接和恢复证据闭合后再切换。外部缺口只阻塞依赖它的切片，也不表示所有 G3 已可编码上线。各轮审计裁决见 §20.1/20.5。

保留身份与位置分离、T/A 事实所有权及目标事实 Owner 的唯一用例编排、集中解释槽号的只读目录、纯 Domain、生产卡协议复用及旧接口兼容。本文将这些原则落到真实写路径、可见节点、作用域关联和有限恢复能力上：目标结构可以演进，部分效果不能被一个成功/提交标签掩盖。

当前图由固定源码支持；目标图、接口和协议为推荐设计，未表示已经实现或批准。缺少匹配 provider、66250、厂商、产品与设备证据的地方均给出默认边界及局部阻塞范围。后续开发沿用既有功能/写权/迁移/测试清单，不再维护另一套业务模型。

评审一个功能时，按 §6 的原功能/WU 定位进入五个问题：真实入口与执行能力是什么；各字段谁唯一接纳、哪个用例是唯一进程所有者（§4.2）；成功、部分失败和超时在哪些节点可见；哪些目标行为需另行批准；用哪个生产切片及外部证据准许切换和回退。由 §3.3/17.1 查槽号与容量解释，§9/16 查行为，§10～14 查异步与存储约束，§22 查门禁；详细清单继续引用对应权威文档。
