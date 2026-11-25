# AF-2 M7A 验证证据

输入本地及远端 main 为 `7edbc86ff2c2c4154f7b11807558c9bdb23b2247`。Developer 与 Verify 使用独立构建目录。

## Legacy 取证

用 Verify ELF 执行 `run_gtest.py --filter RealSimPrimary.ImsPreferencePreservesProviderReturnAndReadSource`，
设置 `SIM_HOST_BOOT_SLOT_COUNT=2` 和 `SIM028_INIT_MODEL=single-active`，实际 1/1 PASS。
原保存透传 0、6、−23，外部直接改 Provider 后读到 33，查询失败、无记录或显式 UNKNOWN
均通过 0 与 −1 的双重断言。4 次保存、6 次查询及 0 次通知覆盖已绑定目标。单激活的空槽另有
读写各一次，谓词实测为空 ICCID，保存返回 0，查询返回 `(0, -1)`。
[报告](legacy-characteristic.json.gz) · [GTest XML](legacy-characteristic.xml.gz) · [原始日志](legacy-characteristic.log.gz)

## Target 实现与独立决策项

Developer 实跑 M1～M7A 51/51。已绑定 IMS 用例的 LLDB 单步记录账户与 Binding、
`imsPreference` 版本、原读写、同步整数和模块销毁。空绑定案例单独执行，保留真实
差异。M7A 空绑定和 M6 未知 ICCID 标签仍待分别裁决。
[Developer](developer.json.gz) · [LLDB 命令](ims-preference.lldb) · [LLDB 日志](ims-preference-lldb.log.gz) ·
[Target 空绑定](target-empty-binding.xml.gz) · [决策项](compatibility-pending.json)

## 本轮完整验证

验证输入对应实现提交 `d975637cfbbca3fe05414607b487ea902de862bd`。以下是本轮实跑结果。

| 入口 | 结果 |
|---|---|
| `test.py all --backend legacy --ci` | 145/145。CI 准入、技术门禁、真实执行审计、测试完整性、设备静态检查、迁移一致性及变异恢复均通过。[Legacy](verify-legacy.json.gz) · [执行审计](legacy-ims-execution.json) |
| `test.py all --backend target` | 153/153。M1～M6 44/44，M7A 7/7，正式 H/C/T/I、字段版本及原效果执行审计通过。[Target](verify-target.json.gz) · [执行审计](target-ims-execution.json) |
| `test.py run --backend both --init-model dual-active-primary0` | 11 个有共同契约的差分场景 22/22，包括 IMS 已绑定场景。Oracle 和两端差异均为 0。[双后端](verify-both.json.gz) · [Oracle 差分](verify-differential.json) |
| 独立兼容取证 | 空绑定读写差异在 Legacy 单激活和 Verify Target 独立验证。Legacy 返回 `0` 和 `(0, -1)` 并触发各一次 Provider 操作。Target 返回 `-1` 和 `(-1, -1)`，Provider 调用为零。[Legacy 单激活](verify-single.json.gz) · [Target XML](target-empty-binding-verify.xml.gz) · [待裁决项](compatibility-pending.json) |
| ASan / TSan | 分别重建现有独立 sanitizer GN 目标，按 M1～M7A 过滤，各 51/51。ASan 开启泄漏检测。[ASan](asan.json.gz) · [TSan](tsan.json.gz) |
| 完整架构 / GN / Ninja | 21 项结构检查、99 个独立 C++ 单元及组合头、5 个编译负控、4 组纯规则和完整 `gn gen --check`、Ninja 均通过。[架构](architecture.log) |
| `test.py editor --backend both` | 8 个 ELF、298 处源码定位。组合用例位于 `c0_topology_integration_test.cpp:2483`。[入口](verify-editor.json.gz) · [位置](editor-locations.json.gz) |

[原始日志包](verification-logs.tar.gz)含构建、GTest XML、测试运行、技术验证、变异恢复及
sanitizer 配置。[Git 恢复核验](recovery.json)确认历史检查点与发布实现可达。首轮特征
编译因测试桩空游标缺少构造参数失败，修正构造后已重跑通过。[首轮编译日志](legacy-characteristic-initial-build.log.gz)

本轮状态为 PARTIAL，范围限于尚待裁决的空绑定 IMS 行为。M6 未知 ICCID 标签保持独立产品待决。
VS Code GUI 人工未验收。
