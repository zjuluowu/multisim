# SIM 跨架构契约防护网

本工程链接未经修改的 `services/sim` 卡状态/PIN 实现，由 RIL 正式通知和请求响应驱动，读取真实公开状态能力及 StateRegistry 出站通知。它是 Linux 组件业务测试；不是完整 SimManager 初始化、IPC、设备、账户或 eSIM 验收。实际结果见 [evidence.json](evidence.json)，范围/规则/缺口见 [inventory.json](traceability/inventory.json)。

从 Git 根运行。GoogleTest 固定来源和摘要沿用 [dependency_lock.json](../systemtest/configs/dependency_lock.json)，准备工具会校验归档及解包树；构建不下载依赖。以下变量由本机选择，产物必须在 Git 仓库外：

```sh
curl --fail --location https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59 -o "$SIM_GTEST_ARCHIVE"
python3 -B telephony_core_service/test/systemtest/scripts/prepare_dependencies.py --archive "$SIM_GTEST_ARCHIVE" --destination "$SIM_DEPS_DIR"
# SIM_GTEST_ROOT 为上一步打印的固定版本源码目录
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=compat --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/compat" --repeat=5 --mutation-check
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=defect --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_GUARD_OUTPUT/defect"
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=multisim --suite=compat --output "$SIM_GUARD_OUTPUT/multisim"
python3 -B telephony_core_service/test/sim_guard/run_sim_guard.py --sut=legacy --suite=target --product=nezha_4c3s --output "$SIM_GUARD_OUTPUT/target"
```

依赖为 Linux、Clang/LLVM18、gold、GN/Ninja、Python3、pthread、libxml2 开发头。GN 复用 `test/systemtest/.gn`，目标 `sim_guard_legacy`。runner 的 `result.json/commands` 记录实际 GN、Ninja、测试二进制和覆盖导出命令及退出码，`artifacts` 定位每次独立执行目录。每次先写 BLOCKED，准备失败不会复用历史 XML；失败/超时返回非零，跳过或零用例不能通过。测试有 2 秒事件排空条件和 45 秒进程 watchdog，不以固定 sleep 判断业务成功。

当前七条 Compatibility：

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

场景、Contract Driver、Oracle、Fake语义由 [semantic_baseline.json](semantic_baseline.json) 摘要冻结。runner 检查摘要及共同代码include边界，记录生产源码、测试/host输入摘要和八条指定生产函数的源级正执行证据。业务规则变化需独立契约依据、版本和审查说明；不能为适配新架构静默更新摘要。

未来接入步骤见 [multisim绑定说明](adapters/multisim/README.md)。当前 `--sut=multisim` 和未接入的 target/nezha 配置明确 BLOCKED、executed=false，绝不回落到 legacy 或返回预置结果。

剩余工作：匹配Provider的14个SimData字段/URI/schema/错误协议后，扩展完整初始化、账户/角色及持久化恢复；单独接入真实eUICC APDU/Profile路径及SA66250边界。完整下载编排属于SA66250，本仓 `IEsimManager` 的 `AuthenticateServer/PrepareDownload/LoadBoundProfilePackage` 不等于 `DownloadProfile`。nezha三待、默认回选、删除/取消回滚、OOBE保留规则和真实卡托/注网时延分别裁决及设备验证。不能把这七条与报告392条换算成覆盖率。
