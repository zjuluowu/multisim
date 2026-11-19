# SIM 重构历史索引与文档整理记录

从[开发文档入口](../README.md)查当前设计、业务开发和实施状态。下列材料保留决策过程、源码证据、测试历史和评审输入。逐篇按记录中的版本、作用范围、批准状态与代码基线使用。

| 资料 | 内容与查阅方式 |
| --- | --- |
| [统一前原稿](pre_unification_20261015/README.md)及[统一后旧专题](MultiSIM/) | 两套内容分别保留。统一后短稿记录了重写、冲突裁决与引用关系，统一前版本保留长稿、原图和原进展。 |
| [被接替的设计](superseded/) | 同 ID 目标草稿、V3 总体及类设计、旧分层补充视图和旧索引。现行目标、文件/类依赖和功能清单分别由主设计、源码详细设计和功能/WU 文档维护。 |
| [独立评审和备选方案](reviews/) | 目录 ADR、独立主设计复审、开闭源边界和评估材料。目录实施及验收由五组方案和迁移记录说明。 |
| [汇报及其派生材料](presentations/) | V22、V23、V24、开发讨论稿、配套备注、PDF 和生成清单。V23 审查说明在 reviews。 |
| [测试工程原始证据](../../services/multisim/test/docs/archive/) | 由测试工程维护，按各批基线、结果和环境解释。 |

2026-11-19 的[独立架构审查](reviews/SIM_Architecture_Independent_Review_20261119.md)记录对象生命周期、14 个场景、三种方案及按切片签收的条件。

## 整理前后

