# telephony_core_service：Linux 独立 SIM 管理测试防护网设计（DDD 重构保护）

> 文档性质：仅设计。本轮**没有修改生产代码或测试代码，也没有编译、运行或通过任何测试**。
> 标注约定：**[事实]** 已核对仓库源码（给出路径:行号）；**[推断]** 由事实推出但未运行验证；**[建议]** 本设计的方案。
> 路径以 `telephony_core_service/` 为根（Git 仓根目录为 `E:\github\multisim`）。

---

## 1. 执行摘要

**保护目标。** 未来可能做以下重构：把 SIM 拆成 `libtel_sim_service.z.so`；从 Slot-Centric 迁移到身份/拓扑模型；分离卡、Profile、订阅、默认角色和 Radio 资源。本防护网要在这些重构中守住三类稳定性：

| 稳定性 | 保护对象 | 允许变化 | 本文套件 |
|---|---|---|---|
| 领域契约稳定 | 业务场景、Contract ID、Invariant ID、业务断言 | 不允许（修订须经业务评审并升版本） | A 套件（`SIM-ST-*`） |
| 对外接口兼容 | 已发布 API/IPC 签名、错误码、公共事件及参数 | 仅按批准的契约版本变更 | B 套件（`SIM-API-*`） |
| 技术适配可变 | 驱动绑定、对象装配、边界替身、事件泵 | 随实现调整，须记录理由 | Adapter/Env 层，不含断言 |

当前 Legacy 行为另外放在 C 套件（`SIM-LEG-*`）。C 套件只探测“非预期变化”，业务确认后可以淘汰或修订。

**核心结论。**

1. **[事实]** 仓内没有 Linux host 工具链目标。官方测试全部基于 `ohos_unittest`，并依赖 `-fno-access-control` 访问私有成员和 Mock 被测类（见 §2、§6）。因此这些测试不能作为本防护网的基础。
2. **[推断]** 在 Linux 上可以信任、可以执行的最小入口是 `ISimManager`（`interfaces/innerkits/include/i_sim_manager.h:33` 起）的应用服务边界。真实 IPC/SA、权限校验（`services/core/src/core_service_sim.cpp`）和 HDI 编解码不在 Linux 验证范围内。
3. **[推断]** 首批 P0 场景要想执行，必须先建立一个可信运行闭包。它至少要满足：真实 `sim_manager.cpp`、`sim_state_*`、`multi_sim_*`、`radio_protocol_controller.cpp` 可以链接；EventHandler/FFRT 能在 host 上运行；RIL、DataShare、系统参数、CommonEvent、StateRegistry 都有边界 Fake。**在闭包打通并实测之前，首批用例一律为 BLOCKED**，不能因为写出了 BUILD.gn 就视为可执行。
4. **[事实]** 工作区里已有一个**未提交**的 `test/systemtest/` Linux 工程（`git status` 显示 `?? test/systemtest/`）。它自带的记录（`test/systemtest/docs/run_record.md:32`）声称只有容量查询 1 条 PASS，状态/账户初始化仍为 BLOCKED。本轮**没有复核**该记录：当前是 Windows 环境，`gn` 和 `clang++` 都不在 PATH 中。该工程只作为输入参考，本设计不继承它的结论。

---

## 2. HEAD 与 Linux 工具链可行性

### 2.1 仓库基线

| 项 | 值 | 性质 |
|---|---|---|
| 仓库 | fork `zjuluowu/multisim`，upstream `openharmony/telephony_core_service`；工程位于子目录 `telephony_core_service/` | 事实（`git remote -v`） |
| 分支 / HEAD | `main` / `2841c6f60635853a929c1755dc08f5543a89eb83`（2026-09-30，“docs: add SIM design…”） | 事实 |
| 与“upstream 当前默认分支”的一致性 | **未核实**：本地 HEAD 是 fork 上的提交，本轮没有 fetch upstream | 风险，需在实施前对齐 |
| 工作树 | 有未跟踪的 `test/systemtest/` 和 `.codegraph/` | 事实 |
| Feature 默认值 | `core_service_support_esim=false`、`core_service_satellite=false`、`core_service_low_power_class_2=false`、`telephony_hicollie_able=true`（缺少 hicollie 部件时为 false） | 事实，`telephony_core_service.gni:14-28` |
| 条件宏 | `CORE_SERVICE_SUPPORT_ESIM`、`CORE_SERVICE_SATELLITE`、`OHOS_BUILD_ENABLE_TELEPHONY_EXT/VSIM`，默认全部关闭 | 事实，gni 同上；`docs/knowledge/code_map/conditional-build.md:3-20` |
| 产品配置 | 槽数来自 `const.telephony.slotCount`（默认 1），VSIM 来自 `const.telephony.vsimModemCount`（默认 "0"） | 事实，`interfaces/innerkits/include/telephony_types.h:63,74-75,100-120` |

**[建议]** 基线 Profile 记为 `BASE-2SLOT`：`slotCount=2`、vsim=0、eSIM 关、卫星关、EXT 关。eSIM、卫星、VSIM 各建独立 Profile，并单独构建。

### 2.2 工具链

| 工具 | 来源/版本 | 状态 |
|---|---|---|
| GN / Ninja | 上游 gn.googlesource 固定 commit；Ninja ≥1.11 | [建议] 锁定在 `configs/toolchain.lock` 中 |
| Clang | LLVM 18（`-std=c++17`）。生产 `BUILD.gn:184-197` 用 `-fno-exceptions -fno-rtti -Os -flto`，host 版应保留 `-fno-rtti`，避免 ABI 行为差异 | [建议] |
| GoogleTest/GoogleMock | 固定 release 的源码构建（如 v1.15.x），记录 SHA 和 License | [建议] |
| 本机验证 | 当前 Windows 主机 `where gn clang++` 找不到这两个工具 | 事实。**本轮未执行任何构建** |

### 2.3 与官方测试框架的关系（只参考规范，不依赖）

- [事实] `test/core_service_test.gni:74` 起重新列出了生产源码。`test/BUILD.gn:34-53` 的 `core_service_test_base` 使用 `-O0 -w -fno-access-control`。`test/mock/` 下有 `mock_multi_sim_controller.h`、`mock_sim_manager.h`、`mock_sim_rdb_helper.h` 等被测对象的 Mock。`test/guard/global_guard.cpp` 通过访问私有成员替换 samgr。
- [建议] 新工程**不得**引用上述目标、Mock、Fixture、guard 和源码清单。允许借鉴的只有以下三项：
  - gtest 命名规范；
  - `use_exceptions` 配置；
  - `test/mock/ffrt` 的**思路**（不复用文件）。

---

## 3. 术语表与语义歧义

“观察方式”只列公开 API 或边界。“不得推导”一列会作为断言设计的硬约束。

