# 多 SIM 重构执行状态

本文件集中维护当前实施位置、证据、局部阻塞和下一批任务；技术决策继续由[目标主设计](SIM_Target_Architecture_Design_v1.2.md)维护，流程由[流程对照](多SIM架构重构_设计说明与业务流程对照.md)维护，历史裁决留在[原评审](SIM_Target_Architecture_Independent_Review_2026-10-29.md)。最近恢复/一致性核对：2026-11-05；G2a最新复验见[§10](#g2a-revalidation-20261105)，测试防护网首批见[§8](#sim-guard-20261104)，第二轮结果及阻塞见[§9](#sim-guard-round2)。此前G2a记录保留，各历史运行结果仍按其原日期、基线和覆盖范围解释。

## 0. 当前核实摘要（最近：2026-11-05）

最新M1任务实际输入为 `origin/main@5e209a76152a7e97411c41420ebd8d0e5ec67966`，已包含此前B+ G2a代码及SIM防护网提交。从该基线新建隔离分支 `work/g2a-bplus-revalidation-20261104`；没有覆盖同名历史实施分支或修改原工作区。干净代码提交 `20f92b4821dcaf8c24e78acf9db60b681b55c57c` 与恢复的历史tip分别重新构建，七场景均PASS、返回差分一致，六函数正执行及架构/GN检查PASS；状态为 **G2a_BPLUS_HOST_VERIFIED**。代码已推送同名远端分支并核实SHA；新证据/导航随后保存到同分支，最终tip按Git查询。完整初始化与产品门禁仍BLOCKED，未改变生产执行路径，详见§10。

以下保留2026-11-02首次适配及其后授权合入的历史快照，不将当时main/运行结果解释为当前HEAD：

M1实施时从干净 `main/origin/main@9ab1e976ce71c463281d13abbd9408d60792a4d4` 建立隔离worktree及 `work/g2a-bplus-integration`，只选择性恢复G2a真实只读代码/测试；该切片当时未修改main工作区和历史检查点；随后用户明确要求直接推送main，已快进合入并核实远端包含2feaf127（见§3.5）。

| 项目 | 本次核实状态与证据 | 下一步/限制 |
| --- | --- | --- |
| B+结构输入 | main含90f4992a结构提交及后续入口文档；本轮源main为上述9ab1e976 | 本表是基线快照；最终HEAD由Git查询，不写自身提交号 |
| 已合入main的G2a | 真实代码提交ff1c6ef2ea73d36ab83a7ac2c276036c6f9a6a58；integration/api增加Query/HostPort，Core旧对象桥及显式源表适配 | 用户本次明确授权快进合入main；产品准入仍未完成 |
| 当前源码集合 | 原main输入88文件/24个testonly source_set；当前main90文件（64h/24cpp及BUILD/README）/25个testonly source_set，原24边界不变 | 新叶子只验证Query；根产品候选直接列cpp，不依赖testonly骨架，其余业务仍旧实现 |
| 历史G2a | origin/work/g2a-readonly-checkpoint-20261030 tip为728ad77ed4c723efbb5cb1baf7b3f10bb9830240，包含cc787945检查点，恢复CHECKPOINT_VERIFIED | 历史JSON/Oracle保留原字节与语义，不将旧PASS晋升本轮结果 |
| 本轮主机闭环 | **G2a_BPLUS_HOST_VERIFIED**：干净ff1c6ef2重新构建/链接/七场景PASS，六条指定生产函数正执行（§3.4） | Core Init/Manager OnInit均未执行，不证明完整产品生命周期 |
| 架构与工程门禁 | 本轮88 C++/合并头、四套规则、12项护栏（含新增负例）、157 include/25 target、迁移及GN/Ninja全部对象PASS | 完整初始化实际BLOCKED；匹配产品/hb/Feature、原SO初始化发布/冒烟未执行 |
| 目标与授权 | 最终由multisim完整替代sim；本轮仅GetRealSimCount；修订B+和T/A/O/R不变量保持 | 两历史ADR仍Proposed；没有写权、设备执行、SO/SA、ABI/IDL/schema或资源政策变更 |

**实际调用链：** 原Core → LegacySimQueryBinding → integration/api/RealSimCountQuery → RealSimCountHostPort → 原SimManager/EXT。主机只模拟严格平台接口及厂商ABI，不替代查询业务。附着复用原对象与控制块，空/重复附着、替换/清理负例仅证明顺序生命周期，不承诺并发热替换。代码提交ff1c6ef2已正常推送至origin/work/g2a-bplus-integration，git ls-remote核实远端SHA一致；本轮文档/证据另行提交到同一分支，最终tip按Git核实，不在本文写自身SHA。

**仍需闭环：** G2a原SO产品准入；完整初始化所缺provider符号契约；T/A全入口写权；成熟卡能力迁入与CardRuntime收窄；真实设备资源域/结束/排空；Feature、产品及设备差分。Carrier Loader/Detect旁路、ARA-M、Records/ADN及Detect初始化风险仍归既有WU（§3.3/§5），未混入本次只读切片。

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
| G0/G1测试护栏 | 冻结功能、真实入口/旧轨迹、独立Oracle、平台边界；与结构工作并行 | 部分已具备：纯协议、容量、受限平台设施；完整无卡/账户/PIN尚未闭合。历史测试各有基线 |
| G2a原SO依赖收口 | 保持原writer/执行者/重试者，真实HostPorts及原对象 | 选定只读查询已运行；匹配产品SO构建、启动/Feature与原对象发布验证未完成 |
| G2b SO工程边界 | 相关依赖、创建/销毁、单例与符号闭合；不强制先完成全部G3 | 独立产物NEEDED/导出/安装/CFI/PAC/LTO及冒烟差分。当前未实施；目录重排不算完成 |
| G2c生命周期 | 仅产品需要重建/卸载时前置，常驻拆分另列限制 | 受理停止、注销、迟到、排空、匹配销毁；未知设备效果独立。当前未验收 |
| G3a～d写权迁移 | 以既有WU覆盖初始化、重载、后台、恢复及全部旧入口；切片有真实写点/外部证据 | 单Owner、无双写/双发/双重试、兼容差分。当前没有生产写权交接 |
| 行为增强（对应G3b/c等） | 固定目标、拒迟到、准入/隔离、队列/安全重试单列旧新轨迹 | 方法级错误、时延、部分效果、结束证据和产品验收，不夹入G2机械迁移。当前未启用 |
| G4能力/契约扩展 | 明确需求、批准、provider/设备/服务支持 | 新身份/schema/API/MEP/N槽等独立能力矩阵与回退；当前不授权 |

## 3. 已完成及当前切片

- **设计/协议**：main主设计§4～6/§9～14、原评审§9～10说明双投影、联合建行、发现最终资格、槽激活、Primary分支、完成、工厂、Profile和结果/资源协议。仍draft/pending；代码落点是`services/multisim`，旧生产在`services/sim`。
- **局部实现/纯规则**：`test/multisim_architecture_rules/run.py`编译运行topology/resource/assignment/runtime_protocol四套；`5eed`集成时全部通过，73个骨架单元严格语法通过。历史PASS不能自动继承到后续HEAD。
- **G2a工作分支实现**：`CoreServiceSim::GetRealSimCount`→`LegacySimQueryBinding`→`RealSimCountQuery/RealSimCountHostPort`→原`SimManager/EXT`。保留Core原TU先读`SIM_SLOT_COUNT_REAL`、空Manager fallback、原Manager宏/EXT读源、返回及重设清旧附着。Core原OnInit/发布点后附着，同一Manager/RIL控制块，原SO和显式测试源表同步，无新写权/设备执行。
- **G2a主机闭包**：真实5个生产cpp实际构建、链接及7场景运行；覆盖精确cpp正执行证据。只模拟参数、严格ITelRilManager平台接口和明确厂商ABI；没有SIM业务替身。Manager OnInit/Core Init、卡在位和IPC/SA没有执行/观察。
- **保存状态**：G2a与入口文档分开提交；原本的临时worktree/patch/JSON仅为本机历史，不是恢复所需输入。可恢复内容为远端commit、跟踪脚本、锁定依赖和归一化精简证据。

### 3.1 目标源码目录落地（2026-10-30，main已保存）

- 输入为干净的`main@18e724f70ab3bfef142674d794ad5dfba04d371d`；实际fetch核对origin main与独立G2a工作分支，恢复工具返回CHECKPOINT_VERIFIED、main_contains_g2a=false。未切换分支或合入G2a；本批源码结果已单独提交并推送。
- 两篇已提交设计的针对性验收通过；T账户/绑定/资料、A角色/Primary、R会话/真实设备资源、C旧语义及H生命周期边界没有发现阻断本轮迁移的冲突。两篇设计本身未改，历史现状路径由[源码README](../services/multisim/README.md)补充当前导航。
- 全量核对原73文件（50头/23源）：48文件以git mv改变路径、25保留主落点，其中6文件拆取现有职责；无原文件无去向、无业务实现删除。现为86个C++文件（63头/23源）。纯关联值、Owner纯types、PIN/EF/Profile契约、三个Adapter、装配内部定义及仓内导出各保持单一定义；[逐文件映射](../test/multisim_architecture_rules/source_migration.json)记录原摘要、职责、状态与去向。
- 原application/model及T/A application/ports撤除；场景与Adapter随Owner。C经具名API函数调用原对象，保留构造签名与类型布局；没有新运行对象/状态或公开契约。LocalCardRequests仍保留原类，实际Profile编排移euicc，PIN/EF仅分契约、业务方法仍未实现；CardRuntime薄化与成熟卡能力迁移尚未完成。
- 实际运行`python3 telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration`（另指定仓外输出目录）：73文件摘要/去向、原类型定义/成员顺序与23个原cpp实现token顺序通过；86头/源独立严格C++17语法及合并头TU通过；四套纯规则实际编译/链接/运行PASS；12项护栏测试通过；156条本地include和24个源集合边界/环检查通过；独立GN gen --check及Ninja全部23个骨架源对象编译通过。新增护栏覆盖宏/续行include、动态GN列表与入口漏挂目标的反例。
- 所有新增GN源集合为testonly，没有接入根产品BUILD/服务发布、共享库或第二条执行路径；services/sim、Core和公共接口未改，生产写权不变。CleanArchCLI不在环境中，以文本include/Target检查替代；没有运行完整骨架产品链接、hb、Core/Manager真实初始化、provider或设备验证。48条git mv移动与其余修复/新增文件随本批同一提交保存；提交本身不改变上述验证范围。

### 3.2 业务可发现性局部优化（2026-10-31）

- 输入：干净main@38c1f2329e5a50349932a19d450c31c403743df1；恢复工具实际fetch后CHECKPOINT_VERIFIED，HEAD与origin/main差异0/0，main_contains_g2a=false。原有其他worktree保持原位；未合入G2a。
- 裁决与T1～T6模拟定位对比见[详细设计§3.4](SIM_Multisim_Source_Architecture_Detailed_Design.md)。topology/domain改accounts（4文件），profile改account_profile（6文件），其余主要路径保留。GN仍以topology_domain约束纯规则；没有新Owner、运行对象、依赖边或生产执行路径。
- 本轮输入88个实际文件（86个C++及BUILD/README）逐项记录在[source_migration.json](../test/multisim_architecture_rules/source_migration.json)的layout_revision；旧73文件溯源保留并更新destinations。C++非预处理token保持，原类型/成员与cpp顺序检查保留。对照输入BUILD，仅替换两组路径后的所有target sources/public/deps/public_deps完全一致。
- README提供任务/Owner/测试导航，部件AGENTS复用安全规则；两篇权威设计刷新实际/计划布局及全部存活源码链接，没有另建平行报告。最终153个本地链接目标存在，Markdown围栏及git diff HEAD --check通过。
- 调整前完整运行run.py --with-gn --migration通过。调整后同命令实际exit0：88文件映射、86头/源独立严格C++17语法与合并头TU、四套纯规则、12项护栏、156条include/24个source_set边界及无环检查、GN gen --check与Ninja全部23个骨架源对象均通过。产物仅在仓外，不作为下次恢复前提。
- 范围：未修改services/sim、Core、产品根GN或公共接口。未运行OHOS完整产品链接、初始化/发布、IPC/SA、Feature矩阵、provider或设备验证；本批结构/规则结果不能证明生产行为等价。CleanArchCLI未用，以现有文本include/target依赖和GN检查限定替代。
- Git保存：用户已明确授权将本批24文件提交并正常推送至origin/main；提交号、远端tip及包含关系通过Git查询，不在同一提交内硬写自引用SHA。不合入G2a、不强推；保留上述产品/设备未验收范围。
- 下一步仍先在独立工作分支适配G2a真实GetRealSimCount只读闭包并复跑7场景/覆盖，再满足原SO产品准入；后续写能力可选WU-05账户号码完整切片，必须先建立卡→DB→current/historical缓存→通知/callback及查询卡→账户→IMPU读源、失败/超时/迟到差分Oracle，不顺带启用固定目标或新重试政策。

### 3.3 修订 B+ 骨架落地（2026-10-31实施，2026-11-02已提交推送）

- 用户已选择独立ADR的局部修订B+作为本轮物理组织实施基线；两份历史ADR仍为Proposed，不追溯改写。不代表G2a产品接入、写权或新资源政策批准。
- 输入为干净 `main@5404aefe66decde4e5c5f03a01f355f6bf127e7e`，origin/main差异0/0。恢复工具实际fetch后CHECKPOINT_VERIFIED、main_contains_g2a=false；未切换/合入其他worktree。
- 输入88文件（63头/23cpp及BUILD/README）全部记录于既有source_migration.json；86个C++移入六组，类型/成员/非预处理token保持；BUILD/README原位更新。24个逻辑source_set及deps/public边界保持，检查器用显式文件集合区分逻辑层，不再由旧父目录推断。各片文本边界检查通过。完整执行 `python3 -B telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/multisim-bplus-after` exit0：原73文件溯源、88文件映射/非预处理token、86独立头/源与合并头TU严格语法、四套纯规则、12项护栏、156条include/24 target及无环、GN gen --check/Ninja全部23个cpp对象均PASS。产物在仓外，不作为恢复前提。
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
- 新机器可读证据：[g2a_bplus_evidence.json](../test/systemtest/configs/g2a_bplus_evidence.json)。历史[g2a_checkpoint_evidence.json](../test/systemtest/configs/g2a_checkpoint_evidence.json)保持原字节。大型XML/profile/map/二进制和日志留仓外；最终文档/证据提交通过记录的生产/测试输入摘要与被测代码提交关联，不把文档HEAD当成重新执行业务测试。
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
python3 telephony_core_service/tools/check_sim_refactor_recovery.py --remote origin --fetch
python3 telephony_core_service/test/multisim_architecture_rules/run.py
```

恢复工具默认只查本地remote refs；`--fetch`才更新远端跟踪引用，不切分支/清工作区。`main_contains_g2a`只判断旧cc787945检查点的祖先关系；本轮选择性移植后即使返回false，也不能据此断言main缺少B+实现。另核对main包含ff1c6ef2/2feaf127以及新证据的源码摘要。工具只核对历史可达性、源摘要和证据，**不会运行或宣布业务PASS**。

### 4.2 G2a历史检查点与B+闭包（B+已在main）

恢复历史实现时，先保护本地修改，再在独立worktree/克隆使用§1远端代码commit；不要把临时路径当必需参数。历史分支可能已有新的入口文档提交，确认它仍包含cc787945检查点。本轮B+是选择性移植，不要求新分支在Git祖先中包含该历史提交；应核对ff1c6ef2及additions源摘要/非预处理token。

```sh
python3 telephony_core_service/tools/check_sim_refactor_recovery.py --remote origin --fetch
# 显式拉取main和工作分支，兼容single-branch clone；remote名称按实际选择
# 例：当前目录继续保留；SIM_RESTORE_DIR由当前机器选择且必须不存在
git worktree add --detach "$SIM_RESTORE_DIR" origin/work/g2a-readonly-checkpoint-20261030
```

上述命令用于历史检查点。复验本轮B+应先显式fetch `work/g2a-bplus-integration`，在另一新worktree检出该远端分支并核实包含ff1c6ef2，不能把历史路径视为本轮入口。新旧runner均沿用下面锁定依赖及参数化命令；新证据见§3.4。

在选定工作区根阅读AGENTS/本状态，再准备依赖。下载URL与archive/tree SHA以`test/systemtest/configs/dependency_lock.json`为准，以下为已锁定GoogleTest：

```sh
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o "$SIM_GTEST_ARCHIVE"
python3 telephony_core_service/test/systemtest/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
# 设置SIM_GTEST_ROOT为SIM_DEPS_DIR下googletest-b514bdc898e2951020cbdca1304b75f5950d1f59目录
python3 telephony_core_service/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query"
```

预期是`g2a-query-result.json`中PASS/executed=true/7 PASS，6条核心函数同时有预期cpp路径和正执行次数。七场景为1/2/0/-2/invalid、厂商2→5/2→-31，并含控制块、重设/清理和空引用反例。JSON必须仍明确`manager_on_init_executed=false`、`core_init_executed=false`。依赖失败应BLOCKED且清旧XML，不制造PASS。原容量/INFRA套数字不得加到此切片。

归一化历史证据位于工作分支`test/systemtest/configs/g2a_checkpoint_evidence.json`。历史观察以5eed dirty工作树为输入，5个源码SHA与保存代码对应；重新克隆后重跑结果按本次HEAD和源摘要记录。路径变化会改变二进制/coverage，不能要求新二进制SHA等于旧值。

### 4.3 完整初始化审计（不运行场景）

Query不需OpenSSL；只有下面审计需要锁定开发头包。按`dependency_lock.json`和`prepare_openssl.py --help`下载/校验并传入`SIM_OPENSSL_ROOT`，不要安装/伪造字段绕过错误。

```sh
# SIM_OPENSSL_ARCHIVE是按dependency_lock.json下载的包；SIM_OPENSSL_ROOT必须为新目录
python3 telephony_core_service/test/systemtest/scripts/prepare_openssl.py --archive "$SIM_OPENSSL_ARCHIVE" --destination "$SIM_OPENSSL_ROOT"
python3 telephony_core_service/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_TEST_OUTPUT/full-init"
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

以下为2026-11-02的G2a建议；2026-11-04用户已另行授权先建设真实旧实现测试防护网，当前交付及后续测试切片见§8，不据历史推荐自动进入生产迁移。

**推荐最小任务：M2匹配OHOS产品准入验证。**本轮G2a B+真实只读适配、主机七场景及覆盖已完成，现已按用户授权合入main（§3.4/§3.5）；不重复做目录比较、查询重写或将旧host PASS继承为产品结论。

- 环境：提供匹配Manifest/fork/provider版本、产品配置、工具链及Feature组合；核实原tel_core_service/SO显式源表。主机完整OnInit另外先获得14个SimData字段/URI/schema/错误协议的真实契约。
- 验收：在产品树按原路径挂接候选分支并构建 `hb build -T //base/telephony/core_service:tel_core_service`，记录实际产品/工具链/Feature命令结果；验证原Core/Manager/RIL共享控制块、原初始化发布及GetRealSimCount只读冒烟。每个请求仅一个读执行者；不自动启用第二服务或合入main。
- 保持：宏求值TU/EXT读源/Fallback、原SO/公开契约及业务Writer不变。并发热替换、G2b/G2c排空和设备结束另验；不启动SetShowNumber或其他M3写业务。

**G2a产品准入的后续门禁（仍未满足，源码已按用户授权合入main）：**源码/测试与远端干净恢复已有本文件§4证据；需在匹配OHOS产品树核对原SO与三份显式源表、构建`tel_core_service`并采集原初始化发布/只读冒烟与适用Feature结果。路径为工作分支Core query/binding、根BUILD和显式测试源表；不改宏求值TU/EXT读源、公开契约、原Manager/RIL身份或G3政策。有环境则实际执行；无环境保留工作分支和具体未验收项，不能把host PASS当合入批准。本次授权只完成源码合入/推送，不据此宣称产品验收或部署完成。

**待外部证据工作：**匹配fork存储字段/URI/schema/错误协议后重跑完整初始化审计，再建立正式RIL ABSENT输入的状态/账户闭包；不猜schema、不补业务成功桩。设备结束/远端服务缺口仅约束其所依赖的切片，不阻塞前述文档裁决。

## 7. 历史交接与更新规则

以下记录2026-10-30历史整合，不表示当前HEAD或最近验证；当前状态以§0/§3.4为准。

2026-10-30本地整合：原工作区内容先保存为提交`26d81e540243375ea9873e6e7e26b4bbb3eb4028`；以`origin/main@9f72e7ff94a924caafff291444414bf4e7bd0037`为整合基线。针对该旧工作树的P1/P2协议，按[原评审§10](SIM_Target_Architecture_Independent_Review_2026-10-29.md)已有整合裁决保留main现行接口及规则测试；被替代的旧接口和旧测试保留在上述父提交，不重复放入活动源码。未合入G2a工作分支或切换生产写权。

按用户授权将整合提交`eb112578a88dd8ddd35f172f3b04facddb968887`正常推送至`origin/main`；推送后通过`git ls-remote`确认远端指向该提交，并核实其包含上述本地历史提交。此记录不改变G2a仍在独立工作分支的状态。

本次整合结果的代码树与上述main基线一致，状态文档除外。实际重跑四套纯架构规则，全部PASS；73个骨架头/源的Clang C++17严格独立语法检查全部PASS。恢复工具验证远端检查点可达，未执行运行时测试；未运行OHOS产品构建、真实初始化、设备或provider验证。上述结果不改变§5局部阻塞及§6的G2a合入门禁。

本轮已将原本只在临时目录的G2a源码、构建、runner及精简历史证据保存到上块远端代码检查点，并把根/子AGENTS作为统一入口。原/tmp worktree、patch或旧结果消失不影响按远端恢复。当前开发位置应由`git branch --show-current`/`rev-parse HEAD`查询，不能把某电脑当前分支写成全仓唯一状态。

新会话：根AGENTS→部件AGENTS→本文件→Git包含关系→按最小任务读取设计/代码→实际验证→更新本状态。工作结束时只更新受影响结论/证据、代码branch/已有SHA及下一批任务；不复制完整会话，不改写历史报告，不将本文最终SHA硬写回同一提交。若状态与真实远端/代码不符，先纠正状态，再继续授权范围内的最小任务。

<a id="sim-guard-20261104"></a>

## 8. 跨架构测试防护网首批工程（2026-11-04）

初始工程任务授权实际测试建设及本地提交，当时未授权推送或合并main；后续远端保存授权见本节末。开始于干净 `main/origin/main@01a86b9f073b20f408d9f8e67da35192aacac439`；恢复工具 `--remote origin --fetch` 实际返回 CHECKPOINT_VERIFIED，初始远端差异0/0，已有worktree保留。新建本地分支 `work/sim-guard-contract-20261104`，最终代码检查点 `2c5364a3dcf9f1487d759b6bedcbf780ba16663e`。未修改任何生产源码、公开接口、生产根GN或既有业务策略。

交付位于 [test/sim_guard](../test/sim_guard/README.md)：最小ISimSutAdapter、Legacy绑定、有状态Fake RIL、公开查询快照、StateRegistry通知Spy、Contract Driver/Oracle、七条Compatibility、一条独立Defect、版本化共同资产摘要、结构化取证/规则/缺口，以及真正可用的GN/命令入口。Fake只维护外设事实、PIN设备应答、请求和回包调度；场景/Oracle不include旧新内部类。只有Adapter接触具体状态实现，真实CMI/EXT未用业务桩替换，CMI的账户路由未装配。

真实链路为 `SimStateManager::Init` → 注册RIL通知 → `RADIO_SIM_STATE_CHANGE` → 真实 `GetSimStatus` 请求 → RIL正式响应事件 → `SimStateHandle::GetSimCardData/ProcessIccCardState` → 真实查询和外服务通知。真实TelEventHandler/TelEventQueue、ObserverHandler及状态/PIN代码均链接运行；新增有界host排空控制，未改生产调度或用私有setter构造状态。这是状态组件切片，不能升格为完整SimManager/Core初始化、公开ISimManager参数校验、账户或产品验收。

| 实际验收 | 结果 | 范围/证据 |
| --- | --- | --- |
| 干净代码检查点重建/执行 | **7个兼容用例×5轮，35 PASS/0 FAIL** | LIFE_001/002、PUK_001锁态、PIN_001、PIN_002命令失败、DUAL_001、RIL_001重复及错误注入；每轮同进程运行七例并清理，八条指定生产函数精确源路径正执行 |
| 真实生产突变检出 | **DETECTED，恢复7/7 PASS** | 仓外生产cpp副本把READY映射破坏为LOCKED，同一LIFE_001及Oracle实际断言失败；随后原始生产二进制重跑通过，工作树生产源始终未改 |
| DEFECT_001 | **1 FAIL/0 PASS，正常非零退出** | 已READY→保留旧查询→较新拔卡回包使查询ABSENT→旧READY回包使公开存在性/状态恢复true/READY；失败及完整观测保留，不Skip/xfail，不把错误写入兼容预期；具体代次/准入修复政策待裁决 |
| 平台设施回归 | **19 PASS** | 原16项及新增跨队列排空、延迟/运行中超时、异常可见性三项；独立计数，不加到SIM业务通过数 |
| 架构设施 | **PASS** | 四套纯规则、结构检查和12项护栏；不作为SIM业务/产品验收 |
| 防伪通过负控 | **按预期拒绝** | 场景摘要漂移、即使重算摘要的内部include、零例/Skip XML均被拒；缺依赖、multisim、target/nezha入口均BLOCKED、executed=false |

实际命令与结果/源码摘要见仓内 [evidence.json](../test/sim_guard/evidence.json)，包括失败轨迹、突变/恢复、八条执行审计、host输入摘要和eSIM编译诊断。完整XML/profile/coverage/map/日志放在当前机器仓外；恢复不依赖这些路径。参数化重跑命令见 [README](../test/sim_guard/README.md)。核心实际入口为：

```sh
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat" --repeat=5 --mutation-check
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=defect --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/defect"
```

规则等级严格区分：公开状态/卡型/PIN结果定义作为本切片强Oracle；错误回包后的状态保持仅记录C级观察，未冻结为业务政策。每次通知只检查本步骤新增集合，不要求非契约全序/次数。广义账户隔离、默认回选、nezha I/R和取消回滚均未升级批准。Slot/Card/Port/Profile/Subscription/角色及网络注册分别建模；未接入维度明确未观测，不由slot合成账户身份。

当前只达到P0真实状态/PIN闭环、P1初始兼容子集及P2接入机制准备；**未完成全模块P1、完整eSIM场景族或第二阶段B验收**。本机无hb、祖先无.repo、仓根无build/ohos.gni；OHOS产品/Feature、SA/IPC/权限、设备未运行。完整初始化/账户/默认卡/持久化仍缺匹配Provider协议；本批未重跑历史完整OnInit审计，不把旧45对象/14字段数字当成本次编译结果。Fake Store/LPA尚未接入，不补业务成功桩。

eSIM额外取证已实际尝试真实 `esim_file.cpp` 的Clang18语法编译，exit1首错为 `core_service.h` 所需 `system_ability.h` 缺失；还需要普通EventHandler/EventRunner合法投递及真实codec/源表闭合。诊断性屏蔽两个具体头后，发现仓内 `EsimProfile::portIndex` 为u16string、实现却按int32/ASN.1整数使用的类型冲突；该诊断不是产品构建结果，完整未解析依赖尚未知，不擅改公开类型。完整DownloadProfile属于SA66250，本仓只含协作/代理契约，不能用Fake补写后计为真实SIM下载通过。

下一最小切片：先取得匹配Provider字段/URI/schema/错误协议，推进真实账户/角色和重启恢复；eSIM另核实portIndex正式类型，补平台EventRunner/头及真实APDU/Profile绑定后运行。已复现迟到问题保留红灯并裁决修复政策。未来业务迁入multisim后只替换 [Adapter及GN/执行审计绑定](../test/sim_guard/adapters/multisim/README.md)，共同场景、输入、Oracle和Fake按v2基线五份摘要保持；没有真实接入时继续BLOCKED。不得从本轮测试建设推定G2a产品、写权迁移或新能力批准。

本批v2基线评审：移除Driver对精确RIL请求次数/队列下标的假设，按外部槽及设备事实批量交付、有界排空；正式PIN失败使用 `UNLOCK_OTHER_ERR`。统一契约、场景数据和业务Oracle相对507b6115逐文件无差异，版本/原因记入semantic_baseline；上述35 PASS、突变检出及恢复7 PASS已在干净2c5364a3上重新构建运行。独立迟到缺陷复现需要两个在途响应的故障调度，未来实现若串行化导致该前置条件不可达，不视为兼容性违约；七条Compatibility不要求该拓扑。

### 8.1 用户授权保存到远端工作分支

用户在本地交付后明确要求“将本轮修改提交到远端”。本次从干净工作区核对origin、分支/HEAD、暂存区及既有worktree，恢复工具 `--remote origin --fetch` 返回CHECKPOINT_VERIFIED；远端main仍为 `01a86b9f073b20f408d9f8e67da35192aacac439`，本地仅领先本轮五个提交，远端尚无同名工作分支。正常非强制推送到 `origin/work/sim-guard-contract-20261104` 并建立跟踪，`git ls-remote` 已核实远端为 `e99caba343e73d6f72d9bbb6969dc90095125e85`，包含本轮五个代码/证据提交及最终被测代码2c5364a3。

本次只发布当前工作分支，未合并或推送main。此后仅补充本段发布记录，同步保存到同一远端分支；最终文档tip按Git查询，避免自身SHA自引用。发布没有改变业务源码、共同测试资产或既有结果，本次未重跑业务测试，不将远端保存解释为完整产品/新架构验收。

<a id="sim-guard-round2"></a>

## 9. 测试防护网第二轮：生命周期扩展与缺陷独立统计

用户本轮指定以e99caba3为已报告基线，禁止未经授权推送/合并main。实测起始工作区干净、HEAD为 `01db5ce64fc5fdb8d80c585bf35d4c260c817b86`；e99caba3是其祖先，两者仅发布记录不同。恢复工具fetch及ls-remote确认远端工作分支实际为01db5ce6，main仍为01a86b9f，分支远端差异0/0；没有根据历史报告假定发布状态。新建本地 `work/sim-guard-round2` 开发，未改用户已有worktree或任何生产源码/公共接口/根产品GN。

本轮测试执行时源码为父提交01db5ce6上的工作区修改（当时未提交、未推送、未合并main）；本轮runner如实记录dirty=true及全部相关输入摘要，不把父提交冒充已含新代码的检查点。源/测试/命令/结果证据保存于仓内 [evidence_round2.json](../test/sim_guard/evidence_round2.json)，新增场景和规则映射见 [round2_catalog.json](../test/sim_guard/traceability/round2_catalog.json)。旧evidence.json、原v2业务契约/场景/Oracle/Fake及七条测试代码块保留；仅把原ContractDriver原样提到共享头供新场景复用，v3基线另外强制核对v2原摘要和测试代码块摘要。负控实测：即使重算当前摘要，改动旧断言或Oracle仍被拒。

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
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/all" --repeat=5 --mutation-check
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat"
python3 -B telephony_core_service/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_GUARD_OUTPUT/full-init"
```

**任务C/D尚未闭合**：重新准备锁定OpenSSL头并实际重跑完整初始化审计，仍exit1、BLOCKED/executed=false：45对象、43诊断、无缺头、14个未匹配SimData字段。实际核对当前可用官方Provider `843a8be7f8d1dc304870a3999b754a03c6cce57d` 头及Git内容，不能提供此fork所需扩展字段；未猜字段值或把Fake Store当账户业务。映射、默认Voice/Data、无效账户、角色失效及DB保存/恢复/失败均NOT_IMPLEMENTED，正式Provider字段/URI/schema/错误协议和部分产品策略是当前具体前置。证据和真实调用路径见 [Provider契约记录](../test/sim_guard/traceability/provider_contract_round2.json)。本机无hb/匹配产品树，完整Core/SA/IPC/设备未验证。

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
| 干净代码20f92b48查询构建/链接/运行 | **7 PASS/0 FAIL/0 BLOCKED，executed=true** | 1/2/0/-2/invalid → 1/2/0/-2/0，参数2、厂商5/-31 → 5/-31；与历史七组返回及共有XML属性一致 |
| 六条指定生产函数正执行 | PASS，次数56/49/28/28/56/56 | Core GetRealSimCount、Attach、Binding HostPort实现、Query、Manager、EXT；次数只是参与执行证据 |
| 初始化隔离 | Manager OnInit=0，Core Init不在覆盖/链接切片 | 两个executed标记均false，不冒充完整启动 |
| 源表/执行审计测试 | 14项PASS（含五份源表注释子例） | 有限文本审计，不解释完整GN或产品Feature |
| 架构/迁移/语法/纯规则/GN | PASS | 88独立单元及合并头、157 include/25 testonly源集、12项护栏、73原文件迁移及88项布局、四套规则，GN全骨架对象 |
| 完整初始化审计 | **BLOCKED/executed=false** | 干净20f92b48重审：45对象、43诊断、无缺头，14个SimData字段不匹配，未链接/运行 |
| 匹配产品/Feature/SO/SA/设备冒烟 | **BLOCKED/NOT_RUN** | 无hb、匹配manifest及build/ohos.gni；文本源表和主机LTO不证明产品准入 |

本轮独立机器证据为 [g2a_bplus_revalidation_20261105.json](../test/systemtest/configs/g2a_bplus_revalidation_20261105.json)，含代码/main/历史SHA、全部相关源和验证输入摘要、实际命令、七组返回/差分、方法符号及正执行次数、工具链、依赖、产物摘要和门禁。原 `g2a_checkpoint_evidence.json` 和 `g2a_bplus_evidence.json` 均保留原字节。XML、二进制、map/profile和完整日志在新建仓外输出根 `/mnt/e/g2a-bplus-m1-artifacts-20261104`；它们是辅助产物，仓内精简证据足以追溯本批结果，不依赖旧/tmp。

实际验证入口（从新worktree根；历史复验在历史detached worktree用同一参数、不同输出执行）：

```sh
python3 -B telephony_core_service/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/query"
python3 -B telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration --output "$SIM_TEST_OUTPUT/architecture"
python3 -B telephony_core_service/test/systemtest/scripts/audit_initialization.py --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --output "$SIM_TEST_OUTPUT/full-init"
```

本机SIM_TEST_OUTPUT即上述仓外输出根；GTest和OpenSSL分别由现有prepare脚本新下载/校验包后解至deps及openssl，完整绝对命令及实际GN/Ninja/覆盖命令在JSON，不把示例变量当执行证据。主机工具链为Clang/LLVM18.1.3、GN1000、Ninja1.11.1、gold2.42。初始化仍缺EFUST/EHPLMN/GID1/GID2/IS_ESIM/LSI/MCC/MNC/MNC_LEN/OPERATOR_NAME/OPKEY/PHY_CARD/SIM_LABEL_INDEX/SPN正式Provider契约，未猜值或增加成功桩。

**保存与下一步：** 代码20f92b48已正常推送并以ls-remote核实 `origin/work/g2a-bplus-revalidation-20261104`，源main仍5e209a76。首次linked worktree推送因Windows凭据工具路径失败，改从原仓库目录显式推送同一工作分支引用；一次TLS中断后使用HTTP/1.1正常推送，没有修改凭据配置或强推。本段及证据另行提交/推送同分支，最终文档tip按Git查询，避免自引用。原工作区分支/HEAD/文件保持不变，不推送或合并main。后续仅推进匹配OHOS manifest/Provider与产品Feature配置下的 `hb build -T //base/telephony/core_service:tel_core_service`、原SO共享对象/初始化发布及只读冒烟；完整OnInit、M2产品准入和G2b/c仍单独验收，本批不进入SetShowNumber或其他M3写业务。
