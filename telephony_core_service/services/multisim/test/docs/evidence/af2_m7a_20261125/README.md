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
