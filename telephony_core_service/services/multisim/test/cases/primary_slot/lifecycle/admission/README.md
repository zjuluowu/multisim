# SIM-028 补丁准入与 Host CI

本轮基线为 `77e3dc4e27dfe33ac728f225843b86b038914f6c`。结论是**生产补丁暂不准入**。Host 执行深度、运行期业务断言和资源释放已有证据，但不等于产品退出协议安全；增量重跑发现 ASan 失败，新副本又捕获 TSan 竞争。当前结果、原始失败和新隔离重构结果查[本轮验证记录](../../../../docs/archive/systemtest/configs/sim028_ci_admission_20261112/validation.json)。本轮未在主工作区应用生产补丁，未提交、推送或合并。

## 本轮实测

| 执行 | 普通96条 | ASan真实对象36条 | TSan真实对象36条 | CI退出码 |
| --- | --- | --- | --- | --- |
| 首次新克隆/原拆分 | 91 PASS / 5既有FAIL | 36 PASS | 36 PASS | 0（无新增回归） |
| 同输入增量重跑 | 91 PASS / 5既有FAIL | 35 PASS / 1 FAIL，exit86 | 36 PASS | 1 |
| 测试寿命修复后新克隆 | 91 PASS / 5既有FAIL | 36 PASS | 35 PASS / 1 FAIL，exit87 | 1 |

最终普通回归中RealSimPrimary 7/7、生命周期8/8、配对4/4、Stop A～H 8/8、新增安全9/9、入口14/14均通过，旧Host仍55/60通过。准入策略测试13/13通过。上述三轮均保留原始XML/日志；缺少XML的sanitizer进程仍计为FAIL。初轮与增量使用旧测试布局，最后一轮使用修正后的独立测试补丁，准确输入摘要分别归档，不能混称同一候选全部通过。新增失败不在五条旧缺陷容许集内。

## 补丁收敛

在仓外副本按下列顺序应用；CI 自动完成，禁止直接对当前生产树应用。

1. [production-minimal.patch](production-minimal.patch)：37份生产候选文件；显式 Stop、订阅解环、回调计数与准入、任务/队列停止、同步修复。已移出 Host 弱引用采样及其编译宏，不含 Host Fake。
2. [test-guard.patch](test-guard.patch)：12份原 Host 文件及3份生产文件中的独立 Host 诊断增量，共15份文件；仅用于隔离 Host 目标，不得进入产品。

两份补丁的并集仍为49份文件。[patch-manifest.json](patch-manifest.json)逐项记录基线、纯生产、组合候选摘要及应用顺序；[review.json](review.json)逐文件列出职责和A/B/C分类，历史产物逐项列为D，E类无用 `<exception>` 已移除。测试补丁额外修复 C1 重试用例失败路径的局部变量寿命：回调记录先于场景构造，保证场景 Stop 完成后才销毁记录，不改断言、等待期限或生产逻辑。原[历史交付套件](../../../../docs/archive/systemtest/configs/sim028_stop_implementation_20261112/README.md)及其42份产物保持原字节，不作为产品输入。

候选职责集合：Manager统一关闭准入并协调停止；Controller保留已接纳B/C1请求原完成与重试通路；Monitor注销Watcher/SA/CES；State/Handle注销状态及RIL观察；File/IccFile/Phonebook注销文件及卡状态观察；OperatorConfig取消APN任务；Observer锁外释放退订强引用；TelEventQueue与生命周期组统计回调、任务并排空。SIM仅退订/释放自己的共享RIL引用，不调用宿主RIL DeInit。正常运行断言与既有五条缺陷均保留；新增停止态错误策略和内部ABI变化仍需审查，不能宣称全面业务等价。

建议产品退出顺序：Core关闭新入口 → 同一控制块Manager进入Stopping → 等待已接纳请求结束（失败则保持根，不继续卸载）→ 封闭回调/新任务 → 注销外部监听并跨回调屏障 → 排空自有线程/队列 → 解除内部订阅和四类强环 → 子对象Stopped并释放 → Core协调其他消费者 → 最后停止共享RIL。停止过程中不可持有任务需要反向获取的锁等待，不在析构后调用shared_from_this，不把RemoveAllEvents当作完成屏障。

## 安全审查与待裁决事项

详细源码定位、修复建议逐项见review.json的R01～R12。当前不能合入的核心问题：

