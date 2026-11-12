# SIM-028：未经修复的旧实现业务基线

此入口执行当前工作区的 `services/sim`，没有应用 Stop 或并发候选补丁，原业务断言和生产调用路径不变。它复用 `host.py` 和原 GN 目标，逐例独立进程运行，保留原始退出码、XML、泄漏诊断和生命周期红灯。**业务结果与完整 GTest 结果是两个字段；本入口仍以原始非零退出码退出。**

## 当前标准场景：双卡切换前态

最终构建已实际完成50轮全部七例、350个不同进程：**业务350 PASS /0 FAIL /0 NOT_RUN**；在卡六例的300份前置事件/就绪/激活/公开角色校验及Strict RIL均PASS；无RIL50例不适用RIL验证。三个事件顺序为native-arrival17轮、reverse17轮、present-first16轮。原始完整GTest仍50 PASS/300生命周期FAIL，每轮原始exit1，业务入口也原样返回1。不存在把NOT_RUN或清理FAIL改成完整GTest PASS。

本次可冻结的是上述明确前态下的**条件性Host业务样本**，不是原生产品Golden或安全Stop。最终50轮使用同一二进制，SHA见[validation.json](../../configs/sim028_dual_prestate_20261112/validation.json)；此前协调版本的另外50轮350业务PASS也单独归档，未混合统计。最终代码修订补足了Host分离线程句柄复用与属性分离的记账，后者100次探针检查通过；不改变真实线程入口或退出行为。

普通Host实测64条59 PASS/5既有FAIL，原60条仍55/5，入口14/14；4条新增初始化屏障/线程完成测试均通过。23条基础设施测试的case/oracle注册已同步，首次回归曾因新增4例缺少元数据而被门禁拒收，原始证据保留；修正后除既有缺陷组外各组门禁PASS。独立分类器13项正负控PASS。未运行本轮ASan/TSan，不继承隔离补丁的通过结论。

按用户要求检查可访问的Repo环境：六个明确根目录9294目录和两处父目录1087目录的有界扫描，未发现.repo；四处实际尝试repo manifest -r因repo CLI缺失而BLOCKED，未安装工具。现有manifest只跟踪master，没有找到可证明对应目标产品的构建记录；产品匹配UNKNOWN，不能将参考仓库SHA拼成产品基线。[扫描与版本证据](../../configs/sim028_dual_prestate_20261112/product-version-check.json)记录具体范围及差异。


用户已明确批准：标准切换场景使用两张激活卡，并通过真实公开接口建立 slot0 旧主卡/旧数据卡；原单卡场景及其失败作为独立反例保留。默认 Legacy 入口使用 `dual-active-primary0`，原始 GTest 直接运行时默认仍为 `single-active`。

准备顺序为：真实三槽对象初始化 → 两张卡各自的真实账户加载事件 → Host 队列运行/到期任务与分离线程调用完成屏障 → 真实 GetRadioProtocol 查询 → 真实 `SimManager::SetPrimarySlotId(0,false)` → 再次屏障 → 公开查询验证两卡 active、Primary/Data 均0、无切换在途 → 原七例业务输入及原业务断言。没有写生产私有成员，也没有将 IsSimActive 预设为 true。

初始化阶段的 StrictMock 仅模拟有接口依据的物理槽0/1设备请求及应答，C1 的 UPDATE 只发送 NOTIFY；真实业务对象自行决策、提交状态、发通知。进入正式业务前用 Times(0) 关闭全部初始化应答规则，再由各原用例设置精确期望。这个设备成功/映射场景仍是 CONDITIONAL，不能代表所有真实芯片。 C0原用例随后通过真实协议查询接收slot1=M0、slot0=M1，从而测试“设备无需切换但逻辑角色需要提交”；这与准备阶段的0=M0映射不同，该外部映射变化是明确fixture，不是已验证的原生设备状态迁移。初始化报错或 Mock 失败不得进入业务 PASS。

