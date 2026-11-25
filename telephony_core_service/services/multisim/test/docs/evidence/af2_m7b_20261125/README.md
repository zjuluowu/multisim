# AF-2 M7B 验证证据

输入本地及远端 main 为 `cd04fbad61941d8584fd8884457910e9447ac8fe`。
Developer 与 Verify 使用独立构建输出。首次贯通用例和后续原效果回归分别执行。

Developer 首次贯通 5/5，随后完整专项 59/59，含 M7B 8/8。真实 LLDB 组合用例走过
兼容受理、原会话、fence、Provider 写、字段接纳、回调及析构，进程退出 0。
[Developer 报告](developer.json.gz) · [LLDB 命令](show-number.lldb) · [原始日志](show-number-lldb.log.gz)
