# AF-2 M4 独立 Data Host 原始证据

输入 main 为 `bced9e9f3df22bcf54f5d017dea04d34cdec8543`。Developer 与
Verify 使用独立输出。最终执行的生产代码和测试脚本可从 `5f9cae36b` 核对，
历史 M1–M3 结果只用于基线比较。命令从仓库根调用统一脚本。

| 阶段 | 命令与实际证据 |
| --- | --- |
| Developer 专项 | `test.py run --backend target --debug-build --filter 'AF2Data.*:AF2Primary.*:AF2Discovery.*:AF2Module.*'`，30/30，通过。 [原始结果](developer-result.json.gz) |
| 架构、迁移和 GN/Ninja | `run_architecture.py --with-gn --migration --output <仓外路径>`，97 独立编译单元、18 结构测试、5 编译负控、四组纯规则及全部 GN 对象通过。 [原始日志](architecture.log) |
| LLDB | Developer ELF 执行 `lldb -b <ELF> -s data-step.lldb`，实际命中资格、原票、写权、参数、投影、完成和析构断点，单步并运行单例至退出 0。 [脚本](data-step.lldb) · [原始日志](data-step.log.gz) |
| Legacy 契约 | Verify Legacy ELF 用 `run_gtest.py --filter RealSimPrimary.IndependentDefaultDataUsesOriginalManagerContract`，固定两实体槽，分别运行 `single-active` 和 `dual-active-primary0`，各 1/1。 [单激活](legacy-contract-single.json.gz) · [双激活](legacy-contract-dual.json.gz) · [双激活日志](legacy-contract-dual.log.gz) |
| Legacy 真实执行 | 新用例独立 profile，含真实 CMI、Manager、Controller、CES、诊断和账户通知方法。 [执行审计](legacy-data-execution.json) |
| 开发期独立 Data 差分 | 双激活 Legacy Verify 观测与 Target Developer 观测经相同 Data Oracle，差异 0。 [对照](data-development-differential.json) |
| Verify Legacy CI | `test.py all --backend legacy --ci`，141/141，准入及全部适用门禁通过。 [原始报告](verify-legacy.json.gz) |
| Verify Target | `test.py all --backend target`，132/132，含 M1–M3 22 项和 M4 8 项。 [原始报告](verify-target.json.gz) · [Data 正执行审计](target-data-execution.json) |
| Verify Both | `test.py run --backend both --init-model dual-active-primary0`，原六场景和新增独立 Data 场景共 14/14。两端与 Oracle 差异均为 0。 [原始报告](verify-both.json.gz) · [差分](verify-differential.json) |
| ASan / TSan | 各在独立 Verify GN 输出设置 `host_asan = true` 或 `host_tsan = true`，用 `ninja -j 1 sim_test_target_host` 后向 `run_gtest.py` 传同一 30 例过滤。ASan 开启泄漏检测。各 30/30。 [ASan](asan.json.gz) · [TSan](tsan.json.gz) |
| 编辑器枚举 | `test.py editor --backend both`，8 个 ELF，273 个源码位置，M4 代表用例定位到 `c0_topology_integration_test.cpp:1578`。 [入口报告](verify-editor.json.gz) · [源码位置](editor-locations.json.gz) |

Verify 构建目录和 Developer 输出隔离。全部命令报告、覆盖审计输出、编辑器 XML 与
两轮初始编译和 LLDB 诊断见[原始日志包](verification-logs.tar.gz)。初始架构脚本
拒绝把 M4 的活动文件当作 M3 历史输入。随后登记 M4 精确源码、重放同一历史比较，
增加当前摘要漂移负控，最终 GN 和迁移门禁均通过。

首轮 Legacy 特征用例将 2 当作内部越界槽。真实 Manager 在夹具中包含第三个内部槽位，
Setter 接纳了 2 并公告缓存身份 0。越界测试改用 100，仍测试合法无账户槽的原行为。
[首轮运行原始日志](legacy-slot-assumption-failed.log.gz)

VS Code GUI 保持人工未验收。范围为 Target Host，产品和设备的门禁遵循唯一执行状态。
