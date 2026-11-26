# AF-2 三项兼容裁决同步证据

2026-11-26 专项以本地及远端 main `cd0330c51aafb9c9697ff560c9008268a5a98039` 为输入，
对提交于 `48532820eb61653cdf3dc1b2349843d9990bbb19` 的裁决同步改动执行。各 Host
报告在命令启动时记录 Git HEAD，构建从包含本轮测试改动的工作区读取源码。
[正式裁决](../../../../../../docs/AF2_Compatibility_Decisions_Approved_20261126.md)

| 验证 | 本轮结果 |
| --- | --- |
| Legacy 单激活专项 | 4/4。CD-01 直接实测了未知 ICCID 对 Provider 的写入谓词、原成功返回及已知账户不变。CD-02 重跑单激活空槽读写断言。[Legacy 报告](legacy.json.gz) · [CD-01 XML](cd01-legacy.xml.gz) |
| Target 专项 | 22/22，含 M6 6 项、M7A 7 项、M7B 9 项。CD-03 新增同一 Binding 下的旧字段版本负控。[Target 报告](target.json.gz) · [CD-03 XML](cd03-target.xml.gz) |
| 共同契约 | 12 个双后端场景 24/24，Legacy/Target 差异与 Oracle 差异均为 0。裁决差异使用各自专项契约。[双后端报告](both.json.gz) · [差分](differential.json) |
| 技术与测试契约 | 框架 119/119，源码表 5/5。独立矩阵检查决策编号、执行状态链接、实际 GTest 注册及共同 Scenario 分类。[技术日志](technical-framework.log.gz) · [源码表日志](technical-source-tables.log) |
| 架构与迁移 | 22 项架构测试、101 个独立 C++ 单元及组合头、5 个编译负控、4 组纯规则、GN 与 Ninja。目录方案及 AF2-08 历史和当前摘要均维持原值。[架构日志](architecture.log) |
| 链接与历史完整性 | 检查本轮 Markdown 新增或改动的本地路径、锚点及源码行号。历史资产完整性通过，保留 317 份历史原件、299 项去向、8 项不可变合同或引用。[检查结果](consistency.log) |

历史 M6、M7A、M7B 证据、共同 Scenario 映射、Oracle、生产源码及装配未变。本轮仅重跑下列
专项和检查。既有全量 Host、ASan/TSan 和 LLDB 结果仍引用各里程碑历史记录。

## 执行命令

在仓库根目录执行。Host 使用已有 Verify 输出，报告与构建目录独立。

```bash
T=telephony_core_service/services/multisim/test
O=/tmp/af2-cd-20261126
V=/tmp/af2-m4-verify
python3 -B "$T/scripts/test.py" run --backend legacy --out "$V" --output "$O/legacy" --jobs 3 \
  --init-model single-active \
  --filter 'RealSimPrimary.AccountMetadataUsesLegacyPredicatesAndFieldCaches:RealSimPrimary.UnknownIccidLabelCanSucceedWithoutChangingAccount:RealSimPrimary.ImsPreferencePreservesProviderReturnAndReadSource:RealSimPrimary.ShowNumberReadsAccountAfterMsisdnFailure'
python3 -B "$T/scripts/test.py" run --backend target --out "$V" --output "$O/target-final" --jobs 3 \
  --filter 'AF2AccountMetadata.*:AF2ImsPreference.*:AF2ShowNumber.*'
python3 -B "$T/scripts/test.py" run --backend both --out "$V" --output "$O/both" --jobs 3 \
  --init-model dual-active-primary0
python3 -B "$T/scripts/run_architecture.py" --with-gn --migration --output "$O/architecture"
python3 -B "$T/scripts/technical.py" --output "$O/technical"
python3 -B "$T/scripts/check_test_integrity.py"
git diff --check cd0330c51
```
