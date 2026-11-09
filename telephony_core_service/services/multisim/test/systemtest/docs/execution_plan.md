# 最小切片、机器结果与Legacy/Split差分

更新：2026-10-15。Linux host组件方案已获批准，现行架构关系见[设计对齐](design_alignment.md)。本文件替代早期等待批准的计划；历史执行证据保持原日期和SHA。

## 已执行与剩余顺序

2026-10-08记录为容量B层1 PASS、INFRA16 PASS、容量负控按预期FAIL；原五条业务场景与C候选仍BLOCKED。不是五条已完成，不代表无卡/账户初始化成功。详见[运行记录](run_record.md)、[阻塞处理](blocked_resolution_oct08.md)。

1. 取得匹配版本的14个存储协议字段及提供方语义，冻结来源而非猜测声明；逐项补齐初始化真实生产闭包。编译/链接成功与生产逻辑可驱动分别给证据。
2. 完成正式RIL状态、存储提供方及必要平台回调适配，验证生命周期、错误、命名空间和清理；不以SIM业务桩补缺。
3. 用ISimManager及正式外部输入建立Absent，公开查询状态与账户，采用有界收敛并关联Oracle。未确认无卡/未就绪/账户保留行为标OPEN或实采后LEGACY，不推导永久契约。
4. 执行最小切片，保留命令、SHA/工作树摘要、工具链、退出码、生产参与及脱敏XML/JSON；确认契约支持时加有效负控。无确认契约不制造规范。
5. 再扩展单pSIM Ready、双卡关系/隔离及受控PIN Locked；条件不足分别SKIP/BLOCKED。不将设施用例或容量测试替代原五业务场景。
6. Phase0/G1事实与轨迹齐备后分别评审G2拆SO、G3迁移writer和G4产品扩展。文档中的目标不授权改公共接口、schema、权限或外仓。

原服务入口SIM-B-001独立调查capability来源与初始化链。不得删除/强制通过检查，也不能因组件通过晋升服务入口通过；拒绝是规则、环境缺失还是缺陷仍依证据判断。

## 机器结果：当前实现与完整设计分开

当前scripts/run.py已提供容量切片及负控的XML/JSON；INFRA输出见其运行说明。初始化审计可记录准备阶段BLOCKED，不生成假GoogleTest已执行XML。完整业务结果协议仍为逐项待实现/验收设计，不能因字段表存在声称运行器全部支持。

| 字段组 | 必填语义 |
|---|---|
| revision / repository / source_manifest | HEAD、dirty及参与文件摘要；Legacy/Split版本标签 |
| toolchain / environment / commands | GN、Ninja、编译器、GoogleTest、平台声明版本，实际命令及退出码 |
| case_id / oracle_ids / level | 每条证据关联Oracle来源，B/C；不得默认A |
| topology / permission / stub_config | 卡与槽关系、匿名身份、权限、输入脚本版本/摘要 |
| status / executed / reason / blocking_stage | PASS/FAIL/SKIP/BLOCKED，准备失败不得记作测试运行 |
| production / link_manifest | 实际目标、源码和链接证据；编译清单不等于运行覆盖 |
| observations / convergence | 公开结果、事件、稳定关系、正式请求；收敛条件、预算、elapsed、timeout |
| safety / cleanup / restore | 合成输入与报告脱敏、隔离命名空间、清理恢复及失败阻断 |
| counts / artifacts | 实际测试数量、分类状态、XML/JSON及审计产物 |

configs/cases.json是候选设计状态，不是运行报告。原始历史SHA和结果不被本轮基线回写。强制断言逐条关联Oracle，DU/INV及外部来源编号不自动等于CONFIRMED_CONTRACT。

## 收敛、隔离与安全

以可观察稳定条件和有界等待判定，不用固定长sleep或毫秒级线程顺序。桩可调度事件，生产实际线程范围须记录；不承诺穷尽交错。绑定代次不能替代同卡RecordsGeneration；合法切换造成的新会话按因果关联处理。

每用例独立进程/临时命名空间，先初始化、后启动，退出后核查和清理；清理失败阻断后续。重启持久化用例显式共享仅该用例的命名空间，用例末恢复。Linux进程重启不等于设备重启，桩重置不等于真实Modem恢复。

身份只在内存比较，输出别名或布尔关系，不让ICCID/IMSI/EID/号码进入ASSERT诊断、XML、JSON、普通日志或差分；真实数据不能以可枚举散列冒充脱敏。生产日志默认不直接转存，使用白名单证据。真实错误PIN尝试、Profile删除和eUICC重置默认禁用。

取消等待、超时和Git回退不等于外部效果撤销；恢复需事实重查并隔离迟到结果，不盲目重发设备命令或恢复旧身份。运行期操作存储不提供跨重启同OperationId续跑承诺。

## Legacy/Split差分

同一套边界用例分别链接两种产物，不新增运行期Feature Flag、不双发硬件命令。只读Shadow不成为writer。当前compare_results.py只支持已实现的容量证据，尚无Split执行记录，不能复制Legacy结果伪造差分通过。

| 比较项 | 判定 |
|---|---|
| 已确认API返回/错误码、稳定状态与账户关系 | 按对应Oracle强制；匿名化不隐藏影响契约的ID关系变化 |
| 必须/禁止事件、关键偏序 | 仅比较有来源的语义；不假定恰好一次或全局顺序 |
| 收敛 | 固定场景和预算，记录timeout；设施预算不是未确认的产品SLA |
| 正式对外请求 | 目标slot/sim与必要参数、必须/禁止语义；不比较无契约次数 |
| LEGACY_BEHAVIOR | 真实采集后差异进入人工审视，不自动判旧/新错误 |
| OPEN_QUESTION | 记录差异与待确认点，不作强制业务判错 |
| 内部类、SO符号、IPC次数、普通日志顺序、数据库表 | 不作为业务比较项 |

固定输入、响应、权限、初态和桩版本；环境不一致记录不可比/BLOCKED。并发只验证已确认的合法结果集合与必要偏序，不假定最终结果唯一。真实IPC/平台投递/设备恢复仍须另有A层证据。

本次只刷新设计与计划，未新增执行证据。
