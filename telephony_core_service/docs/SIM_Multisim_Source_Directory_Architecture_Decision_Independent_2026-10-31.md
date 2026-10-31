# 多 SIM 源码目录组织架构独立决策分析与建议

## 1. 执行摘要

**有条件支持局部修订的 B+，状态 Proposed。** 保留账户、选卡、卡能力、设备、集成、支撑六组，兼顾整体地图与业务定位。卡应用会话归 card/state，Radio 协议会话归 device/radio；SlotCatalog 保持宿主构建、跨模块只读；运营商账户字段写入归 T，Records/Phonebook 按真实依赖拆 target。Card/Device 分组成立，但不是两级总 Manager。T/A 写权、异步资格和依赖约束不变；目录决策不代表产品等价、资源安全或架构批准。

## 2. 评审背景和源码基线

### 2.1 目标与输入

目标是由 `services/multisim/` **完整替代** `services/sim/`，最终只有一套生产 SIM 业务实现；不建设长期并行的新旧执行路径。本次只新增本报告，不实施重组，不修改执行状态、原 ADR、两篇权威设计、源码或构建。

遵循根及部件 AGENTS，读取[唯一执行状态](SIM_Refactor_Execution_State.md)、[目标主设计](SIM_Target_Architecture_Design_v1.2.md)的事实 Owner、兼容、异步及迁移门禁，以及[源码详细设计](SIM_Multisim_Source_Architecture_Detailed_Design.md)的逻辑边界和功能职责。复用其 T/A/O/R 和既有 SIM/WU 编号，不创建平行功能台账。目录裁决仍需批准后回写权威设计。

| 基线项目 | 本次重新核实的事实 |
| --- | --- |
| Git 根 | `multisim` 仓库根；部件位于 `telephony_core_service/`，并非以部件目录为独立 Git 根 |
| 分支 / HEAD | `main` / `6c98e170c6d95a3a39fcdfa1a192a6927cbfd783` |
| 输入工作区 / index | `git status --porcelain=v1` 无输出，工作区和暂存区均干净 |
| 跟踪 | `main -> origin/main`；fetch 后 `HEAD...origin/main` 为 `0 / 0` |
| remote | origin：`https://github.com/zjuluowu/multisim.git`；upstream：`https://github.com/openharmony/telephony_core_service.git`；本次推送目标仅 origin/main |
| worktree | 共 6 个：main、两份历史 docs/integration worktree、恢复 docs worktree及两份 G2a worktree；均已列出核对，没有切换、覆盖或合入它们 |
| 原 SIM 文件 | `services/sim/include/` 61 个 `.h`，`src/` 55 个 `.cpp`；不含其 test 子目录 |
| 产品源表 | 根 `BUILD.gn` 的 `tel_core_service` 显式引用 54 个不同 SIM cpp：51 个基础源 + 3 个 eSIM 条件源；唯一未列入的是 `sim_cache_sync_manager.cpp` [E01] |
| 目标骨架 | `services/multisim/` 88 个文件：63 头、23 cpp、BUILD、README；实际 7 个一级目录，`infrastructure/` 尚不存在 |
| 骨架构建 | 24 个 source_set 全为 testonly，另有 skeleton group；未在根产品源表装配 [E02] |
| 其他分支重要实现 | `origin/work/g2a-readonly-checkpoint-20261030` tip 为 `728ad77ed4c723efbb5cb1baf7b3f10bb9830240`，含 `cc787945792527eeb94bc20a5e955315d6269e83` 真实只读接入检查点；main 不含该检查点 |
| 恢复验证 | `check_sim_refactor_recovery.py --remote origin --fetch` 实际返回 CHECKPOINT_VERIFIED，6 个必需代码路径及保存源码摘要核实；`current_runtime_test_executed=false` |

恢复检查首次在受限环境 fetch 失败，按权限流程重试成功。检查点验证不是当前 HEAD 的主机业务运行证明。工作分支的 `LegacySimQueryBinding / RealSimCountQuery` 不能算 main 已有生产能力；本次不自动合入，也不依赖旧 `/tmp` 产物恢复实现。

### 2.2 证据口径与独立性限制

本报告的 **事实** 来自该 HEAD 的源码、源表、声明和本次实际命令；**建议/推断** 是基于这些事实的结构裁决；**待验证** 是必须由匹配产品、provider、厂商或设备关闭的缺口。行号固定于该 HEAD，文末证据链接均相对本报告。核心读写/回调已定点阅读；其余成熟文件族核对源表、接口和消费者，未声称逐行形式化验证全部生产分支。

执行顺序确为：先源码、骨架、权威职责，记录第一阶段初判，再完整读取原 ADR 全部 526 行，随后回查反证。**这不是完全盲审**：用户提示已给出三树及旧推荐 B+，必要权威文档也有既有架构观点。不能隔离这些先验。分析由本会话 Codex（GPT-6）完成，没有可核验资料证明与上一轮使用不同底层模型，故不声称已完成不同模型的独立复现。

尝试了三个只读子任务，其中一个为无历史上下文定位任务；均因调用额度限制未完成。只收到生产能力子任务的一份初步线索，主 Agent 已重新核实引用；无完成的多模型一致性结论，也无完成的冷启动实验。T1～T8 是主 Agent 的源码支持静态任务推演，不是用户效率测量。

## 3. 独立架构初步判断

以下判断在读取原目录 ADR 前已固定，保留当时的结论，不倒写成“预先证明 B+ 正确”。

| 第一阶段问题 | 初步判断及直接依据 |
| --- | --- |
| 稳定业务分组 | 身份/绑定/账户资料与生命周期、选卡/默认角色、卡能力、共享设备执行、系统集成；另留最小请求机制。Controller 的账户、选卡、设备等待混合不应复制 [E05][E07][E09] |
| 一级目录 | 值得展示上述职责组，让新人先识别模块地图；不以 T/A/O/R 字母或 application/domain 作为主要业务导航 |
| 二级目录 | 发现、激活、号码、默认角色、Primary、PIN 安全、Records、Phonebook、STK、eSIM 等有可定位入口和独立变化原因 [E04][E10][E12][E14] |
| 只需接口/依赖表达 | 纯规则与应用层、平台适配、跨 Owner 结果、O 请求机制不必在每个能力下重复建层级；不同源集合可位于同一目录 |
| Card/Device | Radio 跨槽 phase/session 与 EF/PIN/STK/eUICC 协议不同，分组有价值；“是否调 RIL”不是边界 [E08][E11][E15] |
| Session 的反例 | 当前 SessionRegistry 以 `CardApplicationTarget` 为键，维护应用替换代次；属于卡运行有效性，并非通用 Modem session [E23][E24] |
| 无价值抽象风险 | 通用 CardManager、DeviceManager、全局业务 O、每功能四层空目录以及为了父组增加的转发接口没有得到源码支持 |
| 新集中点 | card 若变总头/总 target、device 若吞全部执行和卡协议、integration 若接管业务重载/重试，都会复现旧集中式 Controller |

初步倾向是“六组导航，但卡应用会话、宿主容量描述和运营商跨 Owner 写入必须修订”，而非原样接受候选树。第一阶段还观察到 `EsimFile::SyncCloseChannel` 的本地清零不能证明设备结束，以及 `CardFileDetectManager` 独立写表和装配顺序问题；第二阶段仅增加证据与可执行约束。

## 4. 设计原则和评价标准

### 4.1 硬约束先于目录偏好

1. T 唯一接纳账户、绑定、active 和账户生命周期；A 唯一接纳角色、Primary、偏好及业务冲突/重试。持久行包含两者字段时由两个 Owner 生成各自计划，仍可一次物理写入，不能演变成两次竞争建行。
2. 原始卡观察、卡缓存、卡应用资格、Radio 协议状态、真实资源租约分别有明确责任者，不能因同属 R 合并为全局可变状态。
3. 发起时关联 request/step/attempt、原会话及传输请求；在真正写点核查资格。调用者等待、业务效果和设备结束分别记录；未知效果不能伪造成成功或安全释放。
4. 具体实现依赖不成环；共享 RIL 仍由 Core 拥有；不复制实例、请求表或设备发令路径。
5. API/ABI/IDL、参数和 schema、错误优先级、部分效果、通知及初始化发布/销毁保持兼容；安全增强与机械重组分开验收。

### 4.2 同范围评价，避免伪精度

| 优先级 | 评价维度 | 可审查问题 |
| --- | --- | --- |
| 首要优化 | 整体可理解性 | 新人能否看出账户、选卡、卡能力、硬件执行和系统接缝？父目录能否解释子能力的共同变化原因？ |
| 首要优化 | 局部可发现性 | PIN、号码、STK 等任务能否命中主要实现、Owner、契约和测试？是否有多个含义相同的目录？ |
| 首要优化 | 变化内聚与传播 | V1～V4 中哪些是语义必需联动，哪些是总头、总 Manager 或重复事实造成的额外传播？ |
| 首要优化 | AI 修改安全 | 能否按任务读取小闭包，识别唯一写者，避免按旧类名误归属，并用检查器拒绝非法 include？ |
| 次要因素 | 深度、数量、迁移成本 | 相比真实理解/维护收益，多一层、重命名和 target 调整是否值得？ |

A 按完整稳态能力评价，不能用只有 86 个 C++ 文件的骨架与 B+/C 的完整生产树比较规模。C 的能力名也可用于 A 的二级目录；`security/number/default_roles` 命名收益不能全归功于 B+ 父目录。三者都能承载相同逻辑 Owner；没有目录天然保证并发或 ABI。不给无实测依据的加权总分、Token 比例或开发时间收益。

## 5. 原 SIM 生产功能与目标架构关系

### 5.1 生产能力闭包

下表为原有功能台账的目录投影，不增加新的 SIM/WU 编号。引用均是旧生产实现，目标分工是建议，不能混为已移交状态。

