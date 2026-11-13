# 多 SIM 测试防护网

测试行为、Golden和既有缺陷保持原义。按职责查找文件：

| 位置 | 增加或维护什么 |
|---|---|
| [cases/compatibility](cases/compatibility/README.md) | 卡状态、PIN/PUK、异步交错、生命周期及5条独立缺陷；场景、Oracle和测试实现就近 |
| [cases/primary_slot](cases/primary_slot/README.md) | 主卡入口、真实七场景、条件性Legacy Golden、业务Oracle及专属Fixture |
| cases/host | 容量、bootstrap、G2a与专属probe |
| cases/infrastructure | 平台设施自测及Python分类器/CI策略自测 |
| [cases/architecture](cases/architecture/README.md) | 依赖边界、纯规则、声明负控及迁移约束；保留独立GN入口 |
| framework/compatibility | 公共端口、RIL Fake、Legacy及Target Adapter；不放业务预期 |
| framework/host | Host GN根、Stub、固定依赖锁及构建环境 |
| scripts | 统一调度、构建/准备、执行及证据采集 |
| docs | [现状审计](docs/Test_Architecture_Current_Analysis.md)、[目标设计](docs/Test_Architecture_Refactor_Design.md)、[使用说明](docs/Host_Usage.md)、[本次验收](docs/Test_Architecture_Validation.md) |
| docs/archive | 历史日志/XML/ZIP/PATCH与Linux编译实验；保留原文和旧索引 |

从Git根执行，输出选择仓外专属目录：

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" --suite all --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT"
python3 -B "$TEST/scripts/test.py" --suite compat --mutation-check --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT"
python3 -B "$TEST/scripts/test.py" --suite pin --mutation-check --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT"
python3 -B "$TEST/scripts/test.py" --suite architecture --output "$SIM_TEST_OUTPUT"
```

`--suite`还支持`host / defect / g2a / primary / target / technical / integrity`。`all`包含Host完整64项、G2a、技术与架构检查、完整性、Primary预检和Target；追加`--mutation-check`执行兼容及PIN突变。Host已包含兼容/缺陷/PIN，不将重复执行累加成覆盖数量。真实Primary还需`--openssl-root`及`--runtime-root`，按固定依赖[说明](docs/Host_Usage.md)准备。

每次调用建立独立运行目录，`result.json`记录命令、原退出码和日志；子runner原报告/XML保持权威。退出0表示所选命令全部通过，1保留FAIL，2表示所选命令有BLOCKED且没有FAIL。**完整Host仍有5个既有FAIL，Target仍BLOCKED，all不会全绿。** 技术检查通过不等于业务、生命周期或产品准入。

增加普通业务测试：在相应cases目录添加场景和断言，使用framework中的公开contracts/Fake；在既有GN target显式登记源文件，再按原分类表登记case ID。仅在确有跨场景复用时抽公共能力。用`--suite compat`、`pin`或`primary`执行相关测试；查看原始XML和子runner结果，不放宽断言以适配旧缺陷。

[完整943文件迁移映射](docs/Test_Architecture_Migration.json)保留输入提交、旧/新路径和前后SHA；[冻结基线](docs/Test_Architecture_Baseline.md)记录本次真实结果。Golden、业务Oracle、Fake/Adapter和历史索引均不改预期。旧证据内路径按其固定提交解释，使用映射定位当前文件；`scripts/check_relocation.py`仍检查原219项来源，`scripts/check_test_integrity.py`检查本次全量映射与受控字节。

Linux历史工程没有实际测试executable，归档保留，未宣称新运行验收。未知用途文件没有删除。产品构建、真实Target写业务、设备/eSIM及原生SA/IPC/HDI仍单独验收。