队列空闲不足以证明后台 `std::thread(...).detach()` 调用已返回：第一版双卡350次出现307业务PASS、1业务FAIL、42 NOT_RUN。补充 Host/Linux pthread 创建和分离记账后，等待真实线程入口返回及捕获析构，不取消、不 join、不终止线程，不参与 SIM 业务决策；配合真实账户完成通知及稳定修订计数，避免旧初始化调用在正式请求后写回 slot0。它只保证已观察到的当前调用完成，未来定时器仍保留；不是原生FFRT、POSIX线程全部TLS析构或安全Stop证明。记账单例只持诊断元数据，不持SIM共享引用。原生命周期FAIL及析构后回调安全 UNKNOWN 继续保留。

```sh
# 新输出目录；全部7例、三种初始化交错、50轮。
# 独立业务可以PASS，但原始生命周期失败使进程仍返回1。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_legacy_primary_contract.py \
  --output /tmp/sim028-dual-repeat-new --repeat 50 --jobs 8 --init-model dual-active-primary0

# 保留原单卡反例；不以此结果冒充上述双卡前态。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_legacy_primary_contract.py \
  --output /tmp/sim028-single-reference-new --repeat 3 --init-model single-active
```


单独运行原C1用例（最终二进制已实测：业务/Strict RIL PASS、原生命周期FAIL，返回1）：

```sh
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh selected \
  --target sim_primary_slot_real_test \
  --filter 'RealSimPrimary.C1ThreePhasesWithStaleCheckResponseRejected' \
  --init-model dual-active-primary0
```

当前证据集中于[双卡复验目录](../../configs/sim028_dual_prestate_20261112/)；历史单卡350次和初始双卡失败不被新结果覆盖。下节保留原单卡阶段结论，其“尚未裁决前态”已由用户本次选择明确。

## 前一阶段：原单卡夹具的初始化稳定性复验

HEAD仍为39d63590。本批测试环境修复和[独立证据](../../configs/sim028_initialization_stability_20261112/validation.json)与下文早一批的35次执行分开保存。50轮全部七例，共350进程：在卡六例的300份账户事件、就绪屏障、在卡和激活检查全部PASS；业务254 PASS/96 FAIL，GTest50 PASS/300 FAIL，所有轮次原始exit1。Strict RIL252 PASS/48 FAIL，另外50个无RIL场景不适用RIL验证。新旧断言均未删改。

三个交错按轮循环：native-arrival17轮与reverse17轮业务各119/119；present-first16轮业务16/112，仅无RIL例通过。这里不是把三种初始化历史视为相同前态：受控变化实际揭示了原七例仅以“active”作为前置条件的不充分性。没有过滤失败顺序、改NOT_RUN为PASS或容许额外Mock调用。

受控LLDB复现确认，账户事件后，MultiSimMonitor::RefreshData→GetListFromDataBase在写锁内清空缓存、尚未回填；Controller同一地址被另一线程调用IsSimActive(1)，旧getter不持该锁，读空缓存返回false。最终负控精确重现real_test.cpp:45原断言，Provider当时slot1/active1。正控在相同写入暂停点使用新就绪屏障，激活检查通过；完整GTest仍因通知差异和清理失败退出1。调试器只读取DWARF字段、暂停/恢复真实线程，不改对象数据；业务测试不依赖调试器。早期表达式诊断失败和一次中断exit130均归档，没有列为通过。

最小环境修复是账户事件后调用AwaitSchedulerReadyIdle(5s)：以稳定schedulerRevision等待当前运行和已到期任务，保留未来定时器；不写激活状态、不取消生产任务、不增加sleep。普通all任务自动清除诊断环境变量。两个新增屏障测试验证跨队列级联任务、仍在运行的任务和未来定时器；原Host60仍55 PASS/5 FAIL，加上两例为62条57 PASS/5 FAIL，入口14/14。生产cache getter的并发缺陷没有修复；Host就绪屏障也不等于原生FFRT完成协议或所有detached线程已结束。