| 完整能力 | 当前入口、状态读写与调用事实 | 推荐责任/落点及兼容关注 |
| --- | --- | --- |
| 初始化与装配 | Core::Init 创建共享 RIL，再 eSIM、SimManager、搜网，最后 CMI 发布 [E03]；SimManager 分槽创建 State/File/Carrier/SMS/Phonebook/STK [E04] | integration/lifecycle 组合根；Core 仍控制宿主顺序与共享 RIL；不把实例数等同物理资源数 |
| 插卡、ICCID、建行 | Monitor::InitData→Controller::InitData→InitIccId，匹配旧行或 Insert，再分别加载 current/all，初始化 active/号码/Primary [E05][E06] | account/discovery 主编排；card/state、records 提供已接纳观察；A 提供初始角色片段，T 一次组织物理建行 |
| 账户身份/绑定/历史 | Controller 按 slot 排 current、按 simId−1 排 all；ForgetAllData 经 Rdb 写 SLOT_INDEX，条件改标签/active [E06][E29] | account 的 AccountRegistry/BindingIndex 与两个兼容读源；不能把硬件槽号当订阅身份，也不静默修正稀疏 simId 可见行为 |
| 缺卡、Unknown、重载恢复 | Monitor::RefreshData 根据卡态、Modem 初始化及 Primary 进度选择清卡缓存、解绑、重读及重试；StateHandle 缺卡清账户加载资格，Unknown 触发主卡复核 [E06][E13] | card/state 接纳观察；T 接纳解绑/重载及业务重试；A 主卡复核；integration 只交付事件，不能把所有 Unknown 解释为拔卡 |
| 普通/force/卫星激活 | 普通路径等 Primary→设备→缓存/DB→两投影→通知/主卡；force 提前返回；卫星 force 路径不等同普通 RIL 流程 [E07] | account/activation 拥有 active/业务进度，A 准入，device/radio 普通设备步骤；不得统一成一个改变分支顺序的流程 |
| Voice/SMS/Data | Voice/SMS 经 DB 与当前角色缓存；Data setter 保存参数并报告行为，不能假定三者使用同一存储/错误轨迹 [E09] | card_selection/default_roles；A 唯一角色计划/接纳；存储和输出适配按方法保留差异 |
| Primary/Radio 握手 | Controller 选择槽3映射、专用无重启命令、已M0和普通协议；普通路径参数预写/失败恢复/重试/成功 Data 联动 [E08] | card_selection/primary 保留意图/策略/重试；T 映射子步骤；device/radio 执行与协议 session；不把参数恢复当设备回滚 |
| 号码/MSISDN | GetShowNumber 读卡→账户→后备来源，可能异步回写；SetShowNumber 写 EF 成功才写 DB/两缓存 [E10]；SimFile 结果用 lastMsisdn_ 更新卡缓存 [E12] | account/number 主编排；card/records 拥有卡号码/EF结果；重构必须保留查询副作用及失败后的部分效果 |
| PIN/PUK/PIN2/锁/认证 | StateManager 等待并映射结果/剩余次数，PIN 成功可将 LOCKED 改 NOT_READY；Handle 创建事件直调 RIL [E11] | card/security 持请求/安全结果；card/state 持唯一公开卡态，通过窄接纳接口更新，不复制状态 |
| EF/Records/Refresh/LOADED | SimFile 持加载计数、文件缓存、语音信箱/CFIS/语言；OnAllFilesFetched 按既有顺序更新加载、通知观察者、Registry、CES、语音信箱与 EXT [E12] | card/records；底层 files/codec 与高层加载分 target；LOADED 不是“所有可选 EF 都成功”，也不是设备资源结束 |
| Phonebook | Manager→Cache/USIM 服务→Handler→FileController；ADN 可变成 PBR；缺卡清缓存、READY 预加载 [E14] | card/phonebook；保持 PIN2/FDN、ANR/IAP/PBR 和请求关联；MSISDN 共享低层 ADN 原语不复制 |
| 卡短信 | SimSmsController 按 CDMA 分支调用 RIL 并等结果 [E15]；SimFile 也解析卡短信 EF | card/sms_storage 操作与列表语义，card/records 保留底层文件协议；不是将整套短信服务迁入 multisim |
| STK | 专属 RIL session/主动命令/终端回应、BMS 订阅及输出重发；特定命令还调用 EsimController 并延时关闭 CA [E16] | card/stk 保持协议会话和专属输出；eSIM 特例用窄契约，不扩大通用 device/session 或全局 events |
| 运营商配置/匹配/权限 | SimAccountManager 装配 ConfigCache/StateTracker；Loader 直接更新 SIM 表；芯片 IMS 结果处理中的报告受 Primary 进度影响；权限控制器另开 APDU 通道 [E17][E18][E19][E43] | card/carrier 承接订阅相关配置/匹配/权限，但账户资料由 T 接纳；平台能力适配随功能；不能因 Account/State 类名分别搬 account/state |
| eSIM/eUICC/Profile/Port | Manager 拥有普通和低优先级 EsimFile；File 处理通道、APDU、BPP/Profile；Manager/File 又写 A 偏好/T恢复提示 [E20][E21] | card/esim 协议/库存/本地操作；T 已接受身份与 Port 绑定；A 偏好；66250 高层任务保持外部边界 |
| 会话/共享资源 | RadioProtocolController 有跨槽会话和单份 activeResponse；骨架 SessionRegistry/ResourceBarrier 有纯规则，但未接真实设备 [E22][E23][E25] | 卡应用资格在 card/state；Radio session 在 device/radio；真实资源资格在 device/resource，不能按 slot 复制屏障 |
| 外部接口/事件/持久化 | 原 I(Sim/TelRil)Manager、Core 入口、CMI/搜网消费者、DataShare、Registry/CES；TelRilSim 反用 SIM 消息 [E03][E26][E27][E28] | integration/api/events/storage + 能力专属适配；公开头仍在原 interfaces；必要消息/codec 精确导出 |

### 5.2 不能漏掉的非主流程写入口

`CardFileDetectManager::SaveCardFileDetectData` 独立按 ICCID 查行、Insert/Update，写入 ICC_ID/CARD_ID、物理卡、LSI、标签、eSIM 和 EF 元数据，成功后尝试刷新 all 缓存 [E30]。它必须进入 account/discovery 的字段接纳/合并协议，不能作为无权写事实的 Card 工具继续直写。

`OperatorConfigLoader::InsertOpkeyToSimDb` 名为 Insert，实际查到行才 Update，字段为 OPKEY/MCC/MNC/IMSI [E18]。匹配算法及配置缓存仍是 carrier 责任，但持久账户资料更新要交 T，以原 ICCID/观察来源及相关字段版本接纳。不能只关闭 Controller 的 setter 就宣布 T 唯一写权，也不能把“同一 SIM 表”推导成所有运营商运行配置都属于 T。

## 6. A/B+/C 三方案完整比较

### 6.1 同一能力范围的候选结构

```text
A（现有职责目录，括号内为尚未迁入能力）
multisim/
  topology/{accounts,discovery,activation,account_profile,api,adapters}
  assignment/{domain,primary,account_roles,api,adapters} (+ default_roles)
  runtime/{state,records,euicc,session,resource,api}
          (+ security,phonebook,sms_storage,stk,carrier,radio)
  foundation/orchestration
  compatibility/{api,output}  assembly/  include/
  (infrastructure：仅真正共享技术设施，现无文件)

B+（原候选，不等于本报告最终修订树）
multisim/
  account/{discovery,activation,number,profile}
  card_selection/{default_roles,primary}
  card/{state,security,records,phonebook,sms_storage,stk,carrier,esim}
  device/{radio,session,resource} + slot_catalog
  integration/{api,events,lifecycle,storage}
  support/request

C（能力一级平铺）
multisim/
  account/{discovery,number,profile}  activation/  card_selection/
  state/  security/  records/  phonebook/  sms_storage/  stk/  carrier/  esim/
  radio/  device/{session,resource} + slot_catalog
  integration/{api,events,lifecycle,storage}  support/request
```

树中的功能目录不自动成为 Context、GN target、SO 或运行对象。A 的当前七组和稳态候选八组需区分，C 的十五组也不意味着十五个独立 Owner。

### 6.2 相同标准下的裁决

| 维度 | A | 原 B+ | C |
| --- | --- | --- | --- |
| 硬约束可实现性 | 已有骨架契约/DAG可沿用，但生产写权未交接 | 能保留；原 session/catalog 归类需澄清，不应误授 Device 权力 | 同样能保留；T 跨 account/activation 不意味着双写 |
| 整体地图 | T/A清晰，Runtime混卡能力/设备，术语需解释 | 账户/选卡/卡/设备直观；carrier、session 是分类例外 | 所有能力一览；state、radio、device 的关系与T范围需额外归组 |
| 局部定位 | 已熟悉现布局者最省切换；新人要学 topology/assignment/runtime | 已知业务通常到二级，路径含明确业务词 | 已知 STK/PIN/Phonebook 最直接；一级平铺优势真实存在 |
| 同变化原因内聚 | 可用 runtime 二级实现；父组较宽 | 卡协议与跨槽执行区别可表达；不是绝对独立 | 单能力内聚不弱；跨能力共同规则较难从父组发现 |
| AI读取/误改 | 现有导航与检查可复用；大Runtime/Service仍有膨胀风险 | 可按功能闭包检索；card/device名字易诱发总Manager、全量上下文读取 | 细功能天然适于独立子任务；更需契约说明隐藏的Shared资源/Owner |
| GN落地 | 当前24 source_set和checker实际存在，是最强即时优势 | 需改路径分类与增加真实协议细分，不能只保留总Runtime边界 | 同样需映射/围栏；不要求一能力一target |
| 迁移成本 | 最低路径成本；混合职责拆分仍必要 | 中等路径、include、导航成本；成熟业务拆分成本三者共有 | 路径和围栏改动较广；不凭目录数量量化成本 |
| 最适用情境 | 正在验收的切片、现有贡献者、路径稳定优先 | 长期完整替代，需新人理解全模块且保留功能命中 | 主要维护少数独立能力、团队已熟悉全局Owner |

**不支持直接把原 B+ 当已证明最优。** 本报告选择其六组导航，并修订会话/catalog归属及内部依赖规则。若仅考察近期切片落地风险，A 优于重排；若仅考察已知功能的一次目录命中，C 优于 B+。本任务优先完整模块未来 3～5 年可理解性，因此接受一次局部重排和一层父组成本，但不在本次执行。

## 7. Card/Device 专项决策

### 7.1 是否构成真实边界

**成立为相对稳定的功能分组；不成立为严格上下两层或两个统一状态 Owner。** 最强支持是 RadioProtocolController 的跨槽 CHECK/UPDATE/COMPLETE、协议 session 和 Modem 观察，与 PIN 剩余次数、EF 路径/解析、STK 主动命令、eUICC Profile/BPP 的规格责任显著不同 [E11][E12][E16][E21][E22]。

最强反证不是“Card 也调 RIL”这么简单：Card 的 SessionRegistry 按应用替换而变化；卡态接纳依赖 Modem/SenseSwitch；eSIM 关闭的本地状态与设备真实结束无法隔离推定；carrier 既含卡规则又含芯片/运营商产品配置 [E13][E18][E21][E23]。因此 Card 的准确含义是“卡能力及紧密相关的订阅配置”，Device 是“实际共享设备执行/资格”；这不是两个纯 DDD 业务上下文，也不承诺所有芯片变化只改 Device。

### 7.2 逐项职责裁决

| 真实类/文件族 | 当前业务/协议及状态 Owner | 设备执行责任 | 目标归属、拆分及额外跳转/环风险 |
| --- | --- | --- | --- |
| SimStateManager / SimStateHandle | Handle 持卡态/锁因，Manager 持等待/结果映射；另含账户加载清理、eSIM验证及Primary副作用 [E11][E13] | 借共享RIL取卡态/解锁；设备观察是输入而非账户事实 | card/state + security；T/A副作用经各自入口。security→state窄接纳，不让state反调security实现；不增加PIN→DeviceManager转发 |
| SimFile / IccFile / FileController | EF缓存、加载计数、Refresh/LOADED、MSISDN/语音信箱/CFIS；Controller处理文件操作与响应 [E12][E31] | 通过ITelRilManager::GetSimIO传输 | card/records 与 files/codec。文件选择/解析不搬Device；先分底层files与上层records target，消除整包依赖环风险 |
| IccDiallingNumbersManager 等 | Phonebook缓存、ADN/PBR/ANR/IAP、等待结果；Handler有进程级静态请求表 [E14][E31] | Handler调用FileController完成EF I/O | card/phonebook；低层Handler可供Records复用，不让Records依整个Phonebook门面；静态关联表不能按新目录复制 |
| StkManager / StkController | Toolkit session、主动命令、专属重发队列及BMS/Primary事件 [E16] | 直接共享RIL；特定命令交eSIM | card/stk；RIL/输出适配就近。Toolkit session不归Device；STK→eSIM窄契约，反向回调不包含STK实现 |
| EsimManager / EsimFile / EsimController | Profile库存/协议、AID/channelId、BPP/通知/验证，夹杂偏好与账户提示 [E20][E21] | RIL开关通道/APDU，普通与低优先级File共用RIL | card/esim；T绑定、A偏好抽走，真实共享资格device/resource。多一条资格契约有安全价值，多一层全能转发无价值 |
| RadioProtocolController | phase/session、Modem映射观察、握手结果和activeResponse [E22] | 跨槽设备命令及等待 | device/radio；业务Primary政策/参数/重试在A，协议重发若有仍属于协议。错误回包关联须补，不能以目录移动宣称安全 |
| MultiSimController | 账户/角色/激活/Primary/号码/缓存/重试混合 [E05][E07][E08][E10] | 普通激活委托Radio，专用Primary仍在自身 [E32] | T/A/Device三向拆分，不能整体改名成新Service；跨Owner子步骤有父关联，不持资格同步等对方完整流程 |
| CardRuntime | 同时借用全部成熟卡对象、Radio、RIL/eSIM，持SessionRegistry/ResourceBarrier/LoadSession及关联 [E33] | 目标执行方法多数仅声明/注释 | 过渡接缝私有留装配侧；state接收卡会话/观察，records接收load，device接收物理屏障/Radio。逐步变有限委托并最终按能力消除，不造CardManager |
| TelRilSim / 相关RIL消息 | HDI命令编解码、传输serial与原事件回送，引用SIM消息及工具 [E26] | 真正RIL边界仍属共享宿主 | 不迁整个RIL到device；消息/必要codec精确独立target。禁止RIL→整个Card/Device→RIL实现环 |

