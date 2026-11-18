# 验证：构建与测试

## 环境前提

- 本仓是 OHOS 部件（part=core_service），**必须**在完整 OHOS 源码树中构建，挂载路径 `base/telephony/core_service`
- 本仓未配置托管CI工作流（无.github/.gitee）；重构门禁由test.py all --ci执行。无rust-toolchain、.clang-format/.clang-tidy。
- Rust ANI workspace（Cargo.toml）仅 IDE/本地开发用；正式构建走 GN（ohos_rust_shared_library/rust_cxx）；依赖 ani_rs 为 git 依赖（communication_netmanager_base 分支 OpenHarmony_feature_20250328），脱离 OHOS 环境无法 cargo build

## 构建

```sh
hb build -T //base/telephony/core_service:tel_core_service   # 主 SA .so
```

- 部件级：`hb build core_service`
- 可选宏：core_service_support_esim / core_service_satellite / telephony_extra_defines（telephony_core_service.gni:14-28）
- 全量 target 清单见 bundle.json build.group_type（含 sa_profile、services/etc 配置、telephonyres HAP、四个 ANI 组等）

## 单测（gtest，非 cargo test）

```sh
hb build -T //base/telephony/core_service/test:unittest   # 聚合 15 工程 28+ 二进制
hb test                                                     # OHOS 测试框架执行
```
- 或直接执行设备上生成的 gtest 二进制
- 测试工程：core_service_gtest、sim_gtest、tel_ril_gtest、network_search_*、esim_gtest、utils_vcard_gtest 等（test/unittest/）
- 测试编译依赖 test/mock/ffrt:ffrt_mocked（FFRT mock）；业务源码经 test/core_service_test.gni 静态编入测试
- **新增接口/修改虚接口后必跑 core_service_gtest**：mock（mock_i_core_service.h、mock_i_network_search_manager.h 等）未同步会编译失败，即编译即校验

## Fuzztest

```sh
hb build -T //base/telephony/core_service/test/fuzztest:fuzztest
```
- 41 个 fuzzer（simfileparse_fuzzer、vcard_fuzzer、unmarshalling_fuzzer、coreservice1-5_fuzzer 等）
- **解码/字符串解析改动必跑对应 fuzzer**：sim_char_decode、sim_number_decode、vcard_decoder_*、ASN.1 codec、parcel 反序列化

## 本地快速验证（无 OHOS 环境时）

无法编译时的替代检查：
1. LSP/IDE 诊断：无新增 error（clangd 需配置 OHOS include 路径，可能不可用；退而求其次人工检查类型/签名一致性）
2. `rg <新增符号> services/ frameworks/ interfaces/ test/` 检查 18 文件同步点是否齐全（见 docs/knowledge/routing/new-interface.md）
3. `rg "std::mutex"` 检查未引入 std::mutex（约束：ffrt::mutex）
4. 对照历史提交文件集：`git show <新增接口类 commit> --stat`（如 AR20260708227592 的 18 文件）

## 验证流程

1. 改代码前：读 docs/knowledge/ 对应场景文档 + 编辑前声明
2. 改代码中：同步 18 文件标准集（新增接口时）
3. 改代码后：`rg` 一致性检查 → hb build（有环境时）→ 对应 gtest → fuzzer（解码类）
4. 按根/部件AGENTS及用户当前授权逐文件提交、常规推送；失败/阻塞如实记录，不扩大生产改动范围

## 独立Linux防护网与产品测试

当前唯一重构测试入口为services/multisim/test/scripts/test.py；Host使用GN/Ninja和固定依赖，原生产源码直接参与执行。产品原生ohos_unittest目标在同一测试目录BUILD.gn，受控组件与实际SA/IPC服务集成分开；产品树、权限、设备/HIL和恢复条件按用例检查，缺条件为BLOCKED/UNSUPPORTED。

从Git根执行：

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" build
python3 -B "$TEST/scripts/test.py" list
python3 -B "$TEST/scripts/test.py" run --filter SimTest.DualSimPinUnlock
python3 -B "$TEST/scripts/test.py" debug --filter SimTest.DualSimPinUnlock
python3 -B "$TEST/scripts/test.py" all --ci
python3 -B "$TEST/scripts/test.py" device --ohos-root <完整产品树>
```

环境、固定依赖与新增TEST_F参见[开发指南](../../../services/multisim/test/README.md)。默认输出.host-out，可用--out/--output隔离；所有入口隔离LLVM_PROFILE_FILE，保留用户default.profraw。真实Primary逐例独立进程；五项旧Host缺陷已修复，resolved策略要求PASS，原签名保留作历史证据。当前124项Host及CI已通过，任何回归仍拒绝。仓内报告目录下的完整/architecture入口会在仓外执行架构检查并回收产物至本次报告，单独run_architecture.py仍要求仓外--output。最新实际结果及未完成产品/GUI验收见[执行状态](../../SIM_Refactor_Execution_State.md)和[验收记录](../../../services/multisim/test/docs/Full_Replacement.md)，历史记录不视为当前HEAD重跑。