| 术语 | 当前 API/代码含义 [事实] | 业务含义已确认？ | 歧义 | 测试观察方式 | 不得推导 |
|---|---|---|---|---|---|
| Physical Slot / Hardware Endpoint | 没有独立类型。RIL 请求以 `slotId` 下发，`RadioProtocolController` 会交换 slot 与 modemId 的对应关系（`radio_protocol_controller.cpp:328-331`） | 否 | slotId 可能是物理卡槽，也可能是 modem 路由 | Fake RIL 按“硬件端点别名”注入卡。别名与 slotId 的绑定放在适配层 | slotId ≡ 物理卡槽号 |
| Logical Slot / 当前路由 | API 参数 `slotId`，取值 0..`SIM_SLOT_COUNT-1`，另有 VSIM=2（`telephony_ext_wrapper.h:37-39`） | 部分 | 主卡切换后 slotId 与 modem 的对应关系会变 | 公开 API 的 slotId 入参/出参 | slotId 是稳定业务身份 |
| pSIM / eUICC | `SimType{PSIM,ESIM}`（`sim_state_type.h:336`）；`IccAccountInfo.isEsim`（:398） | 部分 | eSIM 走独立 SA 66250 | `GetSimLabel`、`GetSimAccountInfo.isEsim` | 每个 Profile 独占一个 Slot |
| Port | 本仓 SIM 公开 API 没有 Port 概念 | 否 | MEP 语义缺失 | BLOCKED（P2） | — |
| Profile | eSIM Profile 由 eSIM Service 管理，本仓只看到结果 ICCID | 否 | Disabled Profile 与“卡不存在”的关系不明 | eSIM 边界 Fake 与 SIM 状态查询 | Profile Disabled ≡ 卡硬件不存在 |
| ICCID | `IccAccountInfo.iccId`（:406），`GetSimIccId` 需要系统应用和 GET_TS 权限（`core_service_sim.cpp:180,184`） | 是（行业标准标识） | 主卡参数中保存的是 SHA-256 摘要（`multi_sim_controller.cpp:1541-1556`） | `GetSimIccId`、`GetSimAccountInfo` | ICCID 与 simId 永久一一对应（换卡、恢复出厂后可能不成立） |
| simId / Account | 由 DataShare 插入时分配（`sim_rdb_helper.cpp:347-362`），代码假设从 1 开始且连续（`multi_sim_controller.cpp:960-965`） | 否 | simId 是“账户”还是“订阅” | `GetSimId(slot)`、`GetSlotId(simId)`、`IccAccountInfo.simId` | simId 的具体数值（只断言有效、稳定、可逆映射） |
| Subscription | 没有独立类型。“默认 xx subscription changed”事件携带 `PARAM_SIMID`（`multi_sim_controller.cpp:1894-1922`） | 否 | 与 simId 等同？ | 事件参数 simId | 一个 Slot 永远只有一个 Subscription |
| Active / Enabled | `IsSimActive(slot)`、`SetActiveSim(slot, enable)`，结果写入 DB 的 `IS_ACTIVE` 列（L932-949） | 部分 | Active 表示“订阅启用”，不等于卡存在 | `IsSimActive`、`GetActiveSimAccountInfoList` | Inactive ≡ 卡不存在 |
| Default Voice/SMS/Data | Voice/SMS 写 DB（`sim_rdb_helper.cpp:287-325`）；Data 写系统参数 `persist.telephony.MainCellularDataSlotId`（`multi_sim_controller.cpp:75,1692-1697`） | 部分 | Data 默认值由 cellular_data 仓消费 | Get* API、默认订阅变更事件 | 三种默认角色必然一致 |
| Primary | `SetPrimarySlotId` / `GetPrimarySlotId`，保存在 `persist.telephony.MainSlotId` | 部分 | 主卡的业务含义是“主 modem 资源”还是“主订阅” | `GetPrimarySlotId`、`COMMON_EVENT_SET_PRIMARY_SLOT_STATUS`、`MAIN_SUBSCRIPTION_CHANGED` | Primary ≡ 默认数据（当前代码成功切换后会同步 Data，L1522、L1685-1690，属于 **LEGACY**） |
| Modem / Radio Resource | `RadioProtocol`（tech、modemId）各阶段（`radio_protocol_controller.cpp:270-301`） | 否 | — | Fake RIL 收到的 `SetRadioProtocol` 参数 | 资源交换必然伴随默认角色变化 |
| Readiness | `SimState`：UNKNOWN(0)…READY(4)、LOADED(5)（`sim_state_type.h:84-118`）。LOADED 来自文件加载（`icc_file.cpp:442-443`） | 部分 | READY 与 LOADED 与“账户就绪”三者不同 | `GetSimState`、`HasSimCard`、账户查询 | SIM Ready ≡ 账户与数据业务全部就绪 |

**领域边界候选**（只作参考，测试契约不依赖）：卡与存在性（Card Presence）、Profile 生命周期、订阅/账户（Subscription Registry）、默认角色策略（Role Policy）、Radio 资源协调（Radio Resource Coordination）。

---

## 4. 被测能力与输入/输出边界

```
                ┌──────────────────── 驱动层 (可变) ─────────────────────┐
 业务场景 ──▶ │ L3: ISimManager 应用服务入口 (Linux 首选)                  │
                │ L2: CoreService 方法 (需权限替身, 后续)                    │
                │ L1: CoreServiceClient + IPC (DEVICE_REQUIRED)             │
                └──────────────────────────┬────────────────────────────────┘
                                           ▼
                    ┌── 被测黑盒：SIM 管理业务能力（真实生产实现） ──┐
 外部输入 ───────▶ │ 卡状态处理 / 账户协调 / Active / 默认角色 / Primary │ ──▶ 边界调用+参数
 (Fake RIL 上报、   │ 与 Radio 资源协调 / 回调通知                         │     (RIL/DataShare/
  DataShare Ready、 └──────────────────────────────────────────────────────┘      参数/StateRegistry)
  用户切换)                                  │
                                              ▼
                  API 返回 · 公开查询 · ISimAccountCallback · 公共事件 ──▶ 断言层 (Contract/Invariant)
```

**输入边界 [事实]**

- 公开 SIM API：`i_core_service.h:65-171`、`core_service_client.h:187-971`（逐项见 §5）。
- 回调：`RegisterSimAccountCallback`（`multi_sim_monitor.cpp:657-694`）。
- RIL：状态上报 `RADIO_SIM_STATE_CHANGE` → `ObtainIccStatus` → `GetSimStatus`（`sim_state_handle.cpp:867-868,551`）。
- DataShare Ready 与用户切换：`multi_sim_monitor.cpp:489-520,583-593`。

**输出边界 [事实]**

- API 返回值、错误码。
- 公共事件：`SIM_STATE_CHANGED`（sticky，**Want 不带 slotId**，`sim_state_handle.cpp:816-830`）；默认 Voice/SMS/Data 变更（携带 `PARAM_SIMID`）；`MAIN_SUBSCRIPTION_CHANGED`（携带 simId 和 slotId，`multi_sim_helper.cpp:88-90`）；`SET_PRIMARY_SLOT_STATUS`（sticky，`multi_sim_controller.cpp:1630-1643`）。
- StateRegistry：`UpdateSimState(slotId, type, state, reason)`（`sim_state_handle.cpp:481-482`）、`UpdateIccAccount`（`multi_sim_monitor.cpp:731-741`）。
- RIL 下行：`GetSimStatus`、`SetActiveSim`、`GetRadioProtocol`/`SetRadioProtocol`。
- 持久化：DataShare URI、系统参数。

---

## 5. 接口稳定性分类

| 分类 | 接口 | 保护方式 |
|---|---|---|
| **A. 已发布外部契约** | NAPI/.d.ts 与 innerkits 的 SIM API：HasSimCard、GetSimState、GetSimAccountInfo、GetActiveSimAccountInfoList、SetActiveSim、Get/SetDefaultVoiceSlotId、GetDefaultVoiceSimId、Get/SetPrimarySlotId、GetSimId、GetSlotId、IsSimActive、UnlockPin、GetSimLabel 等（行号见附录 A）；公共事件名及参数键；`ISimAccountCallback` | B 套件做签名/错误码/参数兼容测试；A 套件做语义测试 |
| **A'. 已发布跨部件协议（需确认版本）** | HDI `IRil` V1_1–V1_5（`BUILD.gn:213-256`）；StateRegistry `UpdateSimState`；DataShare 的 telephony_data SIM 表 URI（与 `telephony_data` 仓的契约） | 参数语义用版本化 Fake 适配；Linux 上不验证二进制协议 |
| **B. 内部实现接缝** | `ITelRilManager`、`SimRdbHelper`、`TelEventHandler`/`InnerEvent`、`RadioEvent` 内部事件、`CoreManagerInner`、`MultiSimController` 等类 | 只允许被**替身接入**（ITelRilManager 除外），不得断言其调用次数或顺序 |
| **C. 测试专用适配** | Driver、Env、Observer、事件泵、别名映射 | 只做接入与观察，禁止实现业务规则 |

**说明。** `ITelRilManager` 在技术上属于 B（内部接缝），但它是 Modem 的唯一代理。**[建议]** Linux 阶段的 Fake RIL 挂在 `ITelRilManager` 上，并把这一选择明确标注为“技术适配可变”：SO 拆分后如果接缝变成 HDI 层，只改 Env 层，Fake 模拟的“Modem 行为剧本”保持不变。

