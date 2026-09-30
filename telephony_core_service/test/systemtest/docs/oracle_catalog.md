# Oracle 来源表与审查规则

基线：2841c6f60635853a929c1755dc08f5543a89eb83 + source_manifest。源码阅读只能形成候选行为。首批O-L01..05仍没有runtime_verified Legacy基线；新增O-L06已在批准的容量切片采集，见文末；VERIFIED_INVARIANT 业务项目前为0，避免把目标模型当已核实不变量。O-H项是测试设施契约，不冒称SIM产品业务契约。

| Oracle ID | 分类 | 主张 / 来源 | 适用前提 | 当前断言策略 / 状态 |
|---|---|---|---|---|
| O-C01 | CONFIRMED_CONTRACT | 状态含义：UNKNOWN/NOT_PRESENT/LOCKED/NOT_READY/READY/LOADED 不混同；interfaces/innerkits/include/sim_state_type.h:87–118；interfaces/kits/js/@ohos.telephony.sim.d.ts:2747–2793 | 已初始化、有效并获准Slot；合成输入按真实RIL协议合法构造；收敛后判断状态语义，不固定READY vs LOADED的最终精确值 | 强制：Absent→NOT_PRESENT；PIN→LOCKED；Ready数据完整→READY或LOADED；业务成功错误码的具体值另审；DOCUMENT_VERIFIED |
| O-C02 | CONFIRMED_CONTRACT | 账户 slotIndex 是设备槽索引，simId 是卡标识；interfaces/kits/js/@ohos.telephony.sim.d.ts:2250–2290；core_service_client.h:358–367 | 查询成功并返回账户时；不保证账户何时可见或simId生成规则 | 强制：返回账户的slotIndex与所查询有效Slot一致；simId只作不透明身份记录；DOCUMENT_VERIFIED |
| O-C03 | CONFIRMED_CONTRACT | ICCID查询是指定Slot的卡身份；interfaces/innerkits/include/core_service_client.h:240；core_service_sim.cpp:178–196 | 授权系统/native caller；成功查询；已通过RIL文件响应提供合成卡数据 | 强制：成功结果与对应合成卡在内存一致；不在日志输出值；DOCUMENT_VERIFIED |
| O-H01 | CONFIRMED_CONTRACT | 测试拓扑、隔离、脱敏和结果真实性；用户任务第3/6/9/10/12节 | 全部准备/执行/报告 | 强制：业务生产逻辑实际执行才可标B通过；无真实身份外泄；cleanup failure阻断下一例；未执行不能PASS；USER_REQUIREMENT |
| O-H02 | CONFIRMED_CONTRACT | 有界等待且不以固定sleep/线程顺序作为正确性；用户任务第10节 | 所有异步例 | 强制：达到声明的公开收敛条件或timeout明确失败/阻塞；超时预算是测试设施参数而非产品SLA；USER_REQUIREMENT |
| O-H03 | CONFIRMED_CONTRACT | 真实卡危险操作默认禁用；用户任务第9/10节 | PIN/Profile等所有入口 | 强制：本套件只允许合成拓扑，真实硬件后端不执行UnlockPin/Profile删除/reset；USER_REQUIREMENT |
| O-L01 | LEGACY_BEHAVIOR | HasSimCard/GetSimState 接收成功与回调成功两个通道；core_service_sim.cpp:63–120；core_service_client.cpp:364–403 | 待运行采集；并非已采集基线 | 当前禁止精确业务断言；运行后记录返回、callback有无及错误；STATIC_CANDIDATE_NOT_CAPTURED |
| O-L02 | LEGACY_BEHAVIOR | 无卡状态候选success+NOT_PRESENT、空active list候选NO_SIM_CARD；sim_manager.cpp:176–184；multi_sim_controller.cpp:1965–1983 | 外部边界确立Absent；已初始化；无历史账户环境 | 待运行；不可直接认定永久错误码契约；STATIC_CANDIDATE_NOT_CAPTURED |
| O-L03 | LEGACY_BEHAVIOR | Ready身份文件加载后账户可见候选行为；sim_manager.cpp:840起；multi_sim_controller.cpp:1204–1234 | 生产文件加载、DataShare成功、Active策略固定 | 待运行记录账户数量/错误/映射；未Ready不能臆定隐藏所有账户；STATIC_CANDIDATE_NOT_CAPTURED |
| O-L04 | LEGACY_BEHAVIOR | SetShowName的账户通知与返回候选；sim_manager.cpp:445–456；core_service_sim.cpp:571–641 | 授权、双卡合成数据、成功存储响应 | 待运行记录局部修改与所有外部影响；不精确一次；STATIC_CANDIDATE_NOT_CAPTURED |
| O-L05 | LEGACY_BEHAVIOR | PIN状态转换及解锁响应转发候选；sim_state_handle.cpp:918–923；core_service_sim.cpp:688–733 | PIN合成输入/响应，无真实卡 | 待运行记录LockStatusResponse/状态/事件；不臆定PIN错误的次数语义；STATIC_CANDIDATE_NOT_CAPTURED |
| O-Q01 | OPEN_QUESTION | 未知/未就绪/无卡及错误码区分；sim_manager.cpp:155–184；sim_state_handle.cpp:151/:164 | SIM负责人确认初始化和错误语义 | 不得强制业务断言；PENDING_OWNER |
| O-Q02 | OPEN_QUESTION | Primary与默认Data的联动条件；multi_sim_controller.cpp:1522/:1689 | Radio策略、平台配置与用户设置路径 | 不得强制总是联动/总是不联动；PENDING_OWNER |
| O-Q03 | OPEN_QUESTION | 无有效卡时Voice/SMS/Data/Primary无效值；telephony_types.h:40–44；multi_sim_controller.cpp:1234起 | 各角色接口分别确认；失败返回与out参数分别记录 | 不得断言默认角色必须始终有效；PENDING_OWNER |
| O-Q04 | OPEN_QUESTION | Profile停用后的active/all账户可见性；multi_sim_controller.cpp:1965/1985；eSIM条件构建 | Profile状态与SIM Active不能混同；需eSIM SDK/设备 | 不得强制账户删除/隐藏；PENDING_OWNER |
| O-Q05 | OPEN_QUESTION | simId稳定性、Slot逆映射与Subscription的关系；sim_manager.cpp:622–651；MultiSIM/07 第2/4节冲突 | 当前无新的Subscription公共模型；重启/换卡分别确认 | 不得断言simId==slot、永久唯一或跨重启不变；同轮双向映射先采集；PENDING_OWNER |
| O-Q06 | OPEN_QUESTION | 事件重复、必要数量、公共事件与查询的偏序；sim_state_handle.cpp:481/:827/:905–928 | 订阅时点、状态改变与重发条件 | 不强制exactly-once；次数上限及事件必达待确认；PENDING_OWNER |
| O-Q07 | OPEN_QUESTION | 过渡期一致性、局部改名隔离与允许的角色联动；multi_sim_controller.cpp、sim_manager.cpp:445–456；用户第8节 | 仅稳定终态；业务负责人确定SIM1哪些属性不得变 | 暂不强制双卡完整快照一致、毫秒时序或并发完全确定；PENDING_OWNER |
| O-Q08 | OPEN_QUESTION | 公开转发入口对slot0/1的capability拒绝；multi_sims_capability_manager.cpp:30–32；.h:23；core_service_sim.cpp:98 | 当前静态分支slot!=3拒绝；不能用桩覆盖生产判断 | 阻塞公开入口正常业务场景；由负责人确认是当前需求/缺陷/需下层组件范围；PENDING_OWNER |
| O-Q09 | OPEN_QUESTION | 必要对外请求、重试与请求参数；i_tel_ril_manager.h:206；sim_state_handle.cpp:551；生产账户DataShare请求 | 只有明确外部协议参数可检查；初始化允许额外合法请求 | RIL状态/文件请求先记录；无契约不要求精确一次或内部偏序；PENDING_OWNER |
| O-Q10 | OPEN_QUESTION | Ready/PIN场景active账户数量及权限剥离行为；core_service_sim.cpp:376/:644；multi_sim_controller.cpp:1204/:1965 | 权限、历史记录、Active/profile分别固定 | 不得强制Locked没有账户或所有Ready均自动active；PENDING_OWNER |

