# OpenHarmony 多 SIM 重构执行指引

## 目标与完成口径

最终由 `services/multisim/` 完整替代 `services/sim/`，形成业务容易定位、事实写权唯一、运行状态分责、依赖可检查的 SIM/eSIM 实现。结构重组、生产接入、写权切换、行为增强和部署变化分别验收；不长期运行两套业务实现。

每批只执行本次授权范围，完成实际修改、构造/使用/构建同步、适用验证、无关修改保护及执行状态更新。明确设计、骨架、主机切片、产品/设备各自的证据；外部条件只阻塞依赖它的切片，继续其他已授权工作。未授权生产切换不由目录实施或文档提交推定。

## 材料职责与冲突处理

| 材料 | 唯一维护职责 |
| --- | --- |
| [目标主设计 v1.2](docs/SIM_Target_Architecture_Design_v1.2.md) §4～6、§9～14、§22 | 业务边界、唯一写权、异步/资源协议和阶段门禁；整体仍draft/pending，不覆盖已批准契约 |
| [源码详细设计](docs/SIM_Multisim_Source_Architecture_Detailed_Design.md) §3/4/17/21 | 当前/稳态目录、全文件归属、具体接口及逻辑GN依赖 |
| [唯一执行状态](docs/SIM_Refactor_Execution_State.md) | 当前基线、远端检查点、实测证据、局部阻塞及下一批任务；AGENTS不维护动态SHA或PASS数量 |
| [功能/WU清单](docs/SIM_Architecture_Refactor_Design.md)、[流程对照](docs/多SIM架构重构_设计说明与业务流程对照.md) | 沿用SIM-001～062及子项、WU-01～09、生产入口与迁移闭包；不另建编号或平行台账 |
| [接口契约](docs/SIM管理对外接口说明_源码核实版.md)、[外仓边界](docs/knowledge/routing/repo-boundary.md) | 外部API、权限、错误、消费者与外仓责任 |
| [源码README](services/multisim/README.md) | 日常任务→路径→Owner→测试导航；不重复完整架构 |
| [独立目录ADR](docs/SIM_Multisim_Source_Directory_Architecture_Decision_Independent_2026-10-31.md)、[原ADR](docs/SIM_Multisim_Source_Directory_Architecture_Decision_2026-10-31.md)、[历史实施评审](docs/SIM_Target_Architecture_Independent_Review_2026-10-29.md) | 取舍、反证和历史证据；两份ADR仍Proposed，用户已选修订B+仅作为结构实施基线 |
| [构建规则](docs/knowledge/verify/build-test.md)、[主机测试入口](test/systemtest/README.md)、[骨架验证](test/multisim_architecture_rules/README.md) | 环境、参数、运行范围和验证限制 |

用户当前明确授权决定本批范围；已发布/批准契约约束行为，固定源码/构建证明现状，主设计维护目标语义，详细设计维护物理组织。历史文档中O作为业务Owner、旧目录和旧方法草图不得覆盖主设计当前裁决。冲突以具体证据定位并同步责任文档，不按日期或文件名决定权威，不把旧缺陷默认为永久契约。

## 恢复与任务选择

1. 读取根/本文件及执行状态；执行 `git status --short`、`git diff --cached --name-status`、`git remote -v`、`git branch -vv`、`git rev-parse HEAD`、`git worktree list`，保护已有工作。
2. 从仓库根运行 `python3 telephony_core_service/tools/check_sim_refactor_recovery.py --remote origin --fetch`，按实际remote替换名称。核对main与G2a检查点可达性；工具不运行业务测试，不能据此声称PASS。
3. 按执行状态定位本批最小任务，读取相关设计章节、源码写点、调用/回调、源表、消费者和Oracle。用 `rg` 定点检索，不默认通读全部历史。
4. 若需其他基线，用独立worktree/克隆且先保护原工作区；不自动切换/合并历史分支。参数化依赖和输出目录，不依赖旧机器路径。
5. 按本批范围修改→验证→修复→核实Diff→更新执行状态。实际命令、输入基线、未验证范围和剩余条件必须可追溯；不虚构PASS或补业务成功桩。

## 源码组织与状态责任

修订B+六组是已选结构基线；完整实际树与未来能力分别查详细设计§3，未有源码的能力不建空目录。

| 目录/机制 | 状态责任与安全规则 |
| --- | --- |
| account / T | 账户身份、Slot/Port/Profile已接纳绑定、资料、active及激活业务进度；唯一事实接纳。生产尚未移交的写点不能因骨架存在宣称已收口 |
| card_selection / A | 默认Voice/SMS/Data、Primary、业务分配/冲突准入、偏好及唯一业务重试；不拥有真实设备租约 |
| card / R卡能力 | 卡状态/锁因、协议、Records/加载、电话簿、卡短信、STK、carrier与eUICC能力各自分责；卡库存不等于T绑定 |
| device / R设备执行 | Radio握手及协议session/phase、真实共享资源资格；不取得T动态绑定/A选卡权。Card/Device不是强制逐层转发链 |
| integration / C/I/H | 兼容入口、共享交付、技术适配和宿主装配；只有必要组合根连接全体实现，不集中业务重载/重试 |
| support / O与纯值 | 小型纯值、H构建的只读SlotCatalog、各用例自有请求机制；O不是第三业务Owner，不建全局编排中心 |

