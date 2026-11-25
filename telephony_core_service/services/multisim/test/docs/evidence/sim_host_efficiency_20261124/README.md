# SIM Host 开发效率验证原始记录

输入基线 `a3f94033cb66202f22ae2393b9611b58de51e1f1`。机器为 Linux x86_64 WSL，源码及默认输出在 Windows E 盘 9p/DrvFS。首次定点验证另使用 WSL ext4 的 Target 输出，GN 参数及 ELF 身份随记录保留。

| 记录 | 方法 |
| --- | --- |
| [旧 Legacy 入口构建阶段](baseline-legacy-warmup-tools.jsonl) | 在 `.host-out` 已存在但部分产物未更新的情况下，运行 `test.py run --backend legacy --filter RealSimPrimary.C0CoreToRealManagerAndController`。计时外壳记录 GN 和 Ninja 命令、耗时和退出值 |
| [Legacy 定点构建与运行](first-legacy-route.json) | `test.py run --filter RealSimPrimary.C0CoreToRealManagerAndController --out .host-out`，由源码及枚举定位真实 Legacy 切片 |
| [Target 定点构建与运行](first-target-route.json) | `test.py run --filter AF2Primary.LiveDualSimSetPrimaryThroughLegacyEntry`，由源码及枚举定位正式 Target |
| [生命周期计时](legacy-lifecycle-timings.json) | 两种模型各 10 次，启动真实 Primary ELF，按日志接收时刻计量清理前、清理后、GTest 完成及进程退出 |
| [单激活首轮日志](legacy-lifecycle-single-active.log)、[双激活首轮日志](legacy-lifecycle-dual-active-primary0.log) | 保留队列 pending/running、next_ms、Manager 清理和 GTest 结果 |
| [LLDB 计时](debug-timings.json)、[关闭外部查找核对](lldb-nofetch.log) | LLDB 18，沿用相同的 Target 无优化 ELF。批处理退出包含断点、调用栈及单例退出验证 |
| [Legacy 双激活单步](legacy-local-step.log)、[正式 Target 主卡单步](target-local-step.log) | 保留命中帧、源码步骤、参数或原操作证据、单例退出 |
| [双激活失败样本](lifecycle-dual-active-primary0-4.log) | 热路径采样 #4，保留额外 Radio 发令、3 秒清理屏障和残留引用失败原文 |
| [基准取样](bench-samples.json) | 暖机、10 次 CLI、10 次真实 ELF 直接运行及前置任务等价调用。记录每轮 GN/Ninja 命令、编译链接计数、发现和进程阶段。原始 `before/legacy` 与 `before-ext4/legacy` 含跨脚本更新轮次，Legacy 配对统计使用 `replay-ext4/legacy`。`task-ext4/legacy` 第 8 轮包含受控 Controller 编译，单独保留 |
| [完整门禁结果](quality-gates.json)、[Legacy 架构](legacy-ci-architecture.log)、[Target 架构](target-verify-architecture.log) | `test.py all --backend legacy --ci`、`test.py all --backend target`、两轮六场景差分、专项目标及技术和完整性 |
| [依赖负控](dependency-controls.json) | 已验证树的同 mtime 内容改动导致本地快速构建在 Ninja 前退出。缓存损坏时回到内容校验 |
| [增量正负控](incremental-controls.json) | 临时改动、相同构建配置并发、缺 ELF 恢复、TEST_F 改名、源码行定位、错误后端和编译失败 |
| [八个 ELF 的枚举](editor-xml/) | `test.py editor --backend both` 构建同一输出下的八个业务程序，并执行真实 GoogleTest XML 发现 |
| [首次 Target 单步构建](target-step-through-first-build.json) | 独立 `--debug-build --target sim_test_target_host` 的初次 GN/Ninja 初始化 |

LLDB 与生命周期文本日志仅规范化终端颜色、缩进和行尾。阶段报告保留原命令、退出值、文件身份、配置及已接受的业务结果。C++ 验证改动均已恢复原字节。
