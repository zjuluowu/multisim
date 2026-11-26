# OpenHarmony 多 SIM 重构执行指引

## 目标与完成口径

最终由 `services/multisim/` 完整替代 `services/sim/`，形成业务容易定位、事实写权唯一、运行状态分责、依赖可检查的 SIM/eSIM 实现。结构重组、生产接入、写权切换、行为增强和部署变化分别验收；不长期运行两套业务实现。

每批只执行本次授权范围，完成实际修改、构造/使用/构建同步、适用验证、无关修改保护及执行状态更新。明确设计、骨架、主机切片、产品/设备各自的证据；外部条件只阻塞依赖它的切片，继续其他已授权工作。未授权生产切换不由目录实施或文档提交推定。

## 任务入口与依据

[开发文档入口](docs/README.md)维护文档职责、阅读路径和冲突处理规则。先查[唯一执行状态](docs/SIM_Refactor_Execution_State.md#current-status)，再按本批授权选择工作。实际修改涉及既有行为时，以已发布或批准的契约为约束，按固定代码和证据复验。

后续 AF-2/AF-3 切片直接继承 [CD-01、CD-02、CD-03](docs/AF2_Compatibility_Decisions_Approved_20261126.md#inheritance)。
保持独立回归，按原门禁验证产品桥和外部调用者适配。三项在后续 Host 开发中沿用已有批准，兼容缺陷登记和新变更按该记录的范围判定。

## 恢复与任务选择

1. 读取根/本文件及执行状态；执行 `git status --short`、`git diff --cached --name-status`、`git remote -v`、`git branch -vv`、`git rev-parse HEAD`、`git worktree list`，保护已有工作。
2. 从仓库根运行 `python3 telephony_core_service/services/multisim/tools/check_sim_refactor_recovery.py --remote origin --fetch`，按实际remote替换名称。核对main与G2a检查点可达性；工具不运行业务测试，不能据此声称PASS。
3. 按执行状态定位本批最小任务，读取相关设计章节、源码写点、调用/回调、源表、消费者和Oracle。用 `rg` 定点检索，不默认通读全部历史。
4. 若验证需其他基线，可用独立worktree/克隆且先保护原工作区；这仅用于验证，不作为默认提交或发布副本。不自动切换/合并历史分支。参数化依赖和输出目录，不依赖旧机器路径。
5. 按本批范围修改→验证→修复→核实Diff→更新执行状态。实际命令、输入基线、未验证范围和剩余条件必须可追溯；不虚构PASS或补业务成功桩。

## 源码组织与状态责任

当前一级目录由[目录设计方案](docs/SIM重构代码目录结构设计方案.md)确定为五组；历史B+六组不再作为新增代码落点。逐文件归属、临时接缝和GN对照见[本批迁移记录](docs/SIM_Directory_Migration_20261112.md)。

新增文件先确定业务事实 Owner，使用目标主设计约束职责与依赖，再按[源码详细设计 §3～4](docs/SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree)确定文件和逻辑 target。构建依赖使用现有精确 public、visibility 和 DAG，业务适配器跟随所属 Owner。CardRuntime 剩余设备职责及其迁移条件见详细设计。

## 生产迁移与关键不变量

- 旧实现变更以当前任务的明确授权为界：用户已单独授权并完成C0及剩余FAIL所需的局部修复，范围和证据查唯一执行状态；该授权不等于后续任意生产修改或迁移许可。文档及测试配套刷新不继续修改 `services/sim/`、旧Core/CMI/wrapper/RIL业务路径或公共接口，不为测试加入friend或生产探针。新实现正确性、旧缺陷修复、内部ABI变化和产品验收分别报告。

- 成熟SIM能力仍由原`services/sim/`实现；2026-11-16按用户授权回退G2a生产接入并同步GitCode主仓，multisim能力保持testonly骨架；G2a Core/Binding仅保留Host测试参考实现，产品验收未完成。以根GN和Core真实装配判定接入，不以目录或对象声明判定。最终替代前按完整能力/WU退出旧写者、执行者和重试者，无双写/双发令/双重试。
- 完整WU包含初始化、重载、后台、恢复、厂商入口和查询回写；特别覆盖Carrier Loader及CardFileDetectManager独立账户写入口，不只迁公开setter。同一行可合并T/A字段计划，不赋予技术Store跨Owner写权。
- 请求发起时保存Owner/epoch/request/step/attempt、原Session及传输关联，实际写点同域准入。迟到结果只更新允许的原效果认知，不污染新请求或复活旧完成出口。
- 调用返回、等待结束、业务效果、通道关闭及真实设备结束分开；本地ChannelId/AID清零不能证明资源可复用。设备结束未证时不得新增释放/安全重试承诺，新的隔离/取消/重试策略须单独批准和差分。
- eSIM普通/低优先级File及ARA-M权限通道按证实的共享域验收；不复制66250高层任务状态机。Profile协议/库存归Card，已接纳绑定归T，偏好归A。
- 成熟Records/Phonebook迁入按低层EF I/O、ADN I/O、高层Records及Phonebook用例的真实DAG复用，不能用两个互依总target或复制算法消除表面环。
- 保留API/ABI/IDL/schema、权限/错误顺序、读源、部分效果、通知偏序和原Feature分支。结构变更与固定目标、迟到拒绝、准入/隔离、排队/重试等行为增强分别验收。
- Core拥有共享RIL，SIM不复制/停止宿主实例；保持eSIM/SIM/搜网/CMI初始化发布顺序、原Manager控制块、进程单例唯一定义及匹配销毁。未排空不宣称可卸载；Detect初始化weak绑定风险独立验证，不混入机械重构修复。
- 新增生产同步遵守仓内FFRT约束，不宣称旧std同步已迁完。编辑保持局部，不做无关行尾、符号或格式重整。

## 阶段与验证门禁

重构新增测试工程、独立 GN 配置、脚本、Fake/Oracle、缺陷记录与证据统一放在 `services/multisim/test/`；恢复工具放在 `services/multisim/tools/`。不在原 `test/` 或旧生产目录新增重构测试设施。当前入口见[测试导航](services/multisim/test/README.md)，历史提交及证据内路径保持原值，通过迁移映射追溯。

G0/G1为基线/护栏，G2a为原SO依赖收口，G2b为产物工程边界，G2c为生命周期，G3a～d按完整WU接管，G4为独立新能力。按主设计§22的依赖关系执行，不把全部阶段强制串行；状态/阻塞/下一批仍只查执行状态。

- 文档：核对源码事实、当前/计划分离、链接/锚点/围栏及 `git diff --check`；不因此声称业务测试通过。
- 纯规则：`python3 -B telephony_core_service/services/multisim/test/scripts/run_architecture.py`。
- 目录/契约/骨架：同步BUILD、include、测试及既有 `source_migration.json`，保留历史追溯；运行 `python3 -B telephony_core_service/services/multisim/test/scripts/run_architecture.py --with-gn --migration --output /tmp/multisim-architecture-check`。输出目录可另选仓外路径。
- 生产接入：运行对应真实切片/Oracle及必要Feature、初始化/消费者差分；匹配OHOS树中构建 `hb build -T //base/telephony/core_service:tel_core_service`。G2a B+主机脚本及证据已合入main，准确命令与被测代码基线查执行状态；历史检查点仍按原路径恢复。
- 产品、SO/SA、provider和设备结束资格按各自门禁验证；骨架检查只覆盖文本/target、有限纯规则、语法和对象，不证明生产链接、行为、字段写权或硬件静止。

## 交付与Git

交付说明实际变化、验证命令/结果/限制、输入基线、未完成项和下一最小切片，同步唯一执行状态。需要改变职责/运行行为时回到主设计裁决，路径/具体接口同步详细设计，导航同步README，功能范围复用原SIM/WU台账。

按根AGENTS中的用户持续授权，每完成一个有实际成果、可复核的阶段进展，完成适用验证及唯一执行状态同步后，默认直接在当前本地工作区逐文件暂存、提交本批成果，并常规推送至 `origin/main`，无需再次确认，不等待所有阶段结束才发布。不要为提交或发布另建独立克隆、副本或worktree；只有用户明确要求时才使用独立副本发布。当前工作区Git不可写、基线未对齐或远端分歧时，保留成果并报告具体阻塞，不自动改用独立副本。已有失败/阻塞必须如实保留，提交推送成功不代替验收。

仅暂存本批明确路径，不覆盖或混入用户已有/无关修改，不强推、不绕过保护；推送前刷新并核对远端。远端新增无关提交、历史分歧或分支保护拒绝时保留本地成果并报告，不为推送成功自动合并/变基或切换工作区。推送后核实远端SHA、包含关系和文件范围，状态记录避免自身提交SHA自引用。持续授权仅覆盖阶段性提交/推送，不授权PR、生产修改或迁移；后续用户明确限制发布或改变目标分支时，以新要求为准。
