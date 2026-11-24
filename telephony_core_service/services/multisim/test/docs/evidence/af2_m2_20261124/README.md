# AF-2 M2 验证记录 · 2026-11-24

输入 `0f83e00cfd70e4a1bb04c09953f31151fa937e33`，被测源码 `87f808d8edc1b9e9c78d5a511e25a401f9f0f60a`。Linux x86_64、Clang 18、GN/Ninja、LLDB 18。[逐例结果与二进制摘要](results.json) · [原始日志、XML 与调试脚本](raw-logs.tar.gz)

| 检查 | 运行结果 |
| --- | --- |
| 增量与 Target 全量 | 从统一 `test.py run --backend target --filter 'AF2Discovery.LiveCardDiscoveryCreatesAccountAndBinding'` 首次运行真实入口，再执行 `test.py run --backend target --out /tmp/af2-m1/host --output /tmp/af2-m2/target-final`。最终 117/117，含 9 项发现相关用例、6 项 M1 组件回归，exit 0。中间回归发现旧直接 T 夹具给撤销与新观察用了相同序号，按严格源顺序修正后新卡正确接纳。现行原操作证据替代旧同值查询核对，旧卡迟到成功只接纳 Historical。 |
| 发现纯规则 | 从 `git archive 0f83e00cf` 恢复受测输入，用 `clang++ -std=c++17 -Wall -Wextra -Werror -UNDEBUG -I .` 构建 `topology_rules.cpp` 与现有 SlotCatalog、BindingIndex、AccountRegistry、DiscoveryObservationLedger、ActivationProgress 对应源码。基线行 182 失败并 abort。修复后原断言和整组规则 exit 0。未归还的准备执行保留原票和资格，确定归还后才可能结清。 |
| 架构与目录 | `run_architecture.py --with-gn --migration --output /tmp/af2-m2/arch-final`，结构 96 文件、159 include、25 targets。73 项 token、88 项布局映射、91 项目录迁移、17 项边界和 5 项编译负控、四组纯规则、独立语法及 GN/Ninja 全部通过。历史对照继续验证原提交与摘要，七项已有合法演进根据固定 M1 Git 内容作精确比较。 |
| ASan / TSan | 现有 Host GN 用 `host_asan=true` 或 `host_tsan=true`，重建 `ninja -C /tmp/af2-m1/<mode> -j 3 sim_test_target_host`。用 `run_gtest.py --binary /tmp/af2-m1/<mode>/sim_test_target_host --output /tmp/af2-m2/<mode>-final --filter 'AF2Discovery.*:AF2Module.*'`，两组各 15/15，exit 0。固定二进制和模块源摘要见逐例报告。 |
| Legacy/Target 差分 | `test.py run --backend both --filter SIM028-LEGACY-C0-DUAL-PRIMARY0 --out /tmp/sim-final-host --output /tmp/af2-m2/both-c0`，Legacy、Target、Oracle 及双后端比较均 PASS，差异 0，exit 0。 |
| LLDB | `lldb --batch -s /tmp/af2-m2/debug.lldb /tmp/af2-m2/lldb-readable/sim_test_target_host`，exit 0。正式 H 工厂完成一次单步，命中 R 会话、T 短登记和原票、Store Match/Write、A 规划、T AccountRegistry/BindingIndex、A Finalize/Confirm 及模块读口。实际观察 epoch `{0,1}`、观察序号 2、原 admission 1、token 1、订阅 41、槽 1、绑定版本 1。优化版部分符号内联，复验时只在独立生成目录去掉 LTO、两个依赖 LTO 的虚表选项，并用 `-O0 -g -fno-inline`。仓内构建设置未更改。首次无 LTO 选项组合失败和优化版记录均归档。 |
| 其余 | 测试完整性 317 份原资产、299 条去向、8 项不可变合同通过。更新源码清单后导航 6/6，Mermaid 的时序图和类图解析通过，工作区已有 profile 校验通过。 |

实际业务闭环在正式 Target 对象图内运行，Host Store 端口提供原调用的可归因证据。真实产品 Provider、外部观察订阅、Reload 因果覆盖、生产写权与设备接入继续按原门禁验证。VS Code GUI 未实测。[操作指南与 M3 接线](../../Host_Usage.md#af2-m2-debug)