### 7.3 状态、Session、Resource 的唯一责任

| 状态或资格 | 唯一逻辑责任 / 推荐物理位置 | 生命周期与禁止事项 |
| --- | --- | --- |
| Subscription/Profile业务身份、已接纳Slot/Port绑定、active | T / account | 卡库存或硬件路由不等于已接纳绑定；Device不维护另一份账户映射 |
| 默认角色、Primary意图/参数、业务冲突资格 | A / card_selection | A可保留冲突登记，但它不是设备租约；不能因业务完成就释放R的真实资格 |
| 模块RuntimeEpoch | H / integration/lifecycle创建，纯值契约跨模块传递 | 每次实例唯一；卡会话、请求和租约引用同一宿主epoch，不各自生成相互不兼容的epoch |
| 卡应用SessionRef、bindingGeneration、ExpectedTransition | R卡有效性 / **card/state/session_registry.*** | 当前实现以slot+application为键 [E23]，与T bindingRevision分开；等待结束不签发新会话，不把Device协议session当卡代次 |
| 卡公开状态、锁因；EF加载；Toolkit session | card/state；card/records；card/stk 各自Owner | LOADED、READY、STK session end不能当物理资源可复用证明 |
| eUICC逻辑通道/AID/APDU关联 | card/esim/channel；carrier权限通道归card/carrier/privilege | 一个句柄一个协议责任者。Device只引用不透明关联和资源键，不再维护权威AID/Profile/开关状态 |
| Radio协议session/phase/member | device/radio | 与RIL serial、业务RequestId均不同；若未来出现独立通用设备会话职责再建目录，当前不预建device/session |
| 真实物理资源租约/栅栏/隔离 | device/resource，R唯一屏障 | 按证实的物理冲突域；不按软件槽/EsimFile实例数量推定独立性，不复制租约 |
| 请求Step/attempt/等待/完成资格 | 各T/A或卡能力用例自有，机制在support/request | 无全局业务协调器；短请求不必经T/A或持久化O |

将 SessionRegistry 置于 card/state 是**物理归属修订**，没有把 R 的代次写权交给 T。Device 需要验证卡执行目标时，只消费卡会话窄只读契约/不可变令牌；Card 需要设备资格时只消费资源契约。具体实现互不包含，由 H 注入，契约 target 保持 DAG。Device重置可产生失效证据，最终卡应用代次仍由唯一卡有效性Owner接纳。

### 7.4 eSIM/权限通道的端到端资格

现状必须同时看到五件事：

1. EsimManager 的普通、低优先级 EsimFile 是不同对象、各自通道状态，共用同一 RIL；名称不能证明设备抢占/QoS或物理独立 [E20]。
2. EsimFile::ProcessEsimOpenChannel 依据MEP参数选择普通/带Port命令；其命令Port与Profile操作的portIndex不可未经外部契约就等同T绑定 [E21]。
3. SyncCloseChannel 最多两轮等待后仍清本地channelId/AID；CloseChannelDone不检验结果错误。OpenChannelDone使用try_lock推断调用者是否已退出，并尝试关闭spare channel [E21]。这是既有补救，不是充分结束证明。
4. `IccOperatorPrivilegeController` 也直接开 ARA-M 通道、传APDU、关闭；超时分支可仅清本地句柄 [E19]。资源冲突调查必须覆盖它，不能只验证两套EsimFile。
5. EsimManager::SwitchToProfile 把卡枚举结果写入出参后仍执行A偏好参数写入；DisableProfile还有前后不同位置的恢复提示 [E20][E21]。返回码、卡枚举结果、偏好节点到达、设备结束是四类事实，不可压成一个bool。

建议的资格流是：具名卡请求保存原Session/Step/Port输入→向唯一物理域屏障申请lease→保存原RIL事件关联后发令→卡协议Owner接纳原命令结果→按原节点向T/A提交提示或观察→分别结束调用者等待和设备执行资格。发令后超时，只关闭本次完成出口并保留原关联；设备是否结束、通道是否静止及资源是否可重用不足时保持隔离/待核对。迟到开通道成功只允许清理其原句柄，不能重开旧业务完成、覆盖新请求句柄或释放别人的lease。

这一协议是**后续安全切片建议**，不是现有设备保证。ResourceBarrier::Complete/AcceptRecoveryEvidence已有纯规则要求commandEnded、resourceReusable及效果资格 [E25]，但尚无真实生产适配器证明这些输入。接入时必须保留既有允许的部分效果；新增超时拒绝/隔离/重发政策单独批准。真实物理域、旧命令终止证据、Port映射和厂商取消语义的缺口不阻塞目录选择，阻塞相关生产安全承诺。

### 7.5 对新增耦合的防线

禁止 `所有业务→CardManager→DeviceManager→RIL`。PIN/EF/STK可使用能力专属窄执行契约或现有共享RIL适配；只有共享执行资格、跨槽Radio机制或平台差异确有价值时才调用Device。Card也不成为统一公共包：无总头、无汇集全部能力实现的public_deps；父目录不持状态。

必须显式允许并控制必要协作：Records↔Phonebook的底层ADN复用、STK→eSIM命令、carrier→T资料接纳、Device重置→卡有效性失效。它们是实际依赖，不应藏入“通用事件总线”伪装解耦。编译依赖通过接口/低层实现拆分成DAG，运行回调保持原因关联及受控顺序。

## 8. 开发人员与 AI 编程可发现性分析

### 8.1 T1～T8：相同任务下的首选目录与 Owner

这是静态模拟。路径以各候选完整稳态为准；A中尚未建立的能力注明为计划。B+栏先按原候选比较，最后列本报告修订。目录路径短不等于必须阅读的逻辑模块少。

| 任务 | A 首选代码目录 | 原 B+ 首选代码目录 | C 首选代码目录 | 正确 Owner / 修订版变化 |
| --- | --- | --- | --- | --- |
| T1 插入、初始化、发现、激活 | assembly；topology/discovery、activation；runtime/state、records | integration/lifecycle；account/discovery、activation；card/state、records | integration/lifecycle；account/discovery、activation；state、records | H装配、R观察/加载、T发现/active、A后续选卡；卡应用会话修订为card/state。复合任务必然跨责任 |
| T2 默认卡和Primary | assignment/primary、计划roles；计划runtime/radio | card_selection/default_roles、primary；device/radio | card_selection/default_roles、primary；radio | A主编排，T提供目标/映射，R执行；不让Device选择默认卡 |
| T3 号码/MSISDN/Records | topology/account_profile；runtime/records；计划phonebook | account/number；card/records、phonebook | account/number；records、phonebook | T账户号码，R卡MSISDN/EF；修订要求低层ADN与files细分target |
| T4 PIN/PUK/锁恢复 | runtime/state及计划security | card/security、state；必要device/session | security、state；必要device/session | R安全请求与公开卡态分责；修订后不用为卡会话先跳Device |
| T5 电话簿/STK/卡短信 | 计划runtime/phonebook、stk、sms_storage | card/phonebook、stk、sms_storage | phonebook、stk、sms_storage | 各R能力Owner；只有跨功能场景才读Records/security/eSIM |
| T6 eSIM Profile/Port/APDU | runtime/euicc、session、resource；T/A接口 | card/esim；device/session、resource；account/card_selection | esim；device/session、resource；account/card_selection | R卡协议、T已接纳绑定、A偏好、R物理资格；修订把卡会话归state |
| T7 三卡到四卡 | foundation/slot_catalog；topology/assignment/runtime；assembly | device/slot_catalog；account/card_selection/device；lifecycle | device/slot_catalog；account/activation/card_selection/radio；lifecycle | H描述/装配，T绑定，A策略，R执行，C容量投影；修订catalog在support，H构建，Device无动态绑定 |
| T8 Modem平台、状态及RIL调整 | runtime的radio/session/resource/state；compatibility/assembly | device/radio/session/resource；card/state及能力适配；integration | radio/device；state及能力适配；integration | R设备执行与卡观察；语义变动再联动T/A/C。修订不让卡代次跟随通用Device集中 |

### 8.2 每项任务的入口、阅读闭包、契约、误判与测试

下表对三个方案使用同一源码闭包和测试标准；前表只改变其物理落点。测试路径相对 `telephony_core_service/`，均已核实存在，**未运行这些生产单测**。列出的负例是建议补齐/确认的验收场景，不宣称现有测试已完整覆盖。

| 任务 | 主要实现入口与必要阅读源码 | 必要跨模块契约 | 易误判位置 / 不必要跳转（A、B+、C） | 相关测试入口与关键负例 |
| --- | --- | --- | --- | --- |
| T1 | CoreService::Init、SimManager::OnInit、Monitor::InitData/RefreshData、Controller::InitIccId/SetActiveCommonSim，State/Records事件 [E03]～[E07] | CardObservationPort、AccountStore、AccountRolePlanningPort、ActivationRuntime/AssignmentPort、宿主生命周期 | A把所有初始化塞topology；B+把插卡等同device初始化；C把activation当第三Owner。三者均不需统一O调度；不为发现扫描Phonebook编解码 | `test/unittest/sim_gtest/sim_manager_gtest.cpp`、`multi_sim_controller_gtest.cpp`、`test/multisim_architecture_rules/topology_rules.cpp`；补插拔/重复ICCID/初始化失败/force/卫星及重载 |
| T2 | Controller::SetDefaultVoiceSlotId/SetDefaultSmsSlotId/SetDefaultCellularDataSlotId/SetPrimarySlotId、RadioProtocolController、AssignmentService/PrimarySwitchPlan [E08][E09][E22][E35] | PrimaryTopologyPort、RadioExecutionPort、AssignmentStore、带父关联的映射/结果 | A因RadioResource名称把真实租约归A；B+先改Device选择政策；C在radio复制Primary决策。三者均不应经CardRuntime总入口解释默认卡 | `test/unittest/sim_gtest/multi_sim_controller_gtest.cpp`、`test/multisim_architecture_rules/assignment_rules.cpp`、`resource_rules.cpp`；四分支、并发失败、旧完成清新进度 |
| T3 | Get/SetShowNumber、SetShowNumberToDB、SimFile::UpdateMsisdnNumber/ProcessSetMsisdnDone、Handler及目标show_number用例 [E10][E12][E31] | ShowNumberRuntimePort、NumberQueryHostPort、AccountStore、完成出口；原卡→DB→两缓存偏序 | A的account_profile与euicc需辨名；B+的account/profile与esim/profile仍需限定；C也不能只改records。避免为了号码读完整大File/Controller，先读调用和写点 | `test/unittest/sim_gtest/sim_file_gtest.cpp`、`multi_sim_controller_gtest.cpp`、`test/unittest/icc_dialling_numbers_handler_gtest/icc_dialling_numbers_handler_gtest.cpp`；卡成功DB失败、查询回写、迟到错号码 |
| T4 | StateManager::UnlockPin/UnlockPuk及Handle发令、结果与SetSimState [E11][E13] | 安全请求/结果、卡会话有效性、公开状态接纳、必要RIL窄执行 | A只看runtime/state漏安全请求；B+因调用RIL跳device；C把security和state看成互不相关。均不需要读取A默认卡逻辑，除非故障包含重载/选卡 | `test/unittest/sim_state_handle_gtest/sim_state_handle_gtest.cpp`、`test/fuzztest/simauthentication_fuzzer/simauthentication_fuzzer.cpp`；剩余次数、超时、锁态恢复及插拔 |
| T5 | 各Manager/Controller/Cache，按需读UsimDiallingNumbersService、Handler、StkController特例、SimSmsController [E14]～[E16] | EF读写、PIN2、卡态/卡会话失效、STK专属输出和eSIM验证命令 | A先学runtime含义；B+多一层card但业务名完整；C首选最短。均禁止为了每条短信/STK结果跳T/A；仅复用底层契约 | `test/unittest/sim_gtest/usim_dialling_numbers_service_gtest.cpp`、`sim_sms_controller_gtest.cpp`、`test/unittest/core_service_gtest/tel_stk_controller_test.cpp`；PBR/缓存失效、BMS未就绪重发、eSIM特例 |
| T6 | EsimManager普通/低优先级数组、EsimFile通道/Profile/BPP、EsimController、LocalCardRequests及ResourceBarrier [E19]～[E25][E36] | T绑定/恢复提示、A ProfilePreferencePort、原双结果、卡会话/通道与共享资源结束证据 | A把euicc当远端任务；B+把device/session当全部会话；C在esim与account双存Port事实。均不需复制66250任务机，不能把通道关闭当业务成功 | `test/unittest/esim_gtest/esim_manager_test.cpp`、`esim_file_test.cpp`、`esim_file_branch_test.cpp`、`test/multisim_architecture_rules/runtime_protocol_rules.cpp`；跨实例/权限通道竞争、开关超时/迟到/MEP |
| T7 | telephony_types容量宏、SlotCatalog、SimManager::InitTelExtraModule、Controller映射/角色、RadioProtocolController::Init、Core投影 [E04][E08][E22][E28][E37] | 静态容量各口径、T绑定、A冲突域、R物理域和C调用者投影 | A散落常量；B+误认为只改device/catalog；C误认为多一槽只多数组。三者均需全消费点审计，不以改目录数量衡量复杂度 | `test/multisim_architecture_rules/topology_rules.cpp`、`assignment_rules.cpp`、`resource_rules.cpp`；`test/linux_sim_guard/`容量设施；产品slot2/slot3/VSIM与并发矩阵 |
| T8 | RadioProtocolController、StateHandle::GetSimCardData/SenseSwitch、TelRilSim消息/响应、各卡能力执行适配、搜网消费者 [E13][E22][E26][E27] | 命令/错误/取消/结束语义、原始观察→卡接纳、失效事件、GN消息/codec导出 | A总Runtime接口扩散；B+所有协议下沉Device；C各能力复制平台判断。均避免读全部T/A实现，只在输入/结果语义改变时扩展闭包 | `test/unittest/tel_ril_gtest/tel_ril_sim_test.cpp`、State/相应能力测试；补旧错误回包/重复通知、Modem重置、READY/LOADED偏序及设备静止 |

