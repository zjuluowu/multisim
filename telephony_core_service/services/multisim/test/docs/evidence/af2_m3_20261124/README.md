# AF-2 M3 C0 宿主验收（2026-11-24）

输入 `2c0145c854600cae9ca6e40e1aa578ec8e443029`，验证代码 `e86d2fcf895abd6293aeb3cf2de9ce1deb1ffd77`。从 `telephony_core_service/services/multisim` 执行下表命令，使用仓外日志目录 `/tmp/af2-m3/`。逐例 JSON 保存测试程序和已核对源码的摘要。同一代码提交的字节与 Test Runner 记录的源码 SHA-256 逐项核对。

| 检查 | 实际命令 | 结果及原始日志 |
| --- | --- | --- |
| 首次兼容入口 | `python3 test/scripts/test.py run --backend target --filter 'AF2Primary.*' --out /tmp/af2-m1/host --output /tmp/af2-m3/entry` | 2/2，退出 0。见 `entry.log`。 |
| 逐键路径增量 | 同一入口，过滤 `'AF2*.*'` 后以 `'AF2Primary.*'` 执行故障注入 | M1/M2/M3 当时定向通过。故障注入版 6/6，退出 0，见 `tracked.log`、`m3.log`。 |
| Target Host 全量 | `python3 test/scripts/test.py run --backend target --out /tmp/af2-m1/host --output /tmp/af2-m3/full-final` | 124/124，失败 0，退出 0。M1 6/6、M2 9/9、M3 7/7，及既有 C0、B、C1、资源、关闭等回归。见 `full-final.log` 及同名原始报告目录。 |
| 双后端及 Oracle | `python3 test/scripts/test.py run --backend both --init-model dual-active-primary0 --filter SIM028-LEGACY-C0-DUAL-PRIMARY0 --target sim_primary_slot_real_test sim_test_target_host --out /tmp/sim-final-host --output /tmp/af2-m3/both-final` | 两个 GTest 2/2，Legacy、Target、Both 差分及 Oracle 均通过，差异 0，退出 0。见 `both-final/differential.json`。 |
| ASan / TSan | 各用 `ninja -C /tmp/af2-m1/<mode> -j 3 sim_test_target_host`，然后执行 `python3 test/scripts/run_gtest.py --binary /tmp/af2-m1/<mode>/sim_test_target_host --output /tmp/af2-m3/<mode>-final --filter 'AF2Primary.*:AF2Discovery.*:AF2Module.*'`。ASan 使用 `ASAN_OPTIONS=detect_leaks=1`。 | 各 22/22，失败 0，退出 0。见 `asan-final.log`、`tsan-final.log`。 |
| 完整架构及 GN/Ninja | `python3 -B test/scripts/run_architecture.py --with-gn --migration --output /tmp/af2-m3/arch-final` | 97 独立 C++ 单元和组合头、165 include、25 个目标、73 原源码、88 历史布局、91 目录输入、17 结构检查、5 编译负控、四组纯规则及 GN 对象构建通过。退出 0。见 `arch-final.log`。 |
| 导航、历史完整性 | `python3 -B test/cases/internal/infrastructure/test_navigation.py`；`python3 -B test/scripts/check_test_integrity.py` | 导航 6/6、317 份历史资产、299 项去向和 8 项不可变合同／参考项通过。退出 0。见 `navigation.log`、`integrity.log`。 |
| LLDB | `lldb -b <ELF> -s /tmp/af2-m3/<script>.lldb`，Legacy 双激活使用 `legacy-dual-step.lldb`，Target 用 `target-step.lldb` | 两个单例各 1/1，LLDB 均实际停住、查看槽并单步，进程及调试器退出 0。原始日志含 Target 四次参数原步、两次角色投影、完成及关闭。 |

Target LLDB 使用已有宿主调试构建目录 `/tmp/af2-m2/lldb-readable`，在该仓外副本以相同 Host GN 参数生成，生成的 Ninja 将 LTO 与对应 whole-program 优化移除并保留 DWARF。用 `ninja -C /tmp/af2-m2/lldb-readable -j 3 sim_test_target_host` 根据当前源码重建。Legacy 使用本轮 Both 的同一实际 Legacy ELF，M0 双激活精确过滤。调试器脚本及 Python 断点观察脚本随原始日志留存。

首轮全量出现两个 B 生命周期断言失败，`CallbacksStillInFlight` 之前后并发封口任务可能已先完成而得到 `DeviceEffectsUnresolved`。用 M2 已归档 SHA-256 的 LLDB 宿主二进制逐例独立进程复现，分别在第 17 次与第 3 次失败。Host 用例改为使用已存在的 Owner 队列暂停/释放控制观察时点，原断言和 B/C1 业务代码保持原语义。相关原始日志为 `m2-flake-final.log` 和 `full.log`。首次 sanitizer 构建命令误用了两个报告目录，Ninja 找不到构建文件并退出 1，使用实际构建目录后成功。默认单激活的 Both 试跑只得到 2 个 GTest 通过，Oracle 明确报告双激活初态未满足。正式结果来自随后显式指定双激活的运行。首次 Legacy LLDB 在已命中主卡保存断点后查询了不存在的局部变量，退出 1。最终脚本用实际源码位置取值并完成测试。

VS Code 图形界面、产品参数原操作供证、真实权限/通知服务、设备及生产切换属于各自的后续验收。正式兼容入口走宿主 testonly 装配。Golden、Oracle、生产写权及 B/C1 产品门禁沿用原输入。

[逐例结果与输入摘要](results.json) · [原始日志与调试脚本](raw-logs.tar.gz) · [重构前后调试](../../M3_Primary_Debug.md)
