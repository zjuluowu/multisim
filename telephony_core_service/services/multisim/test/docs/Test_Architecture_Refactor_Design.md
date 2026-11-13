# 测试工程目标与迁移设计

## 范围与目录

仅测试工程组织、路径和构建引用；不修改生产C++、业务期望、Golden、缺陷判断及统计。逐文件完整映射由 Test_Architecture_Migration.json 维护，保留输入/输出SHA及原因。

```
test/
  README.md
  cases/
    compatibility/        # 生命周期/交错/独立缺陷，场景与Oracle、原runner相邻
    primary_slot/         # 原七例/入口/生命周期，场景、Golden和业务Oracle在本地
    host/                 # 容量、bootstrap、G2a及专属probe
    infrastructure/       # 公共设施自测
    architecture/         # 规则、负控和来源约束
  framework/
    compatibility/        # contracts、fake、Legacy/Target adapters（继续隔离）
    host/                 # 独立Host GN根、Stub、锁、源码表
  scripts/                # 全部活动编排、环境准备、技术脚本；test.py统一入口
  docs/
    archive/              # 历史证据、Linux编译实验及原索引，逐字保留
```

Primary专属fixture、provider及安全接缝随场景保存；不强行抽公共domain层。compatibility的Scenario与Oracle仍用子目录维持原include命名，与测试实现同一场景目录；共享端口/Fake不含业务预期。架构检查作为用例类别，独立GN入口保留在cases/architecture/gn，不并入Host目标。

## 依赖和构建

cases→framework，框架不include具体case。脚本调度case与framework，GN显式列举sources；Host独立root允许使用绝对测试输入路径，保持原逻辑targets/deps/configs和Feature边界。生产multisim BUILD只替换architecture的visibility标签路径，不修改业务源码或扩大可见范围。纯规则独立构建不依赖GTest。公共端口与适配器继续按legacy/multisim隔离。

scripts内统一路径模块定位仓根及活动位置；历史路径通过完整映射解析，旧索引不改写。迁移检查采用原始SHA与明确路径差异对照，不能宽泛忽略内容变化。普通用例只用稳定contracts/Fake，增加业务测试不要求了解GN底层调度。

## 数据与证据

Golden及Scenario/Oracle与用例放一起，内容不变；semantic_baseline、冻结输入锁及历史manifest原文保留，使用迁移映射查当前路径。运行输出统一要求仓外专属目录；日志/XML不作为新测试输入。旧证据包、ZIP/PATCH、失败记录和Linux实验整树保留docs/archive，内部历史路径按原提交解释。历史摘要异常原样报告；新增全树SHA用于证明本批未损坏，不伪称修复旧包。

## 入口

python3 -B test/scripts/test.py --suite all --output /外部目录 [--gtest-root 固定依赖]

suite包括architecture、host、compat、defect、pin、g2a、primary、target、technical、integrity；all逐项执行，即使某套件FAIL也继续，保存每条命令、退出码和原始报告路径。suite结果保留原runner口径，不能把技术PASS当业务PASS，不能以已知缺陷豁免让all变绿。primary真实链仍需固定OpenSSL/native runtime，缺依赖应BLOCKED。脚本只转调现有执行机制，业务断言及分类器留原实现。

## 迁移次序与风险

审计/设计→旧入口实际基线与全输入SHA→逐文件迁址→路径引用同步→新入口逐套复验→内容/依赖/Golden/证据校验→唯一执行状态。源码区.git只读且本批禁止提交：git mv会写index，因此用文件系统rename保留字节，并提供完整映射供后续Git识别，不暂存。

风险：父目录层数假设、动态拼接路径、源码表负控字符串、旧冻结路径键、GN visibility、历史证据内旧引用、VS Code入口。分别用统一根定位、明确映射、原负控、原Golden键保留、新旧GN验证、当前导航更新验证。无法执行产品hb/原生设备、未实现Target以及原五缺陷保持BLOCKED/FAIL；不能在本批修复业务。

## 旧目录到目标目录映射

完整943项逐文件表（含原位文件、原/现SHA和工程修改理由）见 [Migration](Test_Architecture_Migration.json)。以下是维护时使用的规则，例外均在逐文件表展开。

| 原位置 | 目标 |
|---|---|
| systemtest/.gn、BUILD.gn、gn、stubs及活动构建配置 | framework/host；目标名及deps保留 |
| systemtest/tests及support | cases/host及其support；平台设施自测归cases/infrastructure |
| systemtest/primary_slot | cases/primary_slot（包含专属Fixture/provider/lifecycle/Golden）；Python工具归scripts |
| systemtest/scripts、tools | scripts；Python测试归cases/architecture或infrastructure，业务比较器归cases/host |
| systemtest/docs、README、HOST | docs/host、Host_Overview.md、Host_Usage.md |
| systemtest/configs日期证据 | docs/archive/systemtest/configs；共享契约锁、冻结清单/快照归cases/primary_slot/baseline；设施登记及CI策略随对应case |
| sim_guard/contracts、fakes、adapters | framework/compatibility；Legacy/Target隔离不变 |
| sim_guard的Primary Scenario/Oracle | cases/primary_slot；相应使用文档提取Shared_Contract.md随用例保存 |
| sim_guard其他场景、Oracle、测试及traceability | cases/compatibility；执行runner归scripts |
| multisim_architecture_rules | cases/architecture；run.py归scripts/run_architecture.py；历史evidence归docs/archive |
| linux_sim_guard | docs/archive/linux_sim_guard，整树逐字保留，作为历史编译实验 |
| 根check_relocation及两份来源JSON | scripts/check_relocation.py；原始JSON保留docs/archive/relocation |

旧证据内部路径不改；静态脚本使用当前路径，少量动态冻结键由scripts/_paths.py经同一映射解析。没有目录别名、符号链接或重复业务实现。CLI入口与独立构建入口分工明确，统一入口不重写业务Oracle。