新失败解释：present-first令唯一在卡slot1先完成加载，旧实现会自动选主卡/数据卡。C0/C1再次选择同一账户的主卡广播由SendMainCardBroadCast去重，原断言却要求再次收到主卡广播；B还会在正式请求前自动发送SetPrimarySlot(1)，原StrictMock没有覆盖此初始化调用；C1失败例的数据卡参数已在正式输入前为1，原“No data commit”断言把既存状态当作本次副作用。这些红灯表明**原Golden缺少初始主卡/数据卡/通知历史和初始化设备请求的约束**。不通过修改旧代码、放宽StrictMock、重置私有缓存或换成更容易通过的卡模型处理。

冻结结论：激活前置样本通过，完整七例业务基线仍不满足冻结条件。需要裁决“单卡已自动成为主卡后再次选择”和“从另一有效主卡切换”两种独立前态、初始化自动发令的处理及消费者通知要求；现有预期保持不变，仅增加冲突标记。C1夹具现仅投递UPDATE NOTIFY，未伪造额外UPDATE应答；StateRegistry现返回当前真实client无proxy时的IPC连接错误，不再假装通知成功。弱引用单目标RIL订阅、内存Provider、参数和CES成功路径等剩余模拟限制仍为CONDITIONAL/UNKNOWN。

产品版本核对：本地manifest@7be88f147200cdf60a6f542dc758ac5f1d54e036的default/ohos清单均指向master；本地superproject虽有gitlink SHA，但其Manager/Controller/Monitor/Core入口源码与当前fork不同。尚无目标产品锁定manifest、build fingerprint和厂商RIL库，不能拼接31份仓库副本当作同一产品契约。

```sh
# 所有七例，每轮独立进程；一次GN/Ninja构建后并行执行完整轮次。
# 本轮确实存在业务及生命周期FAIL，入口应返回1。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_legacy_primary_contract.py \
  --output /tmp/sim028-init-repeat-new --repeat 50 --jobs 8 --init-model single-active

# 仅用于负控重现旧等待方式，禁止用于声称业务基线通过。
SIM_HOST_BOOT_SLOT_COUNT=2 SIM028_INIT_ORDER=present-first SIM028_INIT_OLD_WAIT=1 \
lldb --batch -o 'settings set target.disable-aslr false' \
  -o 'command script import telephony_core_service/services/multisim/test/systemtest/primary_slot/legacy_contract/reproduce_initialization.py' \
  -o run -- .host-out/build/sim_primary_slot_real_test \
  --gtest_filter=RealSimPrimary.C0CoreToRealManagerAndController \
  --gtest_output=xml:/tmp/sim028-init-negative.xml
```

下面保留上一批审计的来源与边界；其中原固定返回/额外应答的历史状态已由本节更正，原验证目录和历史Golden字节没有覆盖。

```sh
# Git 根目录；复用已配置的 .host-out/host.json，output 必须是新目录。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_legacy_primary_contract.py \
  --output /tmp/sim028-legacy-contract-new --repeat 2

# 分类器正负控；不是新增 SIM 业务通过数。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_legacy_primary_contract.py

# 可重跑的本地跨仓审计；不会拉取/修改外仓。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/audit_primary_contract.py \
  --workspace /mnt/e/telephony --workspace /mnt/e/github/telephony \
  --workspace /mnt/e/work/github/telephony --workspace /mnt/e/OpenHarmony \
  --workspace /mnt/e/code --output /tmp/sim028-contract-repositories.json
```

本轮原始记录见 [验证目录](../../configs/sim028_legacy_contract_20261112/)。`validation.json` 保存实际工具版本、完整 Ninja 命令、二进制摘要、生产字节保护及逐例独立业务分类；`cross-repositories.json` 固定各源码副本 HEAD、remote、实际文件 SHA、源码行和关系搜索。外仓存在多个不同 HEAD，**未核实它们属于同一产品 manifest**。不能用其中一份参考 Vendor 实现代表目标手机芯片。

