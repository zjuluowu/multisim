# 迁移前冻结基线

在任何原文件移动之前，输入 main@abd07fd85e87eba0f0883599538548bbcc09d8f2 的943个原文件SHA逐项复核相同，暂存区为空；default.profraw保持原SHA。本次不提交、不推送。详细命令/日志/XML将随最终证据包保存；源码可由固定Git提交恢复，不依赖/tmp。

| 套件 | 本次真实结果 | 口径 |
|---|---|---|
| Host | 64项：59 PASS / 5 FAIL，exit1 | compat17、PIN4、设施23、容量1、入口14通过；5个独立缺陷原样失败 |
| G2a | 7 PASS / 0 FAIL / 0 BLOCKED | 真实Core→Query→旧Manager/EXT，6函数正执行 |
| 原迁移检查 | 219文件PASS | 原来源摘要/显式演进 |
| 架构 | GN/88单元语法/4组纯规则PASS | 15项结构负控及5项声明编译负控通过，不证明产品链接 |
| Python技术检查 | 源表14、Oracle15、分类器13、CI策略13全部PASS | 不计为业务场景 |
| 状态突变 | 原套件17 PASS / 5 FAIL；突变1项预期FAIL检出；恢复17/17 PASS | 原套件仍exit1 |
| PIN突变 | 原4/4 PASS；突变1项预期FAIL检出；恢复4/4 PASS | 真实PUK记忆hook操作突变 |
| Target | BLOCKED/exit2；差分NOT_RUN | 真实写业务及Adapter未实现 |
| 真实主卡七例 | BLOCKED，未执行 | 新输出目录预检缺配置的固定OpenSSL/native runtime；不沿用历史7/7为本次结果 |
| 产品/设备 | NOT_RUN/BLOCKED | 无匹配OHOS树/hb、设备及原生SA/IPC/HDI条件 |

上述套件有重叠，不相加为总用例数。原始FAIL未豁免。历史sha256索引181项中180匹配，初始化稳定性classifier-tests.log一项与旧索引不符，原文件与原索引均保留；详见historic-index-check.json。现有Golden、C++断言、Fake/Adapter和历史证据已按全量SHA冻结。
