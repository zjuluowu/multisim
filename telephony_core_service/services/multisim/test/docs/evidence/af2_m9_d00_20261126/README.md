# Architecture Ready D00 实施证据

输入 main 与 origin/main 为 `9615088e8d006efaca071c61a490e427c6232e60`。恢复检查通过。
[现行架构决策](../../../../../../docs/SIM_Target_Architecture_Design_v1.2.md#architecture-ready-decisions)及
[CD-04](../../../../../../docs/AF2_Compatibility_Decisions_Approved_20261126.md#cd-04)已生效。

| 验证 | 结果 |
|---|---|
| Legacy 实际 Provider 装载及列表接口 | [2/2](cd04-legacy.json.gz)。连续对照通过。稀疏输入 Current 可读 41，活动槽仍为 1。Historical 遗漏 41 和 49。历史及未知 ID 解析不到活动槽。 |
| 正式 Target SimModule | [2/2](cd04-target.json.gz)。连续对照通过。稀疏 Current 可读 41。Historical 保留 41 和 49，行序交换后各自资料正确。两次视图刷新独立，删除 49 后保留 41。 |
| Target 全量与 Both | [Target 175/175](target-full.json.gz)，[冻结场景 24/24](both.json.gz)，[Oracle 及双端差异均为 0](differential.json)。 |
| 规则／GN/Ninja | [通过](architecture.log.gz)，含 23 项结构护栏、四组规则、独立头文件编译和逻辑对象生成。 |
| 配置和资产 | 后端契约检查 20/20。完整性通过，含 317 项归档原件、299 项去向及 8 项不可变合约或参考。 |

测试以 Provider 镜像模拟行删除后的数据，真实产品删除及持久化接线待独立验证。
Target 通过 `ReadAccount` 查询持久身份。既有 `AF2Discovery.KnownCardReinsertKeepsIdentityAndRejectsDuplicateAndOldSource`
使用稀疏 simId 77 验证正式模块重插及旧绑定失效。尚欠 CD-04 双端多卡换绑组合及产品调用者适配取证。

标识专项检查覆盖五组 C/C++ 实现。订阅 ID 用于持久键、比较与原接口值域，slotId 用于兼容路由及槽描述，
SIM Label 作为账户属性保存。未发现 Target 将 simId−1 或 legacySimId 用作数组下标。
B/C1 槽成员推导及虚构卡应用会话属于 D01 的运行事实问题，保留待办。

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" build --backend both --out "$SIM_BUILD" \
    --gtest-root "$SIM_GTEST_ROOT" --openssl-root "$SIM_OPENSSL_ROOT" --runtime-root "$SIM_RUNTIME_ROOT"
python3 -B "$TEST/scripts/test.py" run --backend legacy --no-incremental-build --out "$SIM_BUILD" \
    --init-model single-active --filter 'RealSimPrimary.*AccountReload*'
python3 -B "$TEST/scripts/test.py" run --backend target --no-incremental-build --out "$SIM_BUILD" \
    --filter 'AF2Reload.*AccountReload*'
python3 -B "$TEST/scripts/run_architecture.py" --with-gn --migration --output "$SIM_ARCH"
python3 -B "$TEST/cases/internal/infrastructure/test_backend_selector.py"
python3 -B "$TEST/scripts/check_test_integrity.py"
```
