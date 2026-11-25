# AF-2 M6 验证证据

阶段一在 `43881d57134e0d1004eed66333154281eed331c5` 基线上编译现有 Verify Legacy，并按双激活模型执行 `RealSimPrimary.AccountMetadataUsesLegacyPredicatesAndFieldCaches`，1/1 PASS。实际观察为账户 ID 匹配、最终空名称、标签 3、7 次 Provider 字段写入、5 次账户通知。Legacy 生产源码未修改。

- [验证编排及结果](legacy-metadata-result.json.gz)
- [原始 GTest XML](legacy-metadata.xml.gz)
- [原始 GTest 日志](legacy-metadata.log.gz)

阶段二交付正式 Target 账户资料入口。Developer M1–M6 44/44。LLDB 实际停在正式 H/C、账户和绑定、原字段命令、字段版本、同步通知及模块销毁位置。Verify 的 10 个双后端场景 20/20，Oracle 与双后端差异均为 0。

- [Developer 专项](developer.json.gz)
- [LLDB 命令](account-metadata.lldb) · [原始单步日志](account-metadata-lldb.log.gz)
- [Verify 差分](verify-differential.json)
