# 目录文档护栏修复证据（2026-11-16）

基线为远端 main@74e25901ddec1c1714998e4ed1b40488f418b5fc；原工作区 HEAD 为 abd07fd8，输入受控文件内容与远端一致。before-architecture.log 是独立检出远端基线后的真实失败；architecture.log 是原工作区修复后的完整成功；publication-architecture.log 是逐文件复制到同基线独立发布副本后的完整成功。其余日志保存完整性、来源及技术检查和两个工具演进负控。

历史 source_migration.json、directory_migration_20261112.json 和 Test_Architecture_Migration.json 原字节不变。documentation_migration.json 单独登记旧副本→权威 docs 路径及原摘要；Test_Architecture_Followups_20261116.json 仅登记五项工具/导航的前后摘要，逐项核对固定提交与历史输出。业务/Oracle/Fake/Golden/历史JSON和证据仍保持原验证。

在仓库根复现（输出目录由调用者选择仓外新目录）：

```sh
python3 -B telephony_core_service/services/multisim/test/scripts/run_architecture.py --with-gn --migration --output "$SIM_ARCHITECTURE_OUTPUT"
python3 -B telephony_core_service/services/multisim/test/scripts/test.py --suite integrity --output "$SIM_TEST_OUTPUT"
python3 -B telephony_core_service/services/multisim/test/scripts/test.py --suite technical --output "$SIM_TEST_OUTPUT"
```

需要固定Git历史、Python3、Clang C++17、GN及Ninja；版本见 tools.json，输入摘要见 input-sha256.json。原始绝对路径仅为本次运行溯源，不是恢复依赖。结构测试在仓外副本注入权威文档缺失/漂移、旧副本恢复和未登记文档；原成员/可见性负控继续保留。独立发布副本还注入了业务文件演进条目及未登记工具漂移，均非零拒绝，原字节随后恢复；记录见 followup-negative-controls.json及followup-negative-controls.log.gz。

本批未重跑完整Host业务、G2a七场景、Target差分或产品/设备；结果不替代生产迁移和产品验收。validation.json 是本次验证事实，sha256.json 索引本目录其他文件；发布提交及远端包含关系按Git查询。

原始负控聚合日志含末尾空行，以gzip保存原字节，避免将日志空行作为源码空白错误；解压内容与本次实际输出一致。
