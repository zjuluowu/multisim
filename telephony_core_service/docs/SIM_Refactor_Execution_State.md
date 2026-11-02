# 多 SIM 重构执行状态

本文件集中维护当前实施位置、证据、局部阻塞和下一批任务；技术决策继续由[目标主设计](SIM_Target_Architecture_Design_v1.2.md)维护，流程由[流程对照](多SIM架构重构_设计说明与业务流程对照.md)维护，历史裁决留在[原评审](SIM_Target_Architecture_Independent_Review_2026-10-29.md)。最近恢复/一致性核对：2026-11-02；各历史运行结果仍按其原日期、基线和覆盖范围解释。

## 0. 当前核实摘要（2026-11-02）

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
