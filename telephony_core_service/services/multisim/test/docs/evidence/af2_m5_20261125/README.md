# AF-2 M5 原始证据

输入 main 为 `85515ad34788d869806d28d522f0d5eed11ae8ee`。Legacy 特征先在双激活
Verify 执行，Voice/SMS 各 1/1，实际测量写入、公告、清除后读取和返回码。
[原始结果](legacy-contract.json.gz)

Developer 专项实测 38/38。代表用例通过 LLDB 实际停在 Voice Provider 的 `sim_id=41`
和 `card_type=1`，观察 Current 从 0 到 1、Historical 仍为 0，随后命中 SMS 原票、
Stop 和析构，程序退出 0。
[专项](developer.json.gz) · [LLDB 脚本](four-roles.lldb) · [LLDB 原始日志](four-roles.log.gz)

结构迁移和最新源码摘要通过。首轮静态检查拒绝了新增标准头 `string_view`，
在原标准头白名单登记该依赖后通过，外部平台包含限制按原规则检查。
[首轮日志](initial-structure.log) · [实测结构迁移](structure.log)


最终验证的生产源码及测试脚本来自已发布的 `74e9a1993`。Developer 和 Verify
输出隔离，以下为本轮实际重新执行的结果。

| 入口 | 实测与证据 |
| --- | --- |
| `test.py all --backend legacy --ci` | 143/143，CI 准入、完整性、变异与恢复通过。 [Verify Legacy](verify-legacy.json.gz) · [执行审计](legacy-execution.json) |
| `test.py all --backend target` | 140/140，M1–M4 30/30，M5 8/8。 [Verify Target](verify-target.json.gz) · [执行审计](target-execution.json) |
| `test.py run --backend both --init-model dual-active-primary0` | 九个场景，18/18，后端及 Oracle 差异 0。 [Both](verify-both.json.gz) · [差分](verify-differential.json) |
| ASan / TSan | 分别重建验证配置的 `host_asan` 或 `host_tsan` 目标，`run_gtest.py` 过滤 M1–M5，分别 38/38。 [ASan](asan.json.gz) · [TSan](tsan.json.gz) |
| 完整架构 | 97 个独立编译单元、19 项结构测试、5 个编译负控、四组纯规则及 GN/Ninja 全通过。 [日志](architecture.log) |
| `test.py editor --backend both` | 八个 ELF 的 283 处位置通过，四角色用例位于 `c0_topology_integration_test.cpp:1900`。 [入口](verify-editor.json.gz) · [位置](editor-locations.json.gz) |

[原始日志包](verification-logs.tar.gz)包含构建、技术检查、执行审计、Mutation
结果、GTest 枚举 XML、sanitizer 参数和运行日志。VS Code GUI 为人工未验收。