## 验收边界与安全

完整通过需要原 GTest 返回0且无失败。独立业务 PASS 只接纳已完成的单例 XML、`business_assertions_before_cleanup=PASS`、`strict_ril_expectations_verified=PASS`，以及仅来自 `real_scenario.h` 的精确 Manager/Graph 清理断言失败；任何新增断言、Mock 失败、跳过、异常退出、超时或 sanitizer 报告均不能获得业务 PASS。准备阶段在请求输入前失败时，业务为 NOT_RUN，原 GTest 仍为 FAIL。初始化无 RIL 场景本身完整通过，没有 StrictMock RIL 对象。

这不是安全停止的新实现。`CoreService::OnStop`（`services/core/src/core_service.cpp:146`）仅停止 Network/RIL/扩展，不等同每条测试释放整张 SIM 订阅图；`SimManager::~SimManager` 为空。常驻服务生命周期与短命 fixture 的全析构模型不同，但**不能据此宣布真实资源环无害**。原测试退出仍有28个 Host worker、RIL use_count=10、未过期账户观察者；计数不等于独立泄漏数量。内部四类强环证据沿用原生命周期审查，本轮没有重新做地址图，也没有应用其修复。

业务断言、显式严格 RIL 验证后，残留对象仍可能持有捕获 fixture/局部变量的回调；未来延迟任务及失败路径的内存安全没有排空保证。本入口不使用 AllowLeak、强杀、延时掩盖或屏蔽清理断言。没有足够条件安全构建完全绿色的常驻 fixture，故选择保留原 FAIL 和明确的 `post_destruction_callback_safety=NOT_VERIFIED`。本轮不是 ASan/TSan 验收，也不继承隔离补丁的通过记录。

历史批次第一次双轮运行额外捕获 C0 的 `real_test.cpp:45 IsSimActive(1)=false`：发生于 `Permit()`/`TRACE input` 和 setter 调用之前，不能归入 SetPrimarySlotId 失败，也不能归入纯生命周期失败。`StartLockedCard` 只等待账户事件；`MultiSimMonitor::RefreshData` 可重新加载缓存，而 `IsSimActive` 没有获取写缓存的 `mutex_`。该历史记录当时尚未确证；本批受控调试已重现同一清空/读空交错，见上文及初始化证据。原始失败记录仍保留，生产getter未修复。

## 真实协作与 Mock 真实性

下表 A=真实业务执行，B=外部接口有源码依据，C=Host 受控假设，D=证据不足。每个边界可以同时拥有不同级别的证据；B不等于真实平台运行。

