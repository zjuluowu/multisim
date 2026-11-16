# 原生产代码回退、上游同步及测试适配

输入：main@37be229d4816824c7931a1c2050f13012f8d1061，原暂存区为空，未跟踪default.profraw保留。GitHub镜像master仍为b2ddf49ab0fc；进一步读取GitCode官方主仓master并fetch，实际最新为43ac8ae7df5b9c17c4efb3433415693d925b98c7（2026-11-16）。最终同步以GitCode为准。

同步所有上游原有常规文件，保留本仓AGENTS、docs及重构新增目录；原Core/Binding/GN接入撤回，两个旧生产Binding文件删除。上游1066项原始字节及无生产Binding由独立工具验证；相对旧基线的大量变化仅恢复上游CRLF/空白，另外包含GitCode比GitHub新增的84文件变更。上游的新增接口、Sense能力及业务变更作为原始同步保留，不混入重构实现。归档upstream.json逐文件记录SHA256。

G2a原Core/Binding四文件从输入提交逐字节保存到framework/host/g2a_reference，来源及SHA在docs/Upstream_Sync_Test_Adaptation.json登记。生产GN和上游原测试GN不引用它；Host两个独立程序分别验证原版Core查询和候选参考接入，各7个参数/vendor场景。参考测试不能证明当前生产接入。保留原Manager及EXT源码执行审计；不调用Manager::OnInit/Core::Init，不证明产品或硬件行为。

适配包括：新增ISimManager方法按原生成器补充抛错声明；Parcel只增加两个无实现unsigned声明；上游插入RADIO_ESIM_UPDATE_START/END，精确断言1129/1130，原ENABLED_PROFILE_NUM随上游变为1131。对应源表负控、历史迁移完整性和精确变更范围同步。Golden、业务Oracle、分类器及CI统计逻辑、历史证据均不改写。历史迁移JSON保留原字节，通过本轮独立清单登记演进。

首次新版Host/G2a编译因Parcel声明缺失失败，首次新版架构因旧事件编号断言失败，原始日志保留在raw-evidence.tar.gz。GitHub基线的中间测试结果只记录中间状态；最终结果见validation.json。全仓git diff --check检测到恢复上游字节带回的既有CRLF/尾空白（原始日志归档）；排除原字节参考快照后的重构/文档暂存差异空白检查通过；参考头文件的3处既有尾空白按来源原样保留，不为取绿改写上游原字节。

复现（从Git根，输出及固定依赖目录由调用方指定）：

```sh
git fetch --no-tags https://gitcode.com/openharmony/telephony_core_service.git 43ac8ae7df5b9c17c4efb3433415693d925b98c7
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/check_upstream_sync.py"
python3 -B "$TEST/scripts/check_test_integrity.py"
python3 -B "$TEST/scripts/check_relocation.py"
python3 -B "$TEST/scripts/test.py" --suite host --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/host"
python3 -B "$TEST/scripts/test.py" --suite g2a --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_TEST_OUTPUT/g2a"
python3 -B "$TEST/scripts/test.py" --suite architecture --output "$SIM_TEST_OUTPUT/architecture"
python3 -B "$TEST/scripts/test.py" --suite technical --output "$SIM_TEST_OUTPUT/technical"
```

固定GoogleTest归档及校验/准备方法继续见Host_Usage.md和dependency_lock.json。证据包保留原始命令日志、JSON、XML与失败，不包含依赖缓存/对象文件。完整产品hb/SO/SA/IPC/HDI/设备/eSIM仍需匹配产品环境。

文件模式边界：同步内容blob，保留本仓既有模式。索引核验1066项内容一致；246项上游100755/本仓100644差异另存file-modes.json，未批量给C++/图片等添加执行位。首次同时比较内容及模式的断言失败后，分别核实了这两类结果。
