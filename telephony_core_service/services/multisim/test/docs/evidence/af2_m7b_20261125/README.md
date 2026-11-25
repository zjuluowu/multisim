# AF-2 M7B 验证证据

输入本地及远端 main 为 `cd04fbad61941d8584fd8884457910e9447ac8fe`。
Developer 与 Verify 使用独立构建输出。首次贯通用例和后续原效果回归分别执行。

Developer 首次贯通 5/5，随后完整专项 59/59，含 M7B 8/8。真实 LLDB 组合用例走过
兼容受理、原会话、fence、Provider 写、字段接纳、回调及析构，进程退出 0。
[Developer 报告](developer.json.gz) · [LLDB 命令](show-number.lldb) · [原始日志](show-number-lldb.log.gz)

Legacy Verify ELF 的 `RealSimPrimary.ShowNumberReadsAccountAfterMsisdnFailure` 在双激活模型
实际 1/1。原 MSISDN EF 发令一次，错误后没有 Provider 更新，真实查询读取已有账户号码
`555123`，原结果为 0。这个观察用于独立的新场景和 Oracle。初次链接暴露旧 Host 没有 IMS
外部符号。补充「IMS 服务缺席」的 Host 边界后重跑通过。
[原报告](legacy-number.json.gz) · [XML](legacy-number.xml.gz) · [原日志](legacy-number.log.gz) ·
[首轮链接](legacy-initial-link.log.gz) · [兼容待决](compatibility-pending.json)

Legacy 执行审计首轮漏认 `SimFileManager::GetSimTelephoneNumber[abi:cxx11]`。
在原覆盖产物中核对到该生产符号有实际执行。审计匹配补充准确的 C++ ABI 标签后，
同一 Verify ELF 及 Profile 的复审通过，保留原方法正向覆盖要求。
[首轮审计](legacy-initial-audit.json) · [修正复审](legacy-corrected-audit.json)

## 本轮完整实测

Target 源码发布于 `6afce256e`。Legacy 特征测试及 Oracle 发布于 `5fbe89d08`。
修正的执行审计发布于 `a3ac857d9`。以下为实际重跑。

| 入口 | 结果 |
|---|---|
| `test.py all --backend legacy --ci` | 146/146，执行审计、CI、技术、兼容探测、完整性、迁移一致性及变异恢复通过。[Legacy](verify-legacy.json.gz) · [执行审计](legacy-number-execution.json) |
| `test.py all --backend target` | 161/161，M1～M7A 51/51，M7B 8/8。号码 H/T/R/I/C 的真实目标方法正向执行通过。[Target](verify-target.json.gz) · [执行审计](target-number-execution.json) |
| `test.py run --backend both --init-model dual-active-primary0` | 12 个约定场景 24/24。Oracle 和 Legacy/Target 差异均为零。新增场景覆盖原 EF 失败后账户读源。[双后端](verify-both.json.gz) · [差分](verify-differential.json) |
| ASan / TSan | 分别重建 sanitizer GN 目标，M1～M7B 各 59/59。ASan 开启泄漏检测。[ASan](asan.json.gz) · [TSan](tsan.json.gz) |
| 架构 / GN / Ninja | 22 项结构测试、101 个独立 C++ 单元及组合头、5 个编译负控、4 组纯规则、`gn gen --check` 和 Ninja 通过。[架构](architecture.log) |
| `test.py editor --backend both` | 8 个 ELF、307 处源码定位。组合用例位于 `c0_topology_integration_test.cpp:2877`。[入口](verify-editor.json.gz) · [位置](editor-locations.json.gz) |

[原始日志包](verification-logs.tar.gz)包含构建、业务 GTest、执行审计、技术检查和变异恢复。
额外尝试 Legacy Developer 无 LTO 链接时，测试框架的 Parcel、OS Account 等原外部符号
仍不能完整链接。正式 Legacy Verify 构建、特征、全量 CI 均通过。Target Developer 及
LLDB 单步使用独立配置。[Legacy Developer 链接日志](legacy-developer-link.log.gz)
· [Git 恢复核验](recovery.json)

VS Code GUI 人工未验收。G3b 行为裁决和原生 MSISDN/Provider 取证继续待定。