- **Core接入缺失**：真实 `CoreService::OnStop` 尚未调用候选Stop；必须使用原Manager控制块的生命周期hook。SIM停止失败时不能继续DeInit共享RIL。Host保留CMI消费者不证明NetworkSearch/SMS退出正确。
- **原生注销与回调屏障**：SA退订返回码未处理，CES/SA成功注册及死亡回调屏障没有原生执行证据。先weak.lock再申请Lease可能暂时保活对象，不能据此承诺Stopped后立即可卸载。
- **组归属和新请求身份**：隐式任务上下文可能把其他消费者的队列纳入SIM组；嵌套Business标记没有request/epoch身份，无法区分同任务外部重入与合法延续。需裁决显式归属/延续许可，或提供平台不可重入的证据。
- **等待预算**：75秒仅覆盖部分等待，join、队列wait、外部注销和子Stop没有统一截止时间；必须定义失败保留与重试、总退出预算和持续繁忙峰值。
- **Watcher串行性**：Lease只计数，不串行业务字段访问。原生同键/跨键并发模型未证实；若改投递队列，须先裁决原有通知顺序，不能无意改变同步行为。
- **异常与ABI**：当前部分初始化用例只覆盖null-RIL路径，尚缺全部注册/分配失败与并发初始化、叶子重复停止矩阵；内部布局及IccFile虚表改变，产品必须整体重编，验证厂商扩展。Stopped实例不重新OnInit，重建新实例。
- **本轮新增失败**：增量ASan运行的C1重试用例在第3会话CHECK后未达原12秒/24请求断言，随后触发测试局部变量越域访问（exit86）。测试补丁仅修复后者；前者仍需独立定位。后续通过不能抹去该失败，当前不宣布重复性准入通过。

- **本轮新增TSan竞争**：新副本B失败分支用例中，StateHandle队列的`SimStateEscape()`写`externalState_`，Monitor队列经`RegisterCoreNotify()`→`IsIccReady()`无共同同步读取，TSan exit87。两侧访问在基线亦存在，不能据此认定由Stop补丁引入；也不能加入五条旧缺陷白名单。需另行授权同步修复及通知顺序审查。

## 可执行 CI

已安装依赖由调用者显式提供；不下载、安装或升级工具。`runtime-root` 是既有Host原生头文件集合，来源/摘要仍由原Host配置锁核对。以下变量应按本机实际位置设置，不继承历史 `/tmp` 路径。

```bash
export SIM028_GTEST_ROOT=/path/to/matching/googletest
export SIM028_OPENSSL_ROOT=/path/to/matching/openssl
export SIM028_RUNTIME_ROOT=/path/to/matching/native-headers
export SIM028_CI_OUT=/path/outside/repository/sim028-ci
python3 -B telephony_core_service/services/multisim/test/cases/infrastructure/test_ci_sim028.py
python3 -B telephony_core_service/services/multisim/test/scripts/ci_sim028.py \
  --out "$SIM028_CI_OUT" --profile all --parallel-profiles --jobs 2 \
  --gtest-root "$SIM028_GTEST_ROOT" --openssl-root "$SIM028_OPENSSL_ROOT" \
  --runtime-root "$SIM028_RUNTIME_ROOT"
```

同一入口可以接入现有CI runner，无需另一套GN或测试框架。`all`依次/可选并行运行regular、address、thread；分档 `contracts` 跑67条业务契约（含5既有失败），`lifecycle` 跑29条生命周期/Stop；`regular`完整96条，address/thread各36条真实对象专项。不同profile使用独立Ninja输出。同一输出目录支持增量编译并保留每次run目录；编译输入/补丁漂移则拒绝复用，要求新输出目录。只改编排器不改变编译对象，编排器SHA另行记录。没有激活GitHub分支保护或远端工作流，本轮只交付可直接执行的CI入口。

CI先本地克隆并显式覆盖实际tracked/untracked输入，再在仓外应用候选；49项SHA必须匹配。配置通过现有host.py生成GN/Ninja及compile_commands.json；没有忽略未解析符号或访问控制绕过。每条用例单独进程运行，保存exit、命令、二进制SHA、日志与原始GTest XML；业务、资源、异步安全仍用原测试property区分。

**原始测试结果与准入结果分离**：五条缺陷始终为FAIL，JUnit保留failure，原进程exit1；只允许与固定用例名、退出码、完整有序断言签名完全相符的五条既有失败进入“无新增回归”判定。路径前缀规范化，文件/行号/断言和值仍参与校验。既有缺陷意外通过、断言变化、crash、跳过、空/错误XML、漏用例、重复用例、新用例失败均使CI非零退出；发现新增用例会实际执行。不存在把五条改成PASS/SKIP的转换。13条策略测试包含负控，证明这些异常确实拒绝准入。

每档保存 `latest.json`、`run-*/result.json`、`junit.xml`、逐例XML/log与logs.zip；总入口保存 `result.json`、`isolation.json`、`ci-artifacts.zip`。新回归使入口exit1，即使其他档通过。本轮重跑中的ASan退出86已实际触发该规则；缺XML也不能绕过。

