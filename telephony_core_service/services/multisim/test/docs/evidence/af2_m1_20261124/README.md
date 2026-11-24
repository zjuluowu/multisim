# AF-2 M1 验证记录 · 2026-11-24

源码提交 `b521a081a1bbf61736f5859d28c3a3c9fb927f07`。签收文档输入 `199920c6f`，目标源码签收基线 `632f6a2a8`。Linux x86_64 / Clang 18 / GN / Ninja / LLDB 18。验证时的构建目录为 `/tmp/af2-m1`。[逐例结果及二进制摘要](results.json) · [日志、XML 与调试命令](raw-logs.tar.gz)

| 检查 | 命令与结果 |
| --- | --- |
| 分批 Host | 先用 `test.py build --backend target` 链接工厂增量，exit 0。随后用 `test.py run --backend target --filter 'AF2Module.*'` 验证首批 5 项，exit 0。最终增补资源争用用例后用 `test.py run --backend target --out /tmp/af2-m1/host --output /tmp/af2-m1/target-final2`，exit 0，108/108，其中 M1 6/6，包含原 C0/B/C1 及发现组件回归。 |
| 屏障 | 从 multisim 根执行 `clang++ -std=c++17 -Wall -Wextra -Werror -UNDEBUG -I . test/cases/architecture/resource_rules.cpp device/resource/resource_barrier.cpp -o /tmp/af2-m1/resource-final`，执行产物 exit 0。遍历所有出现的多键申请分配失败点，逐键复核状态；含同键拒绝、独立键、双键成功、错误 fence／Owner／epoch。Assignment 和 Runtime Protocol 纯规则单独各 exit 0。 |
| Sanitizer | 以同一 Host GN 配置分别加 `host_asan=true` 或 `host_tsan=true`，`gn gen` 后 `ninja -C /tmp/af2-m1/<mode> -j 3 sim_test_target_host`，再运行 `run_gtest.py --binary /tmp/af2-m1/<mode>/sim_test_target_host --output /tmp/af2-m1/<mode>-run --filter 'AF2Module.*'`。ASan 6/6，TSan 6/6，均 exit 0。组过滤直接交给测试二进制时触发单例策略，日志已保存。 |
| 源码结构与 GN | `run_architecture.py --with-gn --migration --output /tmp/af2-m1/architecture` 结构检查通过（94 文件、155 include、25 targets），73 项历史 token 通过。完整入口 exit 1，出错点为签收前已存在的 `assignment/account_roles/account_role_protocol.h` 布局差异。独立调用同一脚本的 `gn_build`、`syntax_check` 均 exit 0，GN/Ninja 骨架对象齐备，94 单元和组合头通过。 |
| 边界、摘要及纯规则入口 | `run_architecture.py --output /tmp/af2-m1/rules` 的结构、17 项边界负控及 5 项编译负控通过，随后因 Topology 行 182 断言 exit 1。使用签收时未修改的输入重新编译纯 Topology 规则，复现 abort。该点依赖发现准备失败处理，M1 的 `AcceptStoredDiscovery` 不进入此路径。签收目录 SHA 以 `199920c6f` 的原字节固定，历史副本 SHA 保留；目录缺失、漂移、旧副本恢复的负控通过。 |
| LLDB | `lldb --batch -s /tmp/af2-m1/debug.lldb /tmp/af2-m1/host/build/sim_test_target_host`，最终 exit 0，工厂、两次启动绑定、A、参数端口、Stop 和 shared_ptr 匹配销毁各有断点命中，工厂单步命中。记录 epoch `{0,1}`、slot 1、订阅 41、bindingRevision 1。前两次调试已运行该业务，末尾命令因优化后析构符号位置未命中而报错。实际销毁使用 `SimModuleDeleter.*_M_dispose` 停住。 |
| 其他 | `test_navigation.py` 6/6，`check_test_integrity.py` 317 原资产、299 去向、8 不可变合同／参考项全部通过。各 Host 报告确认工作区已有 profile 未变。 |

VS Code 配置复用现有 Target Host 入口，GUI 未实测。Legacy 业务及生产写权沿用原验证。原生产品、设备、SO、跨实例热重建和多成员设备提交未验收。[单步与 M2 接线](../../Host_Usage.md#af2-m1-debug)
