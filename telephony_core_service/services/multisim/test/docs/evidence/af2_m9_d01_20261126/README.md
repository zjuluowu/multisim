# D01 Radio 交接与部分成员封口检查点

实施输入为 D00 `f8a1c81b4825fed55a84e227bc97a8a29bc7d095`。
独立 Agent 对 `9615088e8` 做了源码反查，指出 B、C1 初始阶段、响应续步及重试均持 A 锁交接。
定时重试入口递归持锁，单改内部回调仍会阻塞。源级反查也发现 R 封口拒绝尚未提交的成员时，
原组未收到该事实，实际发出的成员已结束也可能永久占用资源。

- `PlatformHandoffDoesNotHoldAssignmentCommitLock` 将 B 与 C1 CHECK 的指定成员挡在平台 Submit 内，
  并由另一线程观察已登记的 A 原请求。旧生产源码上两项都无法在放行前获得 A 锁。
  [旧版运行失败](handoff-lock-baseline.log.gz)
- 初始 B、C1 多成员、C1 续步和两条重试入口现在保留原请求和角色写资格，在 A 锁外 Submit。
  调度重试共用单层提交锁，Submit 返回后按原步核对，避免同步回包完成后反向标记待核对。
  [A 实现](../../../../role_assignment/assignment_service.cpp)
- `ResourceStopSealsUnsubmittedMembersAndRetainsOriginalTransmission` 在首成员 Submit 未返回时封 R，
  下一成员零发令，租约继续记录原在途交接。首成员交接返回后进入隔离。缺状态核对的旧回执不能归还
  资源，精确原传输取得结束、状态及复用证明后释放。
  [R 实现](../../../../device/resource/resource_radio_runtime.cpp)

| 范围 | 实际结果 |
|---|---|
| Radio 组件 | [61/61](radio-component.json.gz)，包括新用例和既有同步回包、拒绝投递、重试及部分交接 |
| Target | [177/177](target-full.json.gz) |
| Legacy | [149/149](legacy-full.json.gz)，使用该批原样继承的 D00 生产归档 |
| Both 与保护 Oracle | [24/24](both.json.gz)，[差异 0](differential.json) |
| 静态、逻辑 GN/Ninja | [新指纹登记前失败](architecture-before-evolution.log.gz)，[登记后通过](architecture.log.gz)。历史基线指纹继续验证。 |

复现从仓库根运行以下入口。已有 Verify 产物须与当前源码匹配。

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" run --backend target --debug-build --out "$SIM_DEV" \
    --filter 'TargetPrimaryB.*'
python3 -B "$TEST/scripts/test.py" run --backend both --out "$SIM_VERIFY"
python3 -B "$TEST/scripts/run_architecture.py" --with-gn --migration --output "$SIM_ARCH"
```

本检查点为组件与共用合同验证。正式图真实协议成员、路由与发令许可线性化、正式回包及同步入口、
B/C1 逐键 Store 以及业务和资源两遍关闭收据仍待闭合。A 的 Store、通知和 Legacy Sense 结束调用
仍有锁内路径。本批未启用正式 B/C1。D02～D06 及产品门禁仍按唯一状态推进。