### 8.3 两种认知成本分别判定

**局部发现：** C 对已知能力首屏最好；B+保留全部相同业务词，通常仅多一个 `card/` 前缀，`rg --files`/符号搜索的命中能力接近。A有现成README导航，熟悉仓库者可能最快；改名并不自动减少实际调用闭包。没有试验支持“B+与C耗时差距可忽略”，这里只判断路径歧义差距有限。

**整体理解：** B+显示两个业务事实组、卡功能集合和共享硬件执行集合，对完整替代目标更合适。C也能通过短导航说明恢复这些关系，不能断言十五个目录导致认知过载。A的Owner父目录本身有价值，但“Runtime”覆盖的变化原因太宽，初次阅读要先解释架构术语。B+的优势是显式分组，不是目录更少本身。

### 8.4 本地 Agent 的可执行工作方式

先按任务词执行 `rg --files` 与 `rg -n '函数|事件|字段'`，再读取主入口→真正写点→请求创建/回调→消费者→测试源表；不要先把整个card或大Manager装入上下文。源码导航只需“业务任务→主路径→事实/运行Owner→测试命令”四列，加禁写/禁依赖规则；AGENTS应引用唯一设计，不复制18章报告。

可以把独立PIN、STK、号码查询切片分给只读或范围限定的Agent；共享资源键、卡应用代次和T/A投影接纳必须由同一明确Owner维护，不能按目录给每个Agent一份新缓存。任务有唯一主要区域不代表只改一个目录：T1/T6/T7需要明确主编排者及合作契约。

在相同需求下，B+修订版减少“卡会话先去device”“SimAccountManager先去account”的导航误导；它不消除现存巨型cpp。真正减少上下文膨胀需要后续按用例拆大型类、收窄public头和可编译测试闭包。上述是维护机制建议，无Token/效率数值承诺。

## 9. T/A/O/R 逻辑架构与 GN 约束

### 9.1 逻辑写权和运行协作

```text
integration/api(C) -> account(T) / card_selection(A) / 具名card能力(R)
T激活用例 -> A业务准入契约 + Device执行契约 -> T接纳active
A主卡用例 -> T目标/映射契约 + Device执行契约 -> A接纳角色/参数
Card协议 -> 必要RIL窄适配 + 必要Device资源契约
Device原始反馈 -> Card观察契约 / 发起用例结果契约
H组合根注入同一组对象；O机制由每个用例持有
```

这是运行关系，不能照抄为实现target双向边。T/A之间交换窄应用契约；T不直接写A聚合，A不持T可写引用。R观察→T发现/失效、R结果→原请求Owner的回调携带原Session/Step，不能在到达时贴当前代次。发现失效、DB锁外调用和最终接纳必须按既有协议控制，而非加一层入口检查 [E33][E34][E35]。

### 9.2 当前 GN 的真实能力

当前24个source_set可按以下逻辑组理解：sim_values；request_protocol/mechanism；topology 的 types/contracts/domain/protocols/cases/adapters；assignment 对应六组；runtime 的 contracts/mechanisms/cases/core；compatibility 的 contracts/core；assembly 的 contracts/core；repository_exports [E02]。目标总数不是优化指标。

`check_structure.py` 检查每个C++文件恰属一个target、路径分类、允许依赖、public头、文本include图和target图无环，限制骨架仅标准库角括号头；检查testonly、check_includes及group覆盖 [E38]。本轮该工具实际PASS。它的分类硬编码当前路径，不能拿当前PASS证明B+树已经通过。

当前visibility通常为同BUILD `:*`及架构测试，范围宽；checker并不验证visibility值。它不识别“调用合法Store但写错Owner字段”、运行对象双实例、持锁等待环、真正RIL结束、Feature组合、生成代码或ABI。反过来，GN check也无法证明请求与回包因果正确。目录合并后必须保留逻辑target；不把24个源集合压成六个。

### 9.3 推荐的可落地围栏

| 规则 | 具体落实方式 | 不能由它证明的事项 |
| --- | --- | --- |
| 纯规则不引平台/O | 保留T/A pure types/domain白名单；同目录中的纯规则target仅依赖自身类型和纯值，不依赖request_mechanism、FFRT、DataShare、RIL实现 | 规则本身是否符合产品语义 |
| T/A实现不互引 | 用例target依赖对方窄contracts，禁止对方Service/aggregate私有头；H负责注入 | 运行期是否出现互相同步等待，还需场景检查 |
| Card/Device实现不成环 | 卡会话只读contract和资源contract分别独立；Device不包含card/state实现，Card不包含RadioController/Barrier私有头 | 真实命令是否正确关联/结束 |
| capability内不是无限开放 | files、低层ADN、records加载、phonebook门面按真实依赖拆target；跨能力只public所需头 | 所有旧耦合已消除 |
| RIL反依受控 | IccToRilMsg/IccFromRilMsg及必要工具精确导出；可依平台事件contract，但不能public_deps整个card/device或纯support伪装平台包 | RIL/厂商ABI与运行结果等值 |
| integration分层 | lifecycle组合根可依具体实现；api/events/storage target不得借同父目录权利依所有业务实现；专属Store适配随Owner | 发布/注销/排空顺序，须运行验证 |
| support受限 | 纯身份/请求值、共享只读SlotCatalog与请求机制分源集合；无业务策略、可写全局缓存、全局重试或平台客户端 | 请求生命周期或SlotCatalog构建值正确 |
| 防止Agent放宽约束 | 文件→target显式清单、未知文件失败；增加反例：纯规则引O、跨Owner私有头、RIL引Card门面、把device总target传递给T | 恶意或语义性绕过，不替代代码评审 |

**Records/Phonebook的具体DAG示例：** `records_load -> adn_io -> card_file_io`；`phonebook_cases -> adn_io/card_file_io`。IccFile包含Handler，而Handler包含IccFileController [E31][E44]，所以若按两个父目录各建一个总target，就会形成records↔phonebook包级环。可把Handler保留在phonebook物理位置但独立为低层adn_io target；不能让files target因IccFile也叫File而吞入高层IccFile。这里选择性增加target有实际依赖收益，不要求增加新一级目录。示例只列关键边，仍须审计SimFileManager、sim_utils、消息/codec及平台头；它不是未经构建即可照抄的完整生产DAG。

### 9.4 三个 Service 的收窄

| 类型 | 应保留的稳定受控入口 | 必须下沉到功能场景 / 应避免 |
| --- | --- | --- |
| TopologyService [E34] | 账户/绑定短提交、发现/失效/加载接纳、只读视图；一个事实写入口不等于全局长锁 | 号码、发现/重载、激活、映射步骤各在对应用例；不把所有I/O/恢复和设备等待放一个Service.cpp |
| AssignmentService [E35] | 角色/投影接纳、Primary/Activation共用业务冲突登记、初始角色资格 | 默认角色、Primary四模式及策略分别具名用例；设备协议/lease不在A；业务重试权保留A，但执行步骤不必全塞Service |
| CardRuntime [E33] | 迁移期成熟对象借用和必要结果接缝；同一实例不双发令 | SessionRegistry→card/state、LoadSession→records、ResourceBarrier→device/resource；PIN/Phonebook/STK/eSIM分能力入口。目标是有限薄委托后按需消除，不让其持全部状态 |

三者风险都存在。减少文件行数不是验收标准：验收应看单一写权、用例边界、依赖扇出、是否持锁跨I/O、是否新增跨全模块调度入口。禁止仅把MultiSimController逻辑搬进TopologyService，也禁止新增CardManager/DeviceManager作为“统一入口”。

### 9.5 OpenHarmony 集成约束

根target是现有 `ohos_shared_library("tel_core_service")`，带version script、CFI、PAC、LTO等配置 [E01]。新目录仍属core_service内部，不能由目录数推导SO、SA、进程、导出或安装产物数量。公开 `interfaces/innerkits` 和IPC契约继续独立维护。

eSIM Manager基础源即使Feature关闭仍编译其关闭分支；File/Controller/OSU callback为条件源。卫星、EXT/VSIM、low power等宏来自gni [E39]，不按整个目录统一开关。Core先初始化共享RIL，再eSIM/SIM/搜网与CMI [E03]，重组不改变RIL控制块身份、发布时点、回调存活或匹配销毁；SIM不得停止宿主共享RIL。

G2a分支只读接入与main骨架严格分开。若后续拆SO，必须另核实NEEDED、导出符号/单例唯一定义、安装、Feature及生命周期；本报告没有运行OHOS产品构建、完整OnInit、IPC/SA或设备测试。

## 10. 四类未来演进的变化影响

M＝语义上必要修改/核查；P＝结构可能引入的额外传播；S＝契约保持等值时预计稳定；?＝缺少外部证据。M在三方案中相同，P才用于比较组织方式；S是条件推断。

