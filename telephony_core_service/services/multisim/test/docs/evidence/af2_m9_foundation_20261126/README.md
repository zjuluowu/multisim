# M9 架构基础交付证据

输入 `411b10672f8e119daa1ad21c9afd9984da99fe44`。架构基础交付后继续处理 Architecture Ready
所需机制。生产 Feature、真机和完整生产替换以各自门禁为准。

## 独立 Host 代码库

`sim_legacy_production_host` 由 `sim_legacy_core_sources` 提供完整 Host Legacy SIM 主闭包。
`sim_target_production_host` 由 `sim_target_core_sources` 提供 Target Host 已启用路径。
Legacy 容量和只读探针刻意使用未初始化的 Manager 归档，但正式 Primary 和 Bootstrap 二进制均链接完整
Legacy 归档。Wrapper、外部平台接缝、测试启动器分别构建。两个主要归档在 Host GN 中均为非
`testonly`，各自独立链接，运行在不同进程。产品根仍由原 SIM 路径承担，eSIM 的条件 CPP
仅由产品开启时纳入。Host 记录 eSIM off，加上原有 EXT 默认关闭的受控边界。vSIM、卫星、eSIM on
和厂商替换经实际产品参数重建，当前环境没有匹配的 OHOS 树、hb 和设备。

`host_build_contract.py` 从生成的 GN 图对照实际生产源、逻辑分工和测试源自动发现。
8 个只参加逻辑 GN 的 Target CPP 逐项在脚本 `TARGET_LOGICAL_ONLY` 记录职责。
`load_session.cpp` 另在纯规则入口执行，`real_sim_count_query.cpp` 另由 Legacy G2a reference
可执行程序使用。加入目标 CPP 时必须满足源文件归属；只有启用相应真实功能才加入 Host 归档。
技术负控覆盖去掉既有 CPP、漏登记新 logically-built CPP、生产目标引入 testonly、
Target 串进 Legacy 和遗漏 Target 测试文件。已有 archive 重建负控继续执行。

## 业务共用源

Voice/SMS、绑定 IMS、独立 Data、Primary C0、号码失败回退和 Reload 的正向刷新使用对应 `cases/shared/` 共用体。
两端执行步骤、返回值和共同断言来自同一源码，各自仍验证 Provider 行为、权限/通知、会话、写点、
阶段及失败。C0 的冻结 Target 对照是组件入口。M6 共用名称和标签直接继承。
Reload 连续 ID 的正向刷新共用断言。Target 的因果保护继续由其独立专项验证。B/C1 的共同 Oracle 继续运行组件用例。
CD-01～03 继续独立回归。

## 复杂任务切片

以下表的方向以已批准主设计 §10.1～10.2、§11、§17 及源码详细设计为依据。
独立反例的位置和预期均在此记录，缺少实际执行图的组合用例保留未运行。

