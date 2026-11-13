## SIM-028 主卡切换：冻结基线与共用契约（2026-11-12）

本节扩展同一防护网，复用systemtest的GN、GTest、七个原场景及独立进程运行；不另建SIM实现或平行业务测试。旧生产代码只读，Golden业务预期不变。当前冻结仅针对明确双卡前态的条件性Host业务，不是原生产品标准。

冻结清单见[基线及SHA](baseline/frozen-baseline.json)：锁定HEAD、Golden、原七例、原Fixture/Runtime、GN和执行脚本；`frozen-inputs.tar.gz`保存修改前实际字节。清单同时固定上一批50轮证据索引和全部42份证据摘要，保留350业务PASS、原始50 PASS/300生命周期FAIL及非零退出码。代码基线是HEAD加当时工作区字节，不能仅凭HEAD复现。旧单卡反例及中间失败继续保留。修改后测试源码的摘要另记本批验证，不覆盖冻结快照。

### 可复用资产与适配边界

| 资产 | 路径与职责 | Legacy耦合处理 |
| --- | --- | --- |
| Scenario | `scenarios/primary_slot.json`：卡、账户、公开角色、输入、设备映射及应答条件、七个稳定场景ID | 公共前态不要求SimManager、三逻辑对象、订阅图或具体构造函数。原准备代码文字放在legacy证据字段；第三逻辑对象仅为Legacy装配细节 |
| Contract | `../../framework/compatibility/contracts/primary_slot.h`：测试端口与纯标准C++观测格式 | 不include OHOS、SIM或multisim；正式输入只有公开setter的slot。userSet=true是Core入口的内部翻译约束，非新增公开参数 |
| Oracle | `oracle/primary_oracle.py`：读取实际API/公开查询/参数/发布/RIL事实，检查条件并归一化 | 不读取私有字段、对象地址、动态类型、队列或绝对session。不能从Golden直接生成实际结果 |
| Legacy Adapter | `../../framework/compatibility/adapters/legacy/legacy_primary_adapter.h` | 透明转调真实CoreServiceSim；公开查询与外部替身观测仍由现有RealScenario提供。未复制业务实现。无RIL例只进入真实Manager，不能算Core全链 |
| Runner | `../../scripts/run_primary_contract.py` | 调用原Legacy执行脚本，原GTest、StrictMock、清理失败分类均不替换；原始exit1仍返回1。额外共享Oracle失败也返回非零 |
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

当前`AssignmentService::StartPrimarySwitch`和`CardRuntime::SwitchPrimaryRadio`仅有声明/流程注释，根产品仍由旧SIM执行；G2a只读查询接入不代表主卡写业务完成。源位置和SHA见[target-readiness.json](../../docs/archive/systemtest/configs/sim028_shared_primary_20261112/target-readiness.json)。只有完成真实分支、状态写入、外部端口与返回/重试闭包、提供真实Target装配与公开查询后，才注册Target Adapter。不能用计划函数、Fake或回调旧SIM获得Target PASS。

差分在相同场景ID、卡/角色/设备映射、外部应答计划、证据范围下执行；先检查输入锁与各自原始运行有效性，再用同一Oracle。比较错误符号、公开角色、契约参数及这几个键的实际写序列、相关发布payload/偏序及RIL请求资源。session归一化为尝试序号，保留尝试内相等及尝试间不同；忽略绝对编号、地址、线程、绝对时间和同阶段两槽发令顺序。真实RIL serial、未采集的Provider数据、所有额外事件及消费者状态不能据此判断等价。正常时间只看契约约束，不比调度快慢。

- 两边真实执行且各自契约PASS、可比较事实一致：只可报告该范围Host兼容PASS。
- Legacy PASS、Target契约失败或契约事实不同：FAIL，阻止兼容准入。
- Legacy FAIL/NOT_RUN、Target PASS：BLOCKED，先区分旧缺陷、模拟差异、准备失败或未经批准变化，人工裁决；不得自动认为新实现正确。
- 缺失Target、缺测量、版本/前态/场景不匹配：BLOCKED/NOT_RUN，不进行成功差分。
- 生命周期独立验收；业务兼容PASS不取消旧FAIL。旧资源失败/新资源通过属于单独修复审查。

`compare()`的合成输入测试仅验证比较算法；没有Target生产执行时即使算法返回相等，也不是Target测试PASS。共用Oracle不取代各实现必要的StrictMock及安全门禁。

```sh
# 一次完整七例，复用现有GN/Ninja；原生命周期失败使原始入口仍exit1。
python3 -B telephony_core_service/services/multisim/test/scripts/run_primary_contract.py \
  --adapter both --output /tmp/sim028-shared-primary-new

# Target尚无绑定：BLOCKED/exit2，不运行Fake业务。
python3 -B telephony_core_service/services/multisim/test/scripts/run_primary_contract.py \
  --adapter target --output /tmp/sim028-target-new

# Oracle技术正负控，不计入SIM业务数。
python3 -B telephony_core_service/services/multisim/test/cases/primary_slot/oracle/test_primary_oracle.py
```

本轮最终实际结果与全部原始记录见[共享契约验证](../../docs/archive/systemtest/configs/sim028_shared_primary_20261112/README.md)：七例原业务及共用Oracle均7/7，六例Strict RIL通过；原完整GTest仍1 PASS/6生命周期FAIL、exit1。Host64为59 PASS/5相同既有FAIL，Oracle技术正负控15项、实际观测副本检查9项通过；这些技术检查不是Target业务测试。

输入保护核对另发现原Legacy脚本在`--gtest_list_tests`阶段缺少独立LLVM_PROFILE_FILE，会覆盖上批遗留的根目录`default.profraw`生成文件。本轮已发生该改写，旧SHA已记录但原字节没有快照，无法恢复；它不是冻结业务证据或生产源码。现将发现阶段profile强制放入本轮输出目录，实际复验确认根profile不再变化。该例外在input-protection中明确保存，不宣称所有起始文件都未变化；原生产源码、Golden和历史验证记录保持原字节。
