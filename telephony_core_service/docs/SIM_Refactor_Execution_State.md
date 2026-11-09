# 多 SIM 重构执行状态

本文件集中维护当前实施位置、证据、局部阻塞和下一批任务；技术决策继续由[目标主设计](SIM_Target_Architecture_Design_v1.2.md)维护，流程由[流程对照](多SIM架构重构_设计说明与业务流程对照.md)维护，历史裁决留在[原评审](SIM_Target_Architecture_Independent_Review_2026-10-29.md)。最近恢复/一致性核对：2026-11-09；测试工程内聚迁移见[§13](#test-relocation-20261109)，本地开闭源复审增量见[§12](#open-closed-followup-20261109)，历史分支合入main及重验见[§11](#main-integration-20261105)，G2a工作分支复验见[§10](#g2a-revalidation-20261105)，测试防护网首批见[§8](#sim-guard-20261104)，第二轮结果及阻塞见[§9](#sim-guard-round2)。此前G2a记录保留，各历史运行结果仍按其原日期、基线和覆盖范围解释。

<a id="current-status"></a>

## 0. 当前核实摘要（2026-11-09增量；历史运行保留原基线）

**当前结论：B+源码结构已落地；G2a达到 `G2a_BPLUS_HOST_VERIFIED`；真实旧SIM组件兼容集已扩展到17条。完整产品准入、生产写权迁移及新能力均未完成。** 成熟业务仍由 `services/sim/` 承担。下面按交付位置和实际测试基线汇总；§3.1～3.5、§8～10保留各批历史过程，不把旧PASS改写为当前HEAD重跑。

2026-11-09用户授权基于[开闭源交叉审视](SIM_Architecture_Cross_Review_OpenClosed_Boundary.md)优化现有文档和代码；该批输入为本地/远端main `a7d3af8b8809513e7b8e73196a94551a8ecff97d`加工作树变更，尚未提交/推送。账户注销修复已按用户要求撤回；保留旧缺陷记录、独立契约护栏及重新验证，统一见[§12](#open-closed-followup-20261109)。下表2026-11-05记录仍按原日期解释，不把后续工作混入原历史证据。

### 0.1 代码保存与分支关系

| 已核实引用 | 提交 | 已保存内容与边界 |
| --- | --- | --- |
| `origin/main`（2026-11-09恢复工具fetch核实） | `a7d3af8b8809513e7b8e73196a94551a8ecff97d` | 已含此前G2a/B+和SIM Guard发布及边界分析文档；本轮优化在本地工作树，未发布。历史文档发布点2db639bc保留在§11 |
| main合入后实际查询/架构测试代码点 | `d5f0527c576d68fb639146d1a26d668728b7445a` | 干净代码重新构建执行7场景及审计/架构检查；与发布点2db639bc之间仅有六份文档差异，不能把文档HEAD当新业务运行 |
| `origin/work/g2a-bplus-revalidation-20261104` | `d5f0527c576d68fb639146d1a26d668728b7445a` | 最新G2a窄include、源表/执行审计和验证证据；此前工作分支查询测试代码为722d0441；本批分支tip d5f0527c已完整合入main |
| `origin/work/g2a-bplus-integration` | `2feaf12777e1dea062ab00711b9910396fd517fd` | 首次B+ G2a交付，已包含在main；保留该检查点，不与最新强化分支混称 |
| `origin/work/g2a-readonly-checkpoint-20261030` | `728ad77ed4c723efbb5cb1baf7b3f10bb9830240` | 包含固定历史代码cc787945792527eeb94bc20a5e955315d6269e83，仍保留旧application/query路径，仅用于历史恢复/对照 |

2026-11-05合入历史：当次用户明确授权将其他分支修改合入main并推送。已在新隔离worktree将本地main由01a86b9f快进至原origin/main 5e209a76，再快进纳入G2a分支5个提交至d5f0527c；远端main先推送并核实d5f0527c，随后六份合入说明已以2db639bc发布；本次已重新核对远端SHA、树及包含关系。原工作区仍为 `work/sim-guard-round2@5e209a76`，其他既有worktree/分支保持不变。独立分析、首次G2a和SIM Guard分支均已是main祖先；旧目录G2a检查点仅保留历史，其业务内容已按B+选择性迁入，不重复合入旧application/query树。此前文档整理及独立分支发布记录见§10，本批验证与合入范围见§11；源码保存不等于产品准入。

### 0.2 已完成与已验证范围

| 工作项 | 实际完成状态 | 测试基线、证据及限制 |
| --- | --- | --- |
| 修订B+结构/骨架 | 六组目录、T/A/O/R边界及逐文件迁移清单已落地 | 90个文件、88个C++单元、25个testonly源集；四套纯规则、语法、include/GN及迁移检查通过。不是完整业务生产替代；§3.3/§10 |
| G2a只读闭环 | **G2a_BPLUS_HOST_VERIFIED** | 工作分支干净722d0441及本批main合入代码点d5f0527c分别7 PASS/0 FAIL/0 BLOCKED；此前恢复历史tip重建7 PASS，七组返回一致。Core→Binding→Query→HostPort→原Manager/EXT；六函数正执行56/49/28/28/56/56；§10及[当前证据](../services/multisim/test/systemtest/configs/g2a_bplus_revalidation_20261105.json) |
| G2a构建/执行护栏 | 14项审计测试PASS；架构/GN检查PASS | 工作分支架构/初始化审计基线20f92b48；本批干净d5f0527c重跑14项审计及完整架构/GN检查PASS，初始化旧阻塞未重审。Core宏/EXT/Fallback、原对象及控制块语义保持；未运行完整Core Init/Manager OnInit |
| SIM Guard兼容集 | 17条×5轮：**85 PASS/0 FAIL** | 原7条35 PASS＋新增10条50 PASS，覆盖卡状态、PIN/PUK、双槽交错及组件销毁重建。第二轮以01db5ce6＋当时dirty工作树执行，摘要对应已保存代码c508bd03；不宣称干净提交重跑；§9及[evidence_round2.json](../services/multisim/test/sim_guard/evidence_round2.json) |
| SIM Guard设施及有效性 | 19项设施PASS；READY→LOCKED突变检出，恢复后17 PASS | 设施、突变与业务兼容数量分开统计，不换算全模块覆盖率；§8/§9 |
| 缺陷回归/待裁决 | **5条×5轮，25 FAIL，保留红灯** | 迟到READY、替换卡旧响应、错误PIN映射、旧PIN回包及Refresh返回契约问题独立记录；含NEEDS_DECISION，不把全部候选说成已批准规则，也不改Oracle消除失败；§9 |
| 跨架构测试接入准备 | 共同场景/输入、Oracle、Fake和Adapter边界已建立 | 当前SIM Guard真实运行的是Legacy Adapter；Multisim Adapter与target/nezha能力尚未接入，继续BLOCKED，不能由G2a单查询PASS推定通过 |

### 0.3 未完成门禁

| 门禁/能力 | 状态 | 关闭所需条件 |
| --- | --- | --- |
| 完整OnInit、Subscription/默认角色、数据库重启恢复 | **BLOCKED/NOT_IMPLEMENTED** | 最新审计45对象、43诊断、14个SimData字段不匹配、无缺头、未链接/运行；需匹配Provider字段/URI/schema/错误协议。组件重新枚举不等于数据库恢复 |
| 匹配OHOS产品、Feature、原SO/SA初始化发布及设备只读冒烟 | **BLOCKED/NOT_RUN** | 现有证据环境无匹配manifest、产品树及hb；需按产品实际配置编译tel_core_service并验证原共享对象/发布与只读请求 |
| eSIM Profile/APDU/LPA业务 | **准备/受阻，未完成业务验证** | 已取证真实IEsimManager与SA66250边界；仍需匹配平台头/EventRunner、portIndex契约及真实生产闭包，不补业务成功Fake |
| G2b产物、G2c关闭/排空、G3写权、G4四卡/MEP等新能力 | **未验收/未实施** | 按既有阶段/WU、产品与正式契约分别推进；没有新增生产Writer、设备执行者或资源政策 |

当前下一步统一见§6。业务架构与写权继续由主设计维护；本汇总不新增架构决策、业务批准或生产切换授权。

## 1. 从远端识别当前代码

<!-- sim-refactor-recovery-v1 -->
```json
{
  "schema_version": 1,
  "main_baseline": "5eed2181628e9e74f618231007d927ebd5cb92cd",
  "g2a_base": "9f58c9727c7455d01cae6b739e75db7b294a6358",
  "g2a_branch": "work/g2a-readonly-checkpoint-20261030",
  "g2a_commit": "cc787945792527eeb94bc20a5e955315d6269e83",
  "evidence_path": "telephony_core_service/test/systemtest/configs/g2a_checkpoint_evidence.json",
  "required_code_paths": [
    "telephony_core_service/services/core/src/legacy_sim_query_binding.cpp",
    "telephony_core_service/services/multisim/application/query/real_sim_count_query.cpp",
    "telephony_core_service/test/systemtest/scripts/run_g2a_query.py",
    "telephony_core_service/test/systemtest/scripts/audit_initialization.py",
    "telephony_core_service/test/systemtest/scripts/prepare_dependencies.py",
    "telephony_core_service/test/systemtest/scripts/prepare_openssl.py"
  ]
}
```

上块是恢复工具所用的最小祖先/检查点，不是当前main HEAD；当前快照见§0。它只固定已存在的代码基线，不写本文自身最终提交SHA。分支tip可能包含后来合入的入口文档；用Git查询，不循环更新自引用。默认remote曾核实为`origin=https://github.com/zjuluowu/multisim.git`；`upstream`为OpenHarmony原部件仓，并非本任务推送目标。重新克隆后先看实际remote名称/地址，不盲信名称。

| 内容 | 已核实位置/状态 | 不代表什么 |
| --- | --- | --- |
| P1-1～8、P2-1～5评审修正 | main包含`5eed2181628e9e74f618231007d927ebd5cb92cd`；设计、骨架和四套纯规则已提交 | 不等于业务写权或设备风险消除 |
| 目录实施基线 | 历史18e724f7目录已由用户选定的局部修订B+替代；90f4992a落地已提交，见§0/§3.3、主设计§6.6及[详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md)§3 | 2026-11-02已核实origin/main包含结构提交；整体设计仍draft/pending，目录落地不等于生产验收 |
| G2a真实只读切片 | 上块为旧目录历史检查点；B+适配代码ff1c6ef2及证据2feaf127已按本次授权合入main | 选择性移植不包含旧检查点祖先；源码合入不代表产品准入 |
| 完整OnInit | 当前主机审计BLOCKED，未链接/运行；阻塞见§5 | 不等于只读查询也无法推进 |
| 产品/设备、G2b/c、G3/G4 | 尚未验收/切换 | 不能由骨架/host结果推导完成 |

## 2. 总体计划与退出条件

阶段依据主设计§22，按实际依赖推进，不设置“所有外部条件都完成才做任何工作”的总门禁。

| 阶段 | 目标及前置 | 退出条件/当前状态 |
| --- | --- | --- |
| G0/G1测试护栏 | 冻结功能、真实入口/旧轨迹、独立Oracle、平台边界；与结构工作并行 | 真实Legacy组件17条兼容场景已运行，5条缺陷/待裁决独立保留；完整服务初始化/账户/IPC未闭合。精确基线及统计见§0.2/§9 |
| G2a原SO依赖收口 | 保持原writer/执行者/重试者，真实HostPorts及原对象 | B+查询已在干净722d0441重建7 PASS；最新强化及状态整理交付d5f0527c已按本批授权合入main并重验。匹配产品SO构建、启动/Feature与原对象发布验证未完成 |
| G2b SO工程边界 | 相关依赖、创建/销毁、单例与符号闭合；不强制先完成全部G3 | 独立产物NEEDED/导出/安装/CFI/PAC/LTO及冒烟差分。当前未实施；目录重排不算完成 |
| G2c生命周期 | 仅产品需要重建/卸载时前置，常驻拆分另列限制 | 受理停止、注销、迟到、排空、匹配销毁；未知设备效果独立。当前未验收 |
| G3a～d写权迁移 | 以既有WU覆盖初始化、重载、后台、恢复及全部旧入口；切片有真实写点/外部证据 | 单Owner、无双写/双发/双重试、兼容差分。当前没有生产写权交接 |
| 行为增强（对应G3b/c等） | 固定目标、拒迟到、准入/隔离、队列/安全重试单列旧新轨迹 | 方法级错误、时延、部分效果、结束证据和产品验收，不夹入G2机械迁移。当前未启用 |
| G4能力/契约扩展 | 明确需求、批准、provider/设备/服务支持 | 新身份/schema/API/MEP/N槽等独立能力矩阵与回退；当前不授权 |

## 3. 已完成及当前切片

- **设计/协议**：main主设计§4～6/§9～14、原评审§9～10说明双投影、联合建行、发现最终资格、槽激活、Primary分支、完成、工厂、Profile和结果/资源协议。仍draft/pending；代码落点是`services/multisim`，旧生产在`services/sim`。
- **局部实现/纯规则**：`services/multisim/test/multisim_architecture_rules/run.py`编译运行topology/resource/assignment/runtime_protocol四套；`5eed`集成时全部通过，73个骨架单元严格语法通过。历史PASS不能自动继承到后续HEAD。
- **G2a工作分支实现**：`CoreServiceSim::GetRealSimCount`→`LegacySimQueryBinding`→`RealSimCountQuery/RealSimCountHostPort`→原`SimManager/EXT`。保留Core原TU先读`SIM_SLOT_COUNT_REAL`、空Manager fallback、原Manager宏/EXT读源、返回及重设清旧附着。Core原OnInit/发布点后附着，同一Manager/RIL控制块，原SO和显式测试源表同步，无新写权/设备执行。
- **G2a主机闭包**：真实5个生产cpp实际构建、链接及7场景运行；覆盖精确cpp正执行证据。只模拟参数、严格ITelRilManager平台接口和明确厂商ABI；没有SIM业务替身。Manager OnInit/Core Init、卡在位和IPC/SA没有执行/观察。
- **保存状态**：G2a与入口文档分开提交；原本的临时worktree/patch/JSON仅为本机历史，不是恢复所需输入。可恢复内容为远端commit、跟踪脚本、锁定依赖和归一化精简证据。

### 3.1 目标源码目录落地（2026-10-30，main已保存）

- 输入为干净的`main@18e724f70ab3bfef142674d794ad5dfba04d371d`；实际fetch核对origin main与独立G2a工作分支，恢复工具返回CHECKPOINT_VERIFIED、main_contains_g2a=false。未切换分支或合入G2a；本批源码结果已单独提交并推送。
- 两篇已提交设计的针对性验收通过；T账户/绑定/资料、A角色/Primary、R会话/真实设备资源、C旧语义及H生命周期边界没有发现阻断本轮迁移的冲突。两篇设计本身未改，历史现状路径由[源码README](../services/multisim/README.md)补充当前导航。
- 全量核对原73文件（50头/23源）：48文件以git mv改变路径、25保留主落点，其中6文件拆取现有职责；无原文件无去向、无业务实现删除。现为86个C++文件（63头/23源）。纯关联值、Owner纯types、PIN/EF/Profile契约、三个Adapter、装配内部定义及仓内导出各保持单一定义；[逐文件映射](../services/multisim/test/multisim_architecture_rules/source_migration.json)记录原摘要、职责、状态与去向。
- 原application/model及T/A application/ports撤除；场景与Adapter随Owner。C经具名API函数调用原对象，保留构造签名与类型布局；没有新运行对象/状态或公开契约。LocalCardRequests仍保留原类，实际Profile编排移euicc，PIN/EF仅分契约、业务方法仍未实现；CardRuntime薄化与成熟卡能力迁移尚未完成。
- 实际运行`python3 telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration`（另指定仓外输出目录）：73文件摘要/去向、原类型定义/成员顺序与23个原cpp实现token顺序通过；86头/源独立严格C++17语法及合并头TU通过；四套纯规则实际编译/链接/运行PASS；12项护栏测试通过；156条本地include和24个源集合边界/环检查通过；独立GN gen --check及Ninja全部23个骨架源对象编译通过。新增护栏覆盖宏/续行include、动态GN列表与入口漏挂目标的反例。
- 所有新增GN源集合为testonly，没有接入根产品BUILD/服务发布、共享库或第二条执行路径；services/sim、Core和公共接口未改，生产写权不变。CleanArchCLI不在环境中，以文本include/Target检查替代；没有运行完整骨架产品链接、hb、Core/Manager真实初始化、provider或设备验证。48条git mv移动与其余修复/新增文件随本批同一提交保存；提交本身不改变上述验证范围。

### 3.2 业务可发现性局部优化（2026-10-31）

- 输入：干净main@38c1f2329e5a50349932a19d450c31c403743df1；恢复工具实际fetch后CHECKPOINT_VERIFIED，HEAD与origin/main差异0/0，main_contains_g2a=false。原有其他worktree保持原位；未合入G2a。
- 裁决与T1～T6模拟定位对比见[详细设计§3.4](SIM_Multisim_Source_Architecture_Detailed_Design.md)。topology/domain改accounts（4文件），profile改account_profile（6文件），其余主要路径保留。GN仍以topology_domain约束纯规则；没有新Owner、运行对象、依赖边或生产执行路径。
- 本轮输入88个实际文件（86个C++及BUILD/README）逐项记录在[source_migration.json](../services/multisim/test/multisim_architecture_rules/source_migration.json)的layout_revision；旧73文件溯源保留并更新destinations。C++非预处理token保持，原类型/成员与cpp顺序检查保留。对照输入BUILD，仅替换两组路径后的所有target sources/public/deps/public_deps完全一致。
- README提供任务/Owner/测试导航，部件AGENTS复用安全规则；两篇权威设计刷新实际/计划布局及全部存活源码链接，没有另建平行报告。最终153个本地链接目标存在，Markdown围栏及git diff HEAD --check通过。
- 调整前完整运行run.py --with-gn --migration通过。调整后同命令实际exit0：88文件映射、86头/源独立严格C++17语法与合并头TU、四套纯规则、12项护栏、156条include/24个source_set边界及无环检查、GN gen --check与Ninja全部23个骨架源对象均通过。产物仅在仓外，不作为下次恢复前提。
- 范围：未修改services/sim、Core、产品根GN或公共接口。未运行OHOS完整产品链接、初始化/发布、IPC/SA、Feature矩阵、provider或设备验证；本批结构/规则结果不能证明生产行为等价。CleanArchCLI未用，以现有文本include/target依赖和GN检查限定替代。
- Git保存：用户已明确授权将本批24文件提交并正常推送至origin/main；提交号、远端tip及包含关系通过Git查询，不在同一提交内硬写自引用SHA。不合入G2a、不强推；保留上述产品/设备未验收范围。
- 下一步仍先在独立工作分支适配G2a真实GetRealSimCount只读闭包并复跑7场景/覆盖，再满足原SO产品准入；后续写能力可选WU-05账户号码完整切片，必须先建立卡→DB→current/historical缓存→通知/callback及查询卡→账户→IMPU读源、失败/超时/迟到差分Oracle，不顺带启用固定目标或新重试政策。

### 3.3 修订 B+ 骨架落地（2026-10-31实施，2026-11-02已提交推送）

- 用户已选择独立ADR的局部修订B+作为本轮物理组织实施基线；两份历史ADR仍为Proposed，不追溯改写。不代表G2a产品接入、写权或新资源政策批准。
- 输入为干净 `main@5404aefe66decde4e5c5f03a01f355f6bf127e7e`，origin/main差异0/0。恢复工具实际fetch后CHECKPOINT_VERIFIED、main_contains_g2a=false；未切换/合入其他worktree。
- 输入88文件（63头/23cpp及BUILD/README）全部记录于既有source_migration.json；86个C++移入六组，类型/成员/非预处理token保持；BUILD/README原位更新。24个逻辑source_set及deps/public边界保持，检查器用显式文件集合区分逻辑层，不再由旧父目录推断。各片文本边界检查通过。完整执行 `python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/multisim-bplus-after` exit0：原73文件溯源、88文件映射/非预处理token、86独立头/源与合并头TU严格语法、四套纯规则、12项护栏、156条include/24 target及无环、GN gen --check/Ninja全部23个cpp对象均PASS。产物在仓外，不作为恢复前提。
- SessionRegistry在card/state，ResourceBarrier在device/resource，SlotCatalog在support、由H构建只读；未创建device/session。CardRuntime/result_sinks在integration/lifecycle保留过渡职责，当前不宣称已薄化或状态拆分完成。T/A分别在account/card_selection，O仍为各用例机制。
- 本轮未修改services/sim、Core、根产品GN、公共接口或两份历史ADR；生产仍为旧实现。未运行产品/设备、完整初始化/SA/IPC、Feature矩阵或provider验证。G2a工作分支仍需适配本结构与独立准入，不能继承旧host PASS。
- 生产切片风险归入既有WU：账户发现/资料写权需同时覆盖CardFileDetectManager及OperatorConfigLoader::InsertOpkeyToSimDb的OPKEY/MCC/MNC/IMSI旁路；同表字段计划由Owner接纳，不能只关闭Controller setter。Records/Phonebook迁入须按card_file_io/adn_io/records_load/phonebook_cases真实DAG细分，不建空target或复制ADN。
- 资源/生命周期切片须调查普通/低优先级EsimFile及ARA-M权限通道的真实竞争域；channelId/AID清零与调用超时不是结束证据，不顺带启用隔离/重发/释放。Detect在SimManager::OnInit先捕获默认空weak、之后才创建Controller，未见重绑；作为待运行差分的装配风险，不在骨架移动中修复。
- 下一步仍优先独立工作分支适配G2a真实GetRealSimCount闭包，再满足原SO产品准入；成熟写能力优先WU-05号码切片（card→DB→current/all→通知/回调、IMPU回退与查询回写），前提完整Oracle、持久化契约及真实结果关联齐备。
- 2026-10-31先保留Diff，2026-11-02经用户授权提交并推送origin/main：`90f4992a27808ec99e76b38ad3a2c0b4e2ad34a3`，101文件（86重命名/15更新）。发布前重新运行完整骨架验证（--with-gn --migration）exit0，远端SHA/tree及工作区干净已核实。此为已存在提交，不是本文自引用；发布不代表生产验收。

<a id="g2a-bplus-m1"></a>

### 3.4 G2a B+ M1只读闭包（2026-11-02本轮实测）

- 输入main：`9ab1e976ce71c463281d13abbd9408d60792a4d4`；历史实现：`cc787945792527eeb94bc20a5e955315d6269e83`，远端tip `728ad77ed4c723efbb5cb1baf7b3f10bb9830240`。新分支 `work/g2a-bplus-integration` 的代码提交为 `ff1c6ef2ea73d36ab83a7ac2c276036c6f9a6a58`；未整个合并/cherry-pick历史树，也未恢复application/query目录。
- 真实路径：`services/core/src/core_service_sim.cpp` → `services/core/src/legacy_sim_query_binding.cpp` → `services/multisim/integration/api/real_sim_count_query.cpp` → 绑定实现的HostPort → 未修改的`services/sim/src/sim_manager.cpp`及`services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp`。查询只投影旧读源，不引入T/A事实、Session、Resource或另一设备执行者。Core宏在原TU原优先级求值；空Manager/Fallback、atoi及EXT原样传播。
- GN：根产品、test/BUILD、test/core_service_test.gni、raw_parcel测试及systemtest源表各明确列Query/Binding一次；原24逻辑骨架target不变，另有无业务依赖的readonly_query testonly叶子。源表检查只证明文本存在/次数，不能替代产品编译链接。
- 依赖：从锁定URL重新获取GoogleTest归档并经prepare_dependencies.py校验archive/tree SHA，未使用未知系统库；OpenSSL头包同样经锁定校验，仅用于初始化审计。Clang18/LLVM18、GN/Ninja/gold及libxml2开发头实际可用；参数化命令见§4，版本/摘要/本机产物位置在新精简证据中。
- 干净代码提交上实际执行`run_g2a_query.py`：**PASS=7、FAIL=0、BLOCKED=0、executed=true、dirty=false**。七场景输入1/2/0/-2/invalid返回1/2/0/-2/0，参数2且厂商5/-31返回5/-31，与冻结Oracle相同。五个真实生产cpp构建/链接，六条精确源路径函数正执行；Core查询56、附着49、Binding读取28、Query读取28、Manager查询56、EXT读取56。`manager_on_init_executed=false`、`core_init_executed=false`。
- 保留历史全部断言，新增失败重附着清旧绑定、重复附着引用稳定、非空Manager替换释放旧对象及替换RIL释放负例。仅测试fixture创建替换对象；生产没有新增Manager/RIL实例。get地址及双向owner_before共同验证控制块，不以裸地址一致代替生命周期检查。
- 本轮完整架构命令`run.py --with-gn --migration`实际PASS：88个C++单元/合并头、四套纯规则、12项护栏（含Query非法跨层include反例）、157 include/25 target、原73溯源/88布局与新增2项历史token、GN gen --check及全部24cpp对象。历史布局和原规则保持，不把目录位置解释为已完成业务解耦。
- 完整初始化重新审计：**BLOCKED/executed=false**，GN成功、Ninja exit1、45对象、43诊断、14缺SimData字段、无缺头；未链接/运行，不猜字段或伪造Provider。当前PATH无hb，所选worktree不是完整产品树（祖先无.repo，仓根无build/ohos.gni），未提供匹配产品环境；产品构建、Feature矩阵、原SO/Core初始化发布、只读冒烟为BLOCKED/NOT RUN，不声明G2a_PRODUCT_ADMISSION_VERIFIED。
- 新机器可读证据：[g2a_bplus_evidence.json](../services/multisim/test/systemtest/configs/g2a_bplus_evidence.json)。历史[g2a_checkpoint_evidence.json](../services/multisim/test/systemtest/configs/g2a_checkpoint_evidence.json)保持原字节。大型XML/profile/map/二进制和日志留仓外；最终文档/证据提交通过记录的生产/测试输入摘要与被测代码提交关联，不把文档HEAD当成重新执行业务测试。
- 本轮只达到**G2a_BPLUS_HOST_VERIFIED**；M2产品准入未完成，M3写业务未开展。已正常推送代码ff1c6ef2至origin/work/g2a-bplus-integration，并以ls-remote核实一致；凭据工具在Linux临时worktree不可识别时，从原仓库路径推送同一显式工作分支，未修改main。原main仍为输入基线且工作区干净；远端保存不等于合入/部署批准。

### 3.5 用户授权保存至main（2026-11-02）

用户在§3.4独立工作分支交付后明确要求“请直接推送到main分支”。重新fetch核实main仍为9ab1e976，工作分支仅领先本轮两个提交，无分叉、无用户未提交/暂存修改；以`git merge --ff-only origin/work/g2a-bplus-integration`快进到`2feaf12777e1dea062ab00711b9910396fd517fd`，再正常非强制推送origin/main。`git ls-remote --heads origin main`已核实远端为该提交，包含真实代码ff1c6ef2及文档/证据2feaf127；未合并整个旧G2a历史分支。本文后续保存状态修正不重写历史JSON、不改生产/测试源码；最终文档提交SHA由Git查询，避免自引用。

源码和根产品显式源表现已在main；§3.4“main未修改/仅工作分支保存”是当时切片记录，已由本节后续授权替代。`G2a_BPLUS_HOST_VERIFIED`仍来自干净ff1c6ef2的实际运行，不把本次推送视为新业务测试。产品、Feature、完整初始化/原SO发布及冒烟门禁仍未完成；没有新增写业务、设备执行者或资源策略，也未执行产品部署。

## 4. 验证入口与可迁移环境

所有命令从Git仓库根运行。路径变量由当前机器选择，不能复制旧电脑的绝对目录。构建/结果必须在仓库外。`SIM_GTEST_ARCHIVE`是下载文件，`SIM_DEPS_DIR`是依赖解包根，`SIM_GTEST_ROOT`是解包后的固定版本目录，`SIM_TEST_OUTPUT`是结果根。

环境：当前可复验主机为Linux amd64、Python3.12+、Clang/LLVM18、GN、Ninja、GNU gold、pthread与libxml2开发头。当前完整初始化的OpenSSL头包为锁定amd64版本；不同架构先核对GN/platform边界，不能假称已支持。完整产品构建需匹配OHOS树及hb。

### 4.1 Git恢复与纯协议（main可运行）

```sh
git remote -v
git status --short
git worktree list
python3 telephony_core_service/services/multisim/tools/check_sim_refactor_recovery.py --remote origin --fetch
python3 telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py
```

恢复工具默认只查本地remote refs；`--fetch`才更新远端跟踪引用，不切分支/清工作区。`main_contains_g2a`只判断旧cc787945检查点的祖先关系；本轮选择性移植后即使返回false，也不能据此断言main缺少B+实现。另核对main包含ff1c6ef2/2feaf127以及新证据的源码摘要。工具只核对历史可达性、源摘要和证据，**不会运行或宣布业务PASS**。

### 4.2 G2a历史检查点与B+闭包（B+已在main）

恢复历史实现时，先保护本地修改，再在独立worktree/克隆使用§1远端代码commit；不要把临时路径当必需参数。历史分支可能已有新的入口文档提交，确认它仍包含cc787945检查点。本轮B+是选择性移植，不要求新分支在Git祖先中包含该历史提交；应核对ff1c6ef2及additions源摘要/非预处理token。

```sh
python3 telephony_core_service/services/multisim/tools/check_sim_refactor_recovery.py --remote origin --fetch
# 显式拉取main和工作分支，兼容single-branch clone；remote名称按实际选择
# 例：当前目录继续保留；SIM_RESTORE_DIR由当前机器选择且必须不存在
git worktree add --detach "$SIM_RESTORE_DIR" origin/work/g2a-readonly-checkpoint-20261030
```

上述命令仅恢复旧目录历史检查点。复验最新B+应显式fetch `work/g2a-bplus-revalidation-20261104`，在另一新worktree核对其包含722d0441；当前已保存交付为097e536a，证据见§10。`work/g2a-bplus-integration@2feaf127`仅用于首次B+交付复验（§3.4），不能把其较早源表检查当成最新强化版本。新旧runner沿用下面锁定依赖及参数化命令。

在选定工作区根阅读AGENTS/本状态，再准备依赖。下载URL与archive/tree SHA以`services/multisim/test/systemtest/configs/dependency_lock.json`为准，以下为已锁定GoogleTest：

```sh
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o "$SIM_GTEST_ARCHIVE"
python3 telephony_core_service/services/multisim/test/systemtest/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
# 设置SIM_GTEST_ROOT为SIM_DEPS_DIR下googletest-b514bdc898e2951020cbdca1304b75f5950d1f59目录
python3 telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query"
```

预期是`g2a-query-result.json`中PASS/executed=true/7 PASS，6条核心函数同时有预期cpp路径和正执行次数。七场景为1/2/0/-2/invalid、厂商2→5/2→-31，并含控制块、重设/清理和空引用反例。JSON必须仍明确`manager_on_init_executed=false`、`core_init_executed=false`。依赖失败应BLOCKED且清旧XML，不制造PASS。原容量/INFRA套数字不得加到此切片。

归一化历史证据位于工作分支`services/multisim/test/systemtest/configs/g2a_checkpoint_evidence.json`。历史观察以5eed dirty工作树为输入，5个源码SHA与保存代码对应；重新克隆后重跑结果按本次HEAD和源摘要记录。路径变化会改变二进制/coverage，不能要求新二进制SHA等于旧值。

### 4.3 完整初始化审计（不运行场景）

Query不需OpenSSL；只有下面审计需要锁定开发头包。按`dependency_lock.json`和`prepare_openssl.py --help`下载/校验并传入`SIM_OPENSSL_ROOT`，不要安装/伪造字段绕过错误。

```sh
# SIM_OPENSSL_ARCHIVE是按dependency_lock.json下载的包；SIM_OPENSSL_ROOT必须为新目录
python3 telephony_core_service/services/multisim/test/systemtest/scripts/prepare_openssl.py --archive "$SIM_OPENSSL_ARCHIVE" --destination "$SIM_OPENSSL_ROOT"
python3 telephony_core_service/services/multisim/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_TEST_OUTPUT/full-init"
```

当前预期是exit1、BLOCKED/executed=false：GN成功、45个初始化对象、43诊断、14缺字段、无缺头；未链接/运行。只有实际输入/环境未变才可比较数字，诊断有截断，不承诺这些是全部后续问题。不能把审计对象编译当场景通过。

| 证据层 | 已有记录 | 本轮/未来复验的限定 |
| --- | --- | --- |
| 设计/骨架 | `5eed`原评审§10，四套协议与严格语法 | 不代表生产接管 |
| G2a主机 | 原评审§11、工作分支精简证据、runner | 真实查询/附着初始化；非Manager/Core完整初始化 |
| 完整OnInit | 原评审§11.4与精简证据 | BLOCKED45对象/14字段；未链接运行 |
| 原SO产品/Feature/IPC/设备 | 无本切片PASS证据 | 需要匹配环境，不能由host/LTO推导 |

### 4.4 本轮远端独立恢复实测（2026-10-30）

- 从上述GitHub远端执行`git clone --single-branch --branch main <已核实remote URL> "$SIM_FRESH_CLONE"`，没有复用旧仓的Git对象；在main运行恢复工具`--remote origin --fetch`，显式取回工作分支。结果`CHECKPOINT_VERIFIED`、`remote_refs_refreshed=true`、`main_contains_g2a=false`；main工作区保持干净，未切换分支。
- 在该克隆中按§4.2创建独立detached worktree，实际使用的克隆、worktree、依赖和输出路径均含空格；不依赖原`/mnt/e/...`或旧G2a worktree。恢复代码HEAD为`f5b2aa9c2e4f76f667994cc1fa17f2f0c198bce6`，包含固定代码检查点`cc787945...`和入口文档合并；两者生产代码/验证配置无差异。
- main执行§4.1四套纯协议均PASS；三份入口Markdown的19个相对链接存在，围栏成对，恢复脚本AST通过。恢复工具另以真实临时Git仓验证缺remote/缺证据/错误摘要/源码及header/runner漂移等出口；这些检查不是业务测试。
- 固定GoogleTest archive经跟踪脚本重新校验/解包至新依赖目录，再从恢复worktree按§4.2构建、链接、运行：`PASS=7/FAIL=0/BLOCKED=0`、`executed=true`、`dirty=false`；七份XML均1测0失败，5个生产cpp摘要与检查点匹配，6条核心函数均有正执行次数。`manager_on_init_executed=false`和`core_init_executed=false`保持明确。
- 同一干净恢复HEAD按§4.3重新校验/解包OpenSSL，完整初始化审计实际重跑：GN exit0、Ninja exit1、45对象、43诊断、14个SimData字段、无缺头；BLOCKED/executed=false，未链接运行，与§5限定一致。
- 本地大日志/产物不入库；必要证据是本节固定代码/测试HEAD、上述命令、工作分支精简历史JSON和可重跑入口。后续只有文档更新不重复声明新HEAD的业务PASS；产品/设备范围仍未验证。

## 5. 局部阻塞

| 缺口 | 实际阻塞范围 | 不阻塞/关闭条件 |
| --- | --- | --- |
| 14个SimData符号：EFUST/EHPLMN/GID1/GID2/IS_ESIM/LSI/MCC/MNC/MNC_LEN/OPERATOR_NAME/OPKEY/PHY_CARD/SIM_LABEL_INDEX/SPN | 完整OnInit主机编译及无卡/账户场景，涉及card_file_detect_manager、multi_sim_controller、multi_sim_helper、operator_config_loader、sim_rdb_helper.cpp | 不阻塞独立只读/纯协议；需匹配fork字段/URI/schema/错误协议证据，不猜常量值 |
| 匹配OHOS树/hb、产品配置及Feature矩阵 | 原SO产品构建/真实Core初始化发布和设备行为 | 不阻塞主机查询；需明确产品/工具链/外仓版本与构建运行结果 |
| 设备结束/通道静止、厂商/66250及Port/MEP证据 | 依赖它的新隔离/安全重试、G2c/G3/G4切片 | 不作为所有G2a工作的统一前置；逐命令因果/结束与外部契约关闭 |

## 6. 下一批推荐任务

以下是当前剩余门禁及依赖顺序，不自动授权新的生产迁移、合并或发布。G2a首次适配、最新主机复验及SIM Guard两轮建设已经完成，不再把这些列为下一批待实现任务。

1. **优先补齐M2产品准入环境。** 提供匹配OHOS manifest/fork、Provider版本、产品/工具链及Feature配置；在最新G2a交付候选上核对原tel_core_service显式源表，实际执行 `hb build -T //base/telephony/core_service:tel_core_service`，采集原SO/Core/Manager/RIL共享对象、初始化发布及GetRealSimCount只读冒烟证据。无环境时继续BLOCKED，不以host PASS替代。
2. **补齐Provider契约后扩展服务测试。** 先取得14个SimData字段、URI/schema和错误协议，重跑完整初始化审计；再实施Subscription映射、默认角色、真实SUT重建与数据库保存/恢复/失败场景。已有组件重建及RIL重新枚举不重复包装成持久化恢复。
3. **独立处置缺陷与待裁决规则。** 保留5条回归红灯及失败事件，分别确认迟到响应/换卡隔离、PIN错误映射与Refresh返回契约；修复需要对应授权及正式规则，不能把旧错误转成兼容预期。
4. **按真实外部契约推进eSIM。** 先闭合平台EventRunner/头、portIndex和本仓APDU/Profile生产路径，再接SA66250边界；不把外部DownloadProfile编排伪造成本仓能力。产品新增四卡/三待/MEP另行验收。

G2a最新强化及状态整理已按本批明确授权合入main（§11）；其他独立分析/测试分支原已包含。后续变更的合入/发布仍按当次明确授权处理。G2b/G2c工程与关闭门禁、SetShowNumber等G3写权迁移均不由上述建议提前启动；保持原宏求值TU、EXT/Fallback、公开契约及唯一生产对象/Writer。

## 7. 历史交接与更新规则

以下记录2026-10-30历史整合，不表示当前HEAD或最近验证；当前状态以§0及对应证据章节§9/§10为准。

2026-10-30本地整合：原工作区内容先保存为提交`26d81e540243375ea9873e6e7e26b4bbb3eb4028`；以`origin/main@9f72e7ff94a924caafff291444414bf4e7bd0037`为整合基线。针对该旧工作树的P1/P2协议，按[原评审§10](SIM_Target_Architecture_Independent_Review_2026-10-29.md)已有整合裁决保留main现行接口及规则测试；被替代的旧接口和旧测试保留在上述父提交，不重复放入活动源码。未合入G2a工作分支或切换生产写权。

按用户授权将整合提交`eb112578a88dd8ddd35f172f3b04facddb968887`正常推送至`origin/main`；推送后通过`git ls-remote`确认远端指向该提交，并核实其包含上述本地历史提交。此记录不改变G2a仍在独立工作分支的状态。

本次整合结果的代码树与上述main基线一致，状态文档除外。实际重跑四套纯架构规则，全部PASS；73个骨架头/源的Clang C++17严格独立语法检查全部PASS。恢复工具验证远端检查点可达，未执行运行时测试；未运行OHOS产品构建、真实初始化、设备或provider验证。上述结果不改变§5局部阻塞及§6的G2a合入门禁。

本轮已将原本只在临时目录的G2a源码、构建、runner及精简历史证据保存到上块远端代码检查点，并把根/子AGENTS作为统一入口。原/tmp worktree、patch或旧结果消失不影响按远端恢复。当前开发位置应由`git branch --show-current`/`rev-parse HEAD`查询，不能把某电脑当前分支写成全仓唯一状态。

新会话：根AGENTS→部件AGENTS→本文件→Git包含关系→按最小任务读取设计/代码→实际验证→更新本状态。工作结束时只更新受影响结论/证据、代码branch/已有SHA及下一批任务；不复制完整会话，不改写历史报告，不将本文最终SHA硬写回同一提交。若状态与真实远端/代码不符，先纠正状态，再继续授权范围内的最小任务。

<a id="sim-guard-20261104"></a>

## 8. 跨架构测试防护网首批工程（2026-11-04）

初始工程任务授权实际测试建设及本地提交，当时未授权推送或合并main；后续远端保存授权见本节末。开始于干净 `main/origin/main@01a86b9f073b20f408d9f8e67da35192aacac439`；恢复工具 `--remote origin --fetch` 实际返回 CHECKPOINT_VERIFIED，初始远端差异0/0，已有worktree保留。新建本地分支 `work/sim-guard-contract-20261104`，最终代码检查点 `2c5364a3dcf9f1487d759b6bedcbf780ba16663e`。未修改任何生产源码、公开接口、生产根GN或既有业务策略。

交付位于 [services/multisim/test/sim_guard](../services/multisim/test/sim_guard/README.md)：最小ISimSutAdapter、Legacy绑定、有状态Fake RIL、公开查询快照、StateRegistry通知Spy、Contract Driver/Oracle、七条Compatibility、一条独立Defect、版本化共同资产摘要、结构化取证/规则/缺口，以及真正可用的GN/命令入口。Fake只维护外设事实、PIN设备应答、请求和回包调度；场景/Oracle不include旧新内部类。只有Adapter接触具体状态实现，真实CMI/EXT未用业务桩替换，CMI的账户路由未装配。

真实链路为 `SimStateManager::Init` → 注册RIL通知 → `RADIO_SIM_STATE_CHANGE` → 真实 `GetSimStatus` 请求 → RIL正式响应事件 → `SimStateHandle::GetSimCardData/ProcessIccCardState` → 真实查询和外服务通知。真实TelEventHandler/TelEventQueue、ObserverHandler及状态/PIN代码均链接运行；新增有界host排空控制，未改生产调度或用私有setter构造状态。这是状态组件切片，不能升格为完整SimManager/Core初始化、公开ISimManager参数校验、账户或产品验收。

| 实际验收 | 结果 | 范围/证据 |
| --- | --- | --- |
| 干净代码检查点重建/执行 | **7个兼容用例×5轮，35 PASS/0 FAIL** | LIFE_001/002、PUK_001锁态、PIN_001、PIN_002命令失败、DUAL_001、RIL_001重复及错误注入；每轮同进程运行七例并清理，八条指定生产函数精确源路径正执行 |
| 真实生产突变检出 | **DETECTED，恢复7/7 PASS** | 仓外生产cpp副本把READY映射破坏为LOCKED，同一LIFE_001及Oracle实际断言失败；随后原始生产二进制重跑通过，工作树生产源始终未改 |
| DEFECT_001 | **1 FAIL/0 PASS，正常非零退出** | 已READY→保留旧查询→较新拔卡回包使查询ABSENT→旧READY回包使公开存在性/状态恢复true/READY；失败及完整观测保留，不Skip/xfail，不把错误写入兼容预期；具体代次/准入修复政策待裁决 |
| 平台设施回归 | **19 PASS** | 原16项及新增跨队列排空、延迟/运行中超时、异常可见性三项；独立计数，不加到SIM业务通过数 |
| 架构设施 | **PASS** | 四套纯规则、结构检查和12项护栏；不作为SIM业务/产品验收 |
| 防伪通过负控 | **按预期拒绝** | 场景摘要漂移、即使重算摘要的内部include、零例/Skip XML均被拒；缺依赖、multisim、target/nezha入口均BLOCKED、executed=false |

实际命令与结果/源码摘要见仓内 [evidence.json](../services/multisim/test/sim_guard/evidence.json)，包括失败轨迹、突变/恢复、八条执行审计、host输入摘要和eSIM编译诊断。完整XML/profile/coverage/map/日志放在当前机器仓外；恢复不依赖这些路径。参数化重跑命令见 [README](../services/multisim/test/sim_guard/README.md)。核心实际入口为：

```sh
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat" --repeat=5 --mutation-check
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=defect --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/defect"
```

规则等级严格区分：公开状态/卡型/PIN结果定义作为本切片强Oracle；错误回包后的状态保持仅记录C级观察，未冻结为业务政策。每次通知只检查本步骤新增集合，不要求非契约全序/次数。广义账户隔离、默认回选、nezha I/R和取消回滚均未升级批准。Slot/Card/Port/Profile/Subscription/角色及网络注册分别建模；未接入维度明确未观测，不由slot合成账户身份。

当前只达到P0真实状态/PIN闭环、P1初始兼容子集及P2接入机制准备；**未完成全模块P1、完整eSIM场景族或第二阶段B验收**。本机无hb、祖先无.repo、仓根无build/ohos.gni；OHOS产品/Feature、SA/IPC/权限、设备未运行。完整初始化/账户/默认卡/持久化仍缺匹配Provider协议；本批未重跑历史完整OnInit审计，不把旧45对象/14字段数字当成本次编译结果。Fake Store/LPA尚未接入，不补业务成功桩。

eSIM额外取证已实际尝试真实 `esim_file.cpp` 的Clang18语法编译，exit1首错为 `core_service.h` 所需 `system_ability.h` 缺失；还需要普通EventHandler/EventRunner合法投递及真实codec/源表闭合。诊断性屏蔽两个具体头后，发现仓内 `EsimProfile::portIndex` 为u16string、实现却按int32/ASN.1整数使用的类型冲突；该诊断不是产品构建结果，完整未解析依赖尚未知，不擅改公开类型。完整DownloadProfile属于SA66250，本仓只含协作/代理契约，不能用Fake补写后计为真实SIM下载通过。

下一最小切片：先取得匹配Provider字段/URI/schema/错误协议，推进真实账户/角色和重启恢复；eSIM另核实portIndex正式类型，补平台EventRunner/头及真实APDU/Profile绑定后运行。已复现迟到问题保留红灯并裁决修复政策。未来业务迁入multisim后只替换 [Adapter及GN/执行审计绑定](../services/multisim/test/sim_guard/adapters/multisim/README.md)，共同场景、输入、Oracle和Fake按v2基线五份摘要保持；没有真实接入时继续BLOCKED。不得从本轮测试建设推定G2a产品、写权迁移或新能力批准。

本批v2基线评审：移除Driver对精确RIL请求次数/队列下标的假设，按外部槽及设备事实批量交付、有界排空；正式PIN失败使用 `UNLOCK_OTHER_ERR`。统一契约、场景数据和业务Oracle相对507b6115逐文件无差异，版本/原因记入semantic_baseline；上述35 PASS、突变检出及恢复7 PASS已在干净2c5364a3上重新构建运行。独立迟到缺陷复现需要两个在途响应的故障调度，未来实现若串行化导致该前置条件不可达，不视为兼容性违约；七条Compatibility不要求该拓扑。

### 8.1 用户授权保存到远端工作分支

用户在本地交付后明确要求“将本轮修改提交到远端”。本次从干净工作区核对origin、分支/HEAD、暂存区及既有worktree，恢复工具 `--remote origin --fetch` 返回CHECKPOINT_VERIFIED；远端main仍为 `01a86b9f073b20f408d9f8e67da35192aacac439`，本地仅领先本轮五个提交，远端尚无同名工作分支。正常非强制推送到 `origin/work/sim-guard-contract-20261104` 并建立跟踪，`git ls-remote` 已核实远端为 `e99caba343e73d6f72d9bbb6969dc90095125e85`，包含本轮五个代码/证据提交及最终被测代码2c5364a3。

本次只发布当前工作分支，未合并或推送main。此后仅补充本段发布记录，同步保存到同一远端分支；最终文档tip按Git查询，避免自身SHA自引用。发布没有改变业务源码、共同测试资产或既有结果，本次未重跑业务测试，不将远端保存解释为完整产品/新架构验收。

<a id="sim-guard-round2"></a>

## 9. 测试防护网第二轮：生命周期扩展与缺陷独立统计

用户本轮指定以e99caba3为已报告基线，禁止未经授权推送/合并main。实测起始工作区干净、HEAD为 `01db5ce64fc5fdb8d80c585bf35d4c260c817b86`；e99caba3是其祖先，两者仅发布记录不同。恢复工具fetch及ls-remote确认远端工作分支实际为01db5ce6，main仍为01a86b9f，分支远端差异0/0；没有根据历史报告假定发布状态。新建本地 `work/sim-guard-round2` 开发，未改用户已有worktree或任何生产源码/公共接口/根产品GN。

本轮测试执行时源码为父提交01db5ce6上的工作区修改（当时未提交、未推送、未合并main）；本轮runner如实记录dirty=true及全部相关输入摘要，不把父提交冒充已含新代码的检查点。源/测试/命令/结果证据保存于仓内 [evidence_round2.json](../services/multisim/test/sim_guard/evidence_round2.json)，新增场景和规则映射见 [round2_catalog.json](../services/multisim/test/sim_guard/traceability/round2_catalog.json)。旧evidence.json、原v2业务契约/场景/Oracle/Fake及七条测试代码块保留；仅把原ContractDriver原样提到共享头供新场景复用，v3基线另外强制核对v2原摘要和测试代码块摘要。负控实测：即使重算当前摘要，改动旧断言或Oracle仍被拒。

| 当前实际验证 | 结果 | 覆盖边界 |
| --- | --- | --- |
| e99caba3干净隔离源码重新构建 | 原7条×5轮，35 PASS | 新下载并校验固定GTest；不依赖上一轮产物 |
| 本轮统一runner | 原7条35 PASS + 新10条50 PASS = **兼容集85 PASS/0 FAIL** | 17条各五轮；并不代表全模块覆盖 |
| 独立缺陷集 | **5条各五轮，25 FAIL** | 保留原迟到READY；新增替换卡旧READY、通用RIL错误分类、旧PIN污染新卡、Refresh返回契约冲突；无Skip/xfail |
| 突变与恢复 | READY→LOCKED检出，恢复17/17 PASS | 相同原LIFE_001/Oracle；只改仓外生产cpp副本，生产工作树未改 |
| 单独compat入口 | 17 PASS、退出0 | 五条缺陷仍在defect_inventory，明确NOT_RUN；全报告all因缺陷退出1 |
| 设施回归 | 19 PASS | 与业务通过数分开；没有重造测试框架 |

新增十条按完整流程覆盖：单/双卡枚举及逆序回包、RIL不可用后真实对象重建、异常载荷/丢包后正常应答恢复、PUK错误及成功、显式刷新、双卡实例销毁重建、停机换卡重枚举、PIN完成与另一槽拔卡交错。11条指定生产函数有正执行覆盖，另精确证明null-RIL初始化和无有效载荷处理分支各执行5次。初始/重建临时状态仅作C级观察，不把未批准缓存/初始化政策变成断言。公开状态/卡型/解锁结果继续用正式类型契约断言。

`Recreate`实际销毁全部旧SimStateManager、确认weak引用失效及worker退出，再创建并初始化新生产对象；双槽重建场景记录4个真实对象创建/销毁。卡设备事实通过Fake RIL保留，停机期间可换卡；这是**硬件重新枚举验证，不是数据库持久化恢复PASS**。

`RefreshSimState`出现来源冲突：CoreServiceClient头注释承诺TELEPHONY_SUCCESS，真实Manager/Handle及Core/Stub/Proxy/Client委托返回状态整数，历史测试也存在非负值口径。原成功码断言完整保留为DEFECT_005红灯并标NEEDS_DECISION；正常刷新场景只断已确认的公开最终状态、原返回值进入证据，不改成“预期2”或由Adapter修正为0。其他缺陷的准入/修复政策也单独裁决，未来真实multisim应在等价故障调度可达时通过相应回归，不能把已知错误固化为兼容要求。

本轮命令入口（Git根，路径自行选择；完整实际argv/退出码在evidence_round2）：

```sh
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/all" --repeat=5 --mutation-check
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_GUARD_OUTPUT/full-init"
```

**任务C/D尚未闭合**：重新准备锁定OpenSSL头并实际重跑完整初始化审计，仍exit1、BLOCKED/executed=false：45对象、43诊断、无缺头、14个未匹配SimData字段。实际核对当前可用官方Provider `843a8be7f8d1dc304870a3999b754a03c6cce57d` 头及Git内容，不能提供此fork所需扩展字段；未猜字段值或把Fake Store当账户业务。映射、默认Voice/Data、无效账户、角色失效及DB保存/恢复/失败均NOT_IMPLEMENTED，正式Provider字段/URI/schema/错误协议和部分产品策略是当前具体前置。证据和真实调用路径见 [Provider契约记录](../services/multisim/test/sim_guard/traceability/provider_contract_round2.json)。本机无hb/匹配产品树，完整Core/SA/IPC/设备未验证。

**任务E完成本轮契约准备，未运行eSIM业务**：再次用当前host参数编译真实esim_file.cpp，exit1首错仍为system_ability.h缺失。核对IEsimManager实际Profile/APDU及AuthenticateServer/PrepareDownload/LoadBoundProfilePackage协作面，DownloadProfile属于外部SA66250；最小接入还需合法EventRunner、真实codec/源表和匹配portIndex类型。本轮没有补写LPA下载决策或Profile生命周期Fake来制造通过。

下一步待提供匹配fork的Provider仓/commit或产品manifest，再推进完整OnInit、账户/默认角色及持久化恢复；返回契约、默认回选/移除策略和eSIM类型分别裁决。当前生命周期/安全扩展仍通过纯测试契约进入Legacy Adapter；未来绑定真实Multisim时增量实现会话/PUK/Refresh能力和生命周期装配，复用同一数据、Fake语义和Oracle，不修改T/A/O/R内部设计。

后续用户明确授权“将当前修改推送到远端main分支”。发布前核实origin/main仍为01a86b9f，是当前分支的祖先；先前工作分支六个提交及本轮改动均属本次发布范围。仅检查已运行源码/资产摘要、保留v2断言和文档一致性，不把本次Git发布当作重新运行业务或产品验收；实际远端发布记录见§9.1。

### 9.1 用户授权推送远端main

用户在第二轮交付后明确要求“将当前修改推送到远端main分支”。重新核对工作区16项改动全部属于本轮，暂存区无既有内容，已运行源码/资产摘要与当前文件一致；origin/main仍为01a86b9f，且为当前分支祖先。仅将本轮明确文件保存为代码/证据提交 `c508bd0390b1c4360beb1d9388c0e84a9e099de0`，随后以 `git push origin HEAD:refs/heads/main` 正常快进发布。`git ls-remote --heads origin refs/heads/main` 已核实远端为该提交，包含原e99caba3/01db5ce6及本轮全部修改；没有强推、变基、自动合并无关提交或修改其他worktree。

测试证据仍是01db5ce6父提交加当时dirty工作树的实际执行记录；已按保存代码逐项验证测试/生产源码摘要一致，不把提交成功声称为干净代码重跑或产品验收。独立缺陷25 FAIL和Provider/eSIM阻塞完整保留。此后仅补充本段及证据中的发布关联并同步到同一远端main；最终文档tip由Git查询，避免自身SHA自引用。

<a id="g2a-revalidation-20261105"></a>

## 10. G2a B+ M1恢复复验与构建边界强化

本批于2026-11-04开始、11-05续跑完成。用户要求恢复已有G2a、以最新main隔离实施并保存独立远端工作分支。实际检查main为 `5e209a76152a7e97411c41420ebd8d0e5ec67966`，已包含代码ff1c6ef2及证据2feaf127；没有按旧提示9ab1e976回退或重做已合入的迁移。恢复工具 `--remote origin --fetch` 为CHECKPOINT_VERIFIED；历史远端tip `728ad77ed4c723efbb5cb1baf7b3f10bb9830240` 包含固定检查点 `cc787945792527eeb94bc20a5e955315d6269e83`，在新建detached worktree只读恢复并实际重跑。旧 `work/g2a-bplus-integration` 及其worktree保持原状，新分支为 `work/g2a-bplus-revalidation-20261104`。

**实际增量：** Query仍在 `services/multisim/integration/api/real_sim_count_query.*`，Core旧对象桥仍在 `services/core/*/legacy_sim_query_binding.*`。五份产品/Core/主机显式源表的六处头搜索收窄到integration/api，绑定和Query改为使用该窄头；readonly_query保持testonly且不再导出全骨架include，原24个逻辑target不变。源表审计从字符串计数改为指定target/字面量列表检查，增加14项负控/自身测试，覆盖注释、错target、条件化、重复、覆盖/移除、窄include和无关业务源。执行审计精确匹配方法名编码/源路径，保留六条正执行条件并核对初始化未执行；厂商场景增加单次附着查询只触发一次厂商调用的检查。原返回值、宏、控制块及顺序生命周期断言全部保留。

真实链路为 `CoreServiceSim::GetRealSimCount → LegacySimQueryBinding::Read → RealSimCountQuery::Read → HostPort::ReadLegacyRealSimCount → 原SimManager::GetRealSimCount/EXT`。Core入口/绑定cpp、原Manager/EXT、Feature gni及宏头与恢复历史逐字节一致；Query除include指令外token一致。没有创建新的生产Manager/RIL、Account/T/A状态或设备执行者，没有切换原Core装配和公开API/IPC/ABI、写权或持久化schema。替换对象仅存在于测试fixture，控制块匹配、空/失败/重复附着和清理反例只证明顺序生命周期。

| 本批实际验证 | 结果 | 范围及证据 |
| --- | --- | --- |
| 恢复历史tip重新构建/链接 | 7 PASS/0 FAIL/0 BLOCKED，dirty=false | 新下载的锁定GTest，原application/query历史路径仅存在于历史worktree |
| 干净20f92b48及最终722d0441查询构建/链接/运行 | **各7 PASS/0 FAIL/0 BLOCKED，executed=true** | 1/2/0/-2/invalid → 1/2/0/-2/0，参数2、厂商5/-31 → 5/-31；与历史七组返回及共有XML属性一致 |
| 六条指定生产函数正执行 | PASS，次数56/49/28/28/56/56 | Core GetRealSimCount、Attach、Binding HostPort实现、Query、Manager、EXT；次数只是参与执行证据 |
| 初始化隔离 | Manager OnInit=0，Core Init不在覆盖/链接切片 | 两个executed标记均false，不冒充完整启动 |
| 源表/执行审计测试 | 14项PASS（含五份源表注释子例） | 有限文本审计，不解释完整GN或产品Feature |
| 架构/迁移/语法/纯规则/GN | PASS | 88独立单元及合并头、157 include/25 testonly源集、12项护栏、73原文件迁移及88项布局、四套规则，GN全骨架对象 |
| 完整初始化审计 | **BLOCKED/executed=false** | 干净20f92b48重审：45对象、43诊断、无缺头，14个SimData字段不匹配，未链接/运行 |
| 匹配产品/Feature/SO/SA/设备冒烟 | **BLOCKED/NOT_RUN** | 无hb、匹配manifest及build/ohos.gni；文本源表和主机LTO不证明产品准入 |

本轮独立机器证据为 [g2a_bplus_revalidation_20261105.json](../services/multisim/test/systemtest/configs/g2a_bplus_revalidation_20261105.json)，含代码/main/历史SHA、全部相关源和验证输入摘要、实际命令、七组返回/差分、方法符号及正执行次数、工具链、依赖、产物摘要和门禁。原 `g2a_checkpoint_evidence.json` 和 `g2a_bplus_evidence.json` 均保留原字节。XML、二进制、map/profile和完整日志在新建仓外输出根 `/mnt/e/g2a-bplus-m1-artifacts-20261104`；它们是辅助产物，仓内精简证据足以追溯本批结果，不依赖旧/tmp。

实际验证入口（从新worktree根；历史复验在历史detached worktree用同一参数、不同输出执行）：

```sh
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query-final"
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output "$SIM_TEST_OUTPUT/architecture"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_TEST_OUTPUT/full-init"
```

本机SIM_TEST_OUTPUT即上述仓外输出根；GTest和OpenSSL分别由现有prepare脚本新下载/校验包后解至deps及openssl，完整绝对命令及实际GN/Ninja/覆盖命令在JSON，不把示例变量当执行证据。主机工具链为Clang/LLVM18.1.3、GN1000、Ninja1.11.1、gold2.42。初始化仍缺EFUST/EHPLMN/GID1/GID2/IS_ESIM/LSI/MCC/MNC/MNC_LEN/OPERATOR_NAME/OPKEY/PHY_CARD/SIM_LABEL_INDEX/SPN正式Provider契约，未猜值或增加成功桩。

**保存与下一步：** 代码20f92b48及其证据f5c680ec已正常推送。最终审查修正新增审计测试的方法编码样例长度，并补完整方法及Init/OnInit正反断言；最终代码722d0441也已推送，以ls-remote核实同名远端分支，源main仍5e209a76。按干净722d0441新建query-final重新构建/运行七场景；其后仅同步本段及证据，不改业务Oracle或源码。首次linked worktree推送因Windows凭据工具路径失败，改从原仓库目录显式推送同一工作分支引用；一次TLS中断后使用HTTP/1.1正常推送，没有修改凭据配置或强推。最终证据与状态已保存为 `097e536a89a668f05b5bfc1fdd70f6bafd4f4cff` 并正常推送同分支；本次文档整理重新以ls-remote核实该SHA，fetch后本地/远端树相同且包含722d0441。原工作区分支/HEAD/文件保持不变，不推送或合并main。后续仅推进匹配OHOS manifest/Provider与产品Feature配置下的 `hb build -T //base/telephony/core_service:tel_core_service`、原SO共享对象/初始化发布及只读冒烟；完整OnInit、M2产品准入和G2b/c仍单独验收，本批不进入SetShowNumber或其他M3写业务。


<a id="main-integration-20261105"></a>

## 11. 用户授权其他分支合入main（2026-11-05）

用户明确要求“将其他分支的修改，合并到main分支并提交到远端”。起始原工作区干净，位于work/sim-guard-round2@5e209a76；远端main为 `5e209a76152a7e97411c41420ebd8d0e5ec67966`。已fetch并逐一审查所有本地分支及origin分支：独立架构分析1afffa4e/c70894f8、目录评审23b9be98、恢复入口9f72e7ff、整合5eed2181、首次B+ G2a 2feaf127、SIM Guard首批01db5ce6及第二轮5e209a76均已包含在main。真正新增的是G2a强化/证据/状态分支的5个提交：20f92b48、f5c680ec、722d0441、097e536a、d5f0527c。

旧分支work/g2a-readonly-checkpoint-20261030@728ad77e保留历史，不整个合入旧目录：当前B+已选择性迁入其真实业务，Core/Binding/Manager/EXT与历史字节一致，Query非预处理token等值，既有Oracle断言保留并加强；不恢复application/query。该分支不是main祖先，不表示存在遗漏的当前业务改动；其余本地及origin分支tip在本批快进后均是main祖先。

在新建隔离worktree检出本地main（原引用01a86b9f），先以 `git merge --ff-only origin/main` 对齐5e209a76，再以 `git merge --ff-only origin/work/g2a-bplus-revalidation-20261104` 快进至 **d5f0527c576d68fb639146d1a26d668728b7445a**，无冲突、无变基或强制覆盖。实际变更21文件，与已交付分支树完全相同；没有额外生产切换、写权或新设备执行者。源main工作区及其他既有worktree不切换、不清理、不覆盖。

| 合入后实际验证 | 结果 | 基线与边界 |
| --- | --- | --- |
| 查询重新构建/链接/执行 | **7 PASS/0 FAIL/0 BLOCKED** | 干净main代码点d5f0527c，executed=true、dirty=false；重新下载并校验固定GTest，五个真实生产CPP参与 |
| 七组返回及厂商边界 | PASS | 1/2/0/-2/invalid → 1/2/0/-2/0，参数2、厂商5/-31 → 5/-31；两次厂商场景单请求各一次调用 |
| 实际生产函数覆盖 | PASS | Core GetRealSimCount/Attach、Binding HostPort、Query、Manager、EXT依次56/49/28/28/56/56次；Manager OnInit=0，Core Init不在链接切片 |
| 源表/执行审计测试 | 14项PASS | 实际重跑，非业务用例数 |
| 架构/迁移/纯规则/GN | PASS，exit0 | 88独立单元/合并头、157 include/25 testonly源集、12护栏、73原文件及88布局、四套规则、GN/Ninja全部骨架对象 |
| 源码/证据保护 | PASS | 当前所有G2a生产与验证输入摘要匹配§10冻结证据；历史JSON、SIM Guard共同资产及已知缺陷记录未改 |
| SIM Guard完整套件/完整OnInit/产品 | **本批NOT_RUN** | 沿用§9/§10的实际结果与阻塞；不将本批查询PASS改记为17场景重跑、初始化或产品准入 |

实际执行命令（仓根为本批新worktree；完整GN/Ninja/覆盖命令在结果JSON）：

```sh
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output /mnt/e/multisim-main-integration-artifacts-20261105/architecture
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root /mnt/e/multisim-main-integration-artifacts-20261105/deps/googletest-b514bdc898e2951020cbdca1304b75f5950d1f59 --output /mnt/e/multisim-main-integration-artifacts-20261105/query
```

本批结果/日志留在上述新建仓外目录，不覆盖§10历史文件。`query/g2a-query-result.json` SHA256为 `8712314da7d9ef704400e01187f9f6744de0d76d1ddb885bcd644d541eacae82`，真实二进制SHA256为 `0c437ec4a06e4a0f5cbd4dc657e288da73b23795b07cb2698f6398e098638f3c`；当前源/验证摘要与仓内g2a_bplus_revalidation_20261105.json逐项匹配。仓内本节固定被测Git代码点、实际命令、返回结果及生产执行次数，使新目录之外也能追溯覆盖范围；旧证据JSON不改写为本批运行。

验证通过后以 `git -c http.version=HTTP/1.1 push origin refs/heads/main:refs/heads/main` 正常快进发布，ls-remote已核实远端main为d5f0527c；只推送main，不改动或删除其他远端分支。其后六份合入说明与本节已保存为 `2db639bcbc7c95235130b7608d79487106df610a` 并正常推送main，已通过ls-remote及fetch核实远端SHA/树，并确认包含整个G2a分支d5f0527c。本次仅补齐发布状态、固定引用与架构入口说明，不改变生产源、GN、测试、Oracle或历史JSON；未重新运行业务/产品测试。当前文档提交由用户明确授权继续推送main，最终tip按Git查询，避免自引用。原工作区仍保持原分支/HEAD和干净状态。本批只完成代码/文档整合与主机重验，产品、Feature、完整生命周期及其他生产迁移门禁不因此变为PASS。


<a id="open-closed-followup-20261109"></a>

## 12. 全过程范围复核：旧实现只读、缺陷只记录（2026-11-09，本地未发布）

用户明确要求“可以将修实现缺陷记录起来，但是不要修改旧的实现；请按照这个要求，审视整个过程”。本节替代上一版把旧实现修复计入本轮成果的描述。HEAD仍为 `a7d3af8b8809513e7b8e73196a94551a8ecff97d`，未提交/推送；原有未跟踪中文分析报告、既有工作树/worktree和暂存区继续保护。

### 12.1 过程审计与纠偏

| 环节 | 复核结论 | 实际处置 |
| --- | --- | --- |
| 初始综合审视 | 读取旧代码建立事实、记录CR-07等疑点符合范围；原报告不构成修改生产行为授权 | 保留12节审视及证据分级，补最新只读边界说明 |
| 权威文档/接口/测试设计补充 | T/A/O/R、B+及公开契约保持，局部消歧与描述缺口可以保留 | 保留四类协议、SIM-OC映射、测试边界/验收环境对照；不把建议写成产品批准 |
| CMI/SimManager/Monitor注销修复 | 把“优化文档和代码”扩大为修复旧实现，越界 | 三个cpp恢复原内容；Monitor与SimManager两个头恢复，新增方法及friend探针全部撤回 |
| 依赖该修复的账户主机切片 | 通过修改旧类和专属链接替换来验收修复，不适合作为本轮旧基线护栏 | 撤除账户专属target/构建参数、runner、用例和fail-fast/weak符号设施；不继续声称账户2例是当前PASS |
| PIN记忆与声明护栏 | 通过已有外部边界观察真实未修改代码、直接编译公共声明符合范围 | 保留独立四例及头文件断言；正常源为原文件，负控仅改仓外故障副本，不给旧实现加探针或修业务 |
| 验证证据与交付说明 | 原PASS真实发生，但使用过修改后的旧代码，恢复后不能直接沿用 | [旧结果](../services/multisim/test/systemtest/configs/open_closed_followup_20261109_withdrawn.json)明确标WITHDRAWN_FROM_CURRENT_ACCEPTANCE；全部保留套件在原基线上重新运行，当前证据另行记录 |

当前源代码变更范围限定为新实现及测试设施；本次并未另行修改multisim业务源码。旧实现问题的发现、记录、行为刻画与新实现的正确性验收分开，不能通过先修旧代码让基线变绿。本要求已加入部件AGENTS；不得为测试向旧类加friend/生产探针，也不得把旧缺陷固化为新实现必须保留的正确行为。

### 12.2 旧实现缺陷记录（不修复）

| 项目 | 当前事实与责任 |
| --- | --- |
| 问题 | CR-07 / SIM-OC-011，E/F/D，P1；账户事件注册和注销静态路由不对称 |
| 源码证据 | 原CMI Register范围含RADIO_SIM_ACCOUNT_LOADED，UnRegister仅到RECORDS_LOADED，账户注销落到RIL；原SimManager注销也缺账户分支；Monitor原注册记录到Observer。具体路径/行号见综合审视S06 |
| 影响等级 | 注销后未来通知、对象重建或在途生命周期有风险；当前只确认静态不对称，恢复基线后未提供完整账户/首帧运行复现，不宣称设备泄漏或UAF已证实 |
| 当前状态 | **旧代码保留，未修复。** 撤回前修改工作树的2例及负控不作为当前旧基线验收；不加入假成功适配或调整既有Oracle掩盖 |
| 新实现要求 | 在C登记/I token/Owner事件出口边界明确注销与在途语义、建立独立行为断言；不复制旧路由错误。生产迁移与新行为按既有门禁另验 |

原17条兼容场景与5条独立缺陷/待裁决均保留；不因本次范围纠正删除旧红灯、改期望值或把缺陷变成兼容规则。PIN拔卡空ICCID仍仅是现有回包行为刻画，不是旧资产清理批准。


### 12.3 恢复旧基线后的重新验证与交付范围

对`services/sim`、Core、wrapper、RIL、frameworks、interfaces、utils和原根BUILD/gni的549个跟踪文件，批量读取Git对象并逐文件比对工作树，按Git行尾规范化后全部一致；五个撤回文件已恢复HEAD原内容。全仓生产路径Git差异为空，当前未增加生产friend/方法、链接符号替换或业务策略。源码树中保留的修改只有执行约束、文档及测试设施。

重新运行结果与旧结果分开，当前[验收证据](../services/multisim/test/systemtest/configs/open_closed_followup_20261109.json)包含129个相匹配的源码/验证输入摘要、549文件完整性核对信息、实际命令、用例、生产正执行、负控及剩余缺口。旧证据外包WITHDRAWN标记保留，不能用其账户2 PASS或其他旧工作树结果替代下表。

| 验证 | 恢复原实现后实际结果 | 边界 |
| --- | --- | --- |
| PIN记忆独立套件 | **4 PASS**；操作类型突变DETECTED，原产物恢复4 PASS | 未修改的StateManager/Handle和既有外部Hook；不证明PIN资产或新安全策略 |
| 原SIM Guard全报告，repeat=1 | **兼容17 PASS、独立缺陷5 FAIL**；READY突变检出，恢复17 PASS | all退出1因5条原红灯；不改Oracle/Fake/冻结场景，不称全套绿色 |
| G2a查询 | **7 PASS/0 FAIL/0 BLOCKED** | 使用恢复后的原Manager/CMI相关编译输入；非完整初始化/产品验收 |
| 源表审计 | **14 PASS** | 当前只保留PIN增量的host GN配置，不含撤回的账户target |
| 架构/迁移/语法/GN | **PASS** | 88单元、157 include、25源集、73原文件/88布局、12结构负控、四套纯规则、全部骨架对象；另5个公共声明编译负控 |
| 账户注销CR-07 | **未修复；当前不计PASS** | 仅静态缺陷记录，后续新实现建立护栏；无当前旧基线完整账户动态验收 |
| 产品/厂端/设备、完整OnInit/Provider | **NOT_RUN/未验收** | 与本次保留测试无关的外部条件未因此关闭 |

可从Git根重跑（固定依赖需校验，输出选择仓外路径）：

```sh
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_pin_memory_contract.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/pin" --mutation-check
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/legacy" --repeat=1 --mutation-check
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/g2a"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output "$SIM_TEST_OUTPUT/architecture"
```

本次实际辅助产物在`/tmp/sim-readonly-review-20261109`，仅用于本次运行；仓内保存的脚本、固定依赖协议、摘要和精简结果不依赖该目录恢复。正常测试读取原生产文件；负控只对仓外副本施加可检出故障，不把副本修复部署回旧实现。原始中文报告与既有历史证据未改动。

后续沿本次只读约束完善新架构协议、目录/测试护栏，先记录旧实现的场景、失败和不确定语义；不再把修旧bug或为测试改旧类作为前置。新实现的注销、文件回退/Opkey、PIN与拓扑能力按已定Owner及适用门禁分别验收；Provider、vendor ABI/资源结束、Port/MEP等仍保留明确前置。当前既无生产写权迁移，也无提交/推送。

<a id="test-relocation-20261109"></a>

## 13. 重构测试工程统一迁入 multisim（2026-11-09，本地未提交）

用户明确要求将重构引入的测试工程、构建与相应文件统一放在 multisim 下面，减少对旧代码/目录的影响。本批只调整物理位置、路径发现、GN标签及导航，不改变业务六组、事实所有权、原测试断言或生产实现。输入仍为 main `a7d3af8b8809513e7b8e73196a94551a8ecff97d` 加§12本地修改；新核对远端 main 同SHA，暂存区为空，既有worktree保留。本次没有提交/推送授权，未暂存或发布。

### 13.1 当前入口及迁移边界

| 原位置（部件根相对路径） | 当前位置 | 处理 |
| --- | --- | --- |
| test/systemtest | services/multisim/test/systemtest | 独立GN、平台Stub、组件/G2a切片、脚本与历史证据整体迁移 |
| test/sim_guard | services/multisim/test/sim_guard | 场景/Oracle/Fake、适配器、独立缺陷及PIN套件整体迁移 |
| test/multisim_architecture_rules | services/multisim/test/multisim_architecture_rules | 独立GN、结构/声明负控、纯规则与业务源迁移清单整体迁移 |
| test/linux_sim_guard | services/multisim/test/linux_sim_guard | 原实验设施原样保留；不宣称其构建/运行已完成 |
| docs/test中的防护网v1.0/v1.1设计 | services/multisim/test/docs | 保留版本关系、业务分层与断言要求，只改位置和导航 |
| 仓根 telephony_core_service_linux_sim_ddd_test_design.md | services/multisim/test/docs | 历史Linux设计输入原字节保留，不升级为当前有效验收基线 |
| tools/check_sim_refactor_recovery.py | services/multisim/tools/check_sim_refactor_recovery.py | 仍通过Git定位仓根、读取固定历史提交，不执行当前业务测试 |

共迁移219个文件，194个原字节一致；其中全部136个C++/头文件及JSON保持原摘要。其余为脚本路径、GN标签和Markdown导航调整；前后路径与摘要在[迁移映射](../services/multisim/test/relocation_manifest.json)，用[核验脚本](../services/multisim/test/check_relocation.py)复查。新建[统一测试入口](../services/multisim/test/README.md)，不保留旧目录的转发脚本或构建副本。

业务骨架的90文件/88个C++统计仅指业务文件，新增顶层test/tools不计入其中；业务源扫描只排除这两个顶层辅助目录，仍检查业务源码全集、依赖DAG和禁止引用测试头的边界。GN只替换原骨架对架构测试入口的精确visibility标签，独立host源表指向新测试位置。原产品BUILD、原test/BUILD、gni及原单测源表本批均未修改。未将旧测试整体搬走或把测试文件加入生产源表。

本文及导航中的当前命令已改为新路径；历史JSON、摘要及§1恢复工具JSON块中的路径保留原值，必须按其固定提交解释。历史命令文字可作当前路径的等价运行入口，原实际运行命令以对应历史证据为准，不能因此声称历史提交已存在新目录。既有5条缺陷/待裁决中央记录现在位于[round2_catalog.json](../services/multisim/test/sim_guard/traceability/round2_catalog.json)；CR-07仍见§12.2，旧实现未修复。

### 13.2 新路径实际验证

本轮重新构建及执行的[验收证据](../services/multisim/test/systemtest/configs/test_relocation_20261109.json)记录运行命令、源码/验证输入摘要、各例结果、负控、历史恢复检查及未关闭项；未复用之前的PASS产物。固定GoogleTest依赖树重新通过摘要检查。

| 检查 | 本轮结果 | 边界 |
| --- | --- | --- |
| 迁移资产核验 | 219项通过，136个C++/JSON原摘要保持 | 不修改Fake/Oracle、业务断言或历史结果 |
| 业务架构/迁移/语法/独立GN | PASS | 88单元、157 include、25源集、73原文件/88布局、12结构负控、5声明编译负控、四套纯规则、全部骨架对象；不证明产品链接/ABI |
| G2a源表审计 | 14 PASS | 使用原产品/单测源表作只读输入，无旧目录挂钩 |
| G2a真实只读查询 | 7 PASS、0 FAIL、0 BLOCKED | 不包含Core Init/Manager OnInit、IPC或设备 |
| PIN记忆契约 | 4 PASS；突变检出，原产物恢复4 PASS | 生产源码只读，负控只用仓外副本 |
| SIM Guard全报告，repeat=1 | 兼容17 PASS、既有缺陷5 FAIL；READY突变检出，恢复17 PASS | all退出1，既有红灯保留，不称全套绿色 |
| 容量/设施工程额外复查 | 容量正常1 PASS、容量负控检出；设施GoogleTest 19 PASS，但runner整体FAIL | 16条设施登记清单与19个实际用例不一致，详下段；不得替换为PASS |
| 恢复工具 | CHECKPOINT_VERIFIED | 读取缓存远端引用及历史对象，不代表当前业务运行；远端main另通过ls-remote核对 |
| 旧生产完整性 | 549文件与HEAD按Git行尾规范化后一致 | 旧实现、公共接口、原构建入口均未修改 |

本轮额外发现**已有测试工程清单缺口**：`systemtest/tests/event_transport_test.cpp` 中三个SchedulerBoundary用例（IdleWaitDrainsCascadedQueues、IdleWaitDoesNotIgnoreDelayedOrRunningWork、IdleWaitExposesTaskFailure）未登记case_id/oracle_id，`configs/infrastructure_cases.json`只有16项。实际19例全部通过，runner严格集合检查仍报FAIL。两文件与HEAD内容一致，迁移前后C++/JSON摘要一致；这不是迁移新增的业务失败。暂记录，后续以独立测试工程任务补正式追踪，不能过滤三例、降低集合校验或改业务期望制造绿色。本次未修改相关用例、清单或校验逻辑。

当前测试产物在仓外 `/tmp/sim-test-relocation-20261109`，仅为辅助诊断；仓内保存可重跑入口、固定依赖协议、完整运行结果及迁移摘要，不依赖该临时目录恢复。实验linux_sim_guard、完整初始化、L2厂端链接/ABI/动态加载和L3设备业务均NOT_RUN，原产品准入/写权/生命周期门禁未关闭。下一步仍按§12及原阶段门禁处理新实现契约，另行安排设施清单追踪缺口，不能将文件内聚迁移作为生产验收。

## 14. 本地复审与测试迁移的发布（2026-11-09）

用户本次明确授权将本地修改提交并推送到远端main，覆盖§12文档/独立契约护栏、§13测试工程内聚迁移、综合复审及原始开闭源分析输入。发布前通过恢复工具实际fetch及ls-remote核对：本地main与origin/main均为 `a7d3af8b8809513e7b8e73196a94551a8ecff97d`，暂存区原为空，旧生产实现及原产品/单测构建入口无差异，其他worktree保持原状。

迁移资产核验及14项源表审计重新通过；§13已记录的完整构建/运行结果继续按对应源码摘要解释，本次发布不改写原五条缺陷红灯、设施清单缺口或L2/L3未验收状态。仅清理README中英文版和构建导航在路径替换时产生的行尾差异，恢复原CRLF格式。提交使用逐文件白名单，排除生成缓存与构建产物；提交与推送结果在完成远端核验后补记，不以发布成功代替产品验收。
