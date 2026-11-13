# 测试工程现状审计（2026-11-12）

## 输入与方法

输入 main@abd07fd85e87eba0f0883599538548bbcc09d8f2；暂存区为空，已有未跟踪 default.profraw 单独保护。远端 ls-remote 同 SHA；恢复脚本 fetch 失败（ref 未刷新）。逐文件清单、SHA256、GTest 声明和源码引用行见 [Inventory](Test_Architecture_Inventory.json)。不依赖旧临时构建声称当前 PASS。

## 当前职责

| 目录 | 真实内容和维护问题 |
|---|---|
| systemtest | 唯一活动 Host GN 根，生产源码 slice、GTest/GMock、Stub、设施测试、容量、初始化、G2a、主卡入口及真实链；scripts 和 tools 两组 Python 编排；configs 混合依赖锁、活动登记、Golden关联和历史运行证据 |
| sim_guard | 无 BUILD.gn；systemtest 显式构建其 Legacy Adapter、兼容17例、5条独立缺陷、PIN4例。Scenario/Oracle/测试文件分散；Primary 七场景 Oracle 被真实链复用；Target目录仅说明，没有真实Target实现 |
| multisim_architecture_rules | 独立 GN 根（component为root）、88单元语法、25逻辑target、4组纯规则、依赖/迁移检查、15项负控、5项声明负控；不是产品链接测试 |
| linux_sim_guard | deps.lock、平台编译实验、生成器、源码表；suites:all 仅依赖 production 和 platform，无 TEST、无 executable 测试。无活动入口引用；不能当另一套已通过测试。保留整树归档，不删除 |
| docs | v1.0/v1.1防护网设计、Linux DDD历史设计；systemtest/docs另有活动使用说明和历史过程，导航分裂 |
| 根元数据 | check_relocation 依赖旧迁移manifest和followups，当前219条来源跟踪；必须保留历史摘要，不能为移动重写旧证据 |

## 活动用例和框架

实际 GTest 声明按文件列于 Inventory；参数化展开数以真实 XML 为准，不能用宏个数代替测试数量。兼容/缺陷完整名来自 round2_catalog.json；设施登记 infrastructure_cases.json；真实主卡七例及条件性Golden来自 primary_slot/legacy_contract。原64例59 PASS/5 FAIL只是历史记录，当前运行另冻结。

框架：平台 stubs（含事件/FFRT/分离线程）、Legacy Adapter与platform_spy、纯端口contracts、RIL fake、Host GN/toolchain、依赖锁、production slice配置。capacity/bootstrap/G2a probe是用例专属驱动，适合随用例放置。real_scenario、real_runtime、安全边界及生命周期观测是Primary专属Fixture，不应包装成公共设施。card_oracle包含具体预期，属于兼容用例；primary_oracle包含Primary业务语义，属于Primary用例。通用比较实现目前无足够跨业务复用证据，不额外抽层。

## 构建和调用关系

Host .gn → gn/BUILDCONFIG → BUILD.gn；sim_guard_legacy、pin、component、primary entry/real、adapter、G2a、bootstrap分别保留。Host根通过sources/include_dirs引用../sim_guard；没有独立sim_guard GN工程。Architecture gn/.gn → :all → //services/multisim:skeleton，生产BUILD visibility精确授权旧测试label。Linux实验 .gn→build→suites仅聚合编译。

Python：host→run_sim_guard/check_semantics、run_g2a_query/digest_tree、build_clock；pin→run_sim_guard；run_primary_contract→primary_oracle并启动run_legacy_primary_contract→host；ci_sim028复用分类器；architecture/run→check_structure→check_directory_migration，以及check_open_closed_contracts；技术测试导入各被测Python模块。没有发现import环；Host依赖脚本中的hash/契约检查，职责耦合但不是业务用例环。完整 import/include/GN/路径行在 Inventory 中。

脚本清单由 Inventory 中 role=script 全量给出。Shell三入口仅转调host，不重复编译机制。兼容、PIN、G2a各自编排GN及coverage，目标和检出协议不同，不能机械合并；准备依赖和时间戳已有共享实现。统一入口应调度现有runner并原样保留非零退出，不另造统计器。run.py同名含义不同，是入口认知负担。

仓外引用检索包括全部*.gn/gni/py/sh/yml/yaml/json及Markdown：.vscode/tasks.json调用host；生产multisim/BUILD.gn有26处架构label；没有发现GitHub Actions配置。工具恢复脚本不依赖业务运行；AGENTS、README和知识文档存在当前命令，也有固定提交/历史路径，必须区分处理。

## 资产与问题

运行日志、XML、patch、zip、tar.gz、JSON摘要分散于systemtest/configs及architecture/evidence。历史JSON不能仅凭扩展名判断用途：dependency_lock是活动输入，frozen-baseline/contract-lock和Golden是受控资产；validation、sha索引及raw包是历史证据。旧初始化证据classifier-tests.log已知摘要漂移必须保留，不能用新SHA覆盖原sha256.json。

复杂性来源是历史工程名与职责正交、脚本两级分散、Primary场景与Oracle跨工程、活动锁和历史输出混放。尚无证据支持删除无法确认的文件，也没有证据说明Legacy/Target适配可合并。重构应保留所有输入，按真实依赖迁址。

文件数量（包含隐藏GN、排除缓存）：
- README.md: 1
- check_relocation.py: 1
- docs: 3
- linux_sim_guard: 23
- multisim_architecture_rules: 27
- relocation_followups.json: 1
- relocation_manifest.json: 1
- sim_guard: 32
- systemtest: 854