| 关系/责任 | 实际源码与边界 | 本轮结论 |
| --- | --- | --- |
| 上游 API/IPC | `frameworks/js/network_search/src/napi_radio.cpp:2635` 的 NativeSetPrimarySlotId，经 CoreServiceClient/Proxy/Stub；ANI radio 也有绑定 | 源码 B。当前不是 JS SIM 文件入口；测试直接进入真实 CoreServiceSim，未执行上层 NAPI/Proxy/Stub/完整 CoreService，运行 D |
| SIM 内部 | CoreServiceSim→SimManager→MultiSimController，C1→RadioProtocolController | A；真实状态、持久化调用、分支和错误码断言。初始无 RIL 例只进入 Manager，不冒称 Core 全链 |
| 厂商 A/VSIM/eSIM | slot==SIM_SLOT_3 优先走 NeedSwitchSimCard→SwapM0M2SimCards→SetTargetPrimarySlotId；VSIM 检查依赖扩展指针，eSIM账户也参与初始化 | wrapper B；真实 libtelephony_ext 的映射、开关、事件与芯片库 D。缺省函数未装载不代表厂商行为；七例不覆盖 A/VSIM/eSIM成功路径 |
| RIL/HDI/芯片 | `TelRilSim` B使用V1_5 SetPrimarySlot，C1使用V1_1 SetRadioProtocol；HRil将 serial/slot 分开并调用 vendor 函数 | 参数/载荷形状 B；Host StrictMock 直接给原 owner/event 投递异步应答 C。真实 Request/serial分配、错误转换链、HDF、物理Modem未执行 D |
| RIL订阅 | 真实 ObserverHandler 保存事件到多个 shared_ptr 观察者，并可能在注册时立即通知已有状态 | Host弱引用 ledger 用于记录，但实际通知表只保留每槽/事件的一个目标，Register 始终返回0且不重现立即通知。C；不能用于所有权、广播扇出或初始通知顺序的权威 Oracle |
| C1 UPDATE | `radio_protocol_controller.cpp:196` 的 UPDATE应答不推进；NOTIFY驱动COMPLETE。参考 `ril_adapter/services/vendor/src/at_sim.c:1083` 的 UPDATE只发NOTIFY | 上一批额外应答已在本批去除，业务状态机完成但present-first仍有独立通知断言失败。具体设备调度C；NOTIFY先后/重复/丢失仍需设备变体验证 |
| Provider/DataShare | `SimAbility::Update`→`RdbBaseHelper::Update`返回RDB状态；`sim_info` URI与列有生产源码。当前本地 sim_data.h 与锁定头规范化行尾后完全一致 | 单行关系/谓词替身 C；列/URI/该Provider返回语义 B。DataShare通用头文档称Update返回行数，与此Provider实现不同，需按产品路径裁决，不能统一猜测。权限、SQL全部语法、事务、重启恢复、通知、Release寿命没有模拟 |
| 参数服务 | systemparameter Get/Set接口；Primary/ICCID/Data/preference 由真实业务代码调用 | 参数名/值/调用顺序 B/A；内存map成功读写 C。DAC/MAC、persist刷盘、写失败、Watcher和跨进程读取 D；LSI等初始化值是指定fixture，不是数据库规范默认值 |
| CommonEvent | CommonEventManager Publish 返回发布操作结果；不能证明所有订阅者收到 | Host捕获 Want 并返回true C，真实payload和发布偏序 A；主卡→Data只证明发布次序，不证明接收顺序或数据连接已建立 |
| StateRegistry | MultiSimMonitor 通知直接账户回调及 UpdateIccAccount；真实registry校验权限，遍历 ICC_ACCOUNT 观察者 | 上一批固定返回0已改为当前client无proxy的IPC连接错误（B），Host未提供registry SA。服务端监听者交付、权限和IPC仍为D |
| Cellular Data | `cellular_data_controller.cpp:194-197`订阅SIM状态/records/account；`incall_data_state_machine.cpp`查询Primary并调整Data，配置 Data订阅URI | 跨仓消费者 B，Host 没有构造这些消费者。数据参数/广播不能推导APN切换、通话数据状态或连接成功 |
| 搜网/SMS/Call | 本仓搜网 RadioInfo/NetworkSearchManager 查询Primary；外仓 sms_short_code_matcher.cpp:195 查询Primary；本次生产文本搜索未发现CallManager/CellularCall直接setter调用 | 间接读状态 B；不是每次setter同步调用全体组件。未找到不等于动态依赖不存在；不把CallManager或state_registry误画成主卡切换下层 |
| FFRT/SA/身份 | Host线程队列、参数Watcher失败返回、SA订阅失败返回、固定native token和Creator身份token | 全为C；真实CES/SA/Binder/权限、Watcher时序和FFRT调度为D。独立进程不能证明原生服务重启安全 |

缺失清单在审计 JSON 中：telephony_ext SIM库、实际芯片 vendor 的 SetPrimarySlot/SetRadioProtocol 实现及固件/产品manifest；版本均 UNKNOWN，不能编造提交。可访问的公开 RIL Adapter 有函数指针契约及参考AT实现，但参考 `vendor_adapter.c` 没有绑定 SetPrimarySlot，不能据此声称B在设备上可执行。`simollie` 在本次明确扫描的工作区未定位到源码，尚无证据证明参与该业务。