---

## 6. 生产运行闭包、替身与真实性限制

### 6.1 最小生产闭包（推断，需实测）

- **SIM 源码**：`BUILD.gn:67-117` 的全部无条件 SIM 源。原因是 `SimManager::OnInit` → `InitMultiSimObject` 会为每个 Slot 创建状态、文件、账户、短信、号码簿、STK 对象（`sim_manager.cpp:41-153`），无法再裁小。
- **工具库**：`utils:libtel_common`、`interfaces/innerkits:tel_core_service_api`（`BUILD.gn:204-207`）。
- **tel_ril 和 network_search**：只需要满足 `CoreManagerInner` 的链接。**[建议]** 链接真实 `core_manager_inner.cpp`，并向它注入 Fake RIL 和 Fake NetworkSearch。这是装配层替身，不实现 SIM 业务。

### 6.2 外部组件可替换性

| 外部组件 | Linux 可用性 | 替身类型 | 真实性限制 |
|---|---|---|---|
| RIL/HDI | 不可用 | Fake `ITelRilManager`，按“剧本”回送正式 `InnerEvent` 响应 | 不验证 HDI parcel、真实 modem 时序 |
| DataShare（telephony_data） | 不可用 | 内存 Fake Provider，**跨被测实例保留数据**，自增 simId，能注入延迟/失败 | 自增和查询语义模仿 telephony_data 的实现，需要与该仓的契约对齐（风险 R3） |
| 系统参数（init/syspara） | 不可用 | 内存 KV Fake，按命名空间隔离，可持久化 | 不验证 SELinux/param 权限 |
| CommonEvent | 不可用 | Spy 记录 action、code、data、Want 参数和 sticky 标志 | 不验证订阅权限与跨进程分发 |
| StateRegistry | 不可用 | Spy，可注入 `IPC_CONNECT_STUB_FAIL` | 不验证真实 SA |
| eSIM Service / IMS / Satellite | 不可用 | 默认不编译；各 Profile 下用边界 Fake | 标为受控边界测试 |
| samgr/IPC/SA | 不可用 | 不启动 SA；驱动用 L3 | **IPC 与 Parcel 序列化未验证** |
| AccessToken/权限 | 不可用 | L3 绕过（`core_service_sim.cpp` 不在调用链上） | **权限校验未验证**，由 B 套件在设备上覆盖 |
| EventHandler/EventRunner | 需要移植（开源，依赖 hilog 等） | 优先使用真实库的 host 构建；否则用“事件泵”适配，它只负责存储、投递和延时 | 要证明适配没有改变同一 handler 的串行语义 |
| FFRT | 是否有 Linux 版本需核实 | 优先真实库；否则用 `ffrt::mutex`/`cv`/`submit` 到 std 的薄适配 | 线程模型差异；`ffrt::submit` 的并发度不同 |
| HiSysEvent/HiLog/HiCollie | 只做输出 | 空 sink（无业务） | — |
| OpenSSL/cJSON/libxml2 | Linux 可用 | 真实库 | — |
| ability/bundle/os_account | 不可用 | 返回固定用户 100 等的 Stub | 用户切换只做受控模拟 |

### 6.3 同步与超时的可控性 [事实 → 推断]

| 等待点 | 位置 | 可控性 |
|---|---|---|
| `activeSimConn_.wait_for(4s)` 循环 | `multi_sim_controller.cpp:975-981` | 只能用真实时间 |
| `cv_.wait` 无超时，靠 10s `MSG_SIM_TIME_OUT_ACTIVE` 事件释放 | L1199；`radio_protocol_controller.cpp:24,471-490` | 事件泵可以提前触发**事件**，但 cv 必须等真实通知 |
| `setPrimarySlotToRilCv_.wait` 无超时，靠 45s 超时事件 | L2099、L2121、L2138-2147 | 同上 |
| `radioProtocolCv_.wait_for(60s)` | `radio_protocol_controller.cpp:116` | 只能用真实时间 |
| Primary 切换使用 detached `std::thread` | `multi_sim_controller.cpp:1088-1092,1116-1147,1604-1608` | **无法 join**，是清理风险 |

**结论。**

- 不采用“虚拟时钟控制所有线程”的方案。
- **[建议]** 事件泵支持“把已调度的延时事件提前到期”。它只作用于 handler 事件，不作用于 cv。
- 等待统一采用**有界轮询**：观察条件 + 墙钟上限。
- 超时类用例（10s/45s）归入 `slow` 标签，单独执行。
- detached 线程要在进程级隔离：每个用例一个进程（`--gtest_filter` 单用例运行，由 runner 负责）。进程退出时残留线程视为“无害 FAIL 候选”，由看门狗记录。

### 6.4 禁止项（防虚假可执行性）

- 禁止用 `-Wl,--unresolved-symbols=ignore-all` 或 `-fno-access-control`。
- 禁止用空实现代替 SIM 生产源。
- 禁止 Fake 自行生成 `SimState` 或账户（状态只能来自 Fake RIL 的原始 `IccSimStatus`）。
- 每个 Fake 要在清单中说明：“返回什么数据、不实现什么业务”。

---

## 7. 独立测试工程与分层设计

### 7.1 目录（建议）

```
telephony_core_service/test/linux_sim_guard/
├── .gn  gn/BUILDCONFIG.gn  gn/toolchain/BUILD.gn     # 独立 host 工具链(clang)
├── BUILD.gn                                           # 所有 target 自声明依赖
├── third_party.lock                                   # gtest/gn/clang/ffrt/eventhandler 版本与 SHA
├── scenario/        # 场景层: 业务动作 + 前置拓扑 + 期望 (与实现无关, 纯数据/DSL)
├── contract/        # 断言层: CT-*/INV-* 检查器 (只读观察记录)
├── driver/          # 驱动层: L3 ISimManager 绑定 (可随重构替换: L2/新服务)
├── env/             # 环境层: FakeRil, FakeDataShare, FakeParam, 事件泵, 平台 stub
├── observe/         # 观察层: CommonEventSpy, RegistrySpy, 回调记录器, RIL 调用日志
├── suites/{a_domain,b_api,c_legacy}/   # gtest 用例, 仅组装以上各层
├── profiles/        # BASE-2SLOT, BASE-1SLOT, TRI-SLOT, ESIM, VSIM
└── tools/run.py     # 进程级单用例执行、看门狗、报告
```

### 7.2 分层规则

| 层 | 可随 DDD 重构变化？ | 规则 |
|---|---|---|
| 场景 | 否 | 只使用别名（Card-A、Sub-A），不出现类名或 slotId 常量 |
| 驱动 | 是 | 负责别名到当前 slotId/simId 的绑定；每次变更写入 `driver/CHANGELOG` |
| 环境 | 是 | 替身接口版本化，例如 `FakeRil@ITelRilManager-v1`、`FakeRil@HDI-V1_5` |
| 观察 | 部分 | 事件和参数键属于契约；采集方式可以变 |
| 断言 | 否 | 以 Contract/Invariant ID 为键，失败信息必须带 ID |
| 运行 | 是 | GN target 名可以变，套件 ID 不变 |

---

## 8. 能力—契约—不变量目录

### 8.1 能力（Capability）

| CAP | 能力 |
|---|---|
| CAP-PRES | 卡存在性与就绪状态查询 |
| CAP-ID | 卡/订阅身份与账户可见性 |
| CAP-ACT | 订阅 Active 控制 |
| CAP-ROLE | 默认 Voice/SMS/Data 角色 |
| CAP-PRI | 主卡与 Radio 资源协调 |
| CAP-PERS | 业务配置持久化与恢复 |
| CAP-NOTIFY | 外部通知（事件/回调/Registry） |

### 8.2 契约（v1 = HEAD 2841c6f 行为基线的公开语义部分）