| 演进 | M：逻辑模块、接口、状态责任 | A 的 P | B+ 的 P 与本次修订 | C 的 P | S 与 ? |
| --- | --- | --- | --- | --- | --- |
| V1 三卡→四卡 | H容量/装配；T Slot/Port/Subscription绑定与旧投影；A默认角色、Primary冲突；R路由/并发；C调用者容量；逐TU检查宏 [E04][E22][E28][E37] | foundation/runtime/topology各自解释槽口径可能复制特例 | 若device/catalog取得动态绑定/选卡权，硬件内部改动传到T/A。修订catalog为共享只读描述，动态T/A不搬 | activation独立和radio/device并列易各自校验槽数；以相同只读契约避免 | S：PIN/EF/TLV/ADN协议，前提命令语义不变。?：真实4路/并发，VSIM/EXT映射、各产品可见数；不能只改MAX_SLOT_COUNT |
| V2 MEP/Profile/Port | Card eUICC协议/卡应用资格；T已接纳绑定与历史投影；A偏好/角色；Device共享物理资格；provider及外部任务 [E20][E21][E23] | 大runtime公共接口把每个Profile字段传播到全部能力 | 原device/session夹卡身份可能形成Device/T双Port图；Card/Device双channel状态也增加P。修订卡会话在state，协议句柄各Card能力唯一、T绑定唯一 | account/esim/state分散使各处重建Port映射；窄binding/观察契约避免 | S：普通卡协议与未变角色规则。?：Port↔逻辑slot、同时激活数量、持久化schema、66250任务语义；现有WithPort分支不能证明完整MEP |
| V3 新Modem/芯片平台 | Device Radio协议/物理域/结束适配；Card设备观察解释、必要命令适配、carrier芯片能力；语义变化时T/A/C联动 [E13][E18][E22][E26] | 总CardRuntime让所有能力重编/重读，平台类型进纯规则 | 统一DeviceManager会把平台枚举传播到全部Card；卡会话移Device会扩大读范围。修订窄执行契约、卡会话与Radio session分开 | 公共device总头或各能力复制平台if同样传播；无父分组不自动隔离 | S：等值错误/完成下的T规则、卡解析、A政策。?：迟到/取消/静止，SenseSwitch、READY/LOADED及故障恢复偏序；不能承诺上层零修改 |
| V4 Telephony/RIL职责调整 | 指定卡协议或设备执行模块、RIL消息/codec、Feature/ABI和源表；结果/取消/超时/完成语义的消费方 [E26][E38][E39] | 成熟SIM工具反依可拉入整个runtime | 把整个RIL搬device或导出整个card会成环；修订精确message/codec target及同一实现 | 平铺易按每能力复制codec或把radio/device互相包括；同样以窄target防止 | S：契约等值的T/A纯规则与不受影响卡能力。?：厂商/HDI版本、外部消费者、取消是否保证设备终止 |

静态SlotCatalog的`configured/routable/assembled/externallyVisible/radioStackSlots`不是同一个数 [E28]；现RadioProtocolController限制最多双槽，Core按MD+1传入容量，EsimManager分配数组后仅对IsSupported且非slot2的位置创建对象，InitTelExtraModule另有slot2特例 [E03][E04][E20][E45]。这些具体约束决定V1范围，不能以“三卡已有实现”推导四卡只需扩容数组。

V2不通过简单增加`account/profile`完成：该目录仅账户资料；卡Profile在card/esim/profile，已接受Profile/Port绑定仍为T的BindingIndex。只有真实需求证明绑定规则需要独立上下文才重新评估，本轮不新增业务Owner。

## 11. 原 ADR 逐项审查

原文为[2026-10-31目录决策](SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md)。下表区分复核原话与质疑其更强解释：原文已经否认“目录即解耦”和“目录证明设备安全”，本报告不把这些正确限制曲解成错误。

| 原ADR主张 / 位置 | 独立结论 | 源码证据 / 反证 | 重要性 | 处理建议 |
| --- | --- | --- | --- | --- |
| B+有助于形成整体SIM功能地图，§7/11 | **Partially Confirmed** | 完整生产可归账户、选卡、卡能力、跨槽执行及集成；但carrier含芯片/产品配置，catalog含可见性 [E17][E18][E28] | 中 | 保留六组；明确分类例外，收益标为静态判断 |
| Card/Device是自然稳定变化边界，§5 | **Partially Confirmed** | Radio phase/session与EF/STK不同；StateHandle的Modem/SenseSwitch/eSIM副作用、通道结束显示边界非绝对 [E13][E21][E22] | 高 | 按状态和变化原因细分，不承诺独立变更 |
| B+与C局部可发现性差距有限，§6/7 | **Partially Confirmed** | 相同业务名通常仅多card前缀；PIN、STK主路径明确；C一级直接命中优势仍在 | 中 | 路径判断可保留；不能推出已证实耗时/Token优势 |
| B+保留T/A/R/O逻辑职责，§8 | **Confirmed**（设计可行性） | T/A契约、pure target和按Owner请求机制不依赖父目录名 [E02][E34][E35][E40] | 高 | 继续保留唯一写权/逻辑target，不能称生产已移交 |
| Card能力内聚且不形成大公共包，§7 | **Partially Confirmed** | 需分别导出能力；IccFile→Handler→FileController显示整包依赖会环 [E31] | 高 | 明确低层ADN/files与高层Records/Phonebook的DAG，禁止card总头/实现public_deps |
| Device不会成为统一转发中心，§5/11 | **Partially Confirmed**（要求合理，未实现） | 原成熟PIN/EF/STK直调共享RIL，CardRuntime当前多能力聚集 [E11][E16][E33] | 高 | “不会”只能作为门禁；允许直接窄RIL适配，禁止DeviceManager |
| Integration/Support能保持受控，§8/12 | **Partially Confirmed** | composition、外部适配和请求机制可分；Monitor持业务重载/重试，TelephonyDataHelper含OpKey错误状态 [E06][E41] | 高 | integration内部区分target权限，专属流程留Owner；技术名字不能免于字段审计 |
| 当前目录方案完整承载旧SIM生产能力，§9 | **Partially Confirmed** | 源表主要能力均有归宿，但Loader直接写账户资料、STK/eSIM跨能力及ADN低层边界未具体关闭 [E16][E18][E31] | 高 | 补充写入口与可执行依赖切片；“有归宿”不等于已承载 |
| 四卡/eSIM/Modem/RIL影响合理隔离，§10 | **Partially Confirmed** | 明确M/P/S/?正确；双槽Radio、MD+1、特殊槽、Port/结束外部契约限制仍在 [E04][E20][E22][E37] | 高 | 接受条件性稳定，扩大carrier/卡会话/权限通道必要联动 |
| GN/接口约束可落地，§8.2 | **Confirmed**（机制）；**Partially Confirmed**（生产细分） | 当前checker实际可检查DAG/public；仅24骨架源集合，未覆盖成熟能力实际依赖 [E02][E38] | 高 | 不按六目录合target；生产迁入新增必要边界与反例 |
| runtime/session整体机械搬device/session，§5.3/5.4/12/13 | **Challenged** | SessionRegistry按CardApplicationTarget维护bindingGeneration/应用替换，非通用硬件执行会话 [E23][E24] | 高 | 放card/state；Device只读资格并报告设备失效，Radio session留device/radio；不变更R唯一写权 |
| SlotCatalog作为静态硬件能力整体归device，§11/12/13 | **Challenged** | SlotDescriptor含VisibilityClass，LegacyCapacityMeasures含assembled/externallyVisible；H构建并供T/A/R/C使用 [E28] | 中 | 保留support的窄纯值target，H构建；若未来抽纯硬件描述另证，不机械拆成两个权威catalog |

原 ADR 对SimAccountManager实际为carrier、CardFileDetect独立入口、eSIM本地关闭不足、共享RIL、CardRuntime风险、GN visibility宽、Proposed状态的认识均被确认。上述不是本报告新发现。主要差异是会话/catalog物理归属、carrier账户字段交接、包级DAG及装配/跨能力验收的具体化；不是为了独立性全面否定B+。

**Unsupported**适用于“六组已证明减少实际读取/维护耗时”这一更强推论，本轮没有证据支持，原文也未作该强断言。没有发现足以把原文主要主张判为**Incorrect**的直接矛盾；会话/catalog属于有反证的归类裁决，标Challenged更准确。若把catalog全部内容解释为硬件能力，才与VisibilityClass字段事实不符；物理存放于Device本身不构成违法。

## 12. 原 ADR 未覆盖的重要问题

以下只列原文没有具体展开的实质问题；原文已有的eSIM关闭风险等不重复包装为新增发现。

| 问题与源码证据 | 架构影响 | 具体处理 / 是否阻塞目录决策 |
| --- | --- | --- |
| 卡应用会话与设备协议会话不应共用物理默认归属 [E23][E24] | 原树引导开发者在Device修改应用代次，可能为Port演进新增第二绑定图 | SessionRegistry放card/state，明确T bindingRevision与R bindingGeneration不同；本轮树可直接修订，不需设备确认 |
| SlotCatalog含调用者可见性/装配口径，不只是硬件 [E28] | 误将容量投影政策和动态拓扑授予Device | 保留共享只读描述及H构建，T/A只依纯值target；不创建多个相互同步catalog |
| Loader直接更新OPKEY/MCC/MNC/IMSI [E18] | 即使Controller/Detect已迁，仍可能有账户资料旁路writer | T接纳字段计划，carrier保留匹配逻辑/技术适配；迁移前扫描全部SIM_INFO_URI/SimData字段写点，阻塞“唯一写权已完成”声明 |
| Records/Phonebook的包级依赖反向 [E31] | 两目录各一个target会成环；把整个IccFile放files还会让低层吸入高层依赖 | 以card_file_io/adn_io/records_load/phonebook_cases形成DAG；只增必要target，不复制ADN实现 |
| STK特定主动命令直接进入EsimController并安排CA关闭 [E16] | “各卡能力自闭合”过强，统一events会丢专属偏序/Feature条件 | 保留STK→eSIM窄命令契约及同Feature源表，补定向用例；不阻塞父目录选择 |
| 权限通道也是共享硬件候选使用方 [E19] | 仅覆盖普通/低优先级EsimFile不足以证明eUICC设备域安全；是否真正冲突尚待设备证据 | 将ARA-M/认证等实际发令方纳入物理资源域核实；协议状态仍各能力唯一，不强造通用ChannelManager |
| Detect构造早于Controller创建 [E04][E30] | 初次OnInit中传入的weak引用为空，后来给成员赋新shared_ptr不会重绑原weak；成功写表后的刷新分支可能不执行 | 全仓成员赋值扫描未见重绑。属于源码支持的初始化缺陷推断，运行影响未测；迁移装配必须区分旧轨迹与单独修复，不偷偷当等价移动 |
| TelephonyDataHelper并非完全无语义连接器 [E41] | 其OpKey错误标志会影响恢复；整体放integration/storage可能把业务判断藏为技术状态 | 连接创建/句柄可共享；OpKey失败含义由carrier消费且单一维护，迁移时保留读写/重试关系 |

这些问题都不要求增设新的业务上下文。它们要求更准确的归属、接口、源集合或兼容验收；产品/厂商缺口只阻塞相关生产切片，不阻塞本次Proposed建议。

## 13. 最终独立架构决策

**最终推荐：有条件支持经过局部修订的 B+，保留六组一级目录。** 如果负责未来3～5年的维护，我选择这一方案：用稳定业务名建立整体地图，卡功能仍在直接可见的二级目录；以明确状态Owner和细粒度构建契约限制其分类例外，而不是要求每一段代码都符合完美的Card/Device二分。

最重要的三个理由：

1. **完整生产模块容易理解。** account/card_selection标明两个业务事实组，card集中可识别的SIM功能，device突出真实跨槽执行/共享资格，integration与support保持技术边界。
2. **功能发现与变化归属兼顾。** PIN/Records/STK/Phonebook/eSIM仍有显式业务路径；Radio握手与卡协议变化分开，减少先学T/A/R术语和在宽Runtime内辨认责任的负担。
3. **不需要改变核心不变量。** T/A唯一写权、各卡状态Owner、原RIL实例、按Owner请求机制与GN DAG可以保持；修订版进一步避免把卡会话、账户绑定或可见性政策交给Device。

**最有力反对意见：** Card并非纯卡协议集合，carrier产品配置、STK/eSIM交互、设备影响卡态都跨组；六组导航仍需解释，且A已有围栏和贡献者知识，重排会增加短期路径/冲突成本。若目标仅是完成当前G2a切片，维持A更合理。这个反对意见成立，但不推翻面向完整替代的稳态选择。

必须接受的取舍：比C多一层Card导航；Card代码规模可能最大；复合用例仍跨T/A/Card/Device；物理归类与target不一一对应；当前24 target不必永远不变。没有证明B+在所有任务或所有团队更快。

**批准目录前必须调整：** 会话归属、catalog定位、carrier账户字段写权、Records/Phonebook目标依赖、禁止总Manager、integration内部权限与实际55源文件覆盖。最后一项以§16的源表核对为准，不引入新的功能编号或数量口径。

