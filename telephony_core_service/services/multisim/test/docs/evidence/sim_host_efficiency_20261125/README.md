# SIM Host 收尾专项原始证据

输入 HEAD 为 `920526203119c1108507cf7bbf457470a0c5e4cb`，优化前基线为
`a3f94033cb66202f22ae2393b9611b58de51e1f1`。两个提交在
`primary_environment.h`、`primary_api.cpp`、`multi_sim_controller.cpp`、
`radio_protocol_controller.cpp` 之间没有业务或夹具差异；本轮只修复 Host
运行时对生产 `TelLifecycle` owned thread 的观察边界。

| 证据 | 内容 |
| --- | --- |
| [fixed control](legacy-fixed-control.log) | 当前 owned-thread 屏障；双激活 C0 通过，Radio 发令为零 |
| [old barrier control](legacy-old-barrier-gated.log) | 测试门控保留初始化 owned thread，恢复旧屏障视图；确定性复现意外 Radio、81 秒未完成通信与清理失败 |
| [repeat controls](legacy-repeat-controls.json) | 修复后空闲及 8 CPU 饱和各 20 次 |
| [I/O controls](legacy-controlled-io.json) | 两个 DrvFS 顺序读取负载下 20 次 |
| [Developer increments](developer-cpp-repeat.json) | ext4 输出、源码仍在 `/mnt/e` 的普通生产 CPP 增量 |
| [Developer first build](developer-first-build.json) | `--debug-build`、`-O0`、无 LTO 的独立首次构建 |
| [Verify first build](verify-first-build.json) | 原正式 `-O1 + LTO` 配置的独立首次构建 |
| [ASan](asan-result.json)、[TSan](tsan-result.json) | M1/M2/M3 22/22 独立进程复验 |
| [quality closeout](quality-closeout.json) | Target、Legacy、六场景 Oracle 及 GUI 边界汇总 |

旧屏障负控使用 `SIM028_HOLD_OWNED_THREADS=1` 与
`SIM028_OLD_OWNED_THREAD_WAIT=1`。前者只在 Host `ffrt::thread` 边界门控已提交
的生产 callable，后者只重建修复前“只观察 pthread-detach，不观察
TelLifecycle owned thread”的准入视图。门控不返回业务结果，也不注入 Radio
状态；正常回归、Verify 和 sanitizer 构建不设置这两个变量。

负控完成记录为 81.018 秒、4 次非预期 `SetRadioProtocol` 调用、`REAL_MANAGER_STOP_FAILED`、
`REAL_GRAPH_LIFETIME_NOT_DRAINED` 和 `REAL_RIL_REFERENCES_NOT_DRAINED`。
它与 `20261124/lifecycle-dual-active-primary0-4.log` 同源匹配，并确认清理失败是
未完成 Radio 通信持有生产图的下游结果，不是独立资源泄漏。

Developer 首次构建位于 WSL ext4 输出，源码仍从 `/mnt/e` 读取：28 次 CXX 与
1 次 LINK，Ninja 59.358 秒。正式 Verify 首次构建 Ninja 101.816 秒。普通
`assignment_service.cpp` 的 Developer 三次独立增量分别为 15.584、13.826、
12.166 秒；先前一次冷缓存样本为 4.537 秒构建加 0.353 秒执行。正式 Verify
同一改动为 36.917 秒 Ninja、37.717 秒构建。性能波动在单 CPP 与最终 LTO/链接
阶段，Ninja 无锁等待；源码与对象仍跨 9p/DrvFS，因此 10 秒目标未稳定达到。

Windows GUI 未再次抢占焦点。本目录不声称 GUI Run、Debug 或测试树刷新通过；
这些仍按 [Host 使用说明](../../Host_Usage.md) 的人工验收边界执行。