| Contract ID | 内容 | 来源 | Oracle |
|---|---|---|---|
| CT-API-STATE-v1 | `GetSimState(slot)` 返回 `SimState` 枚举值，合法 slot 返回 SUCCESS | `i_sim_manager.h:37`、`sim_state_type.h:84-118` | CONFIRMED_CONTRACT（签名）；语义见 INV |
| CT-API-HAS-v1 | `HasSimCard(slot)` 与卡存在性一致 | `i_core_service.h:65` | CONFIRMED_CONTRACT |
| CT-API-ACCT-v1 | `GetSimAccountInfo` / `GetActiveSimAccountInfoList` 返回 `IccAccountInfo`（simId、slotIndex、iccId、isEsim、isActive…） | `sim_state_type.h:386-424` | CONFIRMED_CONTRACT（字段）；无卡时错误码为 OPEN_QUESTION |
| CT-API-ACT-v1 | `SetActiveSim(slot, enable)` 成功时 `IsSimActive` 反映结果；失败时返回错误码且状态不变 | `i_sim_manager.h:57` | VERIFIED_INVARIANT（待评审升级） |
| CT-API-PRI-v1 | `SetPrimarySlotId` 完成后 `GetPrimarySlotId` 反映结果，并发布 `SET_PRIMARY_SLOT_STATUS` | `i_sim_manager.h:64`；`multi_sim_controller.cpp:1413-1426` | VERIFIED_INVARIANT |
| CT-API-ROLE-v1 | Set/GetDefaultVoice（SMS/Data 同理）对称；-1 表示移除 | L1275-1381 | VERIFIED_INVARIANT |
| CT-EVT-SIMSTATE-v1 | 状态变化时发布 sticky `SIM_STATE_CHANGED`，事件本身**不带 slotId** | `sim_state_handle.cpp:816-830` | LEGACY_BEHAVIOR（是否“应带 slotId”为 OPEN_QUESTION Q2） |
| CT-EVT-DEFSUB-v1 | 默认角色 simId 变化时发布对应 `*_SUBSCRIPTION_CHANGED`，携带 `PARAM_SIMID` | L1894-1922 | CONFIRMED_CONTRACT（公开事件） |
| CT-REG-SIMSTATE-v1 | StateRegistry 收到 `(slotId, type, state, reason)` | `sim_state_handle.cpp:481-482` | VERIFIED_INVARIANT |
| CT-CB-ACCT-v1 | 账户变化时 `ISimAccountCallback::OnSimAccountChanged` 被调用 | `multi_sim_monitor.cpp:731-741` | VERIFIED_INVARIANT |
| CT-RIL-ACT-v1 | Active 操作向目标卡所在路由下发 `SetActiveSim(enable)` | `radio_protocol_controller.cpp:471-490` | 受控边界，内部接缝 |

### 8.3 不变量

| INV | 陈述 | 一致性模式 |
|---|---|---|
| INV-PRES-001 | 硬件端点无卡时，该路由的状态查询最终为 NOT_PRESENT，`HasSimCard=false`，且不存在 active 账户 | 收敛窗口后 |
| INV-PRES-002 | 卡上报 PIN/PUK 锁时，状态为 LOCKED，**绝不**报告 READY/LOADED | 每个可观察点 |
| INV-ID-001 | 同一 ICCID 的订阅，其 simId 在路由变化、就绪顺序变化、服务重建后保持不变 | 收敛窗口后 |
| INV-ID-002 | `GetSlotId(GetSimId(r)) == r` 对每个 active 路由成立 | 收敛窗口后 |
| INV-ID-003 | 不同 ICCID 不会共享 simId | 每个可观察点 |
| INV-ISO-001 | 对目标卡的操作不改变非目标卡的 ICCID、simId、存在性；仅允许契约声明的角色迁移 | 每个可观察点 |
| INV-ACT-001 | Active 操作失败或超时后，Active 查询结果与操作前一致 | 操作返回后 |
| INV-PRI-001 | Primary 切换失败或超时后，`GetPrimarySlotId` 等于切换前的值，并发出 `setDone` 状态事件 | 操作结束后 |
| INV-PERS-001 | 已成功设置的默认 Voice/SMS 与 Primary 在服务重建后恢复 | 恢复窗口后 |
| INV-NOTIFY-001 | 每个外部状态变化都有对应的 Registry 更新，最终值等于查询值 | 收敛窗口后 |

---

## 9. Suite Matrix

| 维度 \ 用例 | PRES-001 | ID-001 | LOCK-001 | MULTI-001 | ISO-001 | PERS-001 | ACT-001 | PRI-001 |
|---|---|---|---|---|---|---|---|---|
| 卡数量 | 0/2 | 1 | 1 | 2 | 2 | 1 | 2 | 2 |
| 类型 | — | pSIM | pSIM | pSIM×2 | pSIM×2 | pSIM | pSIM | pSIM×2 |
| 状态 | ABSENT | READY→LOADED | PIN | READY 乱序 | 拔出 | READY | READY | READY |
| 角色 | 无 | 单卡默认 | 无 | 默认/Primary | Primary 在 B | — | 非 Primary | Primary 迁移 |
| 故障 | — | — | — | 顺序 | 热插拔 | DataShare 延迟/失败 | RIL 失败/超时 | RIL 失败/超时 |
| Profile | BASE-2SLOT | BASE-1SLOT | BASE-1SLOT | BASE-2SLOT | BASE-2SLOT | BASE-1SLOT | BASE-2SLOT | BASE-2SLOT |
| 恢复 | — | — | — | — | — | 重建实例 | — | 重建实例 |

P1/P2 场景的覆盖由 §10.9 的 backlog 补齐。

---

## 10. 首批完整用例

**公共约定（适用于全部用例）**

- 驱动：L3 `ISimManager`。
- 收敛窗口 `T_conv = 5s`，采用有界轮询，每 20ms 一次。[建议] 该值是运行参数，不是契约。
- 别名到 slotId 的绑定写在 `driver/binding_base2slot.json`。
- 所有用例：Test Level = 受控边界；Linux Feasibility = **BLOCKED**（前置条件 G1–G5，见 §13）；Owner = 待指派；Status = Draft。
- 通用 PASS/FAIL/BLOCKED 判定见 §15.1。各用例只写差异。

---

### 10.1 SIM-ST-PRES-001 无卡启动与查询

**1. Metadata**

| 字段 | 内容 |
|---|---|
| Test Case ID | SIM-ST-PRES-001 |
| Name | 两个硬件端点均无卡时，启动后所有查询一致报告“无卡、无账户” |
| Capability | CAP-PRES、CAP-ID |
| Requirement ID | 待确认（REQ-TBD-PRES） |
| External Contract | CT-API-STATE-v1、CT-API-HAS-v1、CT-API-ACCT-v1、CT-REG-SIMSTATE-v1 |
| Invariant | INV-PRES-001、INV-NOTIFY-001 |
| Priority | P0 |
| Test Type | Functional |
| Topology | 2 个端点 × 空 |
| Automation | Yes（闭包就绪后） |
| Linux Feasibility | BLOCKED（G1–G5） |
| Oracle | 状态：VERIFIED_INVARIANT；账户查询错误码：OPEN_QUESTION Q1 |

**2. Business Intent.** 设备没插卡时开机，上层（设置、拨号）查询不能看到任何卡或账户。
Out of Scope：紧急呼叫、飞行模式、eSIM。

**3. External Contract**

- Trigger：服务初始化；Fake RIL 对每个端点的 `GetSimStatus` 回应 `ICC_CARD_ABSENT`。
- GIVEN 两端点无卡、DataShare 为空
  WHEN 服务初始化并完成首轮状态查询
  THEN 每个路由的 `GetSimState` 为 `NOT_PRESENT`（证据：API）
  AND `HasSimCard=false`（API）
  AND `GetActiveSimAccountInfoList` 为空，或返回非 SUCCESS 错误码（Q1）
  AND Registry 最后一次收到的该路由状态为 NOT_PRESENT（Spy）。
- 映射来源：`sim_state_handle.cpp:905-910`。

**4. Preconditions / Topology**

