# AF-2 M4 独立 Data Host 原始证据

输入 main 为 `bced9e9f3df22bcf54f5d017dea04d34cdec8543`。Developer 与
Verify 使用独立输出。历史 M1–M3 结果只用于基线比较。

| 阶段 | 命令与实际证据 |
| --- | --- |
| Developer 专项 | `test.py run --backend target --debug-build --filter 'AF2Data.*:AF2Primary.*:AF2Discovery.*:AF2Module.*'`，30/30，通过。 [原始结果](developer-result.json.gz) |
| 架构、迁移和 GN/Ninja | `run_architecture.py --with-gn --migration --output <仓外路径>`，97 独立编译单元、18 结构测试、5 编译负控、四组纯规则及全部 GN 对象通过。 [原始日志](architecture.log) |
| LLDB | Developer ELF 执行 `lldb -b <ELF> -s data-step.lldb`，实际命中资格、原票、写权、参数、投影、完成和析构断点，单步并运行单例至退出 0。 [脚本](data-step.lldb) · [原始日志](data-step.log.gz) |

阶段进展覆盖正式入口、Data 成功路径、绑定撤销、共享键冲突及关闭核对。Verify、
差分、Legacy 契约特征测试和 sanitizer 的最终结果随验收登记。