| 编号与目标 | 源码证据及当前实现 | 最小交错及不变量 | 下一阶段任务 |
|---|---|---|---|
| B08-01 正式协议成员 H01/H02 | `sim_module_factory.cpp::SimModule` 仍为 `ObserveOnly/C0Only`；`assignment_service.cpp::StartPrimarySwitch` 以槽列表形成 C1 成员，用 Binding 版本构造 Session；`radio_execution_port.h` 只有既有单成员交接 | 两槽但同一实际 modem、路由版本在冻结与交接之间变化。冻结原成员，拒绝换路，纯 Radio 不签发卡 Session。组合尚未运行 | R 提供原成员和路由观察版本；A 同域登记意图，H 可控端口装配，先证明 Host |
| B08-02 锁外交接 H01/H02/H07 | `AssignmentService::StartPrimarySwitch/AcceptRadioStep/AcceptC1RetryDue/SubmitC1Round` 持 A 提交锁进 R；`ResourceRadioRuntime::SwitchPrimaryRadio` 在 R 锁外执行平台调用；此前修了同步重入的成员悬空 | 平台在 Submit 中阻塞，同时调用不共写的角色更新；第二分支同步回包。需证明 A 锁外执行、原步骤可核对、无重复发令/完成 | 在 A/T 短提交域冻结，退出锁后交接，随后按原 step 核对。保留历史 C1 重入、部分交接和投递负控 |
| B08-03 PIN 基础链路 H02/H03 | `CoreServiceSim::UnlockPin` 先权限/回调/槽检查，再异步投递；`SimStateManager::UnlockPin` 等 Handle 的有主完成，成功可由 LOCKED 进入 NOT_READY；`pin_requests.h` 仅有 VerifyPin 输入。H 只有号码的异步保活与完成端口，未装配 PIN 桥 | 无 Account 锁卡，先发请求再同步返回错误 PIN 与剩余次数。原错误结果不能生出 READY。平台如果无可复用证据则保留原设备责任 | 接原传输及安全完成，保留 FFRT/原等待节奏。需要以卡应用而非 ICCID 授权的共享原请求和资源钩子，以及可封口的原票晚证据接纳 |
| B08-04 ICCID 与 Refresh H02/H03/H07 | `SimFile::ProcessGetIccIdDone` 经 `IccFile::GetFullIccid/SwapPairsForIccId` 更新两种旧读源，再通知。Target `LoadSession::TryConsumeRead` 有代次和一次消费，`CardObservationRuntime::ObserveVerifiedCard` 从已经解码的 ICCID 开始 | ICCID 旧轮延后，SPN 与另一张卡仍在途。新 ICCID 读失败后重复回包不得接纳；无关 scope 照常完成。本轮纯规则覆盖该成员资格，真实 EF/Records 组合未运行 | 将真实 GET RESPONSE/READ BINARY 原响应送进生产算法及同域 Records 接纳，从该提交点发原身份观察；明确 PIN/EF 与 MSISDN 共享路由及旧证据结清 |
| B08-05 全图关闭 H02/H07 | `SimModule::AdmitModuleCall/ReturnModuleCall/StopAcceptingAndDrain` 管 H 根入口、topology 与 numbers；`PrimaryHostLifecycle::StopAcceptingAndDrain` 管 A/R 分派；没有安全和 EF 的原票保活/结束收据 | Store 与卡命令均在途，Stop 后允许原票核对，重复 drain 稳定，所有者回执不提前释放。尚未对 PIN/EF 运行 | 分别验证封口、外部调用退出、Owner 投递、未决证据和第二遍关闭，不把命令返回当物理静止 |
| B08-06 跨实例 H02/H07 | `CreateCoreSimModule`、`ResourceBarrier::AcceptInitialRecoveryEvidence` 按实例初始化；既有 `TargetPrimaryB.HostLifecycleOldAndNewInstancesRemainIsolated` 属组件 | 模块 A 留原写效果与晚回包，建立 B 后投递旧 epoch/token；B 不接纳或释放自身租约。A 只结自己的责任 | 组合 Store、PIN/EF、旧资源前驱与重新建立根模块，保留原生恢复证据独立 |
| B08-07 运行负控和最终反审 H01/H02/H07 | 上轮已保存 C1 重入、Radio 投递和原结果相关独立反例；本轮新增 Host 源图和 LoadSession 资格负控 | 分别打破应用/读 scope、关联、锁、物理结束和关闭条件，门禁必须拒绝 | 先完成依赖再全量复跑。最终独立反审对照正式对象图、产品边界和 CD |

### 两个样板的当前运行边界

- UnlockPin 已定位真实 Legacy Core → Manager → StateManager → StateHandle → RIL → 原回调。
  密码错误是原结果 `UNLOCK_INCORRECT` 加剩余次数且 API 成功，通用失败是
  `TELEPHONY_ERR_RIL_CMD_FAIL`，等待超时是 `CORE_ERR_SIM_CARD_UPDATE_FAILED`。
  Legacy 双卡、错误 PIN、失败 EXT Hook 和设备错分类用例已在全量 Host 清单。
  Target 预身份会话可用，正式安全调用及设备关联仍缺。未添加替代等待器或写状态缓存。
- ICCID 已定位 `SimFileManager` 按卡型建 File/Controller、`SimFile` 读取 EF、
  `IccFile` 交换半字节的两条原算法，以及 Monitor/Controller 的账户发现。
  两条 Legacy 函数对空和奇数长度的处理不同于一般十进制解析。复制一个严格数字过滤器会改变原行为。
  当前 Target 设备传输端口仅有 MSISDN 写入的资源关联，`ReadEfRequest` 仍是逻辑声明，Records
  写点没有正式装配。没有从已解码 ICCID 注入证明 EF 执行。

## 本轮实际验证

| 范围 | 执行证据 |
|---|---|
| 独立归档、链接、ELF | [干净构建](clean-host-build.json.gz) · [最终 GN/Ninja 构建](build-verified.json.gz) · [归档列表和 ELF](elf-and-archive.log) · [Legacy GN check](gn-check-legacy.log) · [Target GN check](gn-check-target.log) |
| 源码遗漏与恢复 | [生成图拒绝](worktree-guard-omission.log.gz) · [真实重链失败](worktree-link-omission.log.gz) · [恢复](worktree-guard-restored.log) |
| Target 全量 | [174/174](target-full.json.gz) · [T/H 执行](target-topology-audit.json.gz) · [B/C1 组件执行](target-radio-component-audit.json.gz) |
| Both | [24/24](both.json.gz) · [Oracle 及两端差异 0](differential.json.gz) |
| CD-01～03 | [Legacy 2/2](cd-legacy.json.gz) · [Target 8/8](cd-target.json.gz) |
| 静态及规则 | [逻辑 GN/Ninja 和纯规则](architecture.log.gz) · [Host 规则](technical-final.log.gz) · [冻结资产](integrity.log) |
| CLI 和定位 | [冷跑 63.74 秒](worktree-cold.log) · [热跑 0.44 秒](worktree-hot.log) · [新增 CPP 5.68 秒](worktree-added.log) · [生产 CPP 增量 3.96 秒](worktree-production.log) |
| 自动发现 | [临时新用例](discovery-case.cpp.example) · [独立 worktree 174 个定位](worktree-editor-locations.json.gz) · [主候选 320 个定位](editor-locations.json.gz) |
| 真实 LLDB | [Legacy 命令](legacy.lldb) · [Legacy 函数单步](legacy-lldb.log.gz) · [Target 命令](target.lldb) · [Target 函数单步](target-lldb.log.gz) |

