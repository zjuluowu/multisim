# SIM Host 开发效率验证原始记录

输入基线 `a3f94033cb66202f22ae2393b9611b58de51e1f1`。机器为 Linux x86_64 WSL，源码及默认输出在 Windows E 盘 9p/DrvFS。首次定点验证另使用 WSL ext4 的 Target 输出，GN 参数及 ELF 身份随记录保留。

| 记录 | 方法 |
| --- | --- |
| [旧 Legacy 入口构建阶段](baseline-legacy-warmup-tools.jsonl) | 在 `.host-out` 已存在但部分产物未更新的情况下，运行 `test.py run --backend legacy --filter RealSimPrimary.C0CoreToRealManagerAndController`。计时外壳记录 GN 和 Ninja 命令、耗时和退出值 |
| [Legacy 定点构建与运行](first-legacy-route.json) | `test.py run --filter RealSimPrimary.C0CoreToRealManagerAndController --out .host-out`，由源码及枚举定位真实 Legacy 切片 |
| [Target 定点构建与运行](first-target-route.json) | `test.py run --filter AF2Primary.LiveDualSimSetPrimaryThroughLegacyEntry`，由源码及枚举定位正式 Target |
| [生命周期计时](legacy-lifecycle-timings.json) | 两种模型各 10 次，启动真实 Primary ELF，按日志接收时刻计量清理前、清理后、GTest 完成及进程退出 |
| [单激活首轮日志](legacy-lifecycle-single-active.log)、[双激活首轮日志](legacy-lifecycle-dual-active-primary0.log) | 保留队列 pending/running、next_ms、Manager 清理和 GTest 结果 |