- 卡应用SessionRegistry在 `card/state`，管理应用代次/ExpectedTransition；Radio协议session属于 `device/radio`（成熟实现待迁入），不预建 `device/session`。T bindingRevision与R bindingGeneration不混同；epoch由宿主提供。
- ResourceBarrier在 `device/resource`，当前仅有骨架纯规则；真实物理域、结束/恢复证据未接入生产。不能按槽或软件对象复制共享屏障。
- SlotCatalog在 `support`，含容量/路由/装配/可见性描述，由H构建后只读；不持动态账户绑定，不把调用者投影当Modem原始事实。
- CardRuntime/result_sinks位于 `integration/lifecycle` 只是过渡位置，仍承担R运行接缝，尚未薄化；`card/esim/legacy_profile_result.h`仍是C的双结果投影。目录位置不转移Owner。
- Domain仅纯规则/状态；平台I/O、编排、等待、callback在相应外层。T/A仅经窄应用契约协作，不include对方Service/聚合/私有Adapter，不持跨Owner可写引用或嵌套同步等待。
- 保留独立逻辑target、精确public与依赖DAG，不按六组机械合并target。禁止CardManager、DeviceManager、总公共头/DTO池或无证据的总线/日志/平行执行架构。

## 生产迁移与关键不变量

- 成熟SIM能力仍由原`services/sim/`实现；main已包含G2a只读Query及Core旧对象附着，其他multisim能力仍为testonly骨架，产品验收未完成。以根GN和Core真实装配判定接入，不以目录或对象声明判定。最终替代前按完整能力/WU退出旧写者、执行者和重试者，无双写/双发令/双重试。
- 完整WU包含初始化、重载、后台、恢复、厂商入口和查询回写；特别覆盖Carrier Loader及CardFileDetectManager独立账户写入口，不只迁公开setter。同一行可合并T/A字段计划，不赋予技术Store跨Owner写权。
- 请求发起时保存Owner/epoch/request/step/attempt、原Session及传输关联，实际写点同域准入。迟到结果只更新允许的原效果认知，不污染新请求或复活旧完成出口。
- 调用返回、等待结束、业务效果、通道关闭及真实设备结束分开；本地ChannelId/AID清零不能证明资源可复用。设备结束未证时不得新增释放/安全重试承诺，新的隔离/取消/重试策略须单独批准和差分。
- eSIM普通/低优先级File及ARA-M权限通道按证实的共享域验收；不复制66250高层任务状态机。Profile协议/库存归Card，已接纳绑定归T，偏好归A。
- 成熟Records/Phonebook迁入按低层EF I/O、ADN I/O、高层Records及Phonebook用例的真实DAG复用，不能用两个互依总target或复制算法消除表面环。
- 保留API/ABI/IDL/schema、权限/错误顺序、读源、部分效果、通知偏序和原Feature分支。结构变更与固定目标、迟到拒绝、准入/隔离、排队/重试等行为增强分别验收。
- Core拥有共享RIL，SIM不复制/停止宿主实例；保持eSIM/SIM/搜网/CMI初始化发布顺序、原Manager控制块、进程单例唯一定义及匹配销毁。未排空不宣称可卸载；Detect初始化weak绑定风险独立验证，不混入机械重构修复。
- 新增生产同步遵守仓内FFRT约束，不宣称旧std同步已迁完。编辑保持局部，不做无关行尾、符号或格式重整。

## 阶段与验证门禁

G0/G1为基线/护栏，G2a为原SO依赖收口，G2b为产物工程边界，G2c为生命周期，G3a～d按完整WU接管，G4为独立新能力。按主设计§22的依赖关系执行，不把全部阶段强制串行；状态/阻塞/下一批仍只查执行状态。

- 文档：核对源码事实、当前/计划分离、链接/锚点/围栏及 `git diff --check`；不因此声称业务测试通过。
- 纯规则：`python3 -B telephony_core_service/test/multisim_architecture_rules/run.py`。
- 目录/契约/骨架：同步BUILD、include、测试及既有 `source_migration.json`，保留历史追溯；运行 `python3 -B telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration --output /tmp/multisim-architecture-check`。输出目录可另选仓外路径。
- 生产接入：运行对应真实切片/Oracle及必要Feature、初始化/消费者差分；匹配OHOS树中构建 `hb build -T //base/telephony/core_service:tel_core_service`。G2a B+主机脚本及证据已合入main，准确命令与被测代码基线查执行状态；历史检查点仍按原路径恢复。
- 产品、SO/SA、provider和设备结束资格按各自门禁验证；骨架检查只覆盖文本/target、有限纯规则、语法和对象，不证明生产链接、行为、字段写权或硬件静止。

## 交付与Git

交付说明实际变化、验证命令/结果/限制、输入基线、未完成项和下一最小切片，同步唯一执行状态。需要改变职责/运行行为时回到主设计裁决，路径/具体接口同步详细设计，导航同步README，功能范围复用原SIM/WU台账。

提交、推送或PR按当前任务授权；不将前一批授权推定为新任务自动发布。仅暂存本批明确路径，不覆盖用户修改、不强推；远端新增无关提交时保留本地成果，不为推送成功自动合并/变基。推送后核实远端SHA、包含关系和文件范围，更新状态且避免自身提交SHA自引用。
