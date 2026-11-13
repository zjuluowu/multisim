# 测试工程目录重构验收

输入为 main@abd07fd85e87eba0f0883599538548bbcc09d8f2；2026-11-12开始，2026-11-13续办收尾。本批仅在工作树交付，未暂存、提交、推送或合并。开始时唯一已有未跟踪项default.profraw按SHA保护；其他worktree不变。恢复脚本fetch失败，未声称远端ref已刷新；ls-remote核实origin/main与输入同SHA。

## 交付与结构

[审计](Test_Architecture_Current_Analysis.md)、[全量盘点](Test_Architecture_Inventory.json)、[目标设计及映射规则](Test_Architecture_Refactor_Design.md)、[943项逐文件映射](Test_Architecture_Migration.json)、[移动前冻结](Test_Architecture_Baseline.md)构成设计和来源证据；[统一入口](../README.md)提供使用方法。

四个历史工程目录改为cases/framework/scripts/docs。943项原文件全部登记，939项迁址；无业务文件删除。Case包含具体场景、Oracle、Golden和专属Fixture；Framework保留公开端口、Fake、Legacy/Target Adapter、Host Stub及独立GN支持；Scripts统一准备、驱动与结果采集。Python技术测试也归cases。Primary说明从兼容性README提取到Shared_Contract.md，随场景和Oracle放置。

systemtest与sim_guard原本共享一个Host BUILD.gn，现按职责分开，而非强行合并构建目标。Architecture为cases类别，继续独立GN；linux_sim_guard只有编译聚合、没有测试executable，完整归档为历史实验。活动冻结锁/快照、Golden与历史日志/补丁/压缩包分别存放。没有目录别名、符号链接或重复业务实现。

## 本次真实前后对照

套件存在重叠，不相加为覆盖率或总用例数。对照检查原XML中的名字、状态、断言失败文本及原退出码，仅将精确源码迁址位置映回原位置。

| 范围 | 移动前 | 最终移动后 |
|---|---|---|
| Host完整套件 | 64项：59 PASS / 5 FAIL，exit1 | 相同64项、同5条失败及断言，exit1；通过统一入口实际执行 |
| Host分组 | compat17、PIN4、设施23、容量1、主卡入口14通过；缺陷5失败 | 全部分组数量、状态与skipped=0相同 |
| G2a | 7 PASS / 0 FAIL / 0 BLOCKED | 7/7；6函数正执行及源表检查保留 |
| 独立兼容/缺陷runner | 17 PASS / 5 FAIL，exit1 | 相同；未将已知FAIL豁免成全绿 |
| READY→LOCKED突变 | 1项原断言FAIL检出；恢复17/17 PASS | 同样DETECTED；恢复17/17；生产文件未改 |
| PIN记忆 | 4/4 PASS；真实PUK hook突变检出；恢复4/4 | 相同 |
| 架构/GN | 88单元语法、合并头、4组纯规则、15结构负控、5声明编译负控通过 | 相同；25个骨架逻辑target及独立GN对象构建通过 |
| Python技术检查 | 源表14、Oracle15、分类器13、CI策略13通过 | 同数通过；新增迁址处理后CI13再次通过 |
| 原迁移来源检查 | 219项通过 | 219项通过，含test外恢复工具；历史SHA及显式演进未重写 |
| Target | BLOCKED/exit2，差分NOT_RUN | 统一入口仍BLOCKED/exit2，无真实Target业务结果 |
| 真实Primary七场景 | 新输出目录预检BLOCKED；未执行业务 | 同样缺配置的固定OpenSSL/native runtime；不继承历史7/7 |
| 产品/设备 | NOT_RUN/BLOCKED | 未运行匹配OHOS产品hb、原生SA/IPC/HDI、设备/eSIM及新的ASan/TSan |

最终结果见[摘要](archive/test_architecture_20261112/validation.json)与[原始证据索引](archive/test_architecture_20261112/raw-evidence-index.json)。`comparison.json`记录Host、G2a及两类突变的实际前后对照。

## 字节、构建和统计保护

