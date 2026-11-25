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