| 业务对象 | 硬件接入点 | 路由 Slot | 类型 | Profile/ICCID/Port | 卡状态 | 订阅 | Radio |
|---|---|---|---|---|---|---|---|
| EP-1 | EP-1 | 适配层绑定 | — | — | ABSENT | 无 | ON |
| EP-2 | EP-2 | 适配层绑定 | — | — | ABSENT | 无 | ON |

- 默认角色：无。Primary：参数为空（默认 "0"，`multi_sim_controller.cpp:76`）。
- 非飞行模式，用户 100，冷启动。
- 持久化：空。权限：N/A（L3）。Feature：BASE-2SLOT。

**5. Steps**

| Step | 动作 | 期望 | 观察点 | 窗口 |
|---|---|---|---|---|
| 1 | Env 启动，FakeRil 设为 ABSENT×2 | — | — | — |
| 2 | Driver 初始化（`OnInit(slotCount)`） | 返回 true | API | 即时 |
| 3 | FakeRil 上报 `RADIO_SIM_STATE_CHANGE`（两端点） | FakeRil 收到 `GetSimStatus` | RIL 日志 | T_conv |
| 4 | 查询 State/Has/账户 | 符合 §3 | API | T_conv 内收敛 |

**6. Final State.**

- 卡：ABSENT×2。Profile：N/A。账户：无。
- 路由：无绑定。Active：无。默认角色：未设置（值为 OPEN_QUESTION Q3）。
- Radio：不变。持久化：没有写入新账户。

**7. Invariants.** INV-PRES-001（作用域：两个端点，收敛窗口后）；INV-NOTIFY-001。

**8. Transition.** 卡维度：UNKNOWN → NOT_PRESENT。**不要求**观察 UNKNOWN。

**9. Event/Callback**

| 对外事件 | 必须 | 次数 | 偏序 | 身份 | 依据 |
|---|---|---|---|---|---|
| SIM_STATE_CHANGED（code=ABSENT） | 否（是否在初态上报为 Q4） | ≥0 | — | 无 slotId | CT-EVT-SIMSTATE-v1 |
| Registry UpdateSimState(NOT_PRESENT) | 是 | ≥1 | — | slot | CT-REG |
| OnSimAccountChanged | 否 | 任意 | — | — | — |

**10. Isolation.** 两个端点对称，无迁移。
**11. Persistence.** N/A：没有业务配置写入。
**12. Failure.** N/A：故障变体见 PERS-001。
**13. Concurrency.** 两端点的上报顺序任意，终态唯一。
**14. Coverage.** 0 卡 × 2 路由。
**15. Evidence.** 以 API 和 Registry Spy 为证据。日志只做诊断。
**16. Independence.** 不依赖内部类或表。Slot 只作路由。改为 Subscription-Centric 后场景仍成立，只是“无订阅”。拆 SO 后只改驱动。
追溯：CAP-PRES → CT-API-STATE → INV-PRES-001 → PRES-001 → `a_domain/pres_test.cc`。
**17. Protection / DoD.** 保护 API、状态和事件。缺少的 DoD 项：Q1。

---

### 10.2 SIM-ST-ID-001 单卡身份与账户可见性

**1. Metadata.** P0；CAP-ID；CT-API-ACCT-v1、CT-CB-ACCT-v1；INV-ID-001、INV-ID-002；Transition；1×pSIM；BLOCKED；Oracle：身份映射 VERIFIED_INVARIANT，`showName` 默认值为 LEGACY（`DEFAULT_SHOW_NAME=u"Card"`，`sim_state_type.h:424`）。

**2. Intent.** 插入一张已就绪的卡后，调用方能拿到稳定身份（ICCID ↔ simId ↔ 当前路由）和 active 账户。
Out：号码显示、运营商名。

**3. Contract**

- GIVEN Card-A（ICCID=`8986000000000000000A`，虚构值）在 EP-1，状态 READY
- WHEN 服务初始化，文件加载完成
- THEN `GetSimState` 最终 ∈ {READY, LOADED}，并最终到达 LOADED（Q5：是否所有卡型都会到 LOADED）
- AND `GetSimAccountInfo(route)` 的 iccId 为 Card-A 的值，simId>0，isActive=true，isEsim=false
- AND `GetSlotId(simId)` 等于当前路由
- AND 已注册回调至少收到一次 `OnSimAccountChanged`。

**4. Topology.** Card-A / EP-1 / 绑定 / pSIM / ICCID-A / READY / 无（初始） / ON。BASE-1SLOT，DataShare 为空。
**5. Steps.**

| Step | 动作 | 期望 | 观察点 | 窗口 |
|---|---|---|---|---|
| 1 | 注册回调 | — | — | — |
| 2 | 初始化 | 返回 true | API | 即时 |
| 3 | FakeRil 回应 READY 和 ICCID；文件读取剧本返回 EF 数据 | 账户可见 | API/回调 | T_conv |

**6. Final.** 卡存在；Subscription-A active；路由为 EP-1 的路由；单卡时默认角色为 Sub-A（LEGACY：`SimDataBuilder` 在单卡时设为 MAIN，L560-587）；持久化了 ICCID 对应的记录（通过重建后的身份一致性间接观察，见 PERS-001）。
**7. INV.** INV-ID-002（收敛后）；INV-ID-003。
**8. Transition.** 卡：UNKNOWN → READY → LOADED；账户：无 → active。两个维度分开记录。
**9. Events.** SIM_STATE_CHANGED code=READY 必须出现；LOADED 事件来自文件层（`icc_file.cpp:442`），必须出现且在 READY 之后（偏序约束为 VERIFIED_INVARIANT）；OnSimAccountChanged ≥1。
**10. Isolation.** N/A（单卡）。**11. Persistence.** 见 PERS-001。**12. Failure.** N/A。**13. Concurrency.** N/A。
**14. Coverage.** 1×pSIM READY。**15. Evidence.** API、回调、事件 Spy。
**16. Independence.** 身份以 ICCID 表达，不以 slot 表达；改为 Subscription-Centric 后仍成立。
**17. DoD.** 缺少的项：Q5、文件剧本的最小 EF 集合（G6）。

---

### 10.3 SIM-ST-LOCK-001 Locked 与 Ready 差异

**Metadata.** P0；CAP-PRES；INV-PRES-002；Transition；1×pSIM；BLOCKED；Oracle：LOCKED 映射为 VERIFIED_INVARIANT（`sim_state_handle.cpp:919-926`），锁卡时的账户可见性为 **OPEN_QUESTION Q6**（`multi_sim_monitor.cpp:111-116` 在 `RADIO_SIM_STATE_LOCKED` 时也会触发 InitData）。

**Intent.** 锁卡时系统不能表现为“可用卡”。解锁后进入可用状态。
Out：PUK 次数策略。

**Contract.**

- GIVEN Card-A 状态 PIN
- WHEN 初始化
- THEN 状态为 LOCKED，且在任何观察点都不是 READY/LOADED（INV-PRES-002 每点成立）
- AND Registry reason=SIM_PIN
- WHEN `UnlockPin(route, 正确PIN)`，FakeRil 回送成功并把后续状态切换为 READY
- THEN 返回 `UNLOCK_OK`（`sim_state_type.h:267`），状态最终 ∈ {READY, LOADED}。
- 错误 PIN 变体：返回 `UNLOCK_INCORRECT`，remain 取 FakeRil 提供的值，状态保持 LOCKED。

**Topology.** Card-A / PIN。PUK 变体为 LOCKED/SIM_PUK。
**Steps.** 初始化 → 观察 LOCKED → 解锁（错误 PIN）→ 解锁（正确 PIN）→ 收敛。每步窗口 T_conv。
**Final.** 卡 READY；账户 active（Q6 只决定锁定阶段的账户可见性）。
**Events.** SIM_STATE_CHANGED code=PIN 在 READY 之前（偏序）。
**Failure.** FakeRil 对 UnlockPin 不响应 → 期望在有界时间内返回失败（上限为 OPEN_QUESTION Q7）。
**Isolation/Persistence/Concurrency.** N/A（单卡；无配置；无并发）。
**Independence.** 保持。**DoD 缺项：** Q6、Q7。

