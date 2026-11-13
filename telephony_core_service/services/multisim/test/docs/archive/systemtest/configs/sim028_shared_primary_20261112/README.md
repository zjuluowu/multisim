# SIM-028 Legacy冻结与共用主卡契约验证

实际HEAD39d63590；本轮未修改生产代码或Golden业务预期，未提交/推送/合并。

- `frozen-baseline.json`：修改前九份关键输入及上一批42份证据SHA；`frozen-inputs.tar.gz`存原始实际字节。`frozen-production.json`保存旧二进制SHA、190份生产文件及依赖锁摘要，均与原50轮样本一致。
- `contract-lock.json`：共用七场景目录摘要；期望来源为冻结Golden和逐项核对的原断言。冻结Golden不被生成器或当前运行改写。
- `validation.json` / `final-result.json`：最终锁定源输入的实际回归。原业务7/7、共用Oracle7/7，六例Strict RIL通过；原完整GTest1 PASS/6生命周期FAIL、raw exit1，共用入口也exit1。
- `final-raw.tar.gz`：全部七例原始JUnit XML/日志/机器结果、实际GN/Ninja命令；最终二进制SHA在结果中。包含真实45秒B超时，不是即时Fake超时。
- `target-readiness.json` / `target-result.json`：真实源码缺口、SHA和Target实际入口BLOCKED/exit2；差分NOT_RUN，未生成Target业务PASS。
- `coverage.json`：七例覆盖矩阵及继承证据等级；完整人工审查、Mock差异、未覆盖内容和差分判定见[防护网README](../../../sim_guard/README.md)。
- `host-regression.tar.gz` / JSON、`known-defects.json`：64条59 PASS/5相同既有FAIL，原60为55/5，入口14/14。缺陷名称和完整失败文本与冻结样本匹配。
- `oracle-tests.log`：15项技术正负控；`measurement-checks.json/log`：实际观测副本的7项拒绝和2项归一化检查，全9项通过，不计为Target执行。`classifier-tests.log`：原13项分类器验证。
- `exploratory-first.tar.gz`、`exploratory-second.tar.gz`：前两版探索回归，各七条原业务及当时Oracle通过，原始生命周期失败保留。首版Oracle/catalog和第二版Runner快照对应当时源码，不混称最终输入。
- `build-first-failed.log`：首次错误使用数据卡getter签名的构建失败；修正为真实无参数公开getter后真实构建通过。恢复检查首次fetch失败及重试成功分别保存。
- `task.patch` / `changes.json`：相对本批实际起始字节的6份修改和7份新增代码/文档；补丁在仓外原输入副本git apply --check通过，不包含生产修改。
- `input-protection.json`：起始文件逐项核对，明确授权源文件变化与生成文件例外。`generated-artifact-change.json`记录原脚本发现用例时覆盖根default.profraw的事实、旧SHA/新SHA及原字节无法恢复；它不是冻结业务证据。已修复为输出目录内discovery profile，最终实际复验根profile不再变化。

当前Legacy驱动仍使用原GTest的技术应答脚本；共用场景、观测格式和Oracle已可执行，不代表通用Target应答驱动已实现。Target只有在真实业务闭包、公开前态和RIL/外部事件绑定具备后才可注册。A厂商、真实消费者、持久化恢复和原生SA/IPC/HDI继续BLOCKED/UNKNOWN。本轮未运行ASan/TSan，也未验证旧析构后迟到回调安全。

执行方式与七例覆盖矩阵见[防护网README](../../../sim_guard/README.md)。本轮所有完整GTest失败和非零退出码保留；业务通过不能替代生命周期/产品准入。
