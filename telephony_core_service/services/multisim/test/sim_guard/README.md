# SIM 跨架构契约防护网

本工程链接未经修改的 `services/sim` 卡状态/PIN/PUK实现，由 RIL 正式通知和请求响应驱动，读取真实公开状态能力及 StateRegistry 出站通知。它是 Linux 组件业务测试；不是完整 SimManager 初始化、IPC、设备、账户或 eSIM 验收。v2历史结果见 [evidence.json](evidence.json)，第二轮结果见 [evidence_round2.json](evidence_round2.json)，范围/规则/缺口见 [inventory.json](traceability/inventory.json)及[增量场景清单](traceability/round2_catalog.json)。

2026-11-05整理：首批和第二轮资产已保存到main，当前已执行范围如下。第二轮实际运行基线为01db5ce6加当时dirty工作树，输入摘要已对应保存代码c508bd03；不是在最新文档HEAD上重跑。整体交付、G2a分支合入状态及剩余门禁统一见[执行状态 §0](../../../../docs/SIM_Refactor_Execution_State.md#current-status)，本批合入没有改动本工程业务代码、Oracle或历史证据，SIM Guard未重跑；本批新增G2a重验见执行状态§11。

| 分类 | 已保存执行结果 | 状态解释 |
| --- | --- | --- |
| v2原兼容集 | 7条×5轮，35 PASS | 原断言保留，已包含在第二轮总兼容统计内 |
| 第二轮兼容扩展 | 新增10条×5轮，50 PASS；总计17条、85 PASS/0 FAIL | 卡生命周期、PIN/PUK、双槽交错和组件重建；不计成完整初始化/持久化恢复 |
| 独立缺陷/待裁决集 | 5条×5轮，25 FAIL | 保留迟到/换卡/PIN/Refresh问题；不能把5条候选都当成已批准规则，修复政策/返回契约单独裁决 |
| 设施与有效性 | 19项设施PASS；READY→LOCKED突变检出，恢复后17 PASS | 与业务兼容/缺陷分开计数；`--suite=all`因独立缺陷返回非零 |
| Multisim及目标新能力 | BLOCKED，未运行真实新实现 | Adapter接入准备不代表通过；四卡/三待等按正式产品能力另验 |

从 Git 根运行。GoogleTest 固定来源和摘要沿用 [dependency_lock.json](../systemtest/configs/dependency_lock.json)，准备工具会校验归档及解包树；构建不下载依赖。以下变量由本机选择，产物必须在 Git 仓库外：

```sh
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o "$SIM_GTEST_ARCHIVE"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
# SIM_GTEST_ROOT 为上一步打印的固定版本源码目录
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat" --repeat=5 --mutation-check
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=defect --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/defect"
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=multisim --suite=compat --output "$SIM_GUARD_OUTPUT/multisim"
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=target --product=nezha_4c3s --output "$SIM_GUARD_OUTPUT/target"
```

依赖为 Linux、Clang/LLVM18、gold、GN/Ninja、Python3、pthread、libxml2 开发头。GN 复用 `services/multisim/test/systemtest/.gn`，目标 `sim_guard_legacy`。runner 的 `result.json/commands` 记录实际 GN、Ninja、测试二进制和覆盖导出命令及退出码，`artifacts` 定位每次独立执行目录。每次先写 BLOCKED，准备失败不会复用历史 XML；失败/超时返回非零，跳过或零用例不能通过。测试有 2 秒事件排空条件和 45 秒进程 watchdog，不以固定 sleep 判断业务成功。

v2留存的七条 Compatibility（第二轮继续执行）：

| 测试 | 强断言及边界 |
| --- | --- |
| LIFE_001 | 无卡→PIN锁定→READY→拔卡；公开存在性、状态、卡型及每次转换新增通知 |
| LIFE_002 | SIM拔卡后换成USIM；公开状态/卡型；不推断账户身份 |
| PUK_001 | PUK锁定及拔卡；不宣称已验证PUK解锁 |
| PIN_001 | 密码错误、剩余次数、解锁成功、后续RIL READY；不冻结非契约中间时序 |
| PIN_002_CommandFailure | 外设命令失败不得映射为解锁成功 |
| DUAL_001 | 双物理槽交错状态、移除/恢复；分别核对两槽真实查询 |
| RIL_001_DuplicateAndError | 错误回包实际注入并保留观测；正确READY回包及重复回包的最终状态。错误后的状态保持策略仅作C级观察，不作强断言 |

强断言依据是 `sim_state_type.h` 公开状态/卡型/解锁结果定义、按槽查询/事件的正式含义；未批准的默认卡策略、通用去重、精确通知次数不进入 Oracle。通知检查仅搜索本步骤新增记录，避免旧通知掩盖漏发。`DEFECT_001_StaleReadyAfterRemoval` 独立运行且保留真实失败：先前发出的 READY 查询在移除确认后送达，不应使物理无卡显示为 READY；其代次/拒绝政策仍需业务裁决，不能把错误行为固化为兼容预期或用 xfail/skip 隐藏。

`--mutation-check` 在仓外生成一份生产 `sim_state_handle.cpp` 的临时副本，把真实 READY→READY 映射破坏为 READY→LOCKED，用相同场景和 Oracle 运行，要求得到 GoogleTest 断言失败；随后原始生产二进制重新执行兼容集并通过。生产工作树从不被修改。副本、失败XML、恢复XML、源码摘要均进入运行证据；突变失败不计入正常业务失败/通过数量。

依赖边界：有状态 [FakeRil](fakes/ril_device.h) 只保存设备事实、PIN设备应答、请求及可重排/重复/迟到响应；真实 SIM 决定公开状态。RIL接口、平台参数、CommonEvent、Registry、宿主类型映射集中在 [Legacy Adapter](adapters/legacy/legacy_sim_adapter.cpp)。真实 TelEventHandler/TelEventQueue、ObserverHandler、CoreManagerInner 和 EXT 均链接；CoreManagerInner 的 SIM manager 路由未装配，账户副作用未覆盖。SA查找明确不可用，Registry/CES为外服务Spy，FFRT为有限host运行时，不证明设备调度等价。无DataShare/LPA调用的切片不创建虚假Store/LPA实现。

场景、Contract Driver、Oracle、Fake语义由 [semantic_baseline.json](semantic_baseline.json) 摘要冻结。第二轮为复用Driver仅将原fixture移至共享头；[v2摘要](semantic_baseline_v2.json)、四份原业务资产及原七条测试代码块摘要另外受检，不允许通过新版本摘要静默改变旧断言。runner还检查共同代码include边界，记录生产源码、测试/host输入摘要和源级正执行证据。业务规则变化需独立契约依据、版本和审查说明。

未来接入步骤见 [multisim绑定说明](adapters/multisim/README.md)。当前 `--sut=multisim` 和未接入的 target/nezha 配置明确 BLOCKED、executed=false，绝不回落到 legacy 或返回预置结果。

剩余工作：匹配Provider的14个SimData字段/URI/schema/错误协议后，扩展完整初始化、账户/角色及持久化恢复；单独接入真实eUICC APDU/Profile路径及SA66250边界。完整下载编排属于SA66250，本仓 `IEsimManager` 的 `AuthenticateServer/PrepareDownload/LoadBoundProfilePackage` 不等于 `DownloadProfile`。nezha三待、默认回选、删除/取消回滚、OOBE保留规则和真实卡托/注网时延分别裁决及设备验证。不能把这七条与报告392条换算成覆盖率。

第二轮新增十个兼容场景，复用相同Driver/Observer/Oracle：单卡枚举、双卡逆序枚举、RIL不可用后真实对象重建、异常载荷恢复、丢包后重新查询、PUK错误及成功、显式状态刷新、双卡对象销毁重建、停机换卡重新枚举、PIN请求与另一槽拔卡交错。初始/重建时的临时状态仅记录C级观察；强断言针对已知设备事实对应的公开状态、卡型和正式解锁结果。重建过程验证旧对象weak引用全部失效、worker退出及新对象重新创建；这是硬件重新枚举，**不是数据库持久化恢复验收**。

```sh
# 全报告包含原7条、新增10条和5条独立缺陷；已知FAIL使进程非零退出。
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/all" --repeat=5 --mutation-check
```

`suite_counts` 分开统计v2、扩展兼容集、全部兼容集及缺陷；`defect_inventory`在仅运行compat时也保留全部已知问题并标NOT_RUN，不能以兼容集PASS隐去缺陷。新增缺陷包括替换卡被旧READY覆盖、通用RIL错误被解释为错误PIN、旧PIN结果污染新卡锁态。`RefreshSimState`返回值还存在Client头注释（成功码）与真实委托/历史测试（状态值）冲突：正常场景仅断公开状态刷新，成功码断言完整保留在DEFECT_005，待正式裁决；不改成返回2的绿色基线。

完整初始化/账户/默认角色/数据库重启受阻的当前编译和协议证据见 [Provider契约](traceability/provider_contract_round2.json)。本轮实际重跑编译审计与eSIM编译尝试，失败不计业务PASS；没有匹配fork字段或LPA契约时不建立会补写业务状态的Fake Store/LPA。

## SIM-OC-013 独立 PIN 记忆契约增量（2026-11-09）

[新用例](runner/pin_memory_contract_test.cpp) 复用真实 Legacy Adapter、RIL 设备语义和生产 StateManager/Handle，经真实 wrapper 函数指针接入外部 Spy。它是独立的 `sim_guard_pin_memory` 主机目标，不改变原17条兼容场景、5条缺陷、共同 Oracle 或冻结摘要；本套件结果单列，不并入历史85 PASS。

| 用例 | 实际断言与边界 |
| --- | --- |
| PinSuccessUsesOriginalSlotIdentityAndCredential | 双槽下错误 PIN 零 Hook，成功才发送正确 slot/ICCID/PIN_ENABLE/合成 PIN；另一槽公开状态不变 |
| PukSuccessUsesNewPinAndPinAlterOperation | 错误 PUK 零 Hook，成功时传新 PIN 和 PIN_ALTER，不能误传 PUK；生产方法是 UnlockPuk |
| FailedPinDoesNotPublishMemoryHook | 真实失败解锁结果，不发送资产记忆 Hook |
| RemovalUsesAbsentResponseIdentityWithoutCredential | 真实拔卡发送 SIM_ABSENT 与空凭证，载荷身份为当前无卡响应的 ICCID |

凭证仅使用内存合成值，Spy只输出比较布尔值和调用次数，不在断言、日志/XML/JSON打印实际或预期 PIN/PUK。RAII在真实对象销毁与worker停止后恢复原Hook，避免跨例污染。当前生产在处理无卡状态时先把 `iccid_` 更新成无卡响应（本场景为空）再发拔卡Hook；第四例**只是现行为刻画**，不证明旧卡资产可按ICCID安全清除，也不批准厂端清理政策。

从Git根运行，使用与原套件相同的已校验固定GTest源码；所有产物留在仓外：

```sh
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_pin_memory_contract.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/pin-memory" --mutation-check
```

[独立 runner](run_pin_memory_contract.py) 检查原冻结资产、依赖树与平台头摘要、四例集合、真实生产 `UnlockPin/UnlockPuk/HandleSimAbsent` 正执行，并记录实际命令、源码/构建输入摘要。`--mutation-check`只在仓外生产源码副本将 UnlockPuk 的 Hook 从 PIN_ALTER 改为 PIN_ENABLE；新 Spy 必须检出失败，之后原生产二进制四例重新通过。默认空的 `sim_guard_manager_source` 仅为该负控提供输入，正常构建仍编译原生产Manager。

本增量没有接入 AlterPin、锁启停、FDN、迟到拒绝/恢复政策、完整Core/Manager初始化、IPC、vendor asset或设备自动解锁；不能把L1函数指针载荷断言称为L2/L3安全验收。旧独立缺陷及待裁决项完整保留。先前4例通过及突变恢复结果属于撤回旧代码修改之前的工作树，不作为当前验收。恢复旧基线后的结果统一见[执行状态§12](../../../../docs/SIM_Refactor_Execution_State.md#open-closed-followup-20261109)和[当前证据](../systemtest/configs/open_closed_followup_20261109.json)。测试通过外部函数指针观察现有行为，不加入生产friend或探针；负控只在仓外副本施加故障，不修复旧实现。

## SIM-028 主卡切换：冻结基线与共用契约（2026-11-12）

本节扩展同一防护网，复用systemtest的GN、GTest、七个原场景及独立进程运行；不另建SIM实现或平行业务测试。旧生产代码只读，Golden业务预期不变。当前冻结仅针对明确双卡前态的条件性Host业务，不是原生产品标准。

冻结清单见[基线及SHA](../systemtest/configs/sim028_shared_primary_20261112/frozen-baseline.json)：锁定HEAD、Golden、原七例、原Fixture/Runtime、GN和执行脚本；`frozen-inputs.tar.gz`保存修改前实际字节。清单同时固定上一批50轮证据索引和全部42份证据摘要，保留350业务PASS、原始50 PASS/300生命周期FAIL及非零退出码。代码基线是HEAD加当时工作区字节，不能仅凭HEAD复现。旧单卡反例及中间失败继续保留。修改后测试源码的摘要另记本批验证，不覆盖冻结快照。

### 可复用资产与适配边界

| 资产 | 路径与职责 | Legacy耦合处理 |
| --- | --- | --- |
| Scenario | `scenarios/primary_slot.json`：卡、账户、公开角色、输入、设备映射及应答条件、七个稳定场景ID | 公共前态不要求SimManager、三逻辑对象、订阅图或具体构造函数。原准备代码文字放在legacy证据字段；第三逻辑对象仅为Legacy装配细节 |
| Contract | `contracts/primary_slot.h`：测试端口与纯标准C++观测格式 | 不include OHOS、SIM或multisim；正式输入只有公开setter的slot。userSet=true是Core入口的内部翻译约束，非新增公开参数 |
| Oracle | `oracle/primary_oracle.py`：读取实际API/公开查询/参数/发布/RIL事实，检查条件并归一化 | 不读取私有字段、对象地址、动态类型、队列或绝对session。不能从Golden直接生成实际结果 |
| Legacy Adapter | `adapters/legacy/legacy_primary_adapter.h` | 透明转调真实CoreServiceSim；公开查询与外部替身观测仍由现有RealScenario提供。未复制业务实现。无RIL例只进入真实Manager，不能算Core全链 |
| Runner | `runner/run_primary_contract.py` | 调用原Legacy执行脚本，原GTest、StrictMock、清理失败分类均不替换；原始exit1仍返回1。额外共享Oracle失败也返回非零 |
| Target接缝 | 同一端口/Scenario/观测格式，尚无已注册实现 | `--adapter target`明确BLOCKED/exit2；`both`只实际执行Legacy，差分NOT_RUN。不会转调Legacy冒充Target |

原七例保留动态owner类型检查、RIL精确调用次数、等待/迟到应答断言、参数/通知断言和生命周期断言。唯一原断言表达式包装是无RIL setter：将同一次真实调用的返回值先记录，再与原NO_SIM_CARD期望比较；没有第二次请求，也没有改变期望。新增测量及公开getter不替代旧断言。

业务与技术准备仍有耦合：现有应答脚本使用Legacy InnerEvent/owner事件投递、公开Manager查询及Host完成屏障。这些属于Legacy driver，不是共用业务规范。Target需要自己的真实装配、完成事件与外部端口翻译；当前未把七个GTest函数机械改写为Target驱动，也不声称响应注入驱动已完全可移植。

### 七例业务覆盖矩阵

下表是固定场景下的断言覆盖，不代表整个分支或所有外部接口已验收。

| 场景 | 可共用的可观察行为 | 仍属Legacy或未覆盖 |
| --- | --- | --- |
| 无RIL初始化 | 无RIL且Provider不可用；物理槽数2；setter返回NO_SIM_CARD | 真实Manager直调、OnInit(3)=true及队列清理单独保留；同时缺两个依赖不能隔离各自影响；不是Core入口 |
| C0 | 角色准备0后，外部查询报告slot1=M0；setter成功，参数/主卡再数据发布，无设备setter | 查询映射变化是明确条件fixture，非原生一致性证明；具体Controller默认字段不作Oracle |
| B成功 | 专用SetPrimarySlot(slot1)一次，回包后才写Primary/ICCID/Data，成功返回，发布顺序及Done(false,true)/userSet=true | 原阻塞窗口断言保留；未经过真实TelRil serial/HDI及Modem |
| B错误 | 单次专用请求、generic failure，LOCAL_PTR_NULL；原主卡0/数据0保持，无成功广播，Done仍true | 错误映射是旧行为候选事实；不证明设备真实回滚或释放 |
| B超时 | 一次请求无回包，真实45秒等待；原断言及共用观测均要求至少44秒，返回错误并保持角色 | 不证明设备取消；其余时钟/产品预算与长时间资源状态未验证 |
| C1成功+过期CHECK | 两成员CHECK→UPDATE/NOTIFY→COMPLETE；6请求、同会话、对应资源/技术；过期session不推进；成功状态和发布 | owner具体类型及队列属于Legacy；只有CHECK过期反例，不覆盖所有阶段迟到/重复/错误 |
| C1 CHECK失败及5次重试 | 原尝试+5次重试、6个会话、24请求，仅CHECK/失败COMPLETE；参数恢复/不提交Data、12个Done状态 | Legacy固定槽顺序断言保留但不升级为跨实现硬规则；API返回与全部后台重试的时间关系尚非完整共用轨迹契约；失败COMPLETE.status原断言保留，但当前共用事实未采集该字段 |

共同前态是两张PIN锁USIM的持久化active账户，由真实加载绑定；公开Primary/Data均0且初始化已完成。不是硬编码IsSimActive。C0随后从两次真实协议查询收到反向设备映射；逻辑角色与设备资源是独立事实，映射转换仍CONDITIONAL。账户simId1/2、ICCID均为合成数据。

### Fake/Mock可复用性与外仓缺口

| 边界 | 能复用的部分 | 证据限制 |
| --- | --- | --- |
| RIL | 精确专用/协议请求载荷、slot、phase、session关系及外部应答计划 | 现有弱引用单目标通知不等于真实多强订阅；owner/event投递只属于Legacy。serial未创建，不与session混同 |
| Provider | 已核实列/URI/返回协议及两行外部存储 | SQL子集、内存成功路径，无事务、权限、重启恢复、真实持久化；当前共用硬Oracle主要覆盖参数副作用，未声称覆盖全部Provider列变化 |
| 参数 | 实际键值写入及先后、失败恢复 | Host内存服务，写失败/Watcher/落盘/重启恢复不在七例内 |
| CES/StateRegistry | 主卡、数据、状态发布的参数与偏序 | 发布true不等于消费者收到；Registry无代理错误未提供真实观察者交付。共用比较只包含原断言涉及的payload字段 |
| 身份与运行时 | 同一受控native身份和外部设备配置可供两驱动采用 | 无原生SA/IPC权限身份、真实FFRT/Watcher/生命周期屏障验证；Host初始化屏障不是产品完成协议 |

A厂商映射、第三物理卡、telephony_ext/实际芯片实现继续BLOCKED；VSIM/eSIM、插拔/激活变化与并发请求、拒权/非法槽/无卡矩阵、UPDATE/COMPLETE错误、各阶段超时与重复/迟到、请求冲突及完整停止未由这七例覆盖。入口14例及五个独立缺陷保留原分类，不能合并充数。CellularData/APN、搜网、SMS/通话消费者的真实接收和连接状态、跨进程持久化恢复均需匹配产品。manifest只跟踪master，31份参考源码不等于统一产品版本；所有Host结果不能升级为VERIFIED_NATIVE。

### Target准入及差分判定

当前`AssignmentService::StartPrimarySwitch`和`CardRuntime::SwitchPrimaryRadio`仅有声明/流程注释，根产品仍由旧SIM执行；G2a只读查询接入不代表主卡写业务完成。源位置和SHA见[target-readiness.json](../systemtest/configs/sim028_shared_primary_20261112/target-readiness.json)。只有完成真实分支、状态写入、外部端口与返回/重试闭包、提供真实Target装配与公开查询后，才注册Target Adapter。不能用计划函数、Fake或回调旧SIM获得Target PASS。

差分在相同场景ID、卡/角色/设备映射、外部应答计划、证据范围下执行；先检查输入锁与各自原始运行有效性，再用同一Oracle。比较错误符号、公开角色、契约参数及这几个键的实际写序列、相关发布payload/偏序及RIL请求资源。session归一化为尝试序号，保留尝试内相等及尝试间不同；忽略绝对编号、地址、线程、绝对时间和同阶段两槽发令顺序。真实RIL serial、未采集的Provider数据、所有额外事件及消费者状态不能据此判断等价。正常时间只看契约约束，不比调度快慢。

- 两边真实执行且各自契约PASS、可比较事实一致：只可报告该范围Host兼容PASS。
- Legacy PASS、Target契约失败或契约事实不同：FAIL，阻止兼容准入。
- Legacy FAIL/NOT_RUN、Target PASS：BLOCKED，先区分旧缺陷、模拟差异、准备失败或未经批准变化，人工裁决；不得自动认为新实现正确。
- 缺失Target、缺测量、版本/前态/场景不匹配：BLOCKED/NOT_RUN，不进行成功差分。
- 生命周期独立验收；业务兼容PASS不取消旧FAIL。旧资源失败/新资源通过属于单独修复审查。

`compare()`的合成输入测试仅验证比较算法；没有Target生产执行时即使算法返回相等，也不是Target测试PASS。共用Oracle不取代各实现必要的StrictMock及安全门禁。

```sh
# 一次完整七例，复用现有GN/Ninja；原生命周期失败使原始入口仍exit1。
python3 -B telephony_core_service/services/multisim/test/sim_guard/runner/run_primary_contract.py \
  --adapter both --output /tmp/sim028-shared-primary-new

# Target尚无绑定：BLOCKED/exit2，不运行Fake业务。
python3 -B telephony_core_service/services/multisim/test/sim_guard/runner/run_primary_contract.py \
  --adapter target --output /tmp/sim028-target-new

# Oracle技术正负控，不计入SIM业务数。
python3 -B telephony_core_service/services/multisim/test/sim_guard/oracle/test_primary_oracle.py
```

本轮最终实际结果与全部原始记录见[共享契约验证](../systemtest/configs/sim028_shared_primary_20261112/README.md)：七例原业务及共用Oracle均7/7，六例Strict RIL通过；原完整GTest仍1 PASS/6生命周期FAIL、exit1。Host64为59 PASS/5相同既有FAIL，Oracle技术正负控15项、实际观测副本检查9项通过；这些技术检查不是Target业务测试。

输入保护核对另发现原Legacy脚本在`--gtest_list_tests`阶段缺少独立LLVM_PROFILE_FILE，会覆盖上批遗留的根目录`default.profraw`生成文件。本轮已发生该改写，旧SHA已记录但原字节没有快照，无法恢复；它不是冻结业务证据或生产源码。现将发现阶段profile强制放入本轮输出目录，实际复验确认根profile不再变化。该例外在input-protection中明确保存，不宣称所有起始文件都未变化；原生产源码、Golden和历史验证记录保持原字节。
