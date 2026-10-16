# 验证：构建与测试

## 环境前提

- 本仓是 OHOS 部件（part=core_service），**必须**在完整 OHOS 源码树中构建，挂载路径 `base/telephony/core_service`
- 本仓无独立 CI（无 .github/.gitee）、无 rust-toolchain、无 .clang-format/.clang-tidy
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
4. 全部通过后由用户决定提交（禁止擅自提交）

## 独立Linux组件防护网与文档刷新

完整OHOS产品构建仍需上述完整树；test/systemtest有已批准的独立Linux GN适配，不是ohos_unittest，不能将主机测试结果当设备/SA/IPC通过。

从Core目录执行的当前入口示例（本次文档刷新未执行）：

```sh
python3 test/systemtest/scripts/run.py --gtest-root <固定版本GoogleTest源码目录> --output <输出目录>
```

固定依赖、Clang18/gold/C++17、命令及2026-10-08真实结果见[test工程README](../../../test/systemtest/README.md)和[执行记录](../../../test/systemtest/docs/run_record.md)。容量B1 PASS、INFRA16 PASS与原五业务BLOCKED分开。编译45个初始化对象不表示初始化已链接/运行。

文档统一只做来源、链接、代码摘要和一致性静态检查，不宣称hb/gTest/fuzzer已运行。架构Phase0未就绪不妨碍已授权host设施推进，也不授权生产重构。