943项输入SHA逐项与固定Git blob核对，再核对实际目标SHA；838项受控业务/证据资产保持原字节。原C++测试、Fake、Adapter、业务Oracle和所有原JSON均不改预期；唯一C++头改动是Primary平台声明头的相对include，精确映射到同一Stub。Legacy分类函数classify与CI统计/准入函数evaluate的AST保持一致，Primary Python Oracle保持原字节。每个原文件均有去向，新增校验拒绝漏项、重复目标或未登记漂移。

Host BUILD对固定Git输入进行完整GN token比较，只允许声明的路径映射及分离框架include根；target、deps、条件、编译与链接策略不变。test外的生产BUILD仅替换架构测试visibility标签，按原字节精确比较；无生产C++、API、目标架构业务设计或契约修改。其他仓外修改为导航、执行状态及VS Code脚本路径。

CI旧失败签名保留原JSON。normalize仅将行首“精确迁址文件名:行号”映回旧位置，不改断言内容。真实前后64项Host XML经原策略分类相同，仍59 PASS/5 FAIL；追加独立断言的负控仍被拒绝。这不等于旧96项候选准入通过。冻结候选Host BUILD的base SHA在本任务输入时就已不同（32bcb884… vs abf5d2d8…）；实际候选预检拒绝旧布局补丁，保留admission=FAIL/native=BLOCKED，没有套用候选生产补丁。

## 原始轨迹的实测差异

22条独立兼容/缺陷观测中21条逐字段相同，PIN4条全部相同。唯一差异是INIT_DUAL_002的两槽初始status请求记录：移动前为status:0;status:1，最终为status:1;status:0。该用例的槽1先响应、中间/最终公开卡状态、请求数量及槽集合一致；原用例没有跨槽发令先后断言。原C++/Fake/Adapter字节相同。

该差异完整保存于adapter-observation-comparison.json，没有改断言、排序原始日志或增加Oracle归一化来隐藏它。本次只证明原有断言/契约与缺陷检出能力保持，不声称完整异步轨迹逐字相同，也不裁决槽间发令顺序为新的业务契约。

## 历史、复现与已知问题

历史sha256索引181项与移动前结果一致：180项匹配；初始化稳定性classifier-tests.log仍为同一既有不匹配。旧索引和文件均保留原字节，不能宣称旧证据全部校验通过。

首次工程验证捕获并修复路径转换的Python引号、GN空字符串条件及probe的support include遗漏；没有改业务断言。并行构建使状态突变的一次normal构建及另一次mutant构建触发原360秒watchdog，原BLOCKED/FAIL、原始XML及runner计数均保留。最终只串行重跑状态突变，保持时限不变并完整完成；不将探索失败混为最终PASS。

所有常规运行产物写在仓外。本次固定证据归档[raw-evidence.tar.gz](archive/test_architecture_20261112/raw-evidence.tar.gz)保存前后命令、日志/XML、coverage/profile、工具与输入/二进制摘要，省略可重建二进制、对象和第三方源码。索引逐成员保存SHA256，包SHA在validation.json；解包后可独立重放对照，原始绝对路径仅为当时溯源信息：

```sh
python3 -B telephony_core_service/services/multisim/test/scripts/compare_layout_results.py \
  --before "$EXTRACTED/before" --after "$EXTRACTED/after" \
  --recorded-root /tmp/sim-test-architecture-20261112 --evidence-root "$EXTRACTED" \
  --output "$EXTRACTED/rechecked-comparison.json"
```

完整运行复现需固定Git历史、framework/host/configs/dependency_lock.json中的验证依赖及源级覆盖工具；不要依赖旧机器的/tmp缓存。旧版本在独立克隆恢复输入提交，不能切换或覆盖当前工作区。历史实验/候选补丁须按其原固定基线复现，归档目录不是当前准入入口。

文档本地链接初查931项，迁址引入的缺失均已修正并逐项复核；另有9条在本任务输入时已失效的旧生产目录历史链接，记录于navigation-check.json，未在本批改写历史源码引用。

剩余条件：五个旧缺陷；真实Target写业务及Adapter；真实Primary依赖、原生产品及生命周期门禁；旧候选与当前源码的差异需另行授权审查。普通开发者可直接在对应cases扩充场景并用稳定contracts/Fake与既有target登记，无需先理解整个Host调度内部。
