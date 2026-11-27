# Radio 原交接与 Owner 关闭收据

输入为 `79b1470c5447cedd59c2716d42aa8094f595ad9b`。本阶段验证 Target Host 的
Assignment/Radio 组件图。正式 SimModule 的 B/C1 接线仍待实施。

## 超时先于平台交接分类

平台 Submit 阻塞后，原角色请求先超时。原组必须封闭后续发令准入。平台确认原命令完全未发
时还须按原租约取消；如果已有成员发令或交接未定，必须等原组设备结束及资源复用证据。

独立审查在仓外隔离源码运行新增测试。仅加入回归时，两项均到达断言并失败。原 R 将尚未
分类的全未发租约提前隔离，无法按未发取消。修复在 `EndLegacySenseSwitch` 登记组的结束意图，
保留等待分类的租约；`SwitchPrimaryRadio` 按原交接证据选择取消或隔离。修复后独立审查的
Primary 和 Radio 专项通过 66/66。
[原始反例](timeout-baseline.json.gz) [修复复验](timeout-fixed.json.gz)

## Owner 投递中的重复重投

设备证据结清后，投递帧与原组责任在同一 R 锁内登记，覆盖原平台 Post 的完整调用。
投递失败才由原收据进入后续重投。在原 Post 的返回受屏障阻塞时读取关闭状态、尝试重投，
两者都保留原回执。Post 返回后按原票只消费一次。

运行时负控仅删除 `FlushOwnerDeliveries` 检查投递帧的条件，独立编译 R 对象并重链
同一个 Target Host gtest。实际多投一次，原票消费从预期 `{1}` 变成 `{1, 1}`，
同一测试退出 1。原二进制精确复跑退出 0。原仓库源码未被变异。
[变异失败](owner-repost-mutated.log) [恢复通过](owner-repost-restored.log)

## Host 候选

以包含上述两处修复的开发构建运行 Target 全量，207/207。
静态架构护栏、源码演进、独立语法和逻辑 GN/Ninja 均通过。
[完整结果](target.json.gz) [架构检查](architecture.log)

```sh
python3 -B telephony_core_service/services/multisim/test/scripts/test.py run \
  --backend target --debug-build --jobs 8 --out /tmp/af2-m9-foundation/dev \
  --output /tmp/multisim-d04-radio-candidate
python3 -B telephony_core_service/services/multisim/test/scripts/run_architecture.py \
  --with-gn --migration --output /tmp/multisim-d04-radio-architecture
```

原 Store 效果跟踪、A/T 发令许可和全对象图 B/C1 运行仍为必要 Host 工作。
本阶段的原生 RIL/HDI、产品接入及设备验证均为 NOT_RUN。