**可在后续迁移逐步处理：** 拆大类/大文件、收窄GN visibility/public、成熟卡协议适配、真实资源结束桥、初始化问题的单独修复及产品差分。不能拖到生产替代后才处理唯一写权、双发令、资源重复释放或新增依赖环。

待架构师批准的是物理树和约束、权威文档同步方案、分阶段迁移与行为增强门禁；不是本次提交即批准运行策略。新API/schema、四卡/MEP政策、SO/SA变化、超时/隔离/重试行为变化仍需各自证据和授权。

## 14. 最终推荐的稳态源码目录树

这是**局部修订 B+ 的唯一推荐树**，不是当前磁盘树。既有类名可保留，`.h/.cpp`就近，不机械添加include/src或每功能四层。三级只用于已有复杂协议/配置簇；未迁入能力不提前建空目录。树中的文件是职责落点示意，未列出所有成熟类；§16列出完整src覆盖。

```text
services/multisim/
├── BUILD.gn
├── README.md
├── account/                              # T：身份、绑定、资料、active与生命周期
│   ├── account_registry.*                # 纯规则/状态；独立domain target
│   ├── binding_index.*                   # T已接纳绑定，非卡应用代次
│   ├── topology_service.*                # 保留受控事实接纳入口
│   ├── account_types.h / account_store.h # 仅必要契约；与实现分target
│   ├── discovery/                        # 发现、联合建行、重载、Detect/Carrier资料接纳
│   ├── activation/                       # 普通/force/卫星用例、业务进度、Hook
│   ├── number/                           # MSISDN关联、账户号码、查询/独立回写
│   └── profile/                          # 名称、标签、IMS偏好；不含eUICC Profile
├── card_selection/                       # A：角色、Primary、业务冲突及重试
│   ├── assignment_service.*              # 受控接纳/冲突登记
│   ├── assignment_aggregate.*            # 纯规则/状态
│   ├── role_selection.h
│   ├── account_role_protocol.h           # 联合建行角色资格，不重复账户行
│   ├── default_roles/                    # Voice/SMS/Data，保留三者轨迹差异
│   └── primary/                          # 四模式计划、参数恢复、业务重试
├── card/                                 # R卡能力；父目录不设总运行对象/总头
│   ├── state/                            # 卡状态/锁因/观察接纳与失效
│   │   └── session_registry.*            # 卡应用代次/ExpectedTransition；从runtime/session迁来
│   ├── security/                         # PIN/PUK/PIN2、锁操作、认证请求/结果
│   ├── records/                          # 高层卡缓存、加载/Refresh/LOADED、MSISDN、CFIS
│   │   ├── load_session.*
│   │   ├── icc_file.* / sim_file.*        # 高层；不放入低层files target
│   │   ├── sim_file_init.* / sim_file_parse.*
│   │   ├── ruim_file.* / isim_file.*
│   │   ├── voice_mail_constants.*
│   │   ├── files/                        # IccFileController及卡型Controller，低层EF I/O
│   │   └── codec/                        # 单一编码/TLV实现，按消费者精确导出
│   ├── phonebook/                        # ADN/FDN/PBR/ANR/IAP和电话簿缓存/用例
│   │   └── icc_dialling_numbers_handler.* # 物理在此，低层adn_io target可供Records复用
│   ├── sms_storage/                      # 卡短信存储；非完整短信业务服务
│   ├── stk/                              # Toolkit会话、命令、专属事件/重发
│   ├── carrier/                          # 卡匹配/权限及订阅相关运营商配置；非全Telephony配置中心
│   │   ├── config/                       # ConfigCache/Parser/APN与专属适配
│   │   ├── matching/                     # StateTracker/Loader/匹配；账户字段经T接纳
│   │   └── privilege/                    # 证书规则/ARA-M专属通道
│   └── esim/                             # Core本地卡能力；不复制66250任务
│       ├── esim_manager.*                # 保留Feature关闭分支及旧结果适配
│       ├── esim_controller.*             # 卡侧验证/专属服务接缝
│       ├── profile/                      # Profile/Port协议请求、库存、通知/昵称
│       ├── channel/                      # AID、APDU、逻辑句柄、原命令关联
│       └── provisioning/                 # challenge/认证/BPP/安装原语、OSU接缝
├── device/                               # R：共享物理执行/资格，非业务选卡中心
│   ├── radio/                            # 跨槽握手、普通激活/专用Primary执行
│   │   └── radio_protocol_controller.*   # 协议session/phase/member就在此
│   └── resource/                         # 真实域lease/fence/隔离/结束证据
│       └── resource_barrier.*
├── integration/                          # C/I/H：系统边界与组合根
│   ├── api/                              # 分能力兼容入口/完成；公共ABI仍在原interfaces
│   ├── events/                           # 共享订阅、账户callback/death、Registry交付
│   ├── lifecycle/                        # HostPorts、工厂、发布/排空/匹配销毁
│   └── storage/                          # 真实共享连接设施；不持账户/角色策略
└── support/                              # 受限共享叶子和机制；不是公共业务池
    ├── sim_identity.h                    # 纯值：Subscription/Slot/Session引用等
    ├── request_ref.h                     # request/step/attempt/effect纯值
    ├── slot_catalog.*                    # H构建、T/A/R/C只读；含容量/可见性描述
    └── request/                          # O机制；每Owner/请求各自持有
```

### 14.1 目录职责及排除规则

| 一级组 | 逻辑映射 | 允许的主要内容 | 禁止膨胀方向 |
| --- | --- | --- | --- |
| account | T + 其应用O/I | 唯一账户/绑定接纳与用例；Owner专属Store适配就近 | 不因名为account吸入SimAccountManager全体；不拥有R卡态/设备租约 |
| card_selection | A + 其应用O/I | 角色/Primary业务政策、冲突资格及唯一业务重试 | 不直接驱动所有底层协议，不建立真实Modem租约 |
| card | R各能力 + 短请求O/专属适配 | 卡协议、缓存、卡应用有效性、紧密关联配置/权限 | 不持T绑定/A偏好写权，不建全能CardRuntime或卡总target |
| device | R设备运行责任 | Radio执行/协议会话及真实共享资源治理 | 不吞Card协议、动态账户映射、角色政策；不复制RIL控制器 |
| integration | C/I/H | 原契约适配、共用输出/技术连接、组合根 | 不拥有业务重载/重试/字段计划；只有lifecycle target可依全体实现 |
| support | 纯值 + O机制 | 小型通用值、只读描述、请求机制 | 不放FFRT/DataShare/IPC平台客户端，不签发全局业务操作或保存事实 |

### 14.2 Card/Device 关键契约与隔离

复用已有 `SessionRef/StepRef`、`RadioExecutionPort`、`ActivationRuntimePort`、`CardObservationSink`、资源资格/结束证据及能力结果接口，不为统一命名新建总DevicePort。卡应用只读资格接口与资源接口可分target、同处责任目录；协议层不反向include调用者用例。凡新增接口，必须指出它隔离哪个变化或保护哪个不变量。

设备报告重置/执行结果，卡有效性Owner决定哪些应用资格失效；卡协议持原命令/通道关联，设备屏障接纳可验证的结束证据；T依据已接受观察更新绑定；A依据结果与政策更新角色。四类接纳不以单次callback到达互相替代，也不要求一次全局事务。

`device/session/` **不出现在本推荐树**：当前唯一拟迁类并非纯设备会话，Radio已有自己的协议会话。未来若出现独立于卡协议、跨设备能力复用的真实会话实现，再单独说明状态/失效责任后增设；不预建空抽象。SlotCatalog在support但仍由H构建，不能由support反调平台取值。

## 15. 当前目录到推荐目录的映射

“路径调整”只代表可保持代码token/类型/对象身份的搬迁；不等于已消除旧依赖或具备生产方法。所有当前文件必须更新既有 `source_migration.json`，本次不修改该文件。

| 当前路径（相对services/multisim） | 推荐落点 | 处理与约束 |
| --- | --- | --- |
| topology/accounts | account根的account_registry/binding_index | 合并物理层级；纯规则target继续独立，不取得O/平台依赖 |
| topology/discovery | account/discovery | 保留用例/观察账本；成熟Monitor/Detect接纳另切职责 |
| topology/activation | account/activation | 保留进度/用例；Hook、运行/业务准入契约随能力 |
| topology/account_profile | account/number | 现有六个号码h/cpp按功能改名父目录；名称/标签/IMS新增落profile |
| topology/topology_service.* | account/topology_service.* | 保留类型与唯一接纳，后续提取用例；不复制状态 |
| topology/api、adapters | account根及discovery/activation/number/profile | 按消费者/Owner就近契约；整体搬头可机械，拆定义/收窄公开面需契约验收 |
| assignment/domain、account_roles | card_selection根 | 纯模型和角色资格保留各target，不能合成可变common |
| assignment/primary、assignment_service.* | card_selection/primary与根入口 | 模式计划保留，业务步骤后续具名用例；角色/冲突状态仍唯一 |
| assignment/api、adapters | card_selection根及default_roles/primary | 专属接口/适配就近；RadioExecution消费契约可留A侧，不必因实现去device就移接口归属 |
| runtime/state/pin_requests.h | card/security/pin_requests.h | 按实际功能搬；原state目录目前仅PIN契约，不能误称已存在成熟卡态实现 |
| runtime/records | card/records | LoadSession/EF契约原样保留，后续成熟加载按实际写点接入 |
| runtime/euicc、api/local_card_requests.h | card/esim的本地请求及profile契约 | 现LocalCardRequests是Profile编排，不能按LocalCard名称推广成卡总入口 [E36] |
| runtime/session | **card/state** | SessionRegistry保持同一责任/实例，应用代次含义不变；与原ADR落点不同 |
| runtime/resource | device/resource | 保留真实域、lease和恢复规则，不能把骨架Acquired当设备保证 |
| runtime/card_runtime.*、result_sinks.h | 过渡期integration/lifecycle私有接缝，最终分到具名能力契约 | 必须拆责任后薄化；不得整类搬card或device并宣称解耦；临时私有类不是稳态新一级目录 |
| foundation/slot_catalog.* | **support/slot_catalog.*** | 保持纯只读target与H构建，不向Device转移动态绑定或caller可见性政策 |
| foundation/sim_identity.h、request_ref.h | support根 | 小型叶子类型保留单一定义，Owner专用types仍留各Owner |
| foundation/orchestration | support/request | 协议/机制分target；实例和步骤由各用例持有 |
| compatibility门面/api/output | integration/api、共享交付入events；专属输出随能力 | 保留原错误/读源/回调/偏序，不能全合一个巨型API或events类 |
| assembly | integration/lifecycle | 组合根/HostPorts/发布与匹配销毁；保留宿主RIL与对象控制块 |
| include两个仓内头 | 精确导出lifecycle契约；必要过渡forwarder | 审计仓内消费者后去冗余；不改公共innerkits API/ABI |
| 计划infrastructure（现无文件） | 专属适配随Owner；共享连接入integration/storage | 不为旧树齐全创建空目录，不建立全表业务Repository中心 |

前表覆盖当前所有七个一级组及各子目录，不把仍在G2a分支的 `application/query/real_sim_count_query.cpp` 当main待搬文件。未来适配G2a须在对应工作分支核对真实HostPort闭包，不直接移植路径后宣布生产验收。

## 16. 代表性生产文件迁移建议

### 16.1 全部 src 文件的职责覆盖

以下文件名相对 `services/sim/src/`；对应头文件随同一职责审计。M＝可候选机械路径迁移（保持原实现/target/行为并审计消费者），S＝必须按状态/职责拆分后才达到稳态，C＝必须保护跨模块契约，X＝当前未列入根生产源表。M不表示零风险或可直接启用新运行对象。