---

### 10.4 SIM-ST-MULTI-001 双卡不同就绪顺序

**Metadata.** P0；CAP-ID/CAP-ROLE；INV-ID-001、INV-ID-003；Concurrency；pSIM×2；BLOCKED；Oracle：身份稳定为 VERIFIED_INVARIANT，默认角色的初选结果为 **LEGACY**。

**Intent.** 两张卡谁先就绪，不应改变各自的身份绑定。
Out：默认卡选择策略本身（C 套件单独记录）。

**Contract.**

- 变体 A：Card-A 先 READY，Card-B 后 READY。变体 B：顺序相反。两个变体各跑一次（数据命名空间相同，服务分别冷启动；变体 B 先清空持久化）。
- THEN 每个变体内，ICCID-A 与 ICCID-B 的 simId 不同；`GetSlotId(simId)` 指向各自的路由。
- AND 在持久化相同的前提下，变体 A 结束后重建并以变体 B 的顺序就绪 → **simId 与 ICCID 的对应关系保持不变**（INV-ID-001）。

**Topology.** Card-A@EP-1，Card-B@EP-2。
**Steps.** 初始化 → FakeRil 按剧本顺序投递（间隔可配置 0/200/1000ms）→ 收敛 → 记录映射 → 重建 → 反序投递 → 比较。
**Final.** 两个账户都 active；默认角色的具体取值只进 C 套件（SIM-LEG-ROLE-001）。
**Events.** 每卡至少一次 READY 事件（事件无 slotId，因此只做计数 ≥2 + 查询佐证）。
**Isolation.** 卡 B 就绪不改变卡 A 的 ICCID/simId。
**Persistence.** 跨实例保留 Fake DataShare 和参数。这是“服务重建”，不是设备重启。
**Concurrency.** 间隔为 0 时视为近似并发。允许的结果集合：映射唯一，默认角色 ∈ {A, B}。
**DoD 缺项：** 无（除环境阻塞 G1–G5）。

---

### 10.5 SIM-ST-ISO-001 非目标卡身份隔离（拔出 B）

**Metadata.** P0；CAP-ID；INV-ISO-001；Transition；pSIM×2，Primary 在 B；BLOCKED；Oracle：A 身份不变为 VERIFIED_INVARIANT；拔卡后 Primary/Data 的归属为 **OPEN_QUESTION Q8**。

**Contract.**

- GIVEN 双卡 READY，Primary 为 Sub-B
- WHEN FakeRil 把 EP-2 改为 ABSENT 并上报
- THEN 路由 B 为 NOT_PRESENT，Sub-B 不在 active 列表中
- AND Sub-A 的 ICCID、simId、路由、Active 不变（每个观察点都成立）
- AND 允许的变化：默认角色从 B 迁移到 A，或变为未设置（由 Q8 决定），并且必须有对应的默认订阅变更事件。

**Steps.** 建立双卡 → 设置 Primary 为 B（或由初始参数预置）→ 拔出 B → 收敛观察。
**Failure.** FakeRil 在拔出后重复上报 ABSENT 两次 → 结果不变（幂等）。
**Isolation.** 核心条款见上。“A 的其他属性全部不变”**不**作为无条件规则。
**DoD 缺项：** Q8。

---

### 10.6 SIM-ST-PERS-001 外部持久化服务延迟与失败

**Metadata.** P0；CAP-PERS/CAP-ID；INV-ID-001、INV-PERS-001；Failure/Recovery；1×pSIM；BLOCKED；Oracle：最终可见为 VERIFIED_INVARIANT；重试策略（10 次 × 1s，`multi_sim_monitor.cpp:281-319`）为 **LEGACY**，不作为强断言。

**Contract.**

- 变体 D（延迟）：DataShare 在 3s 后才可用 → 账户在“恢复后 + T_conv”内可见，并且回调通知至少一次。
- 变体 F（失败）：DataShare 在整个用例期间一直失败 → 系统**不得伪造**账户（不能出现 simId>0 且 iccId 为 Card-A 的 active 账户）；卡状态查询仍为 READY/LOADED（卡维度与账户维度独立）。
- 变体 R（恢复）：失败后发送 DataShare Ready 通知（`multi_sim_monitor.cpp:512-520`）→ 最终可见。**R 是否能在重试耗尽后恢复是 Q9**。
- 重建：设置默认 Voice 成功后重建服务实例（DataShare 和参数 Fake 保留）→ `GetDefaultVoiceSimId` 与重建前一致（INV-PERS-001）。

**Failure 表**

| 边界 | 注入 | 条件 | 期望 | 恢复 |
|---|---|---|---|---|
| DataShare | 创建 helper 失败 | 全程 | 无伪造账户 | Ready 事件 |
| DataShare | 延迟 3s | 首次查询 | 最终可见 | 自动 |
| DataShare | 插入成功但查询失败 | 一次 | 最终可见或不可见（Q9） | 重试 |

**Persistence 说明.** 重建 = 销毁 SimManager + 事件泵，再重新构造，**不是设备重启**。系统参数中 persist 前缀的键保留，其他键清空。
**DoD 缺项：** Q9。

---

### 10.7 SIM-ST-ACT-001 Active 操作成功、失败与超时

**Metadata.** P0；CAP-ACT；CT-API-ACT-v1、CT-RIL-ACT-v1；INV-ACT-001、INV-ISO-001；Failure；pSIM×2；BLOCKED（超时变体属于 slow 组）；Oracle：成功/失败语义为 VERIFIED_INVARIANT；10s 超时值为 LEGACY（`radio_protocol_controller.cpp:24`）。

**Contract.**

- 成功：`SetActiveSim(route-B, 0)` → FakeRil 收到目标路由的 `SetActiveSim(enable=0)` → 返回 SUCCESS → `IsSimActive(route-B)=false`，Sub-B 不在 active 列表中；Sub-A 不受影响。
- 失败：FakeRil 回送错误 → 返回非 SUCCESS；`IsSimActive(route-B)` 保持 true（INV-ACT-001）。
- 超时：FakeRil 不回送响应 → 在 ≤ 10s + 2s 容差内返回非 SUCCESS，状态保持不变。**这里的 PASS 条件是“正确处理了超时”**。
- 迟到：超时后 FakeRil 再送回成功 → 状态**不得**翻转为 inactive（Q10：迟到响应的处理是否算契约）。
- 恢复：再次调用 `SetActiveSim(route-B, 1)` → active。

**外部调用表**

| 组件 | 方法 | 目标/参数 | 允许结果 | 观察 |
|---|---|---|---|---|
| RIL | SetActiveSim | route-B、enable=0 | 必须出现；**不得**发往 route-A | RIL 日志 |
| DataShare | IS_ACTIVE 更新 | Sub-B | 成功路径存在即可，不锁定调用方式 | 通过重建后的 IsSimActive 间接观察 |

**Concurrency 变体（P1 延后）.** 在 Primary 切换进行中调用 SetActiveSim → 允许先等待（4s 循环，L975-981）；结果集合为 {成功, 失败}，两种结果下 INV-ACT-001 都必须成立。
**Isolation.** Sub-A 的 active、身份不变；如果 B 是默认角色，允许角色迁移，但必须有事件（LEGACY，见 Q11）。
**DoD 缺项：** Q10、Q11。

---

### 10.8 SIM-ST-PRI-001 Primary/Radio 协调成功、失败与超时

**Metadata.** P0；CAP-PRI/CAP-ROLE；CT-API-PRI-v1；INV-PRI-001；Failure/Recovery；pSIM×2，Primary=A；BLOCKED（超时变体属于 slow 组，45–60s）；Oracle：结果/回滚语义为 VERIFIED_INVARIANT；“成功切换后默认数据跟随 Primary”为 **LEGACY**（L1522）；失败后 5 次重试（L1530-1539）为 LEGACY。

**Contract.**