## 审核与晋升

每个业务EXPECT/ASSERT引用Oracle ID；confirmed只约束来源支持的语义。精确错误码、账户数量、事件必须发生、跨卡隔离等先经过业务审核或实际基线采集。源码候选O-L在真正B运行后可带输入/输出证据晋升为runtime captured LEGACY_BEHAVIOR，但仍仅差分评审，不自动升级为永久需求。OPEN_QUESTION不得加入自动业务判错。

O-C01只确认状态含义，RIL CardStatusInfo字段合法性与完整加载条件需要接口SDK验证；如场景无法构造Ready/PIN则BLOCKED，不把错误桩输入当产品失败。Ready时READY/LOADED均可，账户是否收敛独立确认。

“服务可访问”仅记录公开方法实际接收/完成，不等同SA注册或SIM就绪。初次没有RIL状态输入时UNKNOWN/NOT_READY/NOT_PRESENT的具体表现只采集O-Q01/O-L01。

负向控制只破坏已确认的测试输出条件：如报告将合成身份内存相等布尔值由true改false，验证O-C03检查器能失败；或清理结果/执行标志变假验证O-H01。不得改生产代码、不得制造不合法RIL协议后归责产品，不使用未知角色行为制造红灯。

## 已实施容量切片新增Oracle

- O-C11 / CONFIRMED_CONTRACT：sim.d.ts:701–710将getMaxSimCount定义为槽“数量”，仅强制非负；组件返回确切值不由JS文档证明。
- O-L06 / LEGACY_BEHAVIOR / RUNTIME_CAPTURED：当前head+工作树在capacity-parameter-v1单槽、未OnInit、虚拟Modem关闭时观测1；原始XML/JSON及覆盖证据在输出目录。具体值只差分审核，不等同所有产品永久单槽。

只有O-L06采集成功，原五条O-L01..05仍是STATIC_CANDIDATE_NOT_CAPTURED。O-C11负向控制破坏观察证据而非生产代码，仅证明检查器能检出负数量，不证明RIL/参数故障处理正确。


O-Q11：未初始化组件公开查询候选；OPEN_QUESTION，源码sim_manager.cpp:155–184/613–620。实际链接失败，未采集；不建立Legacy基线，不设置强制业务断言。见SIM-CHAR-001设计。
