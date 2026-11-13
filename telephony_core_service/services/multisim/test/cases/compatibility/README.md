# SIM 跨架构契约防护网

本工程链接未经修改的 `services/sim` 卡状态/PIN/PUK实现，由 RIL 正式通知和请求响应驱动，读取真实公开状态能力及 StateRegistry 出站通知。它是 Linux 组件业务测试；不是完整 SimManager 初始化、IPC、设备、账户或 eSIM 验收。v2历史结果见 [evidence.json](../../docs/archive/sim_guard/evidence.json)，第二轮结果见 [evidence_round2.json](../../docs/archive/sim_guard/evidence_round2.json)，范围/规则/缺口见 [inventory.json](traceability/inventory.json)及[增量场景清单](traceability/round2_catalog.json)。

2026-11-05整理：首批和第二轮资产已保存到main，当前已执行范围如下。第二轮实际运行基线为01db5ce6加当时dirty工作树，输入摘要已对应保存代码c508bd03；不是在最新文档HEAD上重跑。整体交付、G2a分支合入状态及剩余门禁统一见[执行状态 §0](../../../../../docs/SIM_Refactor_Execution_State.md#current-status)，本批合入没有改动本工程业务代码、Oracle或历史证据，SIM Guard未重跑；本批新增G2a重验见执行状态§11。

| 分类 | 已保存执行结果 | 状态解释 |
| --- | --- | --- |
| v2原兼容集 | 7条×5轮，35 PASS | 原断言保留，已包含在第二轮总兼容统计内 |
| 第二轮兼容扩展 | 新增10条×5轮，50 PASS；总计17条、85 PASS/0 FAIL | 卡生命周期、PIN/PUK、双槽交错和组件重建；不计成完整初始化/持久化恢复 |
| 独立缺陷/待裁决集 | 5条×5轮，25 FAIL | 保留迟到/换卡/PIN/Refresh问题；不能把5条候选都当成已批准规则，修复政策/返回契约单独裁决 |
| 设施与有效性 | 19项设施PASS；READY→LOCKED突变检出，恢复后17 PASS | 与业务兼容/缺陷分开计数；`--suite=all`因独立缺陷返回非零 |
| Multisim及目标新能力 | BLOCKED，未运行真实新实现 | Adapter接入准备不代表通过；四卡/三待等按正式产品能力另验 |

从 Git 根运行。GoogleTest 固定来源和摘要沿用 [dependency_lock.json](../../framework/host/configs/dependency_lock.json)，准备工具会校验归档及解包树；构建不下载依赖。以下变量由本机选择，产物必须在 Git 仓库外：

```sh
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o "$SIM_GTEST_ARCHIVE"
python3 -B telephony_core_service/services/multisim/test/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
# SIM_GTEST_ROOT 为上一步打印的固定版本源码目录
python3 -B telephony_core_service/services/multisim/test/scripts/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat" --repeat=5 --mutation-check
python3 -B telephony_core_service/services/multisim/test/scripts/run_sim_guard.py --sut=legacy --suite=defect --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/defect"
python3 -B telephony_core_service/services/multisim/test/scripts/run_sim_guard.py --sut=multisim --suite=compat --output "$SIM_GUARD_OUTPUT/multisim"
python3 -B telephony_core_service/services/multisim/test/scripts/run_sim_guard.py --sut=legacy --suite=target --product=nezha_4c3s --output "$SIM_GUARD_OUTPUT/target"
```

依赖为 Linux、Clang/LLVM18、gold、GN/Ninja、Python3、pthread、libxml2 开发头。GN 复用 `services/multisim/test/framework/host/.gn`，目标 `sim_guard_legacy`。runner 的 `result.json/commands` 记录实际 GN、Ninja、测试二进制和覆盖导出命令及退出码，`artifacts` 定位每次独立执行目录。每次先写 BLOCKED，准备失败不会复用历史 XML；失败/超时返回非零，跳过或零用例不能通过。测试有 2 秒事件排空条件和 45 秒进程 watchdog，不以固定 sleep 判断业务成功。

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

依赖边界：有状态 [FakeRil](../../framework/compatibility/fakes/ril_device.h) 只保存设备事实、PIN设备应答、请求及可重排/重复/迟到响应；真实 SIM 决定公开状态。RIL接口、平台参数、CommonEvent、Registry、宿主类型映射集中在 [Legacy Adapter](../../framework/compatibility/adapters/legacy/legacy_sim_adapter.cpp)。真实 TelEventHandler/TelEventQueue、ObserverHandler、CoreManagerInner 和 EXT 均链接；CoreManagerInner 的 SIM manager 路由未装配，账户副作用未覆盖。SA查找明确不可用，Registry/CES为外服务Spy，FFRT为有限host运行时，不证明设备调度等价。无DataShare/LPA调用的切片不创建虚假Store/LPA实现。

场景、Contract Driver、Oracle、Fake语义由 [semantic_baseline.json](semantic_baseline.json) 摘要冻结。第二轮为复用Driver仅将原fixture移至共享头；[v2摘要](semantic_baseline_v2.json)、四份原业务资产及原七条测试代码块摘要另外受检，不允许通过新版本摘要静默改变旧断言。runner还检查共同代码include边界，记录生产源码、测试/host输入摘要和源级正执行证据。业务规则变化需独立契约依据、版本和审查说明。

未来接入步骤见 [multisim绑定说明](../../framework/compatibility/adapters/multisim/README.md)。当前 `--sut=multisim` 和未接入的 target/nezha 配置明确 BLOCKED、executed=false，绝不回落到 legacy 或返回预置结果。

剩余工作：匹配Provider的14个SimData字段/URI/schema/错误协议后，扩展完整初始化、账户/角色及持久化恢复；单独接入真实eUICC APDU/Profile路径及SA66250边界。完整下载编排属于SA66250，本仓 `IEsimManager` 的 `AuthenticateServer/PrepareDownload/LoadBoundProfilePackage` 不等于 `DownloadProfile`。nezha三待、默认回选、删除/取消回滚、OOBE保留规则和真实卡托/注网时延分别裁决及设备验证。不能把这七条与报告392条换算成覆盖率。

第二轮新增十个兼容场景，复用相同Driver/Observer/Oracle：单卡枚举、双卡逆序枚举、RIL不可用后真实对象重建、异常载荷恢复、丢包后重新查询、PUK错误及成功、显式状态刷新、双卡对象销毁重建、停机换卡重新枚举、PIN请求与另一槽拔卡交错。初始/重建时的临时状态仅记录C级观察；强断言针对已知设备事实对应的公开状态、卡型和正式解锁结果。重建过程验证旧对象weak引用全部失效、worker退出及新对象重新创建；这是硬件重新枚举，**不是数据库持久化恢复验收**。

```sh
# 全报告包含原7条、新增10条和5条独立缺陷；已知FAIL使进程非零退出。
python3 -B telephony_core_service/services/multisim/test/scripts/run_sim_guard.py --sut=legacy --suite=all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/all" --repeat=5 --mutation-check
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
python3 -B telephony_core_service/services/multisim/test/scripts/run_pin_memory_contract.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/pin-memory" --mutation-check
```

[独立 runner](../../scripts/run_pin_memory_contract.py) 检查原冻结资产、依赖树与平台头摘要、四例集合、真实生产 `UnlockPin/UnlockPuk/HandleSimAbsent` 正执行，并记录实际命令、源码/构建输入摘要。`--mutation-check`只在仓外生产源码副本将 UnlockPuk 的 Hook 从 PIN_ALTER 改为 PIN_ENABLE；新 Spy 必须检出失败，之后原生产二进制四例重新通过。默认空的 `sim_guard_manager_source` 仅为该负控提供输入，正常构建仍编译原生产Manager。

本增量没有接入 AlterPin、锁启停、FDN、迟到拒绝/恢复政策、完整Core/Manager初始化、IPC、vendor asset或设备自动解锁；不能把L1函数指针载荷断言称为L2/L3安全验收。旧独立缺陷及待裁决项完整保留。先前4例通过及突变恢复结果属于撤回旧代码修改之前的工作树，不作为当前验收。恢复旧基线后的结果统一见[执行状态§12](../../../../../docs/SIM_Refactor_Execution_State.md#open-closed-followup-20261109)和[当前证据](../../docs/archive/systemtest/configs/open_closed_followup_20261109.json)。测试通过外部函数指针观察现有行为，不加入生产friend或探针；负控只在仓外副本施加故障，不修复旧实现。

## 主卡共享契约

主卡七场景、Oracle、冻结基线及Target准入说明现随用例放在 [Primary共享契约](../primary_slot/Shared_Contract.md)。公共端口/Fake/Adapter保留于framework/compatibility，业务预期归对应cases。
