# 多 SIM 重构测试入口

业务源码按五组目录迁移，test/tools位置保持；当前路径与本轮实际运行见[迁移验收](../../../docs/SIM_Directory_Migration_20261112.md)。历史证据包及冻结Scenario/Contract/Oracle/Fake保留原文，不以路径变化更改业务期望。

2026-11-09按用户要求，将重构新增测试工程及其构建、脚本、Stub/Fake、用例、证据和设计资料统一放在 `services/multisim/` 下。旧 `test/` 仅保留原有部件测试；旧生产实现只读。测试读取真实旧源码作为比较基线，负控只修改仓外副本。

Windows VS Code + WSL 的日常构建、筛选测试和调试见 [Host 使用说明](systemtest/HOST.md)。2026-11-11工程增量不改历史迁移摘要；Host首批四份既有构建/设施资产的明确修改由 [relocation_followups.json](relocation_followups.json) 单列，迁移检查同时验证当前摘要，仍拒绝未登记漂移。

`SetPrimarySlotId`新增[真实入口用例与覆盖审计](systemtest/primary_slot/README.md)：14条入口契约单列，使用真实CoreServiceSim/权限/能力源码；ISimManager处是内部测试接缝，完整Stub→RIL部件调用链仍BLOCKED。既有五条缺陷不变，完整范围以唯一执行状态§16及本轮证据为准。

| 目录 | 用途与边界 |
| --- | --- |
| [systemtest](systemtest/README.md) | 独立主机 GN 根、平台 Stub、容量/初始化切片、G2a 查询及历史结果。不是产品或设备验收。 |
| [sim_guard](sim_guard/README.md) | 跨架构兼容场景、冻结 Fake/Oracle、独立缺陷及 PIN 记忆契约；复用 systemtest 的主机构建。 |
| [multisim_architecture_rules](multisim_architecture_rules/README.md) | 业务骨架依赖、迁移溯源、纯规则、声明负控及独立 GN 入口。 |
| linux_sim_guard | 保留原实验构建、依赖锁及环境材料；本次迁移不使其自动成为有效运行基线。 |
| docs | 原测试防护网 v1.0/v1.1 设计与 Linux DDD 设计输入；版本权威关系不因移动改变。 |
| [../tools](../tools/check_sim_refactor_recovery.py) | Git 历史检查点恢复校验，不执行当前业务测试。 |

从 Git 根运行；依赖准备、参数及环境要求见各工程 README，输出目录选择仓外路径：

```sh
python3 -B telephony_core_service/services/multisim/test/check_relocation.py
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output "$SIM_TEST_OUTPUT/architecture"
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py -v
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/g2a"
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_sim_guard.py --sut=legacy --suite=all --repeat=1 --mutation-check --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/legacy"
python3 -B telephony_core_service/services/multisim/test/sim_guard/run_pin_memory_contract.py --mutation-check --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/pin"
```

5条既有缺陷/待裁决的中央记录是 [round2_catalog.json](sim_guard/traceability/round2_catalog.json)，对应独立断言仍在 sim_guard/runner；全套运行保留红灯，不能据此修旧实现或降低期望。账户注销 CR-07 另记于[执行状态 §12](../../../docs/SIM_Refactor_Execution_State.md#open-closed-followup-20261109)。

[relocation_manifest.json](relocation_manifest.json) 保存迁移前后路径和摘要。所有 C++ 用例、适配器、Fake、Oracle 和 JSON 证据保持原字节。历史 JSON 内的旧路径按当时提交解释，用此映射查找当前文件；恢复状态内的历史检查点路径也保持原值，不能替换成当前路径。迁移后的验证与限制见[执行状态 §13](../../../docs/SIM_Refactor_Execution_State.md#test-relocation-20261109)。业务骨架检查只排除顶层 test/tools，仍校验业务源码完整集合并拒绝业务对测试的 include 依赖。各测试 GN 使用显式 testonly 入口，不接入原产品构建。
