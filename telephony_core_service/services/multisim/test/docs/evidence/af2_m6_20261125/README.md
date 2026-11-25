# AF-2 M6 验证证据

阶段一在 `43881d57134e0d1004eed66333154281eed331c5` 基线上编译现有 Verify Legacy，并按双激活模型执行 `RealSimPrimary.AccountMetadataUsesLegacyPredicatesAndFieldCaches`，1/1 PASS。实际观察为账户 ID 匹配、最终空名称、标签 3、7 次 Provider 字段写入、5 次账户通知。Legacy 生产源码未修改。

- [验证编排及结果](legacy-metadata-result.json.gz)
- [原始 GTest XML](legacy-metadata.xml.gz)
- [原始 GTest 日志](legacy-metadata.log.gz)