- 成功：`SetPrimarySlotId(route-B)` → FakeRil 依次处理 CHECK/UPDATE/NOTIFY/COMPLETE，全部成功 → `GetPrimarySlotId==route-B`；`SET_PRIMARY_SLOT_STATUS` 的 setDone=true 在结果可查询之后或同时可见；`MAIN_SUBSCRIPTION_CHANGED` 携带 Sub-B 的 simId 和 slotId。
- 失败：CHECK 阶段 FakeRil 对路由 B 回错 → 最终 `GetPrimarySlotId==route-A`；发布 setDone 状态事件；不得出现“Primary=B 但资源未交换”这类可观察的组合。
- 超时：FakeRil 不回 CHECK → 在 ≤ 60s + 容差内结束，结果同“失败”。
- 身份：全过程中 ICCID ↔ simId 映射不变（INV-ID-001）。**路由与 modem 的交换属于资源变化，不是身份变化**。

**外部调用表**

| 组件 | 方法 | 参数 | 允许结果 | 观察 |
|---|---|---|---|---|
| RIL | GetRadioProtocol / SetRadioProtocol | 各路由；新旧 protocol 交换 modemId | 必须覆盖两条路由；阶段偏序为 CHECK < UPDATE < COMPLETE | RIL 日志（只校验偏序，不校验次数） |
| CommonEvent | SET_PRIMARY_SLOT_STATUS | setDone、isUserSet | 结束时必须有 setDone=true | Spy |
| 系统参数 | MainSlotId | — | 不作为断言（内部接缝），只通过重建后的 `GetPrimarySlotId` 观察 | — |

**Persistence.** 成功后重建 → `GetPrimarySlotId==route-B`（INV-PERS-001）。
**Concurrency.** 由于有 detached 线程（L1088-1092），本用例**必须进程级独立运行**。
**DoD 缺项：** Q12（Primary 与 Data 的关系）、Q13（重试期间对外可见的中间状态）。

---

### 10.9 P1/P2 Backlog（模板待补）

| ID | 场景 | 可行性 | 阻塞/说明 |
|---|---|---|---|
| SIM-ST-ESIM-001 | pSIM + eSIM 共存，身份不混淆 | BLOCKED | 需要 ESIM Profile 构建、eSIM SA Fake |
| SIM-ST-PROF-001 | Profile 切换/停用后，订阅身份与默认配置 | BLOCKED | 需要 eSIM 剧本；Q14 |
| SIM-ST-ROUTE-001 | 路由变化（Primary 交换）后业务身份保持 | BLOCKED | 基于 PRI-001 的扩展 |
| SIM-ST-ROUTE-002 | 同一路由换插另一张卡，身份不复用 | BLOCKED | 热插拔剧本 |
| SIM-ST-ROLE-001 | 默认 Voice/SMS 重建后恢复；移除卡再插回后恢复 | BLOCKED | Q3 |
| SIM-ST-TRI-001 | 三卡（slot 2/VSIM）拓扑 | BLOCKED | VSIM Profile、EXT 宏 |
| SIM-ST-ORDER-001 | RIL 迟到、重复、乱序；StateRegistry `STUB_FAIL` 后重投（`sim_state_handle.cpp:483`） | BLOCKED | — |
| SIM-ST-MEP-001 | MEP 多 Port | DEVICE_REQUIRED | 公开 API 缺少 Port 语义 |
| SIM-ST-QUAD-001 | 四 Subscription | DEVICE_REQUIRED | 槽上限 `MAX_SLOT_COUNT=3`（`telephony_types.h:49`） |
| SIM-ST-SAT-001 | 卫星 Active（不下发 RIL） | BLOCKED | 卫星 Profile |

---

## 11. Oracle、冲突与待确认事项

### 11.1 套件划分

- **A 长期领域契约**：§10 各用例中标为 VERIFIED_INVARIANT 或 CONFIRMED_CONTRACT 的断言。
- **B 已发布 API 兼容**：`SIM-API-001..`。检查签名编译、错误码枚举值、事件名/参数键字符串、`IccAccountInfo` 字段集与 Parcel 字段顺序。Parcel 顺序在设备上验证；Linux 只做编译期和字符串常量检查。
- **C Legacy 特征（可淘汰）**：

| ID | 当前行为 | 源 | 风险 |
|---|---|---|---|
| SIM-LEG-PRI-001 | 主卡切换成功后，默认数据同步到主卡 | `multi_sim_controller.cpp:1522,1685-1690` | 与“Primary≠Data”的领域分离冲突（C1） |
| SIM-LEG-ACT-001 | 新卡入库时默认 IS_ACTIVE=ACTIVE | L565、L571 | 用户停用后换插卡再插回的语义不明 |
| SIM-LEG-EVT-001 | SIM_STATE_CHANGED 不带 slotId | `sim_state_handle.cpp:816-830` | 多卡下无法关联到具体卡（C2） |
| SIM-LEG-ID-001 | simId 从 1 开始、连续 | L960-965 | DB 删除后可能越界 |
| SIM-LEG-RETRY-001 | InitData 重试 10×1s；Primary 重试 5×1s | monitor L281-319；controller L1530-1539 | — |
| SIM-LEG-SAT-001 | 卫星 Active 不下发 RIL | L1036-1082 | — |
| SIM-LEG-ROLE-001 | 双卡首次入库的默认角色都是 NOT_MAIN | L560-587 | — |

### 11.2 疑似缺陷（记录，不纳入断言）

- `sim_manager.cpp:1600-1602`：判空条件疑似写反（`!= nullptr` 时返回空）。
- `multi_sim_controller.cpp:1621-1624`：`REFRESH_LOCAL_CACHE_RETRY` 缺少 break，会贯穿到下一分支。
- `GetMaxSlotCount` 与 `GetMaxSlotCountMd` 共用同一个缓存（`telephony_types.h:100-151`）。

以上仅为静态阅读所见，需要各自建立 issue 确认。**不得**在 C 套件中把它们固化为期望。

### 11.3 冲突

- **C1**：领域上 Primary（资源）与默认 Data（角色）是两个维度，而 Legacy 会让它们联动。A 套件只断言“角色变化必须有事件”，是否联动由 C 套件记录，**等待业务确认**。
- **C2**：多卡场景下状态事件无法区分卡。A 套件只用查询和 Registry 做证据，不依赖该事件区分卡。

### 11.4 待业务确认

| Q | 问题 |
|---|---|
| Q1 | 无卡时账户查询应返回空列表还是错误码 |
| Q2 | SIM_STATE_CHANGED 是否应携带 slotId/simId（涉及契约版本） |
| Q3 | 无卡或卡移除后，默认角色查询的期望值 |
| Q4 | 初态 ABSENT 是否必须广播 |
| Q5 | 所有卡型是否都必须到达 LOADED |
| Q6 | 锁卡期间账户是否可见 |
| Q7 | UnlockPin 无响应时的上限 |
| Q8 | Primary 卡拔出后，Primary/Data 的归属 |
| Q9 | 持久化失败且重试耗尽后，是否必须能恢复 |
| Q10 | Active 迟到响应的处理 |
| Q11 | 停用默认卡后角色是否迁移 |
| Q12 | Primary 是否必然等于默认 Data |
| Q13 | Primary 重试期间对外可见的状态 |
| Q14 | Profile 停用与“卡不存在”的对外表达 |

---

## 12. 拆 SO 与 DDD 重构前后的复用策略

| 重构 | 场景/断言 | 驱动 | 环境 | 说明 |
|---|---|---|---|---|
| 拆出 `libtel_sim_service.z.so` | 不变 | 链接目标改为新 SO；入口仍用 ISimManager 或其新门面 | 不变 | 如果新 SO 改为 dlopen，需要在设备上补充单例一致性验证 |
| Slot-Centric → Subscription-Centric | 不变（本来就以别名/ICCID 表达） | 绑定表改为 Subscription 查询 | 不变 | 若 API 新增 simId 版本，B 套件增加 v2 |
| 拆分卡/Profile/订阅/角色/资源 | 不变 | 可能需要多个门面 | FakeRil 接缝可能下移到 HDI | 使用版本化 Env |
| 内部事件或 Repository 调整 | 不变 | 不变 | DataShare Fake 的 URI 语义若变 → 这是**跨仓契约变更**，需要审批 | — |