| 生产文件（显式列出，便于核对源表） | 推荐落点 | 迁移判断与关键条件 |
| --- | --- | --- |
| sim_manager.cpp | integration/api、lifecycle及各能力 | S/C：门面与装配拆分；保留eSIM/SIM/搜网发布和原对象，Detect顺序修复另验 |
| multi_sim_controller.cpp | account、card_selection、device/radio | S：账户/角色/四种Primary/设备等待按完整WU移交；禁止整体新Manager |
| multi_sim_monitor.cpp | account/discovery、integration/events及能力输入 | S：加载状态/重试归T，系统订阅/回调交付技术化；Unknown/Primary分支保留 |
| multi_sim_helper.cpp | T/A字段计划、对应输出/存储适配 | S：一次建行含双方字段，A规划、T组织；不以Helper名放support |
| card_file_detect_manager.cpp | account/discovery + 卡记录输入 | S：独立Insert/Update、查询和缓存刷新入口全部接管 |
| sim_rdb_helper.cpp | account/card_selection专属持久化适配 | S/C：共享技术连接可复用，业务字段方法不能作为任意Owner可写门面 |
| telephony_data_helper.cpp | integration/storage技术连接 + carrier错误状态消费契约 | S/C：区分句柄/URI和OpKey错误语义，保留恢复观察节点 |
| sim_account_callback_death_recipient.cpp | integration/events | M/C：订阅token/death/注销及回调寿命保持 |
| sim_state_manager.cpp、sim_state_handle.cpp、icc_state.cpp | card/state、security及T/A观察接缝 | S（前二者）/M（纯状态结构部分）：公开卡态唯一，安全结果专属；去掉账户/Primary代写 |
| sim_file.cpp、sim_file_init.cpp、sim_file_parse.cpp、icc_file.cpp、ruim_file.cpp、isim_file.cpp | card/records高层 | M路径/S职责：卡型解析与成熟加载先复用；缓存接纳/输出/跨能力调用须另切，不能整体标纯Domain |
| sim_file_manager.cpp | card/records能力入口及专属委托 | S/C：文件/状态/权限及外部消费者接口分别审计，不复制成熟缓存 |
| icc_file_controller.cpp、sim_file_controller.cpp、usim_file_controller.cpp、isim_file_controller.cpp、ruim_file_controller.cpp、csim_file_controller.cpp | card/records/files | M/C：底层EF I/O/卡型路径；message/codec和RIL依赖精确导出 |
| icc_dialling_numbers_handler.cpp | card/phonebook，独立低层adn_io target | M/C：同一static requestMap/serial体系；供records复用但不反依高层records_load |
| icc_dialling_numbers_manager.cpp、icc_dialling_numbers_cache.cpp、usim_dialling_numbers_service.cpp | card/phonebook | M路径/S依赖收窄：保留PBR/ANR/IAP、PIN2和状态失效，不建第二File缓存 |
| sim_sms_manager.cpp、sim_sms_controller.cpp | card/sms_storage | M/C：CDMA/GSM与结果/等待、原std同步先保留；新增同步遵守仓库FFRT要求，不能宣称旧锁已迁完 |
| stk_manager.cpp、stk_controller.cpp | card/stk | M路径/S跨能力接缝：专属BMS/事件/重发保留，eSIM特例走窄命令，不升格全局调度 |
| sim_account_manager.cpp | card/carrier/config | S（装配收窄）：主要是运营商能力门面，不是T账户聚合 |
| operator_config_cache.cpp、operator_file_parser.cpp、pdp_profile_rdb_helper.cpp | card/carrier/config | M路径/S平台或存储接缝：芯片IMS/Primary观察及APN属于配置能力；T账户字段不在此代写 |
| sim_state_tracker.cpp、operator_config_loader.cpp、operator_matching_rule.cpp | card/carrier/matching | S（Loader写入口）/M其余路径：匹配就绪跟踪不等于公开卡状态，账户资料更新经T |
| operator_config_hisysevent.cpp | card/carrier | M/C：专属诊断与状态跟踪，保留生产报告节点 |
| icc_operator_privilege_controller.cpp、icc_operator_rule.cpp | card/carrier/privilege | M路径/S设备资格：证书/TLV规则留此；ARA-M通道纳入共享域调查，不归eSIM Profile |
| esim_manager.cpp、esim_file.cpp、esim_controller.cpp | card/esim及profile/channel/provisioning；T/A提示归对应Owner | S/C：普通/低优先级对象、双结果/偏好节点、APDU/通道/MEP、验证任务接缝分开；保留Feature关闭实现 |
| start_osu_result_callback.cpp | card/esim/provisioning专属边界 | M/C：当前OnStartOsu空实现 [E42]，不能把类存在当任务闭环；高层OSU不复制进Core |
| radio_protocol_controller.cpp | device/radio | M路径/S安全关联：成熟协议保留；错误回包/超时及专用Primary抽取另验；不把业务参数/重试带入 |
| sim_char_decode.cpp、sim_number_decode.cpp、tag_service.cpp | card/records/codec（精确可复用target） | M/C：先审计外部/Phonebook/RIL消费者和平台头，不整体变纯support |
| sim_utils.cpp | 按功能拆为消息/卡工具及必要窄共享单元 | S/C：RIL反用工具，禁止整包Card导出或复制工具 |
| mcc_pool.cpp | card/carrier/matching并精确共享给搜网 | M/C：MCC表/初始化不是整个carrier接口，FFRT/平台依赖不能漏 |
| plmn_file.cpp | card/records的PLMN数据/编解码适配 | M/C：Parcel消费者及公开类型保留，不因文件名直接标纯codec |
| voice_mail_constants.cpp | card/records | M/C：语音信箱配置与Core/EXT依赖，不能泛化成纯常量池 |
| sim_cache_sync_manager.cpp | X：保留未启用状态，另裁去留 | 根产品54源不包含此文件；不因“全量迁移”把它顺带装配 |

本表显式覆盖55个cpp，其中54个属于本基线根产品源表。头文件中 `sim_data_type.h`、`sim_constant.h`、`sim_rdb_info.h`、`operator_config_matchsim_info.h`、`operator_config_hisysevent_enum.h`、`usim_function_handle.h` 无独立同名cpp，须按消息/字段/能力消费者归属而非统一移support。`services/sim/test/` 的文件是测试，不计生产能力。

### 16.2 哪些能“直接移动”

FileController、独立解析、ADN低层、短信/STK成熟文件族可先做**语义不变路径切片**，前提是原依赖/对象/Feature/通知均保留，源表和消费者全部更新。路径切片本身可暂保旧耦合并明确未达稳态；不能同时宣称跨Owner解耦已完成。

Controller、Monitor、StateManager/Handle、EsimManager/File、Loader、Detect、Rdb/Helper、CardRuntime需责任切片，不能整类机械迁移后作为最终结构。任何同字段写者必须一次闭合完整WU：初始化、主入口、后台重载、恢复、回调、厂商Hook、查询回写均计入；不能只迁公开setter。

## 17. 后续实施与验收建议

### 17.1 顺序与退出条件

| 顺序 | 后续工作 | 退出条件 |
| --- | --- | --- |
| 1 | 架构师批准局部修订B+与责任/契约规则 | 有明确批准记录；本报告提交仍为Proposed，不据此自动搬代码 |
| 2 | 同步主设计、详细设计与既有导航/执行状态 | 唯一稳态树一致；逻辑Owner不变；本报告的新发现回到原WU/问题台账，不创建平行体系 |
| 3 | 在可审查切片内重组当前骨架 | 全88文件映射（含86 C++）无遗漏；保留类型、成员布局、实现token、对象身份，迁移账本追溯；不启用生产 |
| 4 | 同步GN/include/checker/测试源表 | 当前target边界先等值保留，再按真实依赖细分；`run.py --with-gn --migration`、严格语法/规则/负例/GN对象构建；精确public/visibility变更单列 |
| 5 | 成熟SIM能力渐进迁入 | 优先FileController/codec/Phonebook等可闭合能力；同一生产对象/发令者；Feature on/off及消费者兼容；G2a工作分支适配单独复验 |
| 6 | 大型混合类与完整写权单元解耦 | T/A单Writer、R各状态单Owner，Loader/Detect/恢复入口无旁路；无双写、双发、双重试；原部分效果/错误/通知差分 |
| 7 | 产品/系统/设备及资源安全验证 | 匹配OHOS/provider/厂商；真实初始化发布、SA/IPC、原SO产物/Feature、迟到/超时/关闭/共享域及生命周期验收 |
| 8 | 最终替代原生产路径 | 产品源表和运行装配仅选multisim；剩余旧入口移除或只保契约转发；不保留两套SIM业务实现；回退选择与切换粒度经批准 |

阶段并非要求等待全部外部缺口才开展文档/纯规则工作。设备结束未知只阻塞依赖它的新释放/隔离/重试或卸载保证；四卡/MEP新能力不夹入结构搬迁。原生产接口保持但无独立业务实现的兼容薄适配不属于长期双实现。

### 17.2 可执行验收场景

必须覆盖：插拔相同ICCID但新代次；发现写DB后观察失效；current/all独立读失败；卡MSISDN成功而DB失败/查询异步回写；force/普通/卫星激活差异；Primary四分支与旧回包清新状态；EF局部Refresh/LOADED偏序；PIN剩余次数/恢复；PBR与Phonebook缓存失效；STK/eSIM特例；eSIM普通/低优先级/ARA-M共享域，迟到Open、失败Close及重复释放；Modem重启与资源恢复。使用既有功能/WU对应Oracle，不用新成功桩代替设备事实。

卡协议结束、调用返回和设备资源结束分开断言：旧请求不得完成新请求，旧lease不得释放新lease，本地channelId清零不得构造resourceReusable。同一实例的生产写点需要受控执行域，入口处检查不能代替真正写点准入。

### 17.3 本报告本轮检查范围

本轮实际执行Git基线/远端恢复、源码与构建源表重计数、main与G2a包含关系、`python3 -B telephony_core_service/test/multisim_architecture_rules/check_structure.py`。该文本检查返回 `PASS {files:86, includes:156, targets:24}`。

报告质量脚本已实际PASS：18章、摘要252字符、20张表、6个围栏、118处相对链接（101个不同目标/锚点）、45个证据引用、64处关键符号/行号、55个src文件完整且不重复覆盖。路径均存在，源码锚点非空且未越界，显式测试路径存在。人工复核摘要/§13/§14/§18推荐一致，card/state与device/radio会话分责、support/catalog及T/A/R/O映射没有互相矛盾。工作区检查仅本报告一个新增文件；提交前另核实差异空白与index范围。自动检查不证明引用周围全部语义，语义结论仍受§2.2范围限制。

**没有运行** C++编译/链接、GN gen/Ninja、原生产单元/系统测试、匹配OHOS产品、Core/Manager完整OnInit、provider或设备验证；本轮纯文档修改无需借历史PASS充当新测试结果。没有候选树实现、性能基准或用户/Agent受控定位实验。

### 17.4 提交与远端核实方法

仅暂存本文件，使用路径限定提交，消息 `docs(multisim): add independent source directory architecture decision`。提交前再核实HEAD/index/工作区；提交后检查SHA和文件清单。推送前fetch核查origin/main是本提交祖先；若远端新增无关历史则保留本地commit并停止推送，不自动merge/rebase或强推。

正常推送到明确跟踪的origin/main后，以ls-remote核实实际远端tip，fetch后核实包含关系、报告tree路径及blob与本地一致。报告不硬写自身最终SHA，避免自引用；实际提交和Push验证结果在最终交付中给出。用户限定只改报告，故本轮不按一般任务惯例更新执行状态。

## 18. ADR 元信息与待决事项