以 [整理输入 main@155f9c4c3](https://github.com/zjuluowu/multisim/tree/155f9c4c3378f271e5241368bec7616838cb3df1/telephony_core_service/docs) 为原路径和原始 Markdown 的固定来源。下表覆盖该提交的全部 69 个 docs 文件，含 Markdown、TXT、PPTX、PDF 和 JSON。已有的原稿归档保持原相对结构，文件名沿用原名。四个一级目录为 business、migration、knowledge、history，根部保留开发入口及稳定设计、状态、迁址引用。汇报附件和 JSON 以原字节归档，历史 Markdown 的链接重定位到现行路径或包含原证据的 Git 版本。

| 统计项 | 整理前 | 整理后 |
| --- | ---: | ---: |
| docs 文件总数 | 69 | 70 |
| 日常文档（history 之外，含按需材料） | 57 | 19 |
| 初次阅读的必读文档 | 6 篇 docs，含源码与测试导航共 10 篇 | 2 篇 docs，含源码导航共 3 篇 |
| 历史归档原件 | 12 | 50 |
| 非归档区的重复设计或状态入口 | 9 | 0 |
| 完全相同字节的副本 | 0 | 0 |
| 仓库 Markdown 失效的本地链接及锚点 | 108 | 3 |
| 本次迁址新增失效链接 | — | 0 |

整理前的必读范围按旧 README「现行设计与阅读顺序」统计，整理后按新版页首统计。重复入口指非归档区的 v1.16、v1.17、两篇 V3、分层迁移补充、评估优化稿和 MultiSIM 04、05、07。历史原件数量排除本索引；归档中类似标题的原稿和重写稿保留各自内容。

## 一致性检查

| 检查项 | 结论与处理 |
| --- | --- |
| 目标架构与决策状态 | v1.2 的修订为 `2026-11-17-reviewed-cross-capability-impact-09`，输入 `964dc5693badc08ce4abafd70d22f1dcd47cf727`。v1.16 的 `2026-10-23-review` 基线为 `6017003eff50c1bc8c5dfa72336a4bfe42e948b0`。v1.17 无 revision 字段，列出 `00b3faa6b02ac0f1adf77fb47f4a111cd95e665d`，仍缺该稿所需的基线核验。三稿均为 SIM-TGT-001、version 1.2、draft/pending。旧稿的 O 业务责任、持久化和跨 SA 提案按原稿追溯。 |
| 身份与写权 | 核对主设计 §3～6、§9～14、§22。由用例选择 SlotTarget、CardApplicationTarget、SubscriptionTarget，以业务对象和关联表达事实。T/A 各持唯一业务写权，O 提供请求机制，R 承担会话、Records 和设备运行支撑，C/I/H 分别负责兼容、技术适配和宿主装配。跨 Owner 依赖使用窄契约，避免中央转发和重复写权。 |
| 目录与源码 | 确认 account、role_assignment、card、device、infrastructure 的当前树，Infrastructure 内 compatibility、assembly、request 和纯身份类型按目录方案定位。两份目录 ADR 保持 Proposed，修订 B+ 属历史实施阶段；后续五组目录依据目录方案。保留目录方案字节校验及详细设计导航测试的固定路径。 |
| 流程与迁移 | 流程文档 §5～6 改为引用功能/WU 和源码图。功能文档 §4.4 引用目录、详细设计与部署章，原方法、写点、功能编号、WU 和静态证据均可追溯。评审提案及专项分析保留原证据。 |
| 对外接口 | 接口文档保持对权限、错误、消费者、4010、CMI 和 66250 的职责。跨仓实现、版本、Provider 和厂商导出依赖按可用证据核定。 |
| 当前实现 | 核对输入 Git 的根 GN 和 multisim 源表、工厂、Store、Host 夹具及执行状态。修订源码详细设计中 C0 参数 I/O 仍为草图的旧说法，保留真实 Store 验收的原批次证据。Legacy 生产、Target Host、写权迁移和产品验收以执行状态分别核定。 |
| 待确认项 | 目标架构批准与既有 Q 项仍按主设计 §21 管理。外仓证据、产品设备及未批准提案保持待确认；本次未发现必须另行裁决才能完成整理的文档职责问题。 |

## 验证范围

- 入口恢复运行 `python3 -B telephony_core_service/services/multisim/tools/check_sim_refactor_recovery.py --remote origin --fetch`，返回 CHECKPOINT_VERIFIED。Git 验证的是历史检查点和已提交路径。
- 仓库链接以 Markdown-it 解析有效 Markdown 链接和图片，合并 HTML 链接、同仓 main 路径、显式锚点和 GitHub 标题锚点核对。历史证据链接用 Git 树复核对应版本。外部站点可用性超出本次本地导航检查。
- 历史测试证据按原字节保护。剩余三处失效引用是冻结测试归档的固有问题，两个在 `sim028_shared_primary_20261112/README.md` 指向旧 sim_guard README，一个在 `sim028_stop_implementation_20261112/README.md` 指向旧 primary_slot README。这两份归档受测试完整性检查保护。可查固定提交中的 [原 sim_guard README](https://github.com/zjuluowu/multisim/blob/4c45061cd/telephony_core_service/services/multisim/test/sim_guard/README.md)和[原 Primary README](https://github.com/zjuluowu/multisim/blob/4c45061cd/telephony_core_service/services/multisim/test/systemtest/primary_slot/README.md)。
- 运行 `python3 -B telephony_core_service/services/multisim/test/cases/internal/infrastructure/test_navigation.py`，6 项通过。用 `sha256sum` 和现有 `documentation_moves()` 核对固定路径、目录设计原字节及迁址登记。本次没有重跑 Legacy、Target 或产品设备业务用例。
- 按 `git -c core.whitespace=cr-at-eol diff --cached --check` 及 `git diff --cached -M35% --name-status` 校验暂存差异。默认空白检查将三份既有 CRLF 知识页的改动行识别为尾空白，按 CRLF 检查通过。核对 100 个新建或恢复的固定 Git 路径，12 个附件和 TXT 与原件字节相同，70 个 SIM 编号及子项和 9 个 WU 均保留。发布前按文件范围和摘要检查工作区其他修改。保留生产、构建、测试逻辑、业务契约、冻结 Golden/Oracle 和历史测试证据。

## 逐文件盘点和迁移映射

路径均相对于 docs。末次修改取整理输入的 Git 记录。每个目标路径直接指向归位后的文件。处理栏说明保留、修订或归档原因。

### 现行与按需参考

| 原路径与末次修改 | 目标路径 | 类型、权威级别与状态 | 核心内容 | 重复与冲突 | 处理依据 |
| --- | --- | --- | --- | --- | --- |
| `README.md` · `1fd7c9c81` | [README.md](../README.md) | 索引 · Authoritative · 有效 | 开发任务入口和阅读顺序 | 旧统一口径与目标架构重复。旧版只指向阶段 §46～49。 | 改写为 A～E 路径和职责表 |
| `SIM_Architecture_Refactor_Design.md` · `1fd7c9c81` | [migration/SIM_Architecture_Refactor_Design.md](../migration/SIM_Architecture_Refactor_Design.md) | 功能迁移 · Authoritative · 有效 | 功能 SIM-001～062、WU-01～09 和旧轨迹 | 流程对照、分层补充、目标主设计。旧候选接口、物理布局按基线解释。 | 迁入 migration，保留编号、唯一闭包和固定源码证据 |
| `SIM_Directory_Migration_20261112.md` · `74e25901d` | [SIM_Directory_Migration_20261112.md](../SIM_Directory_Migration_20261112.md) | 迁移记录 · Supporting · 历史证据有效 | 五组迁址文件映射、GN、实测与取舍 | 详细设计的文件视图。迁移输入快照中的旧名与当前树有差异。 | 保留路径供冻结测试导航引用，标明记录范围 |
| `SIM_Multisim_Source_Architecture_Detailed_Design.md` · `155f9c4c3` | [SIM_Multisim_Source_Architecture_Detailed_Design.md](../SIM_Multisim_Source_Architecture_Detailed_Design.md) | 源码详细设计 · Authoritative · 有效 | 实际/稳态文件、类和逻辑 GN | V3 类设计、分层补充。旧 Primary 描述漏记 C0 Store 真实实现。 | 改正文案，路径供导航测试读取 |
| `SIM_Refactor_Execution_State.md` · `155f9c4c3` | [SIM_Refactor_Execution_State.md](../SIM_Refactor_Execution_State.md) | 实施状态 · Authoritative · 有效 | 证据基线、阻塞和任务 | 旧 MultiSIM 进展摘要。旧 §6 仍列已修 Legacy 失败。 | 更新页首，保留原记录和恢复协议路径 |
| `SIM_Target_Architecture_Design_v1.2.md` · `719e8b780` | [SIM_Target_Architecture_Design_v1.2.md](../SIM_Target_Architecture_Design_v1.2.md) | 架构设计 · Authoritative · 有效 / draft,pending | T/A、协议、资源和实施门禁 | 两份同 ID 草稿、V3、评估稿。旧稿把 O 当业务 Owner 或把未核实上游章节列为替代。 | 保留主文档及批准状态，修订引用 |
| `SIM管理对外接口说明_源码核实版.md` · `74e25901d` | [business/SIM管理对外接口说明_源码核实版.md](../business/SIM管理对外接口说明_源码核实版.md) | 外部接口 · Authoritative · 有效 | 4010、CMI、66250、权限和错误 | 接口全景、开闭源分析。声明和静态调用不能证明外仓实现。 | 迁入业务参考，保留源与版本 |
| `SIM重构代码目录结构设计方案.md` · `b283ad2f6` | [SIM重构代码目录结构设计方案.md](../SIM重构代码目录结构设计方案.md) | 目录设计 · Authoritative · 有效 | 五组物理职责及 Infrastructure | 两份 ADR、详细设计的目录视图。历史 B+ 六组曾是执行基线。 | 原路径原字节受护栏校验 |
| `SetPrimarySlotId_重构前后实现对比说明.md` · `f8c892ce9` | [business/SetPrimarySlotId_重构前后实现对比说明.md](../business/SetPrimarySlotId_重构前后实现对比说明.md) | 专项业务对比 · Supporting · 部分有效 | Primary 三条旧分支、目标四模式、异步关联 | 流程 §9 和源码 §15。目标设备链仍待验证，C0 后续已有 Host 实现。 | 保留按场景的代码证据，加适用说明 |
| `knowledge/code_map/conditional-build.md` · `84566de51` | [knowledge/code_map/conditional-build.md](../knowledge/code_map/conditional-build.md) | 代码地图 · Supporting · 有效 | 产品宏、Feature 裁剪和部署 | 接口与主设计 §17。按实际宏核对。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/code_map/modules.md` · `4e5142509` | [knowledge/code_map/modules.md](../knowledge/code_map/modules.md) | 代码地图 · Supporting · 有效 | 模块与常用入口定位 | 源码 README 和详细设计。路径以详细设计为准。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/code_map/ril-capabilities.md` · `efbefd48c` | [knowledge/code_map/ril-capabilities.md](../knowledge/code_map/ril-capabilities.md) | 代码地图 · Supporting · 有效 | RIL 能力分布和信令模式 | 外部接口。RIL 声明不代表上层公开。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/expert/constraints.md` · `4e5142509` | [knowledge/expert/constraints.md](../knowledge/expert/constraints.md) | 开发约束 · Supporting · 有效 | 工程注意事项与兼容冻结 | 主设计和 AGENTS。批准以契约和证据为准。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/expert/historical-issues.md` · `84566de51` | [knowledge/expert/historical-issues.md](../knowledge/expert/historical-issues.md) | 历史知识 · Supporting · 有效 | 历史缺陷线索、反例与高冲突文件 | 状态历史与主设计证据。历史缺陷不等于当前 FAIL。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/routing/new-interface.md` · `84566de51` | [knowledge/routing/new-interface.md](../knowledge/routing/new-interface.md) | 操作指南 · Operational · 有效 | 新增/修改 IPC 的同步点 | 接口说明。走正式 API 变更授权。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/routing/permissions.md` · `84566de51` | [knowledge/routing/permissions.md](../knowledge/routing/permissions.md) | 开发导航 · Operational · 有效 | 权限检查路由和敏感信息 | 接口权限表。接口批准按现行契约。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/routing/repo-boundary.md` · `84566de51` | [knowledge/routing/repo-boundary.md](../knowledge/routing/repo-boundary.md) | 跨仓参考 · Supporting · 有效 | 本仓与消费方、RIL 外仓责任 | 外部接口和边界报告。已见声明与不可见实现分开。 | 沿用现有知识路径，更新必要引用 |
| `knowledge/verify/build-test.md` · `1fd7c9c81` | [knowledge/verify/build-test.md](../knowledge/verify/build-test.md) | 操作指南 · Operational · 有效 | 构建环境、工具和参数 | 测试工程入口。实际结果只在状态维护。 | 沿用现有知识路径，更新必要引用 |
| `多SIM架构重构_设计说明与业务流程对照.md` · `26d81e540` | [business/多SIM架构重构_设计说明与业务流程对照.md](../business/多SIM架构重构_设计说明与业务流程对照.md) | 业务说明 · Authoritative · 有效 | 号码、激活、Primary、Records 的新旧时序 | 功能映射及源码树与其他主文重复。旧组目录和仅声明工厂的说法过时。 | 移入业务参考，迁移表和目录改链 |

### 评审、旧设计与汇报

| 原路径与末次修改 | 目标路径 | 类型、权威级别与状态 | 核心内容 | 重复与冲突 | 处理依据 |
| --- | --- | --- | --- | --- | --- |
| `MultiSIM/01_sim_subsystem_baseline.md` · `84566de51` | [history/MultiSIM/01_sim_subsystem_baseline.md](MultiSIM/01_sim_subsystem_baseline.md) | 旧专题与索引 · Historical · 固定基线有效 | 旧构建、源表、入口与状态分工 | 统一前长稿、接口证据。原静态基线区别于现在。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/02_current_architecture_and_evolution.md` · `84566de51` | [history/MultiSIM/02_current_architecture_and_evolution.md](MultiSIM/02_current_architecture_and_evolution.md) | 旧专题与索引 · Historical · 固定基线有效 | 卡形态及方案反例 | 统一前专项长稿、目标设计。当时选定阶段已演进。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/03_esim_slot_and_sa_boundary.md` · `84566de51` | [history/MultiSIM/03_esim_slot_and_sa_boundary.md](MultiSIM/03_esim_slot_and_sa_boundary.md) | 旧专题与索引 · Historical · 固定基线有效 | 两通道、Port、SA 和 SO | 统一前专项长稿、接口 §5。旧产品条件应按现行产品核对。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/04_ddd_target_architecture.md` · `84566de51` | [history/MultiSIM/04_ddd_target_architecture.md](MultiSIM/04_ddd_target_architecture.md) | 旧专题与索引 · Superseded · 已替代 | 四 Context 导读、事实、事件与恢复 | 统一前长稿、目标主设计。四 Context 是旧结论。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/05_detailed_class_and_data_model.md` · `84566de51` | [history/MultiSIM/05_detailed_class_and_data_model.md](MultiSIM/05_detailed_class_and_data_model.md) | 旧专题与索引 · Superseded · 已替代 | 类、状态和数据映射导读 | 统一前长稿、源码详细设计。旧聚合与写入图。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/06_refactoring_proposal_slides.md` · `84566de51` | [history/MultiSIM/06_refactoring_proposal_slides.md](MultiSIM/06_refactoring_proposal_slides.md) | 旧专题与索引 · Historical · 固定基线有效 | 旧汇报提纲与门禁讲稿 | 统一前提纲、演讲材料。旧四 Context 与固定事件数。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/07_subscription_refactoring_status.md` · `74e25901d` | [history/MultiSIM/07_subscription_refactoring_status.md](MultiSIM/07_subscription_refactoring_status.md) | 旧专题与索引 · Superseded · 已替代 | 旧进展、护栏及暂停项 | 统一前进度稿、现行状态。已被唯一执行状态接替。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/08_design_unification_decisions.md` · `84566de51` | [history/MultiSIM/08_design_unification_decisions.md](MultiSIM/08_design_unification_decisions.md) | 旧专题与索引 · Historical · 固定基线有效 | DU 系列冲突取舍与证据 | V3 与目标主设计复审。原裁决适用当时输入，后续两 Context。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/design_alignment_manifest.json` · `84566de51` | [history/MultiSIM/design_alignment_manifest.json](MultiSIM/design_alignment_manifest.json) | 旧专题与索引 · Generated · 派生快照 | 带 dirty 标记和文件摘要的统一分析清单 | 统一裁决、原始文档版本。历史路径/日期应按当时摘要。 | 整组快照归档，保留同名统一前长稿 |
| `MultiSIM/readme.txt` · `84566de51` | [history/MultiSIM/readme.txt](MultiSIM/readme.txt) | 旧专题与索引 · Superseded · 已替代 | 2026-10-15 的旧阅读顺序 | docs README 和统一裁决。原指向 V3 为现行。 | 整组快照归档，保留同名统一前长稿 |
| `SIM_Architecture_Cross_Review_OpenClosed_Boundary.md` · `74e25901d` | [history/reviews/SIM_Architecture_Cross_Review_OpenClosed_Boundary.md](reviews/SIM_Architecture_Cross_Review_OpenClosed_Boundary.md) | 评审与论证 · Historical · 固定基线有效 | 开闭源、源码组织和测试覆盖矩阵 | 边界依据、接口与验收。旧适配器、旧测试入口与现行不同。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_Architecture_Layering_and_Migration_Design.md` · `f3683496c` | [history/superseded/SIM_Architecture_Layering_and_Migration_Design.md](superseded/SIM_Architecture_Layering_and_Migration_Design.md) | 分层补充 · Superseded · 历史证据有效 | 分层/部署映射、功能覆盖和提案对照 | 主设计、功能和详细设计。旧组件名和候选目录不对齐五组。 | 归档原补充视图，交叉复审仍可追溯 |
| `SIM_Layered_Architecture_Class_Design_Version3.md` · `84566de51` | [history/superseded/SIM_Layered_Architecture_Class_Design_Version3.md](superseded/SIM_Layered_Architecture_Class_Design_Version3.md) | 旧设计与导航 · Superseded · 已替代 | V3 聚合、ACL、内部类和状态协议 | 当前源码详细设计；统一前原稿。旧类模型与现行 T/A/O 及类型路径不同。 | 原稿与入口保留历史位置关系并更新可点击链接 |
| `SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md` · `3c6c6d901` | [history/reviews/SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md](reviews/SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md) | 评审与论证 · Historical · 固定基线有效 | A/B+/C 选择与分期 | 独立 ADR。Proposed，推荐六组。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_Multisim_Source_Directory_Architecture_Decision_Independent_2026-10-31.md` · `74e25901d` | [history/reviews/SIM_Multisim_Source_Directory_Architecture_Decision_Independent_2026-10-31.md](reviews/SIM_Multisim_Source_Directory_Architecture_Decision_Independent_2026-10-31.md) | 评审与论证 · Historical · 固定基线有效 | 独立复核、Card/Device 拆分、实务定位 | 原 ADR、五组方案。Proposed，记录较早结构选择。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md` · `e295540fe` | [history/reviews/SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md](reviews/SIM_Multisim_Source_Directory_Review_2026-10-30_Gemini_argon.md) | 评审与论证 · Historical · 固定基线有效 | 方案 C、场景覆盖及未来扩展 | Opus 评审、ADRs。推荐 C，缺当批实施依据。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md` · `bd26b874d` | [history/reviews/SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md](reviews/SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md) | 评审与论证 · Historical · 固定基线有效 | Runtime 和共享机制、候选物理布局 | Gemini 评审、ADRs。旧目录和依赖事实随基线变化。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_Refactored_Software_Architecture_Design_Version3.md` · `84566de51` | [history/superseded/SIM_Refactored_Software_Architecture_Design_Version3.md](superseded/SIM_Refactored_Software_Architecture_Design_Version3.md) | 旧设计与导航 · Superseded · 已替代 | V3 SAD、部署、协作及状态写权 | 目标主设计；统一前原稿。四 Context 和当时阶段描述已替代。 | 原稿与入口保留历史位置关系并更新可点击链接 |
| `SIM_Target_Architecture_Design_v1.16.md` · `3c6c6d901` | [history/superseded/SIM_Target_Architecture_Design_v1.16.md](superseded/SIM_Target_Architecture_Design_v1.16.md) | 架构草案 · Superseded · 已替代 / 待批 | 2026-10-23 静态复核及方案修订 | 同 ID 的目标 v1.2。O 作为业务责任，早期模块草图。 | 归档；使用原 Git blob 核对输入摘要 |
| `SIM_Target_Architecture_Design_v1.17.md` · `489c88fe3` | [history/superseded/SIM_Target_Architecture_Design_v1.17.md](superseded/SIM_Target_Architecture_Design_v1.17.md) | 架构草案 · Superseded · 已替代 / 待批 | 2026-10-23 基于旧图的架构修订 | 同 ID 的目标 v1.2。旧基线未解析，替代上游文档未证实。 | 归档；使用原 Git blob 核对输入摘要 |
| `SIM_Target_Architecture_Independent_Review_2026-10-29.md` · `3c6c6d901` | [history/reviews/SIM_Target_Architecture_Independent_Review_2026-10-29.md](reviews/SIM_Target_Architecture_Independent_Review_2026-10-29.md) | 评审与论证 · Historical · 固定基线有效 | P1/P2、反证、历史骨架和 G2a 实施核验 | 执行状态、主设计。历史生产附着已回退。 | 归档到评审，沿用原输入、结论和版本 |
| `SIM_开闭源架构边界与重构决策依据.md` · `a7d3af8b8` | [history/reviews/SIM_开闭源架构边界与重构决策依据.md](reviews/SIM_开闭源架构边界与重构决策依据.md) | 评审与论证 · Historical · 固定基线有效 | SIM-OC-001～016、符号及兼容性风险 | 原边界报告、对外接口 §10。跨仓导出疑点为静态待证。 | 归档到评审，沿用原输入、结论和版本 |
| `openharmony_telephony_core_service_SIM_software_design.md` · `84566de51` | [history/superseded/openharmony_telephony_core_service_SIM_software_design.md](superseded/openharmony_telephony_core_service_SIM_software_design.md) | 旧设计与导航 · Superseded · 已替代 | 2026-10-15 版导航页 | 入口 README、历史原稿。原指向 V3 作现行。 | 原稿与入口保留历史位置关系并更新可点击链接 |
| `test/SIM卡管理_接口全景与架构决策建议.md` · `01a86b9f0` | [history/reviews/SIM卡管理_接口全景与架构决策建议.md](reviews/SIM卡管理_接口全景与架构决策建议.md) | 评审与论证 · Historical · 固定基线有效 | 外部消费者、写者及独立库/进程评审 | 接口说明、跨仓单页。候选部署和防护网设计未必获批。 | 归档到评审，沿用原输入、结论和版本 |
| `test/SIM卡管理_跨仓接口全景_单页.pptx` · `01a86b9f0` | [history/presentations/SIM卡管理_跨仓接口全景_单页.pptx](presentations/SIM卡管理_跨仓接口全景_单页.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | 跨仓接口、消费者及协作方式单页 | 接口全景评审与接口主文档。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构评估与优化设计.md` · `3c6c6d901` | [history/reviews/多SIM架构评估与优化设计.md](reviews/多SIM架构评估与优化设计.md) | 评审与论证 · Historical · 固定基线有效 | 逐项审视原方案、修订理由与取舍 | 目标主设计、功能迁移。历史 P 类提案及旧 Owner 划分。 | 归档到评审，沿用原输入、结论和版本 |
| `多SIM架构重构_V23_代码审查与优化说明.md` · `3c6c6d901` | [history/reviews/多SIM架构重构_V23_代码审查与优化说明.md](reviews/多SIM架构重构_V23_代码审查与优化说明.md) | 评审与论证 · Historical · 固定基线有效 | V23 源码审查、四项风险和阶段证据 | V23 审阅依据、PDF。评审风险须按实际基线和门禁复证。 | 归档到评审，沿用原输入、结论和版本 |
| `多SIM架构重构_V23_审阅依据.json` · `84566de51` | [history/presentations/多SIM架构重构_V23_审阅依据.json](presentations/多SIM架构重构_V23_审阅依据.json) | 汇报或评审附件 · Generated · 固定版本有效 | V23 评审输入、输出哈希和引用清单 | V23 PPTX/PDF及说明。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_开发讨论版_8页.pptx` · `322ce6c5b` | [history/presentations/多SIM架构重构_开发讨论版_8页.pptx](presentations/多SIM架构重构_开发讨论版_8页.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | 同槽换卡与迟到结果的八页开发讨论 | 配套讲稿及业务流程。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_开发讨论版_8页_含演讲词与备注.md` · `322ce6c5b` | [history/presentations/多SIM架构重构_开发讨论版_8页_含演讲词与备注.md](presentations/多SIM架构重构_开发讨论版_8页_含演讲词与备注.md) | 汇报或评审附件 · Generated · 固定版本有效 | 开发讨论八页讲稿与补充备注 | 开发讨论八页 PPTX。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 保留原讲稿和演讲备注 |
| `多SIM架构重构_开发讨论版_分层与功能迁移补充.pptx` · `efbefd48c` | [history/presentations/多SIM架构重构_开发讨论版_分层与功能迁移补充.pptx](presentations/多SIM架构重构_开发讨论版_分层与功能迁移补充.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | 开发讨论扩展，分层、功能承接和四条时序 | 业务流程对照。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_软件设计说明书_V22.md` · `84566de51` | [history/superseded/多SIM架构重构_软件设计说明书_V22.md](superseded/多SIM架构重构_软件设计说明书_V22.md) | 旧设计与导航 · Superseded · 已替代 | V22 说明书历史入口 | 归档完整原稿。链接和阅读顺序按历史。 | 原稿与入口保留历史位置关系并更新可点击链接 |
| `多SIM架构重构_问题分析与软件设计_V22.pptx` · `84566de51` | [history/presentations/多SIM架构重构_问题分析与软件设计_V22.pptx](presentations/多SIM架构重构_问题分析与软件设计_V22.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | 槽、账户与状态治理问题及设计汇报 | V22 说明书。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_问题分析与软件设计_V22_P4修复版.pptx` · `322ce6c5b` | [history/presentations/多SIM架构重构_问题分析与软件设计_V22_P4修复版.pptx](presentations/多SIM架构重构_问题分析与软件设计_V22_P4修复版.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | V22 第四页修订及其余讨论内容 | 原 V22 演示。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_问题分析与软件设计_V23.pptx` · `322ce6c5b` | [history/presentations/多SIM架构重构_问题分析与软件设计_V23.pptx](presentations/多SIM架构重构_问题分析与软件设计_V23.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | V23 分层、角色事实和兼容轨迹汇报 | V22、V24、V23 优化稿。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pdf` · `84566de51` | [history/presentations/多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pdf](presentations/多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pdf) | 汇报或评审附件 · Generated · 固定版本有效 | V23 代码审阅优化稿的 PDF 输出 | 配套优化 PPTX。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pptx` · `84566de51` | [history/presentations/多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pptx](presentations/多SIM架构重构_问题分析与软件设计_V23_代码审阅优化.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | 补证据、风险与退出条件的 V23 审阅演示 | V23 PDF、JSON 与说明。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |
| `多SIM架构重构_问题分析与软件设计_V24.pptx` · `322ce6c5b` | [history/presentations/多SIM架构重构_问题分析与软件设计_V24.pptx](presentations/多SIM架构重构_问题分析与软件设计_V24.pptx) | 汇报或评审附件 · Generated · 固定版本有效 | V24 更新分层和模块职责的汇报 | V23 与目标主设计。汇报时点的模型和阶段不能用来判定当前批准或验收。 | 按原字节归档 |

### 原有历史归档

| 原路径与末次修改 | 目标路径 | 类型、权威级别与状态 | 核心内容 | 重复与冲突 | 处理依据 |
| --- | --- | --- | --- | --- | --- |
| `history/pre_unification_20261015/MultiSIM/01_sim_subsystem_baseline.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/01_sim_subsystem_baseline.md](pre_unification_20261015/MultiSIM/01_sim_subsystem_baseline.md) | 历史原稿 · Historical · 既有归档 | 旧系统 4+1 与对象协作长稿 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/02_current_architecture_and_evolution.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/02_current_architecture_and_evolution.md](pre_unification_20261015/MultiSIM/02_current_architecture_and_evolution.md) | 历史原稿 · Historical · 既有归档 | SIM 数量扩展与 pSIM/eSIM 演进原稿 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/03_esim_slot_and_sa_boundary.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/03_esim_slot_and_sa_boundary.md](pre_unification_20261015/MultiSIM/03_esim_slot_and_sa_boundary.md) | 历史原稿 · Historical · 既有归档 | eSIM/Slot 与独立 SA 分析原稿 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/04_ddd_target_architecture.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/04_ddd_target_architecture.md](pre_unification_20261015/MultiSIM/04_ddd_target_architecture.md) | 历史原稿 · Historical · 既有归档 | 领域失真与 DDD 重构分析原稿 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/05_detailed_class_and_data_model.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/05_detailed_class_and_data_model.md](pre_unification_20261015/MultiSIM/05_detailed_class_and_data_model.md) | 历史原稿 · Historical · 既有归档 | 卡类模型与数据关联原稿 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/06_refactoring_proposal_slides.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/06_refactoring_proposal_slides.md](pre_unification_20261015/MultiSIM/06_refactoring_proposal_slides.md) | 历史原稿 · Historical · 既有归档 | 统一前汇报原提纲 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/MultiSIM/07_subscription_refactoring_status.md` · `84566de51` | [history/pre_unification_20261015/MultiSIM/07_subscription_refactoring_status.md](pre_unification_20261015/MultiSIM/07_subscription_refactoring_status.md) | 历史原稿 · Historical · 既有归档 | 统一前进展及恢复指南 | 2026-10-15 统一后同名专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/README.md` · `84566de51` | [history/pre_unification_20261015/README.md](pre_unification_20261015/README.md) | 历史原稿 · Historical · 既有归档 | 统一前归档说明 | 统一裁决。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/SIM_Layered_Architecture_Class_Design_Version3.md` · `84566de51` | [history/pre_unification_20261015/SIM_Layered_Architecture_Class_Design_Version3.md](pre_unification_20261015/SIM_Layered_Architecture_Class_Design_Version3.md) | 历史原稿 · Historical · 既有归档 | 统一前 V3 类设计 | 统一后类设计。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/SIM_Refactored_Software_Architecture_Design_Version3.md` · `84566de51` | [history/pre_unification_20261015/SIM_Refactored_Software_Architecture_Design_Version3.md](pre_unification_20261015/SIM_Refactored_Software_Architecture_Design_Version3.md) | 历史原稿 · Historical · 既有归档 | 统一前 V3 SAD | 统一后 SAD。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/openharmony_telephony_core_service_SIM_software_design.md` · `84566de51` | [history/pre_unification_20261015/openharmony_telephony_core_service_SIM_software_design.md](pre_unification_20261015/openharmony_telephony_core_service_SIM_software_design.md) | 历史原稿 · Historical · 既有归档 | 统一前 4+1 源码设计长稿 | 旧源码基线专题。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
| `history/pre_unification_20261015/多SIM架构重构_软件设计说明书_V22.md` · `84566de51` | [history/pre_unification_20261015/多SIM架构重构_软件设计说明书_V22.md](pre_unification_20261015/多SIM架构重构_软件设计说明书_V22.md) | 历史原稿 · Historical · 既有归档 | V22 原说明书，问题、方案及闭环 | V22 演示与 V23 审查。原背景/目标和状态以当年记录为限。 | 原位保留备查 |
