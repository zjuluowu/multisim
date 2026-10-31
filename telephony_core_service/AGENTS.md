# OpenHarmony 多 SIM 重构执行指引

## 目标、范围与完成标准

持续将现有 SIM/eSIM 管理演进为职责、所有权及依赖明确的多 SIM 架构。工作包括设计裁决、可执行协议、测试护栏、原 SO 依赖收口和按完整写权单元实施的迁移；按用户本批授权选择最小切片，不沿用“只做设计、禁止代码”的旧阶段目标。

完成一批任务需要：实际修改及使用/构造/构建一致；受影响验证实际执行；差异无无关用户修改；说明设计、骨架、主机、产品/设备各自状态，并同步唯一执行状态。没有外部证据时限定局部阻塞，继续无依赖工作。

## 当前入口与材料职责

当前已完成评审修正的 main 基线核实；G2a 真实只读查询有主机证据，代码保存在明确远端工作分支，尚未作为 main 的生产能力。完整 Manager 初始化、原 SO 产品构建及 G3 写权未验收。**分支、SHA、证据、阻塞和下一批任务统一查[执行状态](docs/SIM_Refactor_Execution_State.md)，本文件不维护详细进度。**

| 材料 | 使用职责 |
| --- | --- |
| [目标主设计 v1.2](docs/SIM_Target_Architecture_Design_v1.2.md) §4～6、§9～14、§22 | 技术边界、唯一写权、协议和阶段门禁；仍为draft/pending，不覆盖已批准契约 |
| [流程对照](docs/多SIM架构重构_设计说明与业务流程对照.md) | 原路径、分支、目标时序及功能对应 |
| [功能/WU/迁移清单](docs/SIM_Architecture_Refactor_Design.md) | 沿用SIM-001～062及子项、WU-01～09与迁移清单，不另建编号 |
| [对外接口说明](docs/SIM管理对外接口说明_源码核实版.md)、[边界说明](docs/knowledge/routing/repo-boundary.md) | 参数/权限/错误、消费方及外仓责任 |
| [独立评审与实施记录](docs/SIM_Target_Architecture_Independent_Review_2026-10-29.md) | 历史裁决、修改和验证限制，不是新的目标规范 |
| [目录评审](docs/SIM_Multisim_Source_Directory_Review_2026-10-30_Opus.md) | 待独立核实意见；未裁决提案不能直接当迁移要求 |
| [构建验证规则](docs/knowledge/verify/build-test.md)、[测试入口](test/systemtest/README.md) | 工具、主机适配、生产构建与运行范围 |

固定版本源码/构建证明现状，已发布或已批准契约约束行为，目标设计提出方案，评审/历史材料提供待核实输入。冲突回到具体证据和主设计裁决，不按日期、标题或版本文件名决定权威。旧实现缺陷不自动成为永久规则，也不在机械重构中悄改。

## 新会话恢复顺序

1. 读根与本文件，执行`git status --short`、`git remote -v`、`git branch --show-current`、`git rev-parse HEAD`和`git worktree list`，保护已有修改。
2. 阅读[执行状态](docs/SIM_Refactor_Execution_State.md)，确认哪个成果在main、哪个只在工作分支。核实remote仓库地址；可fetch后查看包含关系，不自动切换/覆盖当前工作区。
3. 从仓库根执行`python3 telephony_core_service/tools/check_sim_refactor_recovery.py --remote origin --fetch`（remote名称按实际选择），核对远端检查点。工具只报告Git/文件/历史证据，不证明当前业务通过。
4. 按下一批任务只读对应设计章节、契约、源码和评审裁决，不默认通读全部历史。状态与代码不符时依据证据修正状态。
5. 需要另一个代码基线时建立隔离worktree或独立克隆。依赖和输出目录通过参数/环境变量传入，按状态中的命令复验。
6. 继续一个当前条件可执行的最小任务；结束前更新执行状态的结果、代码所在位置和剩余条件。没有历史/外部条件不能虚构PASS，也不笼统停止全部工作。