| 字段 | 内容 |
| --- | --- |
| 标题 | 多 SIM 源码目录组织架构独立决策分析与建议 |
| 状态 | **Proposed** |
| 决策日期 | 2026-10-31 |
| 分析模型 | 本会话Codex（GPT-6）；未取得与上一轮不同底层模型的可核验标识，不声称多模型复现 |
| Git代码基线 | `main@6c98e170c6d95a3a39fcdfa1a192a6927cbfd783` |
| 被复核原ADR | `SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md`，在上述HEAD；其自身分析基线为b86f9e4，未把旧测试结果继承到本轮 |
| 独立推荐 | **有条件支持经过局部修订的B+**：六组导航；卡会话归card/state，Radio session归device/radio，SlotCatalog归support/H构建 |
| 独立性范围 | 源码先行的两阶段判断；已知候选/旧推荐，非完全盲审；子任务未完成，主要结论由主Agent核实 |
| 与原ADR主要差异 | 上述会话/catalog归属；carrier账户字段writer；Records/Phonebook细分DAG；STK/eSIM、ARA-M资源域、Detect装配顺序和技术helper语义的具体验收 |
| 不改变的基线 | T/A唯一事实写权；O非业务Owner；R各运行状态分责；共享RIL；异步/资源结束不变量；兼容与既有阶段门禁 |
| 待架构师批准 | 六组与必要二/三级目录；SessionRegistry/catalog归属；carrier字段接纳和依赖围栏；权威文档同步与迁移顺序 |
| 需后续独立裁决 | provider字段/事务及Port映射；真实资源域/结束契约；初始化缺陷修复；新的隔离/重试/固定目标政策；四卡/MEP、公开接口或SO变更 |
| 最终替代目标 | services/multisim独占SIM生产业务路径，services/sim不保留并行业务执行 |

下面为本报告自有源码证据索引，原ADR观点不作为源码证据。链接为Git相对路径与固定基线行号；在不支持源码行锚的本地阅读器中，可用路径和函数名定位。

### 18.1 关键写点、回调与构建的精确定位

较长文件不能只靠起始入口定位。下表补充关键副作用/反证的准确行号，供审批者直接回查；这些是当前实现事实，修复或接管方案见前文。

| 事实 / 待保护节点 | Git相对源码定位 |
| --- | --- |
| product 51基础SIM源与3个eSIM条件源 | [根源表起点](../BUILD.gn#L67)、[Feature条件源](../BUILD.gn#L139) |
| 检查器target/classification/public与环 | [allowed_edge](../test/multisim_architecture_rules/check_structure.py#L64)、[testonly](../test/multisim_architecture_rules/check_structure.py#L146)、[target检查](../test/multisim_architecture_rules/check_structure.py#L158) |
| Detect先构造、Controller后创建、weak捕获 | [OnInit](../services/sim/src/sim_manager.cpp#L46)、[InitSingleSimObject](../services/sim/src/sim_manager.cpp#L133)、[构造](../services/sim/src/card_file_detect_manager.cpp#L26) |
| Detect独立Insert/Update及刷新 | [InsertCardFileDetectData](../services/sim/src/card_file_detect_manager.cpp#L98)、[UpdateCardFileDetectData](../services/sim/src/card_file_detect_manager.cpp#L113)、[SaveCardFileDetectData](../services/sim/src/card_file_detect_manager.cpp#L127) |
| ICCID匹配/建行与一次字段计划 | [InitIccId](../services/sim/src/multi_sim_controller.cpp#L421)、[SimDataBuilder](../services/sim/src/multi_sim_helper.cpp#L144) |
| current/all独立加载与不同排序 | [GetListFromDataBase](../services/sim/src/multi_sim_controller.cpp#L733)、[GetAllListFromDataBase](../services/sim/src/multi_sim_controller.cpp#L759)、[排序区段](../services/sim/src/multi_sim_controller.cpp#L786) |
| Unknown/缺卡分支及账户加载资格 | [RefreshData](../services/sim/src/multi_sim_monitor.cpp#L345)、[ResetSimLoadAccount](../services/sim/src/multi_sim_monitor.cpp#L775) |
| StateHandle账户/Primary副作用 | [HandleSimAbsent](../services/sim/src/sim_state_handle.cpp#L448)、[ProcessNewSimStatus](../services/sim/src/sim_state_handle.cpp#L459)、[SenseSwitch卡态过滤](../services/sim/src/sim_state_handle.cpp#L634) |
| 普通force、卫星不同激活轨迹 | [SetActiveCommonSim](../services/sim/src/multi_sim_controller.cpp#L976)、[SetActiveSimSatellite](../services/sim/src/multi_sim_controller.cpp#L1041) |
| Voice/SMS/Data不同提交轨迹 | [Voice](../services/sim/src/multi_sim_controller.cpp#L1280)、[SMS](../services/sim/src/multi_sim_controller.cpp#L1339)、[Data](../services/sim/src/multi_sim_controller.cpp#L1406)、[Data参数节点](../services/sim/src/multi_sim_controller.cpp#L1701) |
| Primary分支、参数恢复与专用执行 | [SetPrimarySlotId](../services/sim/src/multi_sim_controller.cpp#L1483)、[SetPrimarySlotIdWithoutModemReboot](../services/sim/src/multi_sim_controller.cpp#L2071) |
| 查询/写号码、DB/两缓存、IMPU回写 | [GetShowNumber](../services/sim/src/multi_sim_controller.cpp#L1720)、[SetShowNumber](../services/sim/src/multi_sim_controller.cpp#L1757)、[SetShowNumberToDB](../services/sim/src/multi_sim_controller.cpp#L1774)、[GetSimTelephoneNumber](../services/sim/src/multi_sim_controller.cpp#L1861) |
| MSISDN真正缓存写点与LOADED输出顺序 | [ProcessSetMsisdnDone](../services/sim/src/sim_file.cpp#L688)、[OnAllFilesFetched](../services/sim/src/sim_file.cpp#L244) |
| PIN直调RIL及卡态恢复 | [Handle::UnlockPin](../services/sim/src/sim_state_handle.cpp#L204)、[Manager::UnlockPin](../services/sim/src/sim_state_manager.cpp#L197) |
| Records/Phonebook低层复用与static关联 | [IccFile包含Handler](../services/sim/include/icc_file.h#L27)、[Handler包含FileController](../services/sim/include/icc_dialling_numbers_handler.h#L23)、[static requestMap](../services/sim/src/icc_dialling_numbers_handler.cpp#L32) |
| STK到eSIM专属命令/延时关闭 | [ProcessEventExt](../services/sim/src/stk_controller.cpp#L326)、[OnSendRilProactiveCommand](../services/sim/src/stk_controller.cpp#L426)、[EsimController::ProcessCommandMessage](../services/sim/src/esim_controller.cpp#L68) |
| carrier资料Update与芯片/Primary观察 | [InsertOpkeyToSimDb](../services/sim/src/operator_config_loader.cpp#L102)、[UpdateImsCapFromChip](../services/sim/src/operator_config_cache.cpp#L503) |
| 权限通道开通/超时/关闭 | [OpenChannel](../services/sim/src/icc_operator_privilege_controller.cpp#L297)、[ProcessOpenLogicalChannelDone](../services/sim/src/icc_operator_privilege_controller.cpp#L314)、[ProcessCloseLogicalChannelDone](../services/sim/src/icc_operator_privilege_controller.cpp#L398) |
| eSIM开关通道、本地清理与迟到处理 | [SyncCloseChannel](../services/sim/src/esim_file.cpp#L133)、[ProcessEsimOpenChannel](../services/sim/src/esim_file.cpp#L369)、[OpenDone](../services/sim/src/esim_file.cpp#L390)、[CloseDone](../services/sim/src/esim_file.cpp#L437) |
| eSIM普通/低优先级实例与不同副作用节点 | [OnInit](../services/sim/src/esim_manager.cpp#L36)、[Manager::DisableProfile](../services/sim/src/esim_manager.cpp#L115)、[SwitchToProfile](../services/sim/src/esim_manager.cpp#L318)、[File::DisableProfile](../services/sim/src/esim_file.cpp#L826) |
| 外部66250与OSU调用边界 | [服务ID](../frameworks/native/src/esim_service_client.cpp#L29)、[StartOsu委托](../frameworks/native/src/esim_service_client.cpp#L168) |
| Radio双槽限制/错误回包/单份激活响应 | [Init](../services/sim/src/radio_protocol_controller.cpp#L32)、[ProcessResponseInfoOfEvent](../services/sim/src/radio_protocol_controller.cpp#L403)、[SetActiveSimToRil](../services/sim/src/radio_protocol_controller.cpp#L471)、[ProcessActiveSimToRilResponse](../services/sim/src/radio_protocol_controller.cpp#L492) |
| 卡应用会话按应用索引并保留代次 | [Current](../services/multisim/runtime/session/session_registry.cpp#L36)、[PublishVerifiedSession](../services/multisim/runtime/session/session_registry.cpp#L61)、[EndExpectedWait](../services/multisim/runtime/session/session_registry.cpp#L107) |
| 资源真正释放需结束/可复用/效果证据 | [Complete](../services/multisim/runtime/resource/resource_barrier.cpp#L142)、[AcceptRecoveryEvidence](../services/multisim/runtime/resource/resource_barrier.cpp#L171) |
| TelRilSim回传SIM控制消息/错误 | [ErrorIccIoResponse](../services/tel_ril/src/tel_ril_sim.cpp#L84)、[ProcessIccIoInfo](../services/tel_ril/src/tel_ril_sim.cpp#L117) |

[E01]: ../BUILD.gn#L30
[E02]: ../services/multisim/BUILD.gn#L7
[E03]: ../services/core/src/core_service.cpp#L92
[E04]: ../services/sim/src/sim_manager.cpp#L41
[E05]: ../services/sim/src/multi_sim_controller.cpp#L231
[E06]: ../services/sim/src/multi_sim_monitor.cpp#L281
[E07]: ../services/sim/src/multi_sim_controller.cpp#L976
[E08]: ../services/sim/src/multi_sim_controller.cpp#L1483
[E09]: ../services/sim/src/multi_sim_controller.cpp#L1280
[E10]: ../services/sim/src/multi_sim_controller.cpp#L1720
[E11]: ../services/sim/src/sim_state_manager.cpp#L197
[E12]: ../services/sim/src/sim_file.cpp#L150
[E13]: ../services/sim/src/sim_state_handle.cpp#L403
[E14]: ../services/sim/src/icc_dialling_numbers_manager.cpp#L35
[E15]: ../services/sim/src/sim_sms_controller.cpp#L134
[E16]: ../services/sim/src/stk_controller.cpp#L326
[E17]: ../services/sim/src/sim_account_manager.cpp#L42
[E18]: ../services/sim/src/operator_config_loader.cpp#L102
[E19]: ../services/sim/src/icc_operator_privilege_controller.cpp#L297
[E20]: ../services/sim/src/esim_manager.cpp#L36
[E21]: ../services/sim/src/esim_file.cpp#L77
[E22]: ../services/sim/src/radio_protocol_controller.cpp#L103
[E23]: ../services/multisim/runtime/session/session_registry.h#L28
[E24]: ../services/multisim/foundation/sim_identity.h#L44
[E25]: ../services/multisim/runtime/resource/resource_barrier.cpp#L142
[E26]: ../services/tel_ril/src/tel_ril_sim.cpp#L84
[E27]: ../services/network_search/src/network_search_handler.cpp#L590
[E28]: ../services/multisim/foundation/slot_catalog.h#L27
[E29]: ../services/sim/src/sim_rdb_helper.cpp#L585
[E30]: ../services/sim/src/card_file_detect_manager.cpp#L26
[E31]: ../services/sim/include/icc_dialling_numbers_handler.h#L23
[E32]: ../services/sim/src/multi_sim_controller.cpp#L2071
[E33]: ../services/multisim/runtime/card_runtime.h#L70
[E34]: ../services/multisim/topology/topology_service.h#L48
[E35]: ../services/multisim/assignment/assignment_service.h#L35
[E36]: ../services/multisim/runtime/euicc/local_card_requests.cpp#L23
[E37]: ../interfaces/innerkits/include/telephony_types.h#L30
[E38]: ../test/multisim_architecture_rules/check_structure.py#L32
[E39]: ../telephony_core_service.gni#L21
[E40]: ../services/multisim/foundation/orchestration/runtime_operation_store.h#L27
[E41]: ../services/sim/src/telephony_data_helper.cpp#L26
[E42]: ../services/sim/src/start_osu_result_callback.cpp#L27
[E43]: ../services/sim/src/operator_config_cache.cpp#L503
[E44]: ../services/sim/include/icc_file.h#L27
[E45]: ../services/sim/src/radio_protocol_controller.cpp#L32