ASan启用泄漏检测，TSan遇首错失败，不加载自定义抑制。Host FFRT是测试调度实现；系统/部分第三方库未全部插桩，sanitizer通过只适用于参与构建的Host闭包，不证明原生FFRT或HDI安全。单例默认120秒外部看门狗仅用于判定超时失败，不证明被杀进程已经安全清理，不用于下述长链专项。

## 原生 OHOS 迁移计划（尚未执行）

| 层次 | 可复用资产 | 需要替换或补齐 | 本轮状态 |
| --- | --- | --- | --- |
| 场景/断言 | 主卡参数恢复、槽/userSet、通知偏序、C0/B/C1、Stop A～H | 输入/观察适配；实际产品槽目录，原生类型与Want访问，不依赖mangled RTTI或Host内存Provider | Host已执行；原生NOT_RUN |
| FFRT/Watcher | 生命周期用例、登记账本、在途/迟到/重复停止断言 | 真实FFRT、参数服务并发模型与注销屏障 | BLOCKED |
| Core/Stub/IPC | 入口权限/错误/参数契约 | 原生SA、Binder、MessageParcel、token和真实Core组合根 | BLOCKED |
| TelRil/HDI | 请求/phase/session/serial/应答关联断言 | 真实TelRilManager/TelRilSim及匹配HDI；可控测试设备服务与真实设备分层 | BLOCKED |
| APN/长链 | 失败恢复/资源断言 | 成功运营商配置、PDP Provider、真实超时与完整重试 | NOT_RUN |

最小产品依赖需来自同一匹配OHOS manifest：core_service及其匹配telephony_data Provider契约（Host已核实的版本见既有依赖锁）、libffrt、eventhandler、c_utils、init/begetutil、config_policy、IPC/safwk/samgr、CES、access_token、ability、DataShare、RIL HDI（RadioProtocol使用V1_1类型，SetPrimarySlot为V1_5接口），及GN要求的cJSON/libxml2/crypto等传递依赖。不能仅拷贝system_ability.h。产品feature配置和完整依赖闭包以实际 `tel_core_service` GN生成结果核对。

现有 `test/unittest/sim_gtest/BUILD.gn` 及 `core_service_gtest/BUILD.gn` 使用 `//build/test.gni` 的ohos_unittest，但多处依赖 `test/mock/ffrt:ffrt_mocked`。这些可以回归既有功能，不能直接作为真实FFRT生命周期证据。计划在现有multisim测试目录提供产品专用GN入口，复用场景/断言并依赖真实运行时；该入口本轮尚未实现。先在匹配树中构建部件（既有目标）：

```bash
hb build -T //base/telephony/core_service:tel_core_service
```

新增原生测试目标须按该产品工具链与测试模板实现后再给出确切构建/设备运行命令；本轮不虚构已存在的native目标。

Core层先解决R01：保持现有eSIM→SIM→搜网/CMI装配和同一shared控制块，增加内部生命周期hook及Stop失败保留策略；再通过真实SA发布、合法native测试token调用代理→Stub::OnRemoteRequest→OnSetPrimarySlotId→CoreServiceSim→真实Manager。补权限拒绝、错误接口令牌、参数读写和reply错误码/顺序。SA停止/重启、死亡回调、SO卸载必须在设备执行，不能通过Host返回常量验证。

RIL层使用真实TelRilManager的TaskSchedule及TelRilSim，在HDI服务边界记录原始serial、slot、RadioProtocol.sessionId、phase和应答。若用可控HDI测试服务，仅称原生进程/协议验证；再用匹配设备验证真正HDI/Modem。经公开装配点提供边界，不增加private访问或复制业务。B验证专用SetPrimarySlot，C1验证CHECK/UPDATE/COMPLETE；C0断言不发设备命令；A厂商映射仍需对应厂商实现，不要求所有分支经过RadioProtocol。

APN专项从 `OperatorConfigCache::LoadOperatorConfig` 的真实成功配置路径进入，提供合法配置文件/ConfigPolicy与原生DataShare；验证实际PDP_URI和PdpProfileRdbHelper URI（以匹配Provider源码为准），不得猜测成功返回或数据库列。覆盖首次失败、原5次/10秒重试、重试前/执行中/成功后Stop，以及RemoveTask与真实FFRT完成屏障。当前PIN锁卡fixture没有覆盖这一成功分支；缓存SIM/USIM/RUIM及运营商切换矩阵另列NOT_RUN。

长C1专项保留生产45秒通信超时、60秒等待和5次重试，不改常量加速。分别在CHECK/UPDATE/COMPLETE缺应答、错误session、迟到应答时验证原返回时机、状态恢复、必要COMPLETE和请求终点；Stop预算耗尽返回失败后保留Stopping与对象根，原请求终结后重试Stop。外部看门狗可独立配置15分钟作为初始预算，需按实际完整路径校准，不能把75秒当作所有join/重试的总界限。上述长链必须独立串行运行并归档实际时间，不计入当前36条或96条通过数。