## 必须守住的边界

- `services/sim/`是原生产实现；`services/multisim/`是目标区域，是否进入生产以GN/装配及对应阶段为准，目录存在不等于启用。
- T拥有账户/拓扑，A拥有角色/资源及所属切换；O仅为Application关联/等待/效果机制。业务Owner、代码层、目录和SO/SA/进程是不同维度。
- 每类事实、设备执行和同层业务重试有唯一责任者；T/A经受控应用入口协作，R复用成熟State/File/Records/电话簿/STK/eUICC协议，不复制空壳、恢复中央协调器或隐藏代写。
- Domain只放纯规则/状态；编排、I/O、等待、callback、平台类型在对应外层。无足够证据不增上下文、总线、持久日志或平行执行架构。
- 保护公开API/ABI/IDL/schema、SA和进程边界，以及原解析时点、读源、错误、部分效果、完成和通知节点。新公开接口、身份政策、MEP/新容量等须独立需求/证据。
- 结构迁移与固定目标、迟到拒绝、准入/隔离、排队/重试等行为增强分别验收。新旧不得双写、双发令或双重试；未迁路径也遵守已移交字段的唯一写权。
- Core拥有共享RIL；SIM不复制/停止共享实例。保留原Manager控制块、eSIM/SIM/搜网/CMI初始化发布时点与匹配销毁。未排空不宣称可卸载。
- 发起时保存关联，最终实际写点准入；等待结束不等于设备结束。provider事务、结束证据、远端任务、Port/MEP缺口只阻塞依赖它的切片，不补业务成功桩或捏造能力。
- 新增生产同步按本仓FFRT约束；不宣称旧std同步已全迁。使用`rg`先检索实际调用/赋值，编辑保持局部，不混入行尾转换或无关整理。

## 阶段、验证与下一批选择

阶段为G0/G1护栏→G2a原SO收口；G2b SO工程边界、G2c生命周期按各自门禁；G3按完整WU迁移，行为增强单列；G4能力/契约扩展独立。不是所有阶段全局串行，依赖及退出条件见[执行状态](docs/SIM_Refactor_Execution_State.md)与主设计§22。

纯规则：`python3 telephony_core_service/test/multisim_architecture_rules/run.py`。G2a查询和完整初始化审计位于工作分支，准确命令/固定依赖见执行状态；`hb build -T //base/telephony/core_service:tel_core_service`只在匹配完整OHOS树使用。公开接口、解码等检查遵循[构建验证规则](docs/knowledge/verify/build-test.md)。纯文档检查不能声称业务测试通过。

先选状态中的推荐最小任务；核对真实入口、前置证据、修改范围、验证和退出条件后执行。用户明确授权的常规修改/检查/提交不要重复征询；授权未覆盖的生产切换不默认执行。只暂存本次精确文件，正常整合远端并复验冲突；推送后核实远端包含提交。不得为保存成果将未验收工作直接合入main。

## multisim业务定位与本轮目录约束

先读[源码导航](services/multisim/README.md)按T1～T6定位；当前布局/未来能力分开见详细设计§3。T账户/绑定纯规则在topology/accounts，资料场景在topology/account_profile；eSIM Profile在runtime/euicc。纯规则仍由topology_domain target约束，不因改名放松依赖。

请求/Step由各用例持有；等待结束不等于设备结束。SessionRegistry和ResourceBarrier保持唯一责任，不随槽复制真实共享资源。Runtime不得代写T账户或A角色；纯规则不依赖O/平台，跨Owner仅经窄api，不包含对方私有用例/聚合/Adapter。

修改路径同步BUILD、测试及source_migration.json；目录切片运行`python3 telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration`与`git diff --check`。该命令验证骨架、有限纯规则及文本/target依赖，不代替产品/设备或生产行为差分。当前生产仍在services/sim，G2a检查点与准入按唯一执行状态。
