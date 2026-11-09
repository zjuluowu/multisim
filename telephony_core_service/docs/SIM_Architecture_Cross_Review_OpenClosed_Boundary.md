# SIM 架构交叉复审：开闭源边界、源码组织与测试防护网

过程复核要求：旧实现缺陷只记录，不修改旧代码。此前优化中混入的旧实现修复和依赖它的账户测试已撤回；本报告CR-07继续作为未修复缺陷线索，最新过程处置与验收见[执行状态§12](SIM_Refactor_Execution_State.md#open-closed-followup-20261109)。

审视日期：2026-11-09。输入代码：`main@a7d3af8b8809513e7b8e73196a94551a8ecff97d`。本文为审视意见，不新增架构批准，不授权生产切换。

## 1. 执行摘要

**现有核心架构和修订 B+ 六组目录可以继续使用；本轮未证实必须重划业务上下文的 A 类缺陷。新增分析揭示的主要不足，是若干跨界协议没有具体落到迁移合同和有效测试。当前不能据此宣称具备完整替换旧 SIM 的准入条件。**

T 继续接纳账户、绑定、资料及 active；A 继续负责默认角色、Primary、分配和业务重试；O 是用例自有机制；R 负责卡与设备运行能力；C/I/H 承接兼容、技术与宿主。新增事实可以在这些责任内承接，无需新业务上下文、中央 Manager、新 SA 或第二份单例。三份权威设计没有定义当前的 M 上下文，本文不补造其含义。

最高优先级是 PIN 记忆的操作/身份/敏感载荷合同，以及迁移涉及的公共虚表和跨 SO 单实例证明。它们是**相关切片迁移前的 P0 门禁**，不表示已发现 PIN 泄露或现网 ABI 故障。文件扩展、账户订阅注销、厂商拓扑结果接纳、Provider 协议及进程内契约测试属于 P1 闭环。

本轮还发现三个需要先纠偏的事实：原报告与本仓不是同一源码基线；PIN 第二处记忆钩子实际属于 `UnlockPuk`；账户事件注册与注销的路由静态不对称。不能把原报告的函数名、行号、统计或“恒 null”等推断直接冻结为新实现的兼容标准。

测试防护网已经有可保留的真实状态/PIN/PUK组件场景、独立缺陷、突变和结构护栏，但未闭合完整账户/默认角色/持久化、多数进程内开闭源协作和产品 IPC 链。建议先核定产品及源码基线，补四项具体合同和最小行为断言，再按既有 WU/G2/G3 门禁迁移；外部依赖只阻塞实际依赖它的切片。

## 2. 输入基线与证据说明

### 2.1 输入及权威关系

所有下文源码路径均相对 `telephony_core_service/`；源码行号固定于上述 HEAD，不沿用原报告的坐标。文内 S/T 编号仅为本报告证据索引，不替代 SIM、WU、SIM-OC 编号或唯一执行状态。

| 输入 | 本轮读取范围、地位与限制 |
| --- | --- |
| [目标主设计 v1.2](SIM_Target_Architecture_Design_v1.2.md) | 定向阅读身份、§4～6、§7～17、§18～22；业务边界和不变量的上位依据，整体仍有 PROPOSED/pending 内容 |
| [源码详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md) | 读取当前/稳态树、分层、接口、生产映射、WU 和缺口；维护物理组织，不以目录存在推定生产实现 |
| [功能/WU 清单](SIM_Architecture_Refactor_Design.md) | 读取上下文、功能映射、完整写权单元、兼容与迁移章节；其开头明确服从主设计 |
| [新增边界提炼](SIM_开闭源架构边界与重构决策依据.md) | **完整阅读 372 行**；现状分析输入，不是目标设计批准。SHA256：`3e9fff66d241458bf069217ccf99ea296434f5e36ff0b4b2ea5aecbe7d086397` |
| [原始详细报告](../SIM%20开闭源功能边界分析报告.md) | **完整阅读 596 行**；工作区原有未跟踪文件，仅作证据索引，未修改或提交。SHA256：`de66843f129444835c39ab84f3b4a5700e75405f12b57b38b1c2e1a9a58d006e` |
| `SIM_Open_Closed_Source_Functional_Boundary_Analysis.md` | 当前工作区未找到该英文名文件；已读中文原报告，不假定两者是额外的独立证据 |
| [执行状态](SIM_Refactor_Execution_State.md) | 读取恢复、§0、§8～11及门禁；历史 PASS 按原输入和范围解释，本报告不改写该台账 |
| [防护网 v1.0](../services/multisim/test/docs/SIM_测试防护网建设设计说明书_v1.0.md)、[v1.1](../services/multisim/test/docs/SIM_测试防护网建设设计说明书_v1.1_融合系统测试用例分析.md) | 两者均为 Proposed；v1.1 是在 v1.0 通用边界上增加 nezha 场景分析，不自动使示例接口、产品策略或未实现测试成为已批准基线 |
| [接口全景与建议](test/SIM卡管理_接口全景与架构决策建议.md)、测试工程 | 结合真实接口/用例检查；既有单测、组件防护网、架构护栏分别评价 |

原报告标注开源提交 `5de9cebe5392bbe49055dd74bad2a418a5d64854`，本地 Git 无该对象；vendor 提交为 `9652c091b6ae1826404744d9ae9909ff2fef71ba`，本轮不能核实。工作区未发现 vendor `telephony_ext/services/sim`、`simollie`、`4011.json`、实际 vendor versionscript 或目标 ELF。其私有算法、shared 持有侧、旁路请求、调用全集及设备效果均仅有报告线索。没有以闭源不可见为由停止开源审视。

系统测试团队的 nezha 392 条资产在当前可定位材料中由 v1.1 转述；未找到独立原始 xlsx 或该二次分析报告全文。其统计、三待策略和时延不能作为本轮独立复核结果。存在 v1.16/v1.17 等其他设计文件不改变用户指定的三份权威依据。

### 2.2 Git、运行证据与本轮边界

起始 Git 根为 `/mnt/e/github/multisim`；分支 main；origin 为 `https://github.com/zjuluowu/multisim.git`，upstream 为 OpenHarmony 部件仓。本轮 fetch 后 HEAD 与 origin/main 差异为 0/0；暂存区空，原有工作区修改仅上述中文原报告。已核对所有 worktree，未切换、合并或清理它们。本轮仅新增本文，不更新已有执行状态，遵守用户“仅生成一份报告”的范围限制。

| 验证 | 本轮结论及实际范围 |
| --- | --- |
| `python3 telephony_core_service/services/multisim/tools/check_sim_refactor_recovery.py --remote origin --fetch` | exit 0，CHECKPOINT_VERIFIED；核对历史检查点与源摘要，没有运行业务。历史 G2a 分支 tip 为 `728ad77e`，main 不含其祖先与 B+ 已选择性迁入业务是不同命题 |
| `python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py -v` | 本轮 14 项 PASS；源表及执行审计设施测试，不是 14 个业务场景 |
| `python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/sim-cross-review-20261109-architecture` | exit 0：88单元/157 include/25源集结构、73原文件迁移、88项布局、12护栏、88独立单元及合并头语法、topology/resource/assignment/runtime_protocol四套纯规则、GN/Ninja全部骨架对象PASS。无生产链接或启动 |
| `python3 -B` 导入 `services/multisim/test/sim_guard/run_sim_guard.py` 调用 `check_semantics()` | exit 0：当前9份冻结路径、v2资产/旧测试块及include边界校验PASS；不运行业务，不证明历史运行环境被重新复现 |
| SIM Guard、G2a 七业务场景、完整初始化、OHOS 产品/厂版/设备 | 本轮未重新执行。第 7 节历史数字来自仓内证据，不能标为本轮 PASS；当前未找到 `hb` 命令 |

文档校验通过：12节完整、两张矩阵各16条无遗漏、70个本地链接目标存在、UTF-8/表格边界/行尾检查通过，新增文件的 `git diff --no-index --check` 通过。已有跟踪文件和暂存区均无差异；原始未跟踪报告摘要保持不变。

### 2.3 关键源码证据索引

| 索引 | 当前已检查的源码与坐标 | 能证明及不能证明的内容 |
| --- | --- | --- |
| S01 | [Core 初始化](../services/core/src/core_service.cpp)：92～125、146～159；[4010 profile](../sa_profile/4010.json) | wrapper 早于 Manager 就绪；原停止序列可读。不能证明完整启动已运行或设备可卸载 |
| S02 | [CMI 实现](../frameworks/native/src/core_manager_inner.cpp)：34～46；[API GN](../interfaces/innerkits/BUILD.gn)：65～81；[API 导出](../interfaces/innerkits/libtel_core_service_api.versionscript)：22 | 单例及当前产品定义归属；同进程本身不足以证明实际装载无副本 |
| S03 | [真实 wrapper](../services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp)：46～63、186～188、294、408～450；[头](../services/telephony_ext_wrapper/include/telephony_ext_wrapper.h) | 当前查找串、签名及各自缺符号返回；不证明 vendor 的实现和导出 |
| S04 | [IIccFileExt](../interfaces/innerkits/include/i_icc_file.h)：24～104；[IccFile](../services/sim/src/icc_file.cpp)：835～839、873～906、918～924 | 公共虚接口、开源 weak 侧、原生 IO 与 Opkey 广播；不证明闭源 shared 侧安全 |
| S05 | [文件创建](../services/sim/src/sim_file_manager.cpp)：143～148；[SimFile](../services/sim/src/sim_file.cpp)：384～403 | controller/slot/config/Init 后创建扩展；缓存分支开源接缝，不是实际 booster PASS |
| S06 | [CMI 路由](../frameworks/native/src/core_manager_inner.cpp)：140～201；[SIM 路由](../services/sim/src/sim_manager.cpp)：1192～1251；[Monitor](../services/sim/src/multi_sim_monitor.cpp)：380～395 | 账户注册/注销静态不对称；是否残留通知须动态验证 |
| S07 | [RadioEvent](../interfaces/innerkits/include/radio_event.h)：183～210、262～275；[安全枚举](../interfaces/innerkits/include/sim_state_type.h)：290～295 | 当前事件和操作枚举；不把原报告不存在于本仓的 mapping 枚举视为当前 ABI |
| S08 | [安全操作](../services/sim/src/sim_state_manager.cpp)：197～364；[拔卡](../services/sim/src/sim_state_handle.cpp)：448～456 | PIN/PUK/改 PIN/锁开关/拔卡投递节点与参数；未证明迟到时身份正确或 asset 行为 |
| S09 | [多卡控制](../services/sim/src/multi_sim_controller.cpp)：459～462、502～514、1034～1037、2243～2257 | GetResetActiveFlag、换路、激活 Hook、本仓写 IS_ACTIVE；不证明厂商直接写同表 |
| S10 | [身份映射](../services/sim/src/sim_manager.cpp)：622～652；[LSI 消费](../services/sim/src/multi_sim_helper.cpp)：209～225；[Radio 协议](../services/sim/src/radio_protocol_controller.cpp)：35～80、103～121 | 厂商优先/回退、标签与双槽限制；不证明全域双射或三/四卡支持 |
| S11 | [STK](../services/sim/src/stk_controller.cpp)：71～78、399～423；[STK Manager](../services/sim/src/stk_manager.cpp)：71～72；[CMI 回送](../frameworks/native/src/core_manager_inner.cpp)：2534～2549 | BIP 初始化/转发和 TR/Envelope 回注；闭源协议及 socket/APN 未运行 |
| S12 | [Loader 写库](../services/sim/src/operator_config_loader.cpp)：102～137；[Detect 写库](../services/sim/src/card_file_detect_manager.cpp)：98～139 | 真实独立旧写入口；完整 WU 须接管，不只是公开 setter |
| S13 | [multisim GN](../services/multisim/BUILD.gn)；[根 GN](../BUILD.gn)：67～75、134～147、207～214；[Feature](../telephony_core_service.gni) | 25 个 testonly 源集与根产品显式 query/binding；不是 25 个产品业务模块 |
| S14 | [宿主端口](../services/multisim/integration/lifecycle/host_ports.h)：30～45；[工厂](../services/multisim/integration/lifecycle/sim_module_factory.h)；[运行接缝](../services/multisim/integration/lifecycle/card_runtime.h)：98～108 | 共享 RIL、发布与声明边界；CardRuntime 仍有 R 状态，工厂声明不等于最终替换实现 |
| S15 | [PIN 请求](../services/multisim/card/security/pin_requests.h)：26；[EF 请求](../services/multisim/card/records/ef_requests.h)：26；[映射入口](../services/multisim/account/topology_service.h)：75；[Radio 端口](../services/multisim/card_selection/primary/radio_execution_port.h)：27～61 | 当前窄骨架，不是完整安全、文件或厂商拓扑实现 |

### 2.4 必须纠正后再使用的原报告结论

| 原报告主张 | 当前交叉核实 | 本轮判断 |
| --- | --- | --- |
| `RetrieveSimActiveState` 在 MEP 门控内回写 active | S09 是 `GetResetActiveFlag(slotId,isActive)` 后本仓 UpdateDB，无该门控；S03 查找同名当前符号 | F：双基线差异，OC-014 保留业务意义，重建当前方法映射 |
| `SwapTwoModemSimCards`、`PublishMultiCardChange`、`RADIO_MULTICARD_MAPPING_BEGIN`、multisim 消费 `*Link` | 在当前 services/frameworks/interfaces 定向检索未命中上述方法/枚举及所述 PIN `*Link` 消费；`SwapM0M2SimCards` 存在 | 不据此新建同名接口；须取得匹配产品的头、消费者及版本 |
| 第二个 PIN 记忆触点是 UnlockPin2 | S08：237～275 为 UnlockPuk，263～265 投递 `PIN_ALTER/newPin` | 当前冻结矩阵必须含 PUK，不能制造“PIN2 成功必须记忆”的假规则 |
| `SetActiveSimFunc` 属冲突路径/可表示完成 | S09：普通激活成功后调用；S03：void | 保留具体分支时点，不能将返回当设备结束 |
| 注册与注销路由同构 | S06：CMI 注册范围含 ACCOUNT_LOADED，注销仅至 RECORDS_LOADED；SIM 注销也无账户分支 | E：静态不对称已证；动态影响未证，不能长期固化为正确兼容行为 |
| 85 个 Hook 统一判空跳过、无必需返回 | S03：GetRealSimCount 返回输入；GetResetActiveFlag 返回 false；Swap 返回 -1 | 逐符号保持返回与副作用，不用统一 fallback 改变旧行为；数量不作为覆盖率 |
| 同进程消除了单例副本风险；weak/shared 推出无环 | S02 仅给当前定义归属；S04 只核实开源 weak 侧 | 仍需 L2 的真实 SO/控制块/销毁证明 |
| vendor 脚本缺导出即“恒 null、产品断链已实证” | 本轮只可见开源 dlsym 与判空，vendor 脚本/ELF不可见 | 四步证据必须分开：脚本疑点→目标 ELF→运行解析→启用消费者业务影响 |

## 3. 业务能力与目标架构覆盖矩阵

“可承接”表示设计责任明确，不表示成熟代码已迁入。表中 V-SIM/V-TOPO/V-ASSET 指报告中的闭源参与者，与目标 A/C 代号无关。旧内部类仅用于事实定位，不是未来必须保留的组织。

| 现有功能/稳定范围 | 开闭源真实职责与协作 | SIM-OC | 目标承载及对外事实 | 当前支持、扩展与缺口 |
| --- | --- | --- | --- | --- |
| SIM 状态/锁因，SIM-005～011 | StateManager/Handle 执行；热插拔等 Hook 对外，闭源扩展细节仅报告 | 006/010/011/013 | R `card/state` 原始卡态，T 接纳绑定，C 输出 | SessionRegistry 有纯规则，生产卡态未迁入；真实 Legacy 场景存在，迟到缺陷仍在 |
| EF/Records/Refresh，SIM-012～019 | File/Controller 真 IO；V-SIM booster 缓存与回退 | 007/008 | R `card/records`、LoadSession，既有 IIccFileExt 外部虚接口 | 已有 EF 请求和加载规则；缺双向对象、请求关联、缓存失效/回退合同 |
| 运营商配置/Opkey，SIM-041～045 | 开源 Loader/Tracker，外部版本/roaming broker/Opkey 回调 | 003/009/010 | R `card/carrier`；账户 OPKEY/MCC/MNC/IMSI 经 T；I/C 原通知 | 详细设计已覆盖 Loader 独立写点；缺具名 Opkey 可见性/回调测试 |
| 账户及持久化，SIM-020～023/055～056 | Controller/SimRdbHelper 物理写库，厂商标志是决策输入 | 006/014/015 | T account；角色字段由 A 提供；技术 CRUD 不获跨 Owner 写权 | 设计明确、骨架有限；匹配 Provider 缺口阻塞完整初始化/恢复，不证实闭源直接双写 |
| PIN/PUK/PIN2/锁/认证，SIM-005～011 | RIL 真应答，StateManager 决定公开结果 | 006/013 | R `card/security` 具名短请求；原安全 API/错误/次数由 C 保持 | VerifyPinRequest 仅一个契约；PUK/锁/记忆需成熟迁移，不要求先有账户 |
| PIN 记忆，SIM-057 与安全子项 | 开源成功/拔卡节点传 slot/ICCID/操作/PIN；asset 保管与自动恢复在 V-ASSET | 013 | R 安全结果，I 原 ABI 投递，身份关联与清理由原用例负责；资产不迁入 T | 新增分析使安全合同更具体；无专用载荷和迟到身份强断言，P0 门禁 |
| STK 与 BIP，SIM-036～040 | STK 在开源，BIP/APN/socket 专有逻辑在 V-SIM，TR/Envelope 经 CMI 回送 | 006/012 | R `card/stk`；I 扩展能力；C 保持公开命令 | 目录计划可承接，未迁入；旧测试有分支/IPC输入，未证完整请求应答闭环 |
| 基础单/双/多卡、主副卡，SIM-025～031 | 开源默认角色/Primary/Radio，厂商参与换路 | 003/014/015 | T 绑定；A 默认角色/Primary/重试；R device 执行 | 模型可表达多端点；当前协议至多双槽，N 槽产品策略非已实现 |
| 卡启停，SIM-024 | 开源普通/force/卫星分支，成功后厂商 Hook | 006/014 | T active/激活进度；A 冲突资格和复核；R 执行 | 各分支不同错误和部分效果须分别差分，void Hook 不作结束证据 |
| eSIM Profile 卡侧，SIM-047～054 | Manager/File/Controller 本地原语；66250 高层任务 | 006/010/016 | R `card/esim` 库存/协议；T 已接纳绑定；A 偏好 | 有本地请求骨架，APDU/Profile真实业务未闭合；库存≠绑定，下载不迁中央 O |
| eSIM 电路/拓扑，SIM-031/057 | V-TOPO 的 LSI/SCI/换路策略来自报告；开源消费查询/事件/结果 | 003/011/014/015 | T 映射子用例，A 父切换/准入，R 原始路由/资源，I vendor 执行 | vendorMappingRequired/BeginRouteMapping 只是接缝；缺正式结果接纳和设备结束协议 |
| VSIM，SIM-057～060 | wrapper/DB/状态特殊分支；部分符号在两目标闭源目录之外的 vsim_service | 003/015/016 | H 槽种类/允许路由；T 绑定；A 策略；C 特殊整数与权限投影 | 不把 VSIM 2 当第三实体槽；闭源范围未覆盖全集，回退/无效值需测试 |
| 事件/注册/注销，SIM-032～035 | CMI→SIM→File/State/Monitor/vendor；有首帧与外部消费者 | 002/010/011 | C 登记，I token/技术重连，Owner 原节点发布事实 | 有明确职责，缺有效生命周期断言；账户注销静态问题见第 4 节 |
| 插拔/异常/重启恢复，WU-01/06/09 | RIL/卡状态驱动，Monitor重载及厂商重启检测参与 | 002/011/014 | R 会话，T 身份/持久化，A 角色复核，H 生命周期 | 组件重建已测不等于 DB/进程重启；Provider/迟到/排空分别验收 |
| slot/sim/label/Port 映射，SIM-004/020/031 | Manager 厂商优先映射与原路径回退，LSI 查询；Port观察证据不足 | 014/015 | support 纯身份，H 静态目录，T 动态绑定，A 分配，C 旧投影 | 不假设全域双射、slot=身份或请求 portIndex=真实绑定；MEP 新能力 G4 |
| IoT 卡检测，SIM-056/057 | 开源 Detect 有独立 Insert/Update；私有 HTTP/通知增强在 V-SIM | 003/006/016 | R 探测，T 字段接纳，I 扩展 | 已列 WU，需接管独立写点，不能只迁 Controller setter |
| Voicemail、功耗、CHR、分布协同，SIM-057～060及相关卡能力 | voicemail 转发、质量/功耗/协同存在私有参与者 | 003/006/016 | voicemail 随 records，技术输出 I/H，业务事实仍 T/A/R | 按真实 Feature/消费者细分；GetRealSimCount 通过不代表这些能力通过 |
| 卡短信、电话簿、编解码，SIM-036～045 | 开源成熟卡能力；工具可能由多模块消费 | 006/007/016 | `card/sms_storage`、`phonebook`；records_load→adn_io→file_io | 计划归属完整；不整体搬进 utils，不复制算法或公共 DTO 以消除环 |

## 4. 目标架构设计审视

### 4.1 核心上下文、写权与查询

主设计 §4.1/4.2（214～268 行）已经明确 T/A 两个业务上下文。T 拥有账户身份、资料、已接受绑定和激活进度；A 拥有角色/Primary/分配与切换进度。O 不提供独立业务状态查询；R 原始状态、T 接纳后的绑定和 A 的决策视图可以分别存在，语义不同不构成重复事实源。闭源不能通过可写聚合引用或技术 Store 获得旁路写权。

当前 `GetResetActiveFlag→UpdateDBActiveByIccId→SimRdbHelper`（S09）证明厂商提供决策输入、开源执行物理写入。它不能证明 vendor 直接写 SIM 表。真正已证的旧独立写入口包括 Loader/Detect（S12）；这两类已被详细设计 §9.1/9.2、§17.2 纳入 WU，属于既有迁移要求，不应报告为本轮首次发现的 A 类缺陷。APN DataShare 与 asset 的外仓责任也不能硬并入 T。

兼容查询应由 C 选择原读源，经 T/A/R 具名查询承接。号码查询回写按 T 独立用例管理，不让 C 或 O 持第二份账户缓存。现有 G2a 查询实际仍读原 Manager/EXT，仅证明只读桥，不证明查询全面改由新领域事实提供。

### 4.2 三份权威文档之间的不一致

| 位置 | 与上位定义的关系 | 最小处理与是否裁决 |
| --- | --- | --- |
| 功能清单 §4.1，第219行 T“配置清单” | 主设计 T 不拥有槽配置，H 持只读 SlotCatalog；可能混淆配置与运行清单 | B/P2 描述消歧：改为“依据 H 目录维护运行清单及已接纳绑定”；不重新裁决 Owner |
| 功能清单 SIM-023.b，第497行“O 编排一次修改”、旧 application/operation 路径 | 同文开头及 §5.2.1 已服从主设计；主设计 O 非业务 Owner，详细设计号码归 account/number | B/P2 历史称谓残留：写明 T 号码用例使用 O 机制，并指向当前路径 |
| 主设计 §19 保留早期测试基线，执行状态 §9～11已有后续结果 | 是有日期/基线的历史范围，不是架构原则冲突 | 用执行状态检索最新执行证据；可补导航，不覆盖旧数字或把其视为当前重跑 |
| 详细设计当前树 vs 稳态树、CardRuntime 暂位 | 设计已显式说明计划与过渡，逻辑职责未变 | 不列新原则冲突，不为目录整齐立即搬迁 |

本轮没有证实尚需重新裁决的 T/A 原则性冲突。PIN 安全、厂商结果、外部 ABI 的具体协议仍需 Owner 决定，见第 10 节；模型建议不自动成为批准。

### 4.3 多卡演进和数据模型

主设计 §3.3、§17.1 已区分位置、身份、端点类型、允许路由、Radio 域及调用者可见性。H 的只读 SlotCatalog 不能保存 Swap 后的当前路由；T 接纳绑定，A 接纳用于决策的分配观察，R 维护实际协议观察。两份模型都使用 vector 不能证明 3/4 卡或 MEP 可用：S10 当前 RadioProtocolController 把参与槽数限制至双槽，并对越界/VSIM槽返回 0。

| 演进场景 | 现有架构如何承接 | 继续实施前的具体证据 |
| --- | --- | --- |
| 双卡→第三实体卡，保留 VSIM 索引2 | H 容量/种类，T 标签/绑定，A 换路策略，C 旧权限投影，R vendor执行 | 稀疏槽0/1/3与VSIM2分开；同一旧入口容量值/可见值等值差分 |
| 四个端点、仅两套 Radio 栈 | T 候选身份，A 显式目标分配与冲突集合，R 真实换路 | 已批准产品配额/默认角色/非目标不变；真实参与集合与失败回读，不能只扩数组 |
| PSIM+eSIM、一个/多个 Port | R Profile库存与Port观察→T接纳位置绑定，A角色 | 产品 eUICC/Port→逻辑槽契约；缺标签、失败请求后读回、一对一和多对一分别核实 |
| 主卡/默认卡与启停交错 | A父请求、T映射子步骤；T active；A原子冲突资格 | 父子关联、无嵌套等待、不重复申请父资格；只释放本请求资格，不等同设备租约释放 |
| 热插拔/重启及未知执行结果 | R会话/观察，T绑定/恢复，A复核；按WU清旧writer | 有状态持久化失败/重启及迟到回包；Unknown后的查询与恢复证据，不盲目重放 |

基础多卡管理与 vendor 专有电路编排保持分责。T 不接管 LSI/SCI 私有算法，A 不成为全部拓扑事实的 Owner，R 不取得账户写权。若外部旁路直接发 RIL 请求，本仓 ResourceBarrier 只约束受控路径；全产品共享域安全必须有共同准入/结束协议，不能从本仓骨架推出。

### 4.4 生命周期、状态机、事件和扩展协议

S01 显示 wrapper 加载早于 SIM/CMI 全部就绪；符号存在不代表依赖已就绪。S02 显示 CMI 定义在既有 API 库；后续拆 SO 必须保持唯一定义、原 shared_ptr 控制块与匹配销毁。Stop、等待超时、channelId 清零或 void 函数返回均不证明设备停止，也不授予卸载资格。CardRuntime 的 R 接缝暂位已经登记，不应为兼容再造一个全局 Runtime。

需具体化的四项合同均能在现有边界补齐：

1. **文件双向扩展**：保持 IIccFileExt 虚表，R records 管理原 File/LoadSession；外部回调须能追溯原对象/会话/请求。不能为了带 epoch 改旧公共签名，也不能回包时用当前 slot 反填新代次。是否可借对象代次保留关联要用实际厂商行为证明。
2. **PIN 记忆**：冻结真实五类操作节点、成功/失败、ICCID取得点、重复迟到和拔卡清理。固定身份或抑制迟到投递若改变旧轨迹，应作为安全行为修正单独批准；敏感参数寿命、日志/证据脱敏需安全 Owner 签收。
3. **厂商拓扑结果**：明确入站命令、策略查询、有副作用执行、兼容输出各自方向；外部事实由 T/A 在原接纳点接收，R 结束/恢复证据另列。不把外部 bool、回调完成与物理稳定合并。
4. **必需/可选扩展**：按产品逐符号记录缺库、缺符号、失败、重入、卸载行为和实际消费者。历史判空不等于产品允许缺失；新增 fail-loud 也不能未经批准改变错误轨迹。

S04 的 `OnOpkeyLoad` 先 NotifyObserver，随后置 `isOnOpkeyLoaded_=true`。通知为同步还是排队、消费者究竟能看见什么需继续追踪并动态断言；不得写成“通知前所有状态必已提交”。应冻结载荷 `[slotId,opkey,opName]`、原节点及允许偏序。

账户事件是明确的旧实现疑点：CMI 注册 ACCOUNT_LOADED→SimManager→Monitor；CMI 注销范围不含该值，落 RIL；直接 SimManager 注销也无账户分支。已证实路由不对称，尚未证实泄漏、UAF或设备故障。先增加“注册→首帧→注销→再次变化”的可执行复现，修复独立裁决，不把该不对称固化为永久兼容要求。

## 5. 代码目录与模块边界审视

### 5.1 当前树与 GN

当前 `services/multisim` 有90个文件：64头、24cpp、BUILD及README。六组物理目录与25个逻辑 source_set 是不同维度；25个 source_set 均 testonly。根产品显式编入 query/binding，同时仍编成熟旧SIM源；不能把 testonly 骨架视为生产替换已完成。`esim_manager.cpp` 在根 GN 第68行无条件列入，只有 Controller/File等在141～147行受 Feature控制，不能按整个 card/esim 目录一刀排除。

| 目录/逻辑边界 | 当前职责及层次 | 新事实落点和最小调整 |
| --- | --- | --- |
| account / topology_* | T纯模型、类型、应用入口/用例及Owner专属Adapter分target | active、Loader/Detect写回及厂商映射由T接纳；补映射合同，不重划T |
| card_selection / assignment_* | A角色/Primary纯规则、应用、窄执行和输出端口 | 外部拓扑父用例/冲突资格仍A；不把设备租约搬入A |
| card/state、security、records、esim / runtime_* | R会话、加载规则和短请求骨架 | IIccFileExt随records；PIN记忆触发随security/state；成熟实现迁入时补合同 |
| device/resource，未来device/radio | 真实资源纯规则；成熟Radio仍旧目录 | 共享域/真实结束的端口必须有设备证据；不预建device/session或DeviceManager |
| integration/api、events | C入口、只读桥、完成/订阅翻译 | 公开CMI/ISimManager保持，内部调用具名T/A/R应用入口；不复制每个旧Hook为新Service |
| integration/lifecycle、exports | H装配及过渡R接缝、精确仓内导出 | 原wrapper共享定义经窄端口使用；工厂/销毁与CardRuntime薄化分别验收 |
| support、support/request | 纯身份/目录值、请求协议与各Owner自有机制 | 不存动态绑定/路由/账户状态，不变成公共DTO池或中央协调器 |

详细设计 §4 已约束 `*_cases→本Owner domain/窄contracts`、`domain→纯types/values`，禁止 T/A 包含对方Service/聚合/Adapter、R反依具体业务用例；H 是必要组合根。同一物理目录有多个target是合理的。Records/Phonebook应保持 `records_load→adn_io→file_io` 和 `phonebook_cases→adn_io/file_io`；carrier消费快照/窄配置端口，避免整包互依。公共枚举和既有头维持单一定义。

### 5.2 目录级修改判定

| 当前位置 | 新需求/缺口 | 建议 | 架构影响与更小替代 |
| --- | --- | --- | --- |
| card/records/ef_requests、load_session | 未表述缓存与原IO双路径、外部File对象/回调寿命 | 先在详细设计§9/10写具名协议；迁成熟File时接原IIccFileExt | 不改虚表/公共路径；先用真实旧File+脚本扩展测试，无需立即新目录 |
| card/security/pin_requests | 只有VerifyPinRequest，不代表PUK/锁/记忆能力齐备 | 先补真实操作矩阵，按安全切片补请求和窄Hook | 仍R能力；不用T账户或每Hook一个Adapter |
| account映射、card_selection/primary、device | BeginRouteMapping仅声明、vendor标志不足以定义整链 | 补父子结果、T接纳、A准入、R结束合同后实现 | 保持已定Owner，无需RoutingManager或厂商中央桥 |
| integration/lifecycle/CardRuntime | 仍持sessions/resources/loads等R状态 | 按既有计划随成熟能力薄化，迁移时保留控制块/epoch | 已知过渡，不是本轮必须搬目录的缺陷；先保持原接缝 |
| 计划card/stk/carrier/phonebook | 真实成熟能力尚未迁入 | 迁入相应能力时再创建目录和精确target | 不空建全套目录，不将BIP/asset私有逻辑搬入 |

本轮未发现新增闭源需求已经导致当前骨架新include环或新生产双写。该判断限定于已存在骨架；未迁入的成熟路径不能靠骨架DAG推定未来也无环。

## 6. 开闭源兼容性契约审视

状态指“设计责任＋具名合同＋当前落实”的综合程度；“部分覆盖”绝不等于已验收。所有16条均保留编号，原报告不准确的具体实现描述按第2.4节修正。

| SIM-OC | 当前证据及外部约束性质 | 设计覆盖/承载 | 接口、Adapter及缺口 | 厂版验证 |
| --- | --- | --- | --- | --- |
| 001 | S01/4010及S03装载可证；4011同进程/depend/双用途仅原报告 | 部分覆盖：主设计§7，H/API库 | 维持现有SA/进程意图；补4011双用途合同，无新Facade | L2核对匹配profile/实际装载，不能仅看本仓4010 |
| 002 | S01早期窗口/停止顺序，是初始化行为约束 | 部分覆盖：H lifecycle、主设计§14 | 既有wrapper+原发布次序，缺完整就绪/失败/重入/排空断言 | L2首访与重建，L3退出/恢复 |
| 003 | S03查找串、typedef、各自fallback，稳定动态ABI | 部分覆盖：I/H共享wrapper，§17 | 内部窄执行/查询/输出端口按能力；缺当前产品逐符号必需/可选清单 | L2真实加载与消费者；L1只验证协议 |
| 004 | vendor脚本/ELF不可见；原报告“断链”不能确证 | 待验证：G2b/ABI工程门禁 | 保留现有两个eSIM查找；不要凭疑点删链或补另一套实现 | L2脚本→ELF→dlsym→消费者，影响确定后定级 |
| 005 | S02单例/注入/库归属，外部对象共享约束 | 部分覆盖：主设计§7/14、S14 | 既有CMI不复制；H注入原接口/控制块；缺真实不同SO地址与寿命证明 | L2动态符号/NEEDED/控制块/匹配销毁 |
| 006 | S02/06/08/11公开方法委托；vendor消费全集不可复扫 | 部分覆盖：C→T/A/R具名应用 | 保持CMI及ISimManager等签名/布局/错误/线程；LegacyFacade少量方法非完整替身 | L1调用方替身＋真实业务；L2实际vendor方法集 |
| 007 | S04/05虚表、枚举、weak、原IO；booster算法仅报告 | 部分覆盖：R records、C/I保ABI | 保留IIccFileExt，内部关联原Session/LoadSession；双向对象/缓存/回退合同未完整具名 | L2虚表/CFI/闭源shared侧，L3真实booster |
| 008 | S05配置/Init后CreateExt顺序可证 | 部分覆盖：R文件生命周期＋H | 保留必要前置就绪，类名与具体构造链可适配；缺扩展缺失/重建/销毁测试 | L2厂版对象回注/释放，L3切卡在途 |
| 009 | S04载荷及Notify后置flag，行为/偏序约束 | 部分覆盖：R记录、carrier消费、T字段 | C/I保原输出，不能统一延后通知；缺可见性/载荷/旧会话回调断言 | L2真实消费；L3运营商配置 |
| 010 | S07现有数值/区间可证，报告mapping项本仓缺失 | 部分覆盖：外部枚举单定义、C/I事件翻译 | 不改现有事件ABI；补枚举/载荷快照及Feature集合，不复制新事件池 | L2消费方编译/载荷，L3真实乱序 |
| 011 | S06路由可证且账户注销不对称 | 部分覆盖＋旧实现E项：C登记/I token/Owner出口 | 缺范围边界、首帧、注销/重建合同；不新增总线，先复现再决定修复 | L1公开门面强断言；L2闭源订阅与卸载 |
| 012 | S11 InitBip/SendEvent/TR/Envelope可证 | 部分覆盖：R card/stk＋I，尚未迁入 | 原公开CMI入口保留；BIP专有协议在vendor；缺完整往返与错误/超时合同 | L2真实BIP消费，L3socket/APN/Modem |
| 013 | S08真实五类节点及明文跨界可证 | 部分覆盖：R security/state＋I，安全合同未闭环 | 不新增资产Owner；冻结操作/身份/载荷/时点/敏感寿命，PIN2线索先纠偏 | L1脱敏Spy；L2/3真实asset与自动解锁 |
| 014 | S09当前GetResetActiveFlag/Swap/SetActiveHook；专有完整拓扑仅报告 | 部分覆盖：T映射/active、A切换、R执行、I厂商 | 禁止复制旧混合Controller；缺外部旁路、结果接纳、结束/恢复和失败轨迹 | L2厂商交互/Provider；L3TSTS/电路/重启 |
| 015 | S10和wrapper特殊值，不能推出全域双射 | 部分覆盖：H目录/T绑定/A分配/C投影 | 原整数/无效值/EXT优先回退保留；缺LSI读回/Port正式协议 | L1稀疏槽/无效映射；L2真实vendor；L3硬件路由 |
| 016 | S13与gni的真实Feature/源/依赖可证 | 部分覆盖：精确target/导出，G2b门禁 | 保留既有API/utils消费名；esim_manager非整包排除；缺公/厂版产物矩阵 | L2构建/安装/CFI/PAC/LTO及ABI，L3启动 |

稳定公共契约包括现有CMI/虚接口签名、已发布整数值和动态符号签名；行为契约如文件创建就绪、缓存回退、Opkey载荷和PIN触发可经现有兼容边界承接。旧具体Manager数量、内部链路、插桩行号可改变。未匹配的 `*Link` 消费、第三卡/VSIM产品映射、外部RIL结束保证属于待协商合同；变更虚表、进程、存储schema或安全政策需专项裁决。不能用“兼容”要求长期保留旧sim整套实现。

## 7. 测试防护网审视

### 7.1 测试边界与验收等级不能混用

三类测试边界与 L1/L2/L3 是两个维度。本文沿用本次新增分析的验收口径：L1开源验证、L2闭源集成、L3设备业务。防护网 v1.1 §2.2 的旧“L2”指真实 IPC/Inner API，可能在开源平台即可执行；因此下文称“IPC契约测试”，不把它自动视为闭源集成通过。

| 测试边界 | 应保留的真实SUT和输入 | 必要断言 | 验收限制 |
| --- | --- | --- | --- |
| 外部IPC黑盒契约 | 真实Client/Proxy/Stub、权限与调用者上下文 | 序列化、校验/权限错误顺序、返回/回调、服务死亡/重连、可见状态 | 本地直接调用Manager不能替代；本轮无实际IPC验收 |
| 进程内/跨SO契约 | 真实CMI/ISimManager路由、wrapper、File及事件；仅外部参与者可替身 | 初始未就绪、实际状态变更、对象控制块、输出时点、重入/注销 | Fake动态库不能证明实际vendor ELF/CFI，L2仍需真实消费方 |
| 系统级业务场景 | Contract Driver→真实SIM；有状态RIL/依赖失败调度 | 状态/身份/账户/角色/通知/外部副作用及非目标不变 | UI/真实射频/电路/asset/BIP仍L3；不能全降成私有类单测 |

### 7.2 当前资产、断言质量与执行基线

| 证据 | 现有能力与本轮评价 |
| --- | --- |
| T01 [sim_guard README](../services/multisim/test/sim_guard/README.md)、[旧证据](../services/multisim/test/sim_guard/evidence.json)、[第二轮证据](../services/multisim/test/sim_guard/evidence_round2.json) | 真实Legacy State/PIN/PUK、正式RIL输入、状态/卡型/解锁结果与新增通知。历史17条×5轮=85 PASS；不是全模块或IPC覆盖 |
| T02 [Legacy Adapter](../services/multisim/test/sim_guard/adapters/legacy/legacy_sim_adapter.cpp)、[Fake RIL](../services/multisim/test/sim_guard/fakes/ril_device.h)、[Oracle](../services/multisim/test/sim_guard/oracle/card_oracle.h) | Fake保存设备事实与可调度应答，真实SIM决定公开状态。CMI虽真链接，SIM manager路由未装配；SA不可用，Registry/CES为Spy，无假DataShare/LPA业务成功 |
| T03 [runner](../services/multisim/test/sim_guard/run_sim_guard.py)、[v2冻结](../services/multisim/test/sim_guard/semantic_baseline_v2.json)、[当前冻结](../services/multisim/test/sim_guard/semantic_baseline.json) | 旧七条、共同场景/Fake/Oracle受摘要保护；历史19项设施PASS与业务统计分列。有非零失败、零例/跳过拒绝及仓外READY→LOCKED突变。multisim/target分支明确BLOCKED，不回落legacy |
| T04 [Provider记录](../services/multisim/test/sim_guard/traceability/provider_contract_round2.json)、[场景增量](../services/multisim/test/sim_guard/traceability/round2_catalog.json) | 14个SimData字段未匹配，Provider的URI/schema/错误协议仍待核实闭合；完整初始化/账户/默认角色/DB恢复未完成。硬件重枚举≠持久化恢复 |
| T05 [systemtest](../services/multisim/test/systemtest/README.md)、[G2a用例](../services/multisim/test/systemtest/tests/g2a_query_test.cpp)、[冻结证据](../services/multisim/test/systemtest/configs/g2a_bplus_revalidation_20261105.json) | Core→Binding→Query→HostPort→原Manager/EXT七场景有历史真实运行证据；控制块/空绑定/厂商返回受检。Manager OnInit/Core Init未执行 |
| T06 [linux_sim_guard](../services/multisim/test/linux_sim_guard)、[架构规则](../services/multisim/test/multisim_architecture_rules/README.md) | 既有设施/独立GN与纯规则，不等于成熟SIM业务或新架构Adapter已可运行；source_migration保存历史追溯 |
| T07 [旧CMI单测](../test/unittest/sim_gtest/core_manager_inner_gtest.cpp)、[eSIM门面](../test/unittest/esim_gtest/core_manager_inner_test.cpp)、[Core旧测试](../test/unittest/core_service_gtest/)、[SIM用例](../test/unittest/sim_gtest/) | 存在进程内/IPC/分支测试，不能说“没有”；很多只断Mock返回、私有状态或不崩溃，执行状态未在本轮产品树验证 |

历史第二轮业务输入是 `01db5ce6` 加当时dirty工作树，相关摘要对应保存代码 `c508bd03`；不是最新a7d3af8b干净代码重跑。G2a干净main代码重验为 `d5f0527c`。设施通过数、兼容场景数和缺陷失败数分列，不能相加成覆盖率。

以下既有成果必须保留：原七条及新增十条兼容断言；五条独立缺陷/待裁决（迟到READY、换卡旧READY、RIL错误被当错PIN、旧PIN污染新卡、Refresh返回冲突）；READY→LOCKED突变检出及恢复；共同资产冻结、生产执行审计。历史独立缺陷5条×5轮=25 FAIL。Refresh正常场景仅断最终状态，成功码断言在DEFECT_005保留红灯，不能改成状态整数制造绿色。并非所有缺陷候选政策都已批准。

旧测试存在不等于断言充分。实际查到 [sim_test.cpp](../test/unittest/sim_gtest/sim_test.cpp) TR用例1216～1230、Envelope用例1281～1295在helper超时分支执行 `EXPECT_TRUE(true)`，无卡路径只记日志；[CMI单测](../test/unittest/sim_gtest/core_manager_inner_gtest.cpp) 266～278、879～906只验证Mock返回/空指针。[zero_branch_test2.cpp](../test/unittest/core_service_gtest/zero_branch_test2.cpp) 232～269调用OnOpkeyLoad后又直接改flag，最终只断false；272～293虽设置扩展/调用FileChangeToExt，最终断的是配置观察者weak引用存在。均不证明缓存回退、载荷偏序、销毁或实际闭源消费。应补关键断言，不重写全部旧测试。

### 7.3 SIM-OC逐项测试追踪矩阵

“现有用例”列除明确注明本轮运行的设施外，均指已读测试源码/仓内历史证据，不能推定当前产品执行通过。每行验收需同时记录SUT、Feature、输入基线、断言与实际执行。

| 契约ID | 业务场景/架构承载点 | 测试层级 | 现有用例/证据 | 缺失的关键断言 | 验证环境/优先级 |
| --- | --- | --- | --- | --- | --- |
| SIM-OC-001 | 同进程双用途装载；H/API | 进程内/产品 | Core启动相关旧测，S01/S13静态配置 | 实际4010/4011依赖、共享库实例/加载次数、失败启动 | L2真实产品profile/ELF，P1 |
| SIM-OC-002 | 提前加载、首次访问、重建；H | 进程内/场景 | T05顺序绑定/清理；T04初始化BLOCKED | wrapper已载而Manager未就绪、注册失败、完整发布/停止排空 | L1故障宿主＋L2启动，P1 |
| SIM-OC-003 | 缺库/缺符号/有效Hook；I/H | 进程内 | T05仅GetRealSimCount；旧wrapper Mock | 当前符号/typedef清单、每种fallback、重入/失败副作用与无重复Hook | L1脚本动态库，L2真实vendor，P1 |
| SIM-OC-004 | eSIM注册符号解析；I/H | 构建/进程内 | test/mock/telephony_ext_wrapper头第35行假句柄与静态lookup；T05仅受控SO的GetRealSimCount，非实际eSIM注册解析 | 查找集对实际导出集、两个符号解析、真实订阅效果 | L2目标ELF/消费者，P1；必需功能故障证实后重定级 |
| SIM-OC-005 | 共享单例/对象/销毁；H/API | 进程内/跨SO | T05原Manager/RIL控制块；旧CMI单测 | 两个真实SO的CMI地址/注入对象一致、唯一定义、匹配析构、在途阻止卸载 | L1局部探针＋L2产物，P0拆SO门禁 |
| SIM-OC-006 | CMI完整方法行为；C→T/A/R | 进程内/IPC | core_manager_inner_gtest、sim_core_service等；T02未装配CMI→SIM | 公共方法→真实状态/通知/副作用、未初始化错误、线程重入、真实消费方法集合 | L1公开门面真实SUT，L2厂商，P1 |
| SIM-OC-007 | 缓存命中/失效/回退；R records | 进程内/场景 | zero_branch_test2.cpp：124～130的IIccFileExtImpl、272～293的Expand004 | 命中不发多余IO；失败只按旧策略回退；原请求归属、过期结果不入新卡；双向寿命 | L1真实File＋脚本扩展，L2/3booster，P1 |
| SIM-OC-008 | 配置后创建/销毁；R/H | 进程内/场景 | SimFileManager/SimFile旧分支测试 | CreateExt时slot/controller/config可用，销毁中回调、weak失效、库在途寿命 | L1对象探针、L2真实shared侧，P1 |
| SIM-OC-009 | Opkey异步完成及配置可见；R/carrier/T | 进程内/场景 | zero_branch的OnOpkeyLoad、OperatorConfig旧测 | `[slot,opkey,opName]`与最终查询一致，Notify/flag原偏序，重复/旧加载回调 | L1观察者Spy，L2消费方、L3运营商，P1 |
| SIM-OC-010 | 事件数值/载荷/区间；C/I | 进程内/构建 | 状态Spy和旧事件测试只覆盖子集 | RadioEvent/FileChangeType/PinOperationType快照，区间端点、Feature事件集合 | L1快照/路由＋L2头/消费者，P1 |
| SIM-OC-011 | 注册→首帧→注销→再变化；C/Owner | 进程内/场景 | SimManager/CMI旧调用测试；T01状态通知 | ACCOUNT_LOADED注销效果、VSIM槽、重复注册、死亡清理、重建/在途注销 | L1真实CMI→Monitor→观察者，L2外部消费，P1 |
| SIM-OC-012 | STK→BIP→TR/Envelope；R/I | IPC/进程内/场景 | tel_stk_controller_test、sim_test中TR/Envelope入口；超时弱断言 | 对应请求收到正确TR/Envelope并抵达Fake RIL；失败/超时不伪成功、无重复发送 | L1脚本BIP参与者＋真实STK，L2/3真实BIP/APN，P1 |
| SIM-OC-013 | PIN/PUK/改PIN/锁/拔卡记忆；R安全/I | 进程内/场景 | T01 PIN/PUK；sim_core_service_test.cpp：375～395只在句柄有效时断Hook指针非空，未断载荷 | 五类操作的ICCID/slot/op/脱敏值、失败零Hook、迟到错卡、重复/拔卡清理；不持久化真实PIN | L1安全Spy，L2asset、L3自动解锁，P0安全迁移门禁 |
| SIM-OC-014 | 换路/启停→DB/角色→通知/恢复；T/A/R/I | 场景/进程内 | multi_sim_controller_gtest、T01双槽交错；无完整拓扑证据 | 厂商标志和DB真实写回、Swap失败/部分效果、非目标不变、结束不明后第二请求、重启读回 | L1匹配Provider＋脚本vendor，L2/3真实拓扑，P1 |
| SIM-OC-015 | 稀疏slot/sim/label/VSIM/Port；T/A/C/H | 场景/IPC | vsim_test；capacity/T05为容量或真实槽数只读切片，不证明身份映射；目标nezha BLOCKED | EXT成功/失败优先序、无效值、VSIM2/实体3/权限投影、Port观察不造绑定 | L1已支持配置，L2LSI、L3MEP/硬件，P1 |
| SIM-OC-016 | 公/厂版Feature、源/导出；构建/H | 架构/产物 | T06架构规则；本轮14源表审计PASS | 完整产品链接/安装/NEEDED/重复定义/导出、CFI/PAC/LTO；eSIM off/on/VSIM组合 | L1骨架，L2匹配manifest产品，P1 |

### 7.4 仅补真实缺口的测试组合

优先扩展现有 Contract Driver/Oracle 和真实生产源审计，不建立平行测试框架。表中用例族是后续任务建议，沿用SIM/WU/SIM-OC追踪，不把它们当已实现测试。

| 增补组合 | 输入与必须观察的效果 | 复用/限制 |
| --- | --- | --- |
| 文件/配置最小闭环，007～009 | 命中、未命中/失效、异步原IO完成、Opkey；期间切卡/销毁。断文件内容/配置查询/载荷/回退次数及旧对象结果归属 | 真实IccFile/Controller，Fake仅模拟外部扩展和RIL。先记录旧偏序，行为增强另批 |
| 拓扑/持久化，014～015 | 双槽既有基线→厂商标志/Swap失败→DB成功/失败→重载；断active/绑定/默认角色及非目标不变 | 先匹配Provider；3/4卡/三待/Port用独立产品配置，不用Legacy不支持场景伪造PASS |
| 安全记忆，013 | PIN/PUK/改PIN/锁启停成功与失败、FDN、拔卡、迟到旧回包 | 成功Hook矩阵与禁止Hook分支分别断；只用合成凭证，输出比较结果而非敏感明文 |
| 生命周期/事件，002/005/010/011 | 未就绪/注册失败、首帧、重复注册、注销、丢失/乱序、对象重建 | 断当前公开状态与新增事件窗口；不把所有重复通知都判错，次数按正式契约 |
| STK/BIP，012 | proactive命令→外部BIP→TR/Envelope→RIL；失败/超时/迟到 | 去掉“超时也PASS”的验收漏洞；原弱测试可保留历史定位，新强用例独立验收 |
| IPC契约子集，006/015/016 | 权限/无效槽/映射/返回码/回调、服务死亡恢复 | 复用真实Proxy/Stub与调用者token；组件测试不冒充权限/序列化通过 |

Fake Provider应只实现正式字段/URI/schema/错误和技术存储，不替T/A决定active、角色或初始化成功；Fake vendor只脚本化请求、应答和可观察副作用，不实现被测业务策略；Fake LPA不在本仓重造66250下载编排。缺匹配协议时继续明确BLOCKED，不能猜14个字段使编译转绿。

L1可以独立证明固定开源边界下的返回/状态/事件/副作用、错误分支、关联门控和依赖规则。L2必须证明真实闭源头/导出/虚表/控制块/装载/订阅消费；L3才能证明实际Modem/电路、booster一致性、PIN资产恢复、BIP网络及设备重启。任何一级都不能由另一类PASS替代。

## 8. 架构问题与风险清单

A=目标原则缺陷；B=描述不完整；C=骨架/目录落实；D=测试；E=旧实现/集成；F=待验证/裁决。下表复合分类表示同一问题的不同缺口，避免重复计数。P0均标明适用迁移门禁，不扩张成全仓停工令。

| 问题ID | 分类 | 优先级 | 问题 | 证据/涉及既有决策 | 影响 | 处理建议 | 是否需裁决 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CR-01 | F | P1 | 两份新增报告与本仓方法/枚举/消费基线不一致 | §2.4、S03/08/09；主设计§16/17 | 冻结不存在接口/错误PIN规则 | 匹配产品commit，按当前符号/真实分支修订契约引用 | 产品/vendor确认基线 |
| CR-02 | B/D | P1 | IIccFileExt外部ABI有原则，内部会话/缓存/回退/对象寿命合同不足 | S04/05/15；主设计§10～12/17、详细§9 | 换卡旧缓存、重复IO或悬挂风险，未证明发生 | 补原对象/请求关联及最小真实File闭环 | 外部关联/生命周期由vendor确认；不改Owner |
| CR-03 | B/D | P1 | Opkey输出与配置可见性缺行为断言 | S04 Notify后置flag；§15/16 | 错误统一提交点改变消费时序 | 冻结载荷/偏序，补异步完成及旧回调用例 | 如改变可见节点需Owner |
| CR-04 | B/D/F | P0（安全切片） | PIN记忆敏感载荷/原身份/迟到投递未闭环 | S08/15、T01；§10/11/17 | 可能对错误卡恢复凭证；未证实泄露 | 五类操作合同＋脱敏Spy＋失败/错卡负控 | 安全＋产品/vendor |
| CR-05 | B/D/F | P0（触ABI/拆SO） | 真实跨SO虚表、单例和销毁仍无产品证据 | S02/04/14；§7/14/17.3 | ABI不匹配、重复状态/错误释放风险 | 保持公开布局，核实实际SO导出/定义/控制块/匹配析构 | 改ABI/库归属/卸载条件需架构师 |
| CR-06 | B/F | P1 | vendor RIL旁路未有共同准入/结束协议 | 原报告旁路、S09/15；§12/17.2 | 本仓屏障不能证明全产品资源安全 | 列外部执行者/共享域/结果与结束观察，未证则不新增重试承诺 | vendor＋设备Owner |
| CR-07 | E/F/D | P1 | 账户事件注册/注销静态不对称 | S06；§14/15、WU-09 | 注销后通知/重建风险，动态影响未证 | 保留静态路由证据与未修复状态；不改旧实现；新实现独立建立注销契约护栏 | 旧行为修复需Owner；复现无需重划架构 |
| CR-08 | F | P1 | eSIM事件注册导出疑点未取得ELF/运行证据 | S03；原报告R1；§17.3 | 潜在厂版功能缺失，不能写现网故障 | 四步取证并登记启用消费者；必要性明确后定级 | vendor符号/产品Owner |
| CR-09 | B/D | P1 | 厂商拓扑→T/A事实→DB/通知闭包不足 | S09/10/15；§4.2/9/13，WU-01～04 | active/绑定/默认角色可能不一致 | 冻结当前标志/Swap/Hook的完整成功/失败轨迹；无双writer | 模型不改；产品策略/结束保证需确认 |
| CR-10 | D/E | P1 | Provider不匹配阻塞初始化/账户/持久化 | T04；§13/19、WU-01～05/09 | 缺DB及重启差分，状态组件PASS不足 | 提供匹配Provider/manifest，按字段和错误协议接技术Fake | Provider Owner提供合同 |
| CR-11 | D | P1 | CMI/文件/STK旧用例多为弱断言或Mock转发 | T07、§7.3；§19 | 有用例却不能检出跨界行为丢失 | 公开契约＋真实业务状态/输出强断言，保留有效旧测试 | 正式语义争议才需裁决 |
| CR-12 | D/F | P1 | 多数IPC、真实vendor及3/4卡/MEP未验收 | T01～06、S10；§16/17/22 | 覆盖误报、未批准产品规则变强断言 | 维持BLOCKED/NOT_RUN，按产品配置建矩阵，L1/L2/L3分列 | 新能力/配额需产品批准 |
| CR-13 | B/D | P1 | 当前必需/可选扩展与方法消费集合未冻结 | S03及原报告统计；§17.2/17.3 | 缺符号被误当允许降级或错误改fallback | 在既有接口清单补consumer/Feature/缺失语义/验证级别 | 产品与vendor签收 |
| CR-14 | B | P2 | T“配置清单”/O号码旧称谓残留 | 功能清单219/497；主设计§4 | 后续实施误读Owner/目录 | 局部措辞/链接消歧，不重写设计 | 依既有权威关系处理 |
| CR-15 | C | P2 | CardRuntime过渡R职责尚未薄化、成熟能力仍声明 | S14/15、详细§19 | 若误当稳态即遗漏迁移 | 沿现有计划逐能力接入，完成一个WU再退出旧实现 | 不需新架构决策；生产切换另授权 |

**A类结论：本轮未证实。** 不能将未实现骨架、外部证据缺失或旧代码静态问题直接升级为目标模型错误。若后续正式Port/资源协议证明现有T/A边界无法表达独立不变量，再按主设计既有重开条件裁决。

## 9. 最小必要修改建议

以下仅为后续授权任务建议；本轮未修改表内文件。优先补当前文档中的窄段落/表行和相应场景，不新建整套目标设计、全局Adapter框架或平行契约台账。

| 修改对象/精确位置 | 原因与对应契约 | 具体最小内容 | 设计影响 | 验证方式 |
| --- | --- | --- | --- | --- |
| 主设计§17.2/17.3、§14.2 | CR-02/05/13；005～008/016 | 将IIccFileExt、CMI共享实例、wrapper双用途连接到既有所有权/ABI规则，补外部回调原关联及在途库寿命 | 不改T/A/O/R；不改公共ABI | 对照S02～05及L1对象探针、L2产物 |
| 主设计§16/17.2 | CR-04；013 | 加操作/成功条件/身份取值/脱敏/迟到与拔卡合同索引；安全行为变更单列 | 保持R安全和vendor资产责任 | 五类节点正反测试、安全Owner签收 |
| 主设计§12.3/17.2 | CR-06/09；014～015 | 标明本仓屏障可约束的发令路径；列vendor旁路所需共同准入/结束观察 | 收紧证明范围，不另设中央资源Owner | 外部动作→观察→T/A接纳→R结束链路复核 |
| 功能清单§4.1、SIM-023.b及既有SIM-057～060/WU行 | CR-01/14；003/013～015 | 消除T配置/O主编排历史称谓；在原功能/WU加SIM-OC引用和当前基线分支 | 描述对齐；编号及迁移单元保持 | 与主设计/详细设计及当前调用链交叉检查 |
| 详细设计§9.1/9.2、§10.2、§17 | CR-02/03；007～009 | 补“公开IIccFileExt→R records→对象/LoadSession→回退/Opkey→测试”一组行，写明Notify后flag旧节点 | 不加业务上下文，不改文件虚表 | 缓存/回退/销毁/载荷及异步偏序断言 |
| 详细设计§8/9、§19 | CR-04/15；013 | 区分VerifyPinRequest现状与PUK/锁/记忆计划；明确Hook随R安全用例 | 无目录重排 | 当前完整操作清单、失败零Hook/错卡负控 |
| 详细设计§7/11/15/19 | CR-09；014～015 | 补BeginRouteMapping父子契约、vendor执行端口与T/A结果接纳/结束证据 | 沿既有窄接口；不扩大万能HostContext | Swap失败、DB失败、部分效果和重启轨迹 |
| 目录/GN，成熟能力迁入时 | CR-05/15；007/012/016 | 保留25逻辑边界及精确public/deps，按真实DAG增加成熟源；保持原Feature与工具/单例唯一定义 | 当前无需改六组树；不得先空建target | 结构/迁移/语法/GN＋匹配产品链接/导出/CFI |
| 既有[公开接口说明](SIM管理对外接口说明_源码核实版.md)相关安全/文件/CMI/EXT行 | CR-01/04/13；全部SIM-OC交叉引用 | 增consumer、当前符号/签名、Feature、缺失语义、ABI归属、测试与L1/L2/L3列 | 一处维护正式消费集合，不另建全新清单 | 查找串↔typedef↔调用点↔vendor头/导出对照 |
| 防护网v1.1§2.2及追踪章节 | CR-11/12；全部 | 明确三测试边界与三级环境的映射，把16条映射接入既有场景索引 | 保留v1.0通用边界和nezha独立产品规则 | 每条契约可反查真实SUT/断言/运行结果或缺口 |
| test/sim_guard及systemtest按切片扩展 | CR-02～04/07/09～11 | 实现第7.4节最小组合；复用Driver/Oracle，Fake仅替边界，分别保留defect/decision/blocked | 不降低原断言，不把新规则悄然加入旧兼容基线 | 旧兼容/缺陷分列、突变/负控、实际生产正执行 |
| 匹配产品/闭源集成任务 | CR-05/08/12/13；001～016相关项 | 收集manifest/Provider、vendor符号和消费者、真实SO控制块、实际设备配置 | G2b/c/G4各自验收 | ELF/装载/调用探针＋产品场景，不由hostPASS替代 |

## 10. 架构决策事项

以下需负责人签收，本文仅提出问题及约束，不给出未经授权的新政策。

| 待决定问题 | 责任人与必要输入 | 决策边界/不得默认 |
| --- | --- | --- |
| 哪个产品及开闭源commit构成本次最终兼容基线？原报告新增方法是否属于目标版本？ | 产品、vendor、集成Owner；manifest、公共头/消费者、实际符号表 | 不把本仓缺少同名方法直接判功能遗漏，也不删除其他产品需求 |
| PIN记忆何时取ICCID、哪些成功触发、迟到/换卡如何处理、敏感数据寿命如何限制？ | 安全、SIM、asset/vendorOwner；当前五类节点与真实恢复协议 | 机械迁移与固定身份/迟到抑制分验；不能推定明文传递现状即长期安全批准 |
| 哪些扩展在何种Feature下必需？缺符号允许什么公开结果？ | 产品与vendor；当前fallback和正式能力表 | 不统一改为静默成功，也不自动新增报错/重试 |
| 账户通知注销正确目标是什么，首帧/重复/在途如何定义？Refresh成功码冲突如何解决？ | API/业务Owner；S06复现及既有DEFECT_005 | 不改Oracle消除红灯；错误历史不成为永久约束 |
| vendor旁路谁准入、谁发令、结果由谁接纳、什么才证明设备结束？ | vendor/RIL/设备与T/AOwner；真实共享域与失败轨迹 | 不把软件请求结束等同硬件静止，不增加盲目fallback/重试 |
| 3/4端点与两栈、多Port、VSIM/实体3如何投影和限制？ | 产品、协议/设备、API；Port→槽合同、配额与权限 | 不将nezha三待规则普遍化，不按slot或label生成订阅身份 |
| 拆SO是否要求重建/卸载，匹配工厂/销毁、ABI及单例归属如何验收？ | 架构、构建、生命周期及vendorOwner；真实产物/在途证明 | 目录不决定SO数量；未知在途未排空不能批准卸载 |

原架构T/A写权、O非业务Owner、共享RIL归Core及不长期双实现无需因本报告重新裁决。上述外部合同只有在与既有不变量出现具体不可调和冲突时才重开核心决策。

## 11. 分阶段实施建议

阶段依赖按问题收口，不把所有闭源/设备条件变成全部host工作的总前置。下表是下一步建议，实施需另次授权。

| 阶段 | 工作、依赖与优先级 | 完成标准 |
| --- | --- | --- |
| R0：基线与必要裁决 | CR-01；先核定产品/commit、真实符号/方法，签收013安全合同与相关Feature必需性 | 16条每项有当前证据/源报告线索/缺口标签；不存在按错误PIN2或不存在方法构建的强断言 |
| R1：先立兼容护栏 | 基于R0补文件/Opkey、PIN记忆、CMI账户注销、STK往返和逐符号fallback；可与Provider准备并行 | 真实Legacy最小闭环；关键负控能失败；已知缺陷独立红灯；未就绪/缺符号/错误/超时不伪PASS |
| R2：局部设计消歧 | R1暴露真实节点后补第9节窄表；保留原业务Owner与目录 | “事实→SIM-OC→Owner→公开/内部边界→断言→验收环境”逐项可反查；所有新政策有Owner决议 |
| R3：骨架/生产切片 | 按WU-01～09接相应成熟能力；Provider齐备后才验账户闭包；不与G4新能力混验 | 受影响初始化/后台/恢复/查询回写入口全接管；无双writer/双发令/双重试，旧轨迹差分和适用架构/GN通过 |
| R4：产品/闭源集成 | 相关源码、接口/Feature和G2b准备完成，匹配manifest/Provider/vendor | 公/厂版链接安装、真实CMI/虚表/导出/装载/首访/订阅、对象唯一性通过；eSIM导出疑点分四步有结论 |
| R5：设备与退出旧实现 | 匹配产品拓扑/MEP策略；需要卸载时先G2c，否则明确常驻限制 | L3拓扑/换路失败/恢复、PIN记忆、BIP/booster/重启通过；所有成熟能力有退出旧writer/执行者证据，最终删除旧业务实现，不长期双跑 |

只读G2a工程边界与不依赖Provider的L1测试可以继续推进；账户写权、安全、文件、拓扑等生产切片须先关闭各自P0/P1，不能仅凭当前骨架/查询PASS切换。

## 12. 最终审视结论

1. **是否发现实质性遗漏？** 发现跨界合同和测试层面的实质遗漏：IIccFileExt双向寿命/回退、PIN记忆安全、厂商拓扑结果、真实进程内订阅与ABI证明。未证实目标核心模型无法承接必要能力。
2. **是否改变核心架构决策？** 当前不需要。保留T/A、O机制、R运行能力及C/I/H；不得恢复中央O或补造M上下文。
3. **目录是否合理？** 六组仍合理，逻辑target继续分层。成熟能力按原计划迁入；CardRuntime暂位是已记录过渡，不作立即重排依据。
4. **哪些依赖可由现有架构承接？** CMI稳定门面、wrapper查询/执行/输出、文件能力、事件登记、active/身份映射、STK回送及已有外部eSIM协作，分别进入既有T/A/R/C/I/H。
5. **哪些需新增稳定边界？** 需把既有公共面背后的四项合同具名化：文件扩展、PIN记忆、厂商拓扑接纳/结束、逐产品必需扩展。优先补内部窄协议和说明，不新增公共ABI或每Hook一个Adapter。
6. **最大测试缺口？** 真正的账户/默认角色/持久化系统闭包，以及CMI、文件扩展、事件、PIN记忆、拓扑、STK的跨界行为强断言。17条状态/PIN/PUK场景与单一查询不能覆盖这些链路。
7. **闭源不可见能独立保证什么？** 对固定开源基线可验证公开返回/状态/输出、原始依赖边界、请求归属、原节点差分、源表/DAG和约定ABI声明；只有实际跑过的切片可作通过结论，不能保证未知消费全集。
8. **哪些留到闭源集成？** 匹配vendor导出及虚表、真实多SO唯一实例/销毁、4011装载、真实booster/asset/BIP、专有电路/旁路协议及Port/产品能力。最终物理业务和重启恢复仍需L3。
9. **能否继续迁移？** 可以继续有边界的护栏、合同和工程准备；可以推进已具备Oracle与匹配依赖的局部迁移。当前不具备整体生产替换验收条件，不能直接接管未闭合的安全、账户或拓扑写业务。
10. **迁移前必须关闭哪些P0/P1？** 相应切片先关闭CR-04安全和CR-05 ABI门禁；按涉及范围关闭CR-01基线、CR-02/03文件、CR-06/09厂商拓扑、CR-07订阅、CR-10 Provider、CR-11强断言与CR-13消费清单；CR-08/12须有匹配集成结论或明确不适用证明。关闭一个切片不代表全部产品门禁完成。

本报告不改变任何既有设计、生产源、GN、测试、Oracle或历史证据；原始未跟踪报告保持原样。所有建议均服务于局部后续任务和Owner裁决，提交/推送不在本轮授权范围内。