**差分运行。** 同一场景在 Legacy 和 Refactored 两个构建上分别执行，比较三项：

- 领域终态；
- 外部事件集合与关键偏序；
- API 返回。

允许内部调用序列、表结构、线程和内部事件不同。C 套件的差异进入人工评审，**不自动判定失败**。

---

## 13. 可行性清单

**Linux 闭包前置门槛**

| Gate | 内容 | 状态 |
|---|---|---|
| G1 | SIM 无条件源 + tel_common + innerkits 在 host 上用 Clang **无忽略未解析符号** 地完成链接 | 未验证 |
| G2 | EventHandler 与 FFRT 的 host 可用性（真实库或经审计的薄适配） | 未验证 |
| G3 | Fake RIL 通过正式 `InnerEvent` 响应驱动 `GetSimCardData`（`sim_state_handle.cpp:634`） | 未验证 |
| G4 | DataShare/参数/CommonEvent/Registry 的边界 Fake，并附清单审计 | 未验证 |
| G5 | 进程级隔离的 runner、看门狗、残留线程检测 | 未验证 |
| G6 | 文件加载剧本（READY→LOADED 所需的最小 EF 集合） | 未调查 |

**用例状态**

| 状态 | 用例 |
|---|---|
| EXECUTABLE | **当前无**（G1–G5 打通并实测后，§10.1–10.8 才可转为 EXECUTABLE） |
| BLOCKED | SIM-ST-PRES/ID/LOCK/MULTI/ISO/PERS/ACT/PRI-001；ESIM、PROF、ROUTE、ROLE、TRI、ORDER、SAT；B 套件中的 Parcel 部分 |
| DEVICE_REQUIRED | MEP-001、QUAD-001；IPC/SA/权限（B 套件 L1）；真实 Modem 时序；真实 DataShare/telephony_data；设备冷重启恢复 |

---

## 14. 构建、执行、清理与结果

- **构建**：`gn gen out/lsg --root=test/linux_sim_guard --args='production_root=... gtest_root=... profile="BASE-2SLOT"'`，然后 `ninja -C out/lsg lsg_domain lsg_api lsg_legacy`。
  - 链接开启 `-Wl,--no-undefined`，并生成 link map。
  - 用 map 审计被测符号来自生产 `.o`，而不是 Fake。
- **执行**：`tools/run.py` 为每个用例单独起一个进程，参数为 `--gtest_filter=<ID>` 和 `--gtest_output=xml`。
  - 看门狗：普通用例 30s，slow 用例 120s。
  - 标签：`fast`、`slow`、`legacy`。
- **结果**：GTest XML 加 `result.json`，内容包括：
  - HEAD SHA、工作树脏标志、Profile、工具链版本、替身清单版本；
  - 每个断言的 Contract/Invariant ID 和 Oracle 类别。
  - BLOCKED/SKIP 单独计数，**不计为 PASS**。
- **负控**：每个 Invariant 检查器配一个“被篡改观测”的负向用例，证明它会 FAIL。
- **清理**：
  - Fake 持久化使用临时目录命名空间，用例结束时删除；
  - 事件泵排空；
  - 残留线程和 detached 线程由进程退出回收，并在报告中记录。

---

## 15. 风险与实施顺序

### 15.1 通用判定

- **PASS**：前提有效、动作已执行、契约与不变量成立、在窗口内收敛、无非预期跨卡影响。
- **FAIL**：可执行的断言被违反、不收敛、错误的跨卡影响、崩溃或看门狗超时。
- **BLOCKED/SKIP**：前提或环境不具备。

### 15.2 风险

| R | 风险 | 缓解 |
|---|---|---|
| R1 | EventHandler/FFRT 在 host 上的适配改变串行语义 | 用真实库优先；适配层做单独的语义一致性测试 |
| R2 | detached 线程、无超时的 cv 导致悬挂 | 进程隔离 + 看门狗 |
| R3 | DataShare Fake 语义偏离 telephony_data | 与该仓确认 URI 和自增契约，Fake 版本化 |
| R4 | 链接闭包太大（STK、短信、号码簿） | 接受真实源；不裁剪业务 |
| R5 | 本地 HEAD 是 fork 提交 | 实施前与 upstream 默认分支对齐并重新核对行号 |
| R6 | 未提交的 `test/systemtest` 与本方案并存 | 实施前决定合并还是替换，避免出现两套 Oracle |

### 15.3 实施顺序

1. 对齐 HEAD，锁定工具链。
2. G1 链接审计。
3. G2 事件和线程。
4. G3/G4 边界 Fake 及清单。
5. G5 runner。
6. PRES-001 首个垂直切片。
7. ID/LOCK/MULTI/ISO。
8. PERS/ACT。
9. PRI（slow）。
10. B 套件的编译期兼容检查。
11. C 套件。
12. P1。

---

## 16. 最终问答

**哪些测试在 DDD 重构后可以保留场景和断言？**
§10 中 A 类断言全部保留：无卡查询、ICCID↔simId↔路由的可逆映射与稳定性、LOCKED 不冒充 READY、非目标卡隔离、持久化失败时不伪造账户、Active 与 Primary 的失败回滚、默认角色变化必有事件、重建后配置恢复。

**哪些技术适配需要随重构调整？**
驱动入口（ISimManager → 新门面或 CoreService）、别名到 slotId/simId 的绑定、Fake RIL 的挂载层（ITelRilManager → HDI）、DataShare/参数 Fake 的键和 URI、事件泵与线程适配、GN 链接目标（新 SO）。

**哪些现有行为仅应暂时作为 Legacy 特征？**
§11.1 C 表中的全部行为：Primary 带动 Data、新卡默认 active、状态事件不带 slotId、simId 连续、重试次数与超时值（4s/10s/45s/60s）、卫星 Active 不下发 RIL、双卡初始默认角色。

**哪些业务规则尚未确认？**
Q1–Q14，其中最关键的是：

- Q2：事件身份；
- Q6：锁卡账户可见性；
- Q8/Q11：卡移除或停用后的角色迁移；
- Q12：Primary 与 Data 的关系；
- Q14：Profile 停用的对外表达。

**当前 Linux 方案未验证什么？**

- samgr/SA 启动与 IPC Parcel 序列化；
- `core_service_sim.cpp` 中的系统应用和权限校验；
- HDI IRil 二进制协议与真实 Modem 时序；
- 真实 DataShare/telephony_data、StateRegistry、CommonEvent 的跨进程分发和订阅权限；
- eSIM SA、IMS、卫星；
- 设备冷重启与真实持久化；
- SELinux 与系统参数权限；
- 真实热插拔。

以上全部属于 DEVICE_REQUIRED。

> 判断标准：重构可以改变系统内部“如何完成”，但不能未经批准改变系统对外“应当完成什么”。

---

## 附录 A：已核对 API 行号（i_core_service.h / core_service_client.h / core_service_sim.cpp 校验）

| API | i_core_service | client | 校验（core_service_sim.cpp） |
|---|---|---|---|
| HasSimCard | 65 | 187 | 未见 |
| GetSimState | 66 | 196 | 未见 |
| GetSimIccId | 86 | 240 | Sys:180 + GET_TS:184 |
| IsSimActive | 89 | 266 | 未见 |
| GetSlotId / GetSimId | 90/91 | 274/282 | 无 |
| GetSimAccountInfo | 102 | 367 | GET_TS:379 |
| SetDefaultVoiceSlotId | 103 | 375 | Sys:396 + SET_TS:400 |
| SetPrimarySlotId | 106 | 836 | Sys:458 + SET_TS:462 |
| GetActiveSimAccountInfoList | 114 | 437 | GET_TS:647 |
| UnlockPin | 69 | 456 | Sys:691 + SET_TS:695 |
| SetActiveSim | 117 | 555 | Sys:1053 + SET_TS:1057 |
| GetSimLabel | 171 | 971 | 仅能力检查:1541 |

“未见”表示在该函数体附近的 grep 结果中没有看到校验，**需要逐函数复核**后才能纳入 B 套件断言。
