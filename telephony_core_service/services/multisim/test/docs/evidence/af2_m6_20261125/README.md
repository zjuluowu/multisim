# AF-2 M6 验证证据

阶段一在 `43881d57134e0d1004eed66333154281eed331c5` 基线上编译现有 Verify Legacy，并按双激活模型执行 `RealSimPrimary.AccountMetadataUsesLegacyPredicatesAndFieldCaches`，1/1 PASS。实际观察为账户 ID 匹配、最终空名称、标签 3、7 次 Provider 字段写入、5 次账户通知。Legacy 生产源码未修改。

- [验证编排及结果](legacy-metadata-result.json.gz)
- [原始 GTest XML](legacy-metadata.xml.gz)
- [原始 GTest 日志](legacy-metadata.log.gz)

阶段二交付正式 Target 账户资料入口。Developer M1–M6 44/44。LLDB 实际停在正式 H/C、账户和绑定、原字段命令、字段版本、同步通知及模块销毁位置。Verify 的 10 个双后端场景 20/20，Oracle 与双后端差异均为 0。

- [Developer 专项](developer.json.gz)
- [LLDB 命令](account-metadata.lldb) · [原始单步日志](account-metadata-lldb.log.gz)
- [Verify 差分](verify-differential.json)

最终验收对应发布实现 `4e6b6b2c9652df3ade51ec022b206f7471e7b9ac`。Developer 与 Verify 使用独立构建输出。Verify、差分及 sanitizer 均在本轮实际执行。

| 入口 | 实测结果 |
|---|---|
| `test.py all --backend legacy --ci` | 144/144，CI 准入通过，已知失败 0，兼容矩阵、完整性、产品静态检查、迁移一致性、两项变异负控及恢复均通过。[Verify Legacy](verify-legacy.json.gz) · [真实执行](legacy-metadata-execution.json) |
| `test.py all --backend target` | 146/146，含 M1–M5 38/38、M6 6/6。真实 H/C/T/I/Registry/RuntimeOperationStore 正向执行审计通过。[Verify Target](verify-target.json.gz) · [真实执行](target-metadata-execution.json) |
| `test.py run --backend both --init-model dual-active-primary0` | 既有九个场景与新增资料场景，两端共 20/20。Legacy/Target 和 Oracle 差异均为 0。[双后端报告](verify-both.json.gz) · [差分](verify-differential.json) |
| ASan / TSan | 分别用已有独立 GN sanitizer 输出重建目标 `sim_test_target_host`，ASan 开启泄漏检测。M1–M6 各 44/44。[ASan](asan.json.gz) · [TSan](tsan.json.gz) |
| 架构与技术 | 99 个独立 C++ 单元及组合头、20 项结构检查、5 个编译负控、4 组纯规则、GN `--check` 和 Ninja 均通过。首轮技术检查发现源码清单数量仍为 M5 的 99 个模块文件。更新为 101 个后，Standalone、Target、Legacy 均重跑通过。[架构](architecture.log) · [首轮技术日志](initial-technical.log.gz) |
| `test.py editor --backend both` | 8 个 ELF、290 个位置通过，组合用例定位在 `c0_topology_integration_test.cpp:2201`。[入口](verify-editor.json.gz) · [位置](editor-locations.json.gz) |

[原始归档](verification-logs.tar.gz)包含 Verify 运行与构建日志、GTest XML、执行审计、技术及变异日志，以及两项 sanitizer 的配置和运行结果。[Git 恢复及发布核验](recovery.json)确认历史检查点可恢复，远端 main 已包含阶段实现。VS Code GUI 为人工未验收。