源码遗漏负控从独立 worktree 的 `sim_target_sources.gni` 移除 `slot_catalog.cpp`。
实际 GN 图拒绝此缺口，Ninja 重新生成归档后原对象消失，链接因原符号缺失失败。
恢复源表后，GN 图及 ELF 中的 `SlotCatalog::Find` 恢复。

CLI 复跑从当前 Git 根开始。依赖根路径使用固定版本的已准备目录。
冷/热输出各自使用相同工作区和独立构建根。

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" build --backend both --out /tmp/sim-verify
python3 -B "$TEST/scripts/test.py" run --backend both --no-incremental-build --out /tmp/sim-verify
python3 -B "$TEST/scripts/test.py" run --backend target --debug-build --out /tmp/sim-feature --filter AF2Discovery.ApplicationPrecedesIdentityAndStalePromotionCannotCreateBinding
python3 -B "$TEST/scripts/host_build_contract.py" /tmp/sim-verify/build
python3 -B "$TEST/scripts/check_editor.py" --out /tmp/sim-verify --output /tmp/sim-editor
python3 -B "$TEST/scripts/run_architecture.py" --with-gn --migration --output /tmp/sim-architecture
```

B08-03 与 B08-04 共用的未运行反例条件为在真实应用资格读后与命令登记前插入可控屏障，
于屏障内替换实际卡应用。现有 `MsisdnRuntime::WriteMsisdn` 的读取、资源 Acquire 和原票登记
位于不同边界。新 PIN/EF 不直接借该时序或其号码专用缓存。需验证失效先发生时申请零发令，
提交先发生时只允许原证据结清，按实际资源域互斥。此为源码取证及待运行条件。

完整 Legacy CI 含变异及恢复已执行，启动时验证的是 147 登记基线，早于新增 Reload 对照。
[CI 原报告](legacy-ci-baseline.json.gz) · [变异](legacy-ci-mutation.json.gz) ·
[Legacy 真实执行](legacy-real-audit.json.gz)。ASan、TSan 对新增 Reload 稀疏 ID 探针及
本批 Target 涉及路径各跑 130 项，当前正式共用用例使用连续 ID 的等价子域。
[ASan 探针阶段](asan-probe-suite.json.gz) · [TSan 探针阶段](tsan-probe-suite.json.gz)

初次 Reload 采用稀疏 simId 41、49，Legacy Current 读取通过，Historical 名称读取为空；
相同 Target 见证保留了这两行。Legacy `MultiSimController::SortAllCache` 以 simId−1
写入长度为结果行数的数组。差异保留原始证据，本批不变更生产行为或既有 Oracle。
[Legacy 稀疏 ID](legacy-reload-sparse-probe.json.gz) · [Target 稀疏 ID](target-reload-sparse-probe.json.gz)

Legacy 无 LTO 的 clean worktree 链接复现平台缺符号，标准 Verify ELF 的真实生产断点已通过。
[无 LTO 失败](legacy-developer-link-failure.log.gz)


Reload 正向脚本从实际 Provider 图像读取 Current/Historical。Legacy 执行
`MultiSimController::GetListFromDataBase/GetAllListFromDataBase` 和原列表查询，
Target 执行 `SimModule::ReloadAccounts` 和真实账户读取。两端用同一场景验证新 Current
不会覆盖 Historical。该新增共用语义分别执行两端源级断言，12 项原冻结 Oracle 场景照常执行。
[Legacy 连续 ID](reload-shared-legacy.json.gz) · [Target 连续 ID](reload-shared-target.json.gz) ·
[含新测试的枚举与定位](complete-editor-locations.json.gz)


最终候选全量 Legacy 148/148、Target 174/174。12 个冻结共同契约在新增 Reload
后复跑，后端和 Oracle 差异均为零。新增 Reload 的原操作通过逐方法计数确认
Legacy 两次 Current 及两次 Historical 入口，Target 四次真实加载及接纳。
[Legacy 全量](legacy-full.json.gz) · [Target 全量](target-full.json.gz) ·
[最终 Both](both-after-reload.json.gz) · [最终差分](differential-after-reload.json.gz) ·
[新增 Reload 生产执行](reload-production-audit.json) · [源码与测试盘点](source-and-test-inventory.json) ·
[测试源摘要](test-inputs.sha256)

共同语义已证 13 个。32 个 P 子包、62 个 SIM、9 个 WU 均记录源码职责和当前证据状态。
其余测试中 Legacy 110 个暂未建立对照，25 个属框架。Target 143 个属机制专项，
18 个尚未证明可否与 Legacy 外部语义合并。设备矩阵 12 项 BLOCKED。完整产品语义数量
继续随产品和未迁业务补实。