## 分支及返回时机

| 分支 | 返回前 | 返回后/未知 |
| --- | --- | --- |
| A | 校验在卡，厂商swap返回后保存target并投递ready等待事件，返回SetTargetPrimarySlotId结果 | ready后进一步决策与实际设备完成；缺厂商实现，BLOCKED |
| B | userSet且专用能力开启，发送专用请求并同步等待业务应答或45秒超时；成功才保存Primary/Data并发布；失败保留旧主卡，返回LOCAL_PTR_NULL | 设备实际资源释放、各消费者交付未知；setDone=true只表示该软件阶段结束 |
| C0 | 经真实查询应答确认目标已是modem0；无设备setter，保存主卡及Data、发布通知后返回成功 | 跨进程消费者异步处理未知 |
| C1 | 保存旧参数，先写目标Primary/ICCID；CHECK→UPDATE/NOTIFY→COMPLETE后成功提交广播/Data，失败先恢复旧参数再返回失败 | 原失败返回不等于五次后台重试已结束；重试使用新session。C1状态广播固定userSet=false与调用输入true不同，保留观察并请求裁决 |

`TELEPHONY_ERR_LOCAL_PTR_NULL` 对设备错误/超时的映射、偏好先写且写失败仍转发、HasSimCard返回码未使用、C1提前写可被其他读者观察、原失败返回后的后台重试均是兼容性候选事实，**不是自动批准的正确规范**。保留现有五条独立DEFECT用例；生命周期失败和准备阶段失败不加入五条缺陷容许集。

## 统一契约与原生校准

[golden.json](golden.json)含7条已执行场景和A分支阻塞场景的稳定ID、前置条件、输入、应答、API返回、最终状态、副作用、偏序、来源、可信等级和变更策略。优先级1原OHOS执行轨迹本轮没有；优先级2采用真实HDI/接口及已有系统测试作为接口证据；优先级3跨仓源码；优先级4本轮Host结果。没有将历史隔离补丁全绿结果写成旧实现基线。

当前 Legacy Adapter 是上述执行入口；原C++断言仍为执行 Oracle，不能用JSON预期值反向生成业务结果。Target Adapter尚未实现/运行，不以转调旧SIM或纯Fake冒充。未来统一场景需要两Adapter接受相同外部应答，输出规范化观测：API返回、公开状态查询、参数/Provider变更、事件payload和偏序、RIL slot/phase/resource、每次尝试内部一致且跨尝试更新的session关联。只比外部行为，忽略对象地址/队列/线程/绝对session序号；serial保持独立。Target必须有自己的真实业务闭包，不能仅替换类型名。

原GTest红灯与业务观测分别比较；旧FAIL/新PASS先区分准备失败、模拟差异、已知旧缺陷修复、未经批准的行为变化和平台差异，再人工裁决。未知Mock时序不得自动成为硬性门禁；业务断言本身不因此降低。Target差分、原生IPC/设备整链本轮均NOT_RUN/BLOCKED。

原生最小补证：匹配manifest的core_service、telephony_ext、ril_adapter及芯片库、drivers_interface生成头、telephony_data/DataShare/RDB、CES、StateRegistry、CellularData、samgr/IPC/真实FFRT和systemparameter。在设备用合成/脱敏卡信息记录API进入/返回、vendor模式开关、slot、RIL serial与协议session、CHECK/UPDATE/NOTIFY/COMPLETE及错误/超时、每次参数变化和跨仓接收时间。分别验证B、C0、C1失败/重试/迟到及A映射；抓Data/APN和通话中的消费者行为、参数写失败/重启恢复、所有订阅者初始通知/扇出。固定版本后再把已裁决部分提升为Golden规范。APN、物理Modem取消、完整长超时、SA重启和厂商A仍需平台环境。
