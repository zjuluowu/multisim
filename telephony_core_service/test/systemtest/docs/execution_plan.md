# 最小切片、机器可读结果与 Legacy/Split 差分

原设计顺序作为历史保留；当前已实施容量组件切片，见文末与run_record.md。

## 有条件的实施顺序

1. 完成当前调查/边界/15项设计/Oracle审查输入（已写文档；业务审查未完成）。批准architecture.md中host产品目标/适配选项，并确定公开入口范围。保留生产capability分支，不能绕过。
2. 固定GoogleTest、平台接口SDK、GN/编译器、C++标准和参数宏；取得平台host机制证据或批准本目录最小GN适配。只创建自身构建入口，不引用旧测试/Mock。
3. 编译真实产品target探针，记录缺失头/符号闭包；把业务依赖仍编入产品target，平台依赖登记替身。先确认最小入口能执行真实SimManager状态查询，而非仅Mock转发成功。
4. 开发独立进程runner、外部RIL注册与状态响应、runtime最低适配、XML/JSON输出和隔离清理。首个业务切片推荐SIM-B-002 Absent，其次001生命周期探测；两者同时报告入口阻塞或正式降级组件范围。
5. 有效负向控制：仅在已确认Oracle的证据校验中破坏合成身份相等结果/执行标志/清理结果，验证检测器失败。没有身份路径时先验证O-H01设施控制；不修改生产源码来制造红灯。baseline replay差异审核不是生产正确性红灯。
6. 实际运行Absent，必须证明输入来自正式RIL边界、输出来自公开查询、产品源target真实参与、清理成功；保留命令/环境/退出码/数量。此时才可将该条标AUTOMATED及PASS。
7. 加Ready文件响应与账户存储协议；实际采集O-L03后添加单卡目标。再双卡映射/隔离及PIN受控响应。每个条件不足用SKIP/BLOCKED，不能以查到Ready一项覆盖全部能力。
8. Legacy输出基线固定后，Split使用同输入/权限/桩脚本/收敛条件运行；差分分类详见下表。

本轮停止在1之后：host源码编译适配需用户确认、公开入口异常需负责人判断、GoogleTest及平台SDK缺失。五条测试设计不满足用户“至少一个关键场景执行SIM生产逻辑”的整体验收条件，当前第一阶段整体未完成。

## 结果协议 v1（设计，未实现运行器）

产物：每case子进程GoogleTest XML（--gtest_output=xml），父runner整合JSON。预备编译失败只写JSON BLOCKED，gtest_execution=null，不生成伪执行XML。并行执行尚未支持；首批串行且一用例一进程，cleanup失败阻断队列。

JSON字段约定：

| 字段 | 必填语义 |
|---|---|
| schema_version / run_id / revision_label | 协议版本、唯一运行ID、Legacy或Split标签 |
| repository / head_sha / dirty / source_manifest_digest | 仓库及实际文件基线；HEAD不足时必须摘要 |
| toolchain / environment | gn/ninja/cxx/gTest/SDK版本、target_os、cpu、OS/WSL、编译命令和退出码 |
| case_id / oracle_ids / level | Case和每条可观察证据的Oracle分类；B或C，禁止默认A |
| topology / permission_profile / stub_config_version / scenario_digest | 卡数与Slot数、pSIM/eSIM、匿名slot关系、权限、调度脚本摘要 |
| status / executed / reason / blocking_stage | PASS/FAIL/SKIP/BLOCKED；编译/配置/初始化/业务阶段区分 |
| production_target / production_source_manifest / link_manifest | 实际产品代码参与链接证据；源码名单不等于运行覆盖 |
| observations | API接收/回调返回、稳定状态、账户别名关系、事件语义、外部请求必要参数；逐条Oracle关联 |
| convergence | condition_id、budget、elapsed、timed_out；公开条件支持证据，设施预算不是业务SLA |
| safety | synthetic_only、identity_plaintext_emitted=false、危险操作策略、字段白名单检查结果 |
| cleanup / restore | 临时命名空间/进程回收、成功标志、失败原因、是否阻断后例 |
| commands / exit_codes / counts / artifacts | 实际构建与运行命令；测试总数和PASS/FAIL/SKIP/BLOCKED；产物路径 |

O-H01/O-H02管控机器证据真实性。任何“已编译/运行/通过”都带实际命令、SHA+摘要、工具链、exit code、数量和安全证据。当前configs/cases.json是候选设计状态，绝不是执行报告。configs/environment_probe.json只记录环境调查。

身份控制：fixture内存保存合成ICCID/IMSI/EID/号码/PIN，报告只写CARD-A/CARD-B或relation_equal布尔值。真实敏感数据不收集；不以可枚举hash代替脱敏。GTest ASSERT不直接比较/打印完整身份字符串，只比较bool并用别名诊断；XML、JSON、差分及生产日志捕获同样按白名单过滤。默认丢弃可能含身份的原始生产日志，仅使用明确脱敏字段辅助诊断。源文件hash是代码基线摘要，与身份脱敏不同。

清理：父runner创建安全随机临时目录，限制所有store/参数文件路径在命名空间内；先空环境后生产启动。正常场景保存公开快照后退出子进程，父runner检查退出、清理目录、恢复桩/环境；异常watchdog终止子进程，仍执行清理，失败阻断后例。持久化重启场景保留同一隔离namespace给第二个子进程，用例末一次清理。不得保存真实参数/数据库到共享系统环境。

## Legacy/Split 差分规则

| 比较项 | 自动判定/审核规则 |
|---|---|
| 已确认API返回语义、错误码 | 只有来源确认的精确码可强制；其余O-L进入人工评审 |
| 稳定状态、身份与账户关系 | 按来源语义比较，simId输出匿名关系；不能隐藏可能影响契约的稳定ID变化，需单独O-Q05确认 |
| 必须/禁止事件与必要偏序 | 只有确认Oracle支持的集合/偏序；重复次数允许范围待确认 |
| 收敛 | 同输入固定预算，分别记录timed_out；产品SLA未确认不比较毫秒级性能 |
| 对外请求 | 只slot/sim对象及必要协议参数、必须/禁止语义；重试/额外查询不按无依据次数判错 |
| LEGACY_BEHAVIOR | 实际运行捕获后比对，差异进入人工审核，不自动声明Split错误 |
| OPEN_QUESTION | 保留观察差异和来源，不自动判错 |
| 内部类、IPC次数、SO符号、普通日志顺序、内部调用次数 | 不比较 |

每条比较以Case+Oracle关联；固定RIL输入/响应/事件队列、DataShare脚本、权限与初始持久化环境。任何环境/源码/配置变化显式写diff_context；无法保证一致标不可比/BLOCKED而非失败。默认不规定并发最终值完全确定，后续采用经确认的合法状态集合与偏序。

平台与SA拆分相关观察可能需A补充：B不经过真实IPC，不能证明IPC ABI、SA可发现性、平台公共事件投递、真实存储恢复或Modem行为。当前没有Split执行版本，也没有Legacy实测基线。

## 当前实施状态

用户已批准组件适配，实际完成新SIM-COMP-001只读容量切片及有效检查器负控；路径与证据见run_record.md。原五条服务入口场景和Absent初始化仍BLOCKED，不用容量通过替代它们。GoogleTest依赖已解决，状态/账户所需平台运行时语义未解决。
