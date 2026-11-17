# 测试防护网全量重建记录

以下首批历史记录输入为 main@877162ff1b2425fee0c9db1aadd7221497fd32db；当前续建输入及验收见下节。

## 2026-11-17 API 收敛续建

本轮输入 ce6441aa1，重新审计及重跑108项：86 PASS/22 FAIL、CI仍拒绝17项Primary/生命周期失败。公开头7个、直接Host包含8处、原Primary业务文件21处Mock设置/操作。C0改用SimTest业务接口，C1通过平台无关PrimaryProtocol显式等待/回包；真实Core→Manager→Controller链保持。原型业务均PASS，原对象图清理FAIL保留；容量改为同一SimTest、Mock收进实现。首次TEST/TEST_F suite混用错误已修复并原样归档。此为已验证的接口批次，其他接口/目录/Device及完整回归继续实施。

[本轮输入、基线及原型证据](archive/api_convergence_20261117/index.json)。后续最终结论将更新本节，不沿用上一轮通过代替当前实测。

## 2026-11-17 最终切换及验收边界

输入 main/origin main `807d73fbefdd6d7652a88187ae06211cc98e6de4`，Linux x86_64 / WSL、Clang 18、固定 GoogleTest 1.15.2。开始时仅有未跟踪 default.profraw，已保护；实际 fetch 恢复核对 CHECKPOINT_VERIFIED。生产 C++ 与正式生产 GN 不改。本次完成 Host 全量迁移及活动入口替换；**完整产品编译、设备运行和 VS Code GUI 尚未验收，不宣称 Full Replacement 全平台验收通过。**

### 逐项映射与实测

[逐例覆盖矩阵](Full_Replacement_Coverage.json)含全部 105 项历史运行配置及 3 项新增/此前未运行项；[逐文件处置及退役清单](Full_Replacement_Migration.json)关联输入 SHA、原文件 SHA、迁移目标/退役原因。原始活动树 317 文件逐字节归档，完整性工具通过 Git 对象验证，不依赖旧代码继续运行。原 Golden、历史 XML 和被冻结的查询参考源码保持原字节。

| 范围 | 原基线 | 新框架实测 |
|---|---|---|
| 原 Host 64 项 | 59 PASS / 5 FAIL | 59 PASS / 同五个 FAIL |
| 真实 Primary/生命周期 27 项 | 10 PASS / 17 FAIL | 10 PASS / 同 17 FAIL |
| G2a 参考查询 7 配置 | 7 PASS | 7 PASS |
| 原版 Core 查询 7 配置 | 7 PASS | 7 PASS |
| 双卡 PIN / 复制新增场景 | 首批新增一项 | 两项 PASS |
| 初始化前状态刻画 | 旧用例未被旧入口执行 | 1 PASS |
| 合计 | 历史 105 项 | 108 项：86 PASS / 22 FAIL |

新旧比较保留原失败正文、observations、external_calls、pending_responses、创建/销毁计数；最终切换运行 105 项全部 MATCH；较早一次运行 104 项 MATCH、逆序枚举一项仅 external_calls 的 status 请求总顺序不同，保留 DIFFERENT 与原值，另记 contract_equivalent。生产 SimStateManager::Init 为各槽构造处理器，各槽队列独立；FakeRil 的记录互斥只保证单次记录完整，并不建立跨槽调度先后契约。两种次序的请求多重集合相同，逆序响应、最终状态、通知及生命周期观测相同；不删除字段，也不放宽响应及状态断言。

五个 Host 缺陷保持原精确失败签名；完整性检查根据不可变原 XML 认证签名；运行分类仅读取经 SHA 锁定的活动 policy，不依赖历史执行器或证据包。真实 Primary 17 项不在豁免名单，完整门禁 FAIL。C0 业务断言 PASS，真实对象图清理失败（worker/RIL 强引用/观察者未释放、REAL_GRAPH_LIFETIME_NOT_DRAINED），保留原诊断及退出码。没有忽略 Mock 泄漏或移除清理检查。额外以 frozen dual-active-primary0 前态逐进程执行七个 Primary 业务场景，七项富业务契约均 PASS；六条正业务场景仍因清理 FAIL，不能由业务通过替代生命周期验收。

### 唯一活动框架和构建边界

普通 cases 仅使用 sim_test.h + TEST_F；复杂独立切片分别使用 sim_primary_test.h、sim_primary_entry_test.h、sim_capacity_test.h、sim_lifecycle_test.h、sim_query_test.h。PrimaryEnvironment 直接调用真实 CoreServiceSim，无旧 LegacyPrimaryAdapter/Scenario/ContractDriver。底层外部 RIL、参数、安全、事件调度边界复用为一份，业务结果来自真实 SUT。

七个 Host 二进制保留各自真实链接闭包，所有测试由 test.py 发现、逐例进程执行和汇总。bootstrap 已链接真实初始化完整闭包及既有参数/安全外部边界，不新增虚假生产 Stub。framework/component_sources.gni 为 Host/Device 共用生产源定义；source_set 复用同平台对象，GoogleTest 用 static_library，跨平台分别编译。生产库入口、服务启动和测试 main 分离；没有将未获授权的重构实现切入正式产品。

已删除旧 Scenario/Oracle/Adapter 运行层、重复基础设施测试副本、旧 Host/Primary/G2a Runner 和旧 GN 可执行目标。纯 Primary 业务期望与分类函数保留为 cases/primary_expectations.py / scripts/primary_results.py，直接验证真实观测，不再驱动旧执行链。历史 admission 技术测试继续验证纯策略；正式 CI 为 test.py all --ci + ci_gate.py。test.py 全量入口包括架构/GN、技术、完整性、七切片、正执行、迁移比较、变异恢复、profile 防覆盖检查。旧历史脚本仅存在不可变归档，不参与活动导入、GN 或 IDE。仓内没有 GitHub Actions 调度文件，本次切换的是仓内正式 CI 命令及 IDE 入口，不声称外部 CI 服务已运行。

正执行证据：原 SimStateManager/SimStateHandle 11 方法、Primary Core/Manager/Controller 的 SetPrimarySlotId、G2a 六方法和原版查询三方法均要求 LLVM count > 0，并记录实际来源文件。READY 映射与 PUK hook 两种仓外源码变异均被指定测试检出；恢复后两测试 PASS，正常二进制 SHA 恢复，仓内生产源码不变。

### Device Framework：实现与未验收项

原生 test/BUILD.gn 使用真实 //build/test.gni、ohos_unittest 及本地存在的 tel_core_service_api 标签。状态订阅依赖 tel_state_registry_api/part state_registry 已对照实际上游 GN；libsec_shared 对照固定原生依赖声明。没有修改正式生产聚合目标。

* sim_test_device：相同普通 cases，通过真实 CoreServiceClient、SA/IPC、权限及 TelephonyObserverClient 验证运行中服务。
* sim_test_device_component：同源普通/协议/平台 cases，真实生产组件 + 受控外部环境。容量、Primary入口、真实Primary、bootstrap、查询各有原生组件目标，全部引用同一 cases/sources.gni。该层不证明 SA/IPC 或平台 ABI。

Cases 无 Host/Device 条件编译。设备集成先检查 GET/SET_TELEPHONY_STATE、SA、受控资源，记录初始卡状态/类型/ICCID并订阅真实通知；操作调用真实 UnlockPin，结束要求恢复并查询验证、注销订阅和观察者释放。不具备权限、服务、卡资源或控制器时 BLOCKED/UNSUPPORTED，不能 PASS。

真实硬件状态制造要求外部受控 HIL/厂商测试控制器，本仓未提供实际设备自动化。资源文件私有 0600，键为 SIM_DEVICE_CONTROL_SOCKET、SIM_DEVICE_FIXTURE_TOKEN、SIM_DEVICE_ALLOWED_PINS。Unix socket 协议：`SIM_TEST_V1 RESERVE <token>` 应答 `OK CHANGE PIN RESTORE NOTIFY DUAL`；CHANGE 携业务卡名/技术槽号/类型/状态/锁，成功答 OK；RESTORE 答 OK 后仍须由真实服务查询确认。控制器不能代替生产查询或断言。控制器缺失是明确的设备资源阻塞，不是可运行性声明。

当前可访问 /mnt/e/OpenHarmony 缺匹配完整产品树/build/test.gni、hb、交叉工具链，且无 hdc/设备。原生 GN 语法、共用源清单、API/标签存在性及框架职责静态检查 PASS；八个原生目标编译/执行均 NOT_RUN，实际调度 BLOCKED。未把 Host PASS 当成设备 PASS；产品实际编译仍可能暴露需修正的 SDK 适配问题。

### 开发体验和 profile 防覆盖

cases/developer_copy_test.cpp 从普通双卡场景复制 TEST_F，只改业务步骤与断言；自动清单维护、CLI 列表/独立运行 PASS，LLDB 在真实 SimStateManager::UnlockPin 停下、slotId=1、调用栈/变量可见并继续完成 PASS。无需改 Fixture、Scripts 或手写 GN。VS Code 配置单例进程和统一 Debug 模板已接入；本机无 GUI，Testing 面板点击及独立初级开发人员操作 NOT_RUN。

原 default.profraw 事件保留：原 SHA bbb8d8d54bc9586adf968048bcdf7f08cf72cd1321d4766bafe30b8814ef5f1e 未恢复。本轮开始及结束 SHA 均 d35445fd920ef489cfb908b6968c9d3dea874688ea39daa53328c9f76be2d675，不提交/不删除。所有执行和调试入口设置独立 profile；编译提供绝对路径 %p 回退，ELF preinit 修正不安全环境值，main 再绑定安全路径。仅在 main setenv 的首次防护尝试被私有 sentinel 测试检出失败，失败日志原样保留；修正后 unset、relative、异常信号退出三类直接运行均保持 sentinel 字节不变，LLDB 另有隔离证据。

本轮原始报告、XML、日志、调试、失败防护尝试及修复、变异恢复、设备阻塞与覆盖数据在 [证据索引](archive/full_replacement_20261117/evidence-index.json)，压缩包保存原始字节。路径中的 /tmp 仅说明执行现场，恢复和审阅以仓内归档及源码锁为准。

---

以下为 2026-11-16 原始过程记录；其“尚未完成/未删除”只描述当时阶段，当前状态以上节及逐项处置表为准。

## 原始基线

本地/远端main一致；恢复检查CHECKPOINT_VERIFIED，暂存区为空。已有未跟踪default.profraw不纳入提交。原始输入及运行证据见[完整基线](archive/full_replacement_20261116/all_baseline/baseline.json)和其raw-results.tar.gz。1005项资产逐项记录原Git内容SHA256、采集时工作树SHA256及处置意向；资产行bytes是采集时文件大小，不用于认证原Git内容。原始Golden、冻结输入及历史索引没有改写。

实际执行旧统一入口all：Host64项59 PASS/5 FAIL；G2a参考及原版Core各7场景PASS；完整架构、完整性、迁址及四组技术检查PASS；Primary准备因缺OpenSSL BLOCKED，Target因真实目标写路径缺失BLOCKED。原始命令、XML、日志及退出码均归档。历史PASS不代替本轮执行。

随后显式取得固定OpenSSL包及三个固定原生依赖源码，编译真实Primary完整闭包。本轮27项10 PASS/17 FAIL，不将其混入原Host64项。C0业务断言PASS，清理后28个worker、10个RIL引用、account_observer_expired=false，REAL_GRAPH_LIFETIME_NOT_DRAINED/exit1；其他失败原文见本批Primary原始XML。未修复生产代码。

## 审计结论与旧资产处置

完整逐文件处置意向在基线JSON的assets中。下表给出主要资产的工程裁决；“删除”均以对应迁移、真实执行、断言及变异验证完成为前提，当前没有提前删除旧失败测试。

| 旧资产 | 有效价值/问题 | 处置 | 完成条件 |
|---|---|---|---|
| compatibility/scenarios、contract_test、card_oracle | 物理卡状态/通知、PIN、双卡、重复回包；一条测试被拆成多层 | 迁移到cases中的TEST_F；删除旧Scenario/Oracle/Driver | 逐例状态、失败信息、观测及正执行/变异证据 |
| lifecycle_extension_test | 初始化、缺RIL、逆序枚举、损坏/丢包、PUK、重建、在途PIN、五条缺陷 | 迁移为lifecycle_test，保留高级传输控制 | 原始失败不能消失；对象/订阅/线程检查保留 |
| pin_memory_contract_test | 厂商记忆hook的身份、操作、凭证、失败不通知 | 业务断言迁到pin_memory_test，Spy下沉Framework | PUK操作变异检出及恢复 |
| LegacySimAdapter / ISimSutAdapter / ISimSessionAdapter | 真实生产调用和严格Mock有价值，接口继承/工厂没有跨平台实现 | 重写成SimTest的私有Environment；删除旧层级 | 新代码不得调用旧Adapter/Runner |
| FakeRil / RilFaultControls / platform_spy | 外部设备事实、精确回包、真实出站通知观察 | 复用实现到底层Framework，迁移结束只保留一份 | 原Fake不生产SUT结果；严格清理不放宽 |
| Host stubs、FFRT/EventRunner、外部Provider | 确有平台依赖；线程/队列检出有价值 | 复用底层设施，统一初始化/释放 | 原生ABI与Host模拟边界分开验收 |
| infrastructure五个C++文件 | 23项基础设施行为，已是直观GoogleTest | 迁到cases，去掉专属main | 原断言保持，统一进程执行器 |
| capacity / bootstrap | 真实Manager只读查询、初始化前特征 | 保留测试价值，独立SUT编译边界 | 不与Manager seam混链扩大/伪造闭包 |
| Primary entry | 真实Core权限/错误顺序，Manager为内部seam | 保留独立编译边界并迁移测试入口 | 不称整链Controller/RIL覆盖 |
| real_test / real_runtime / real_scenario | 真实Core→Manager→Controller，Provider及RIL时序、通知 | 业务迁入用例，环境设施重写/下沉 | 七场景及生命周期全部逐例比较，C0清理失败保留 |
| Primary lifecycle / proposed.patch | 线程、对象图、三槽、在途/迟到回调检出；候选补丁不是生产修复 | 测试迁移；补丁留历史证据 | 不应用候选补丁让测试通过 |
| Primary Golden / frozen inputs / Oracle | 已批准外部预期和原始观测 | 原文归档，预期内联/转换须等价证明 | Golden不依赖新运行结果反向生成 |
| Target Adapter说明/Runner | 目前没有可执行Target写业务 | 删除空抽象，准确记录BLOCKED | 不以Fake或跳过冒充Target通过 |
| G2a参考Core/Binding及原版Core | 两种互斥Core定义；14次真实查询执行 | 保留两个编译程序，同一调度/结果协议 | 候选参考不能称当前生产接入 |
| architecture、源表、接口/依赖边界、负控 | 有效静态/纯规则，不是GoogleTest业务 | 保留适合的执行机制，统一汇总 | 不为退役测试放松生产边界 |
| Python Oracle/分类器/CI策略测试 | 业务与清理失败分类、失败准入规则 | 保留技术验证，逐步解除旧Runner依赖 | 未知失败和新增断言仍拒绝 |
| run_sim_guard / run_pin_memory / host等重复编排 | 构建/执行/预期/冻结混合 | 重写构建、发现、独立进程执行、报告；删除重复Runner | 所有业务迁移后切换唯一入口 |
| check_test_integrity / check_relocation | 来源与Golden保护有效，冻结框架实现阻碍替换 | 历史认证与当前工程检查分开演进 | 不改历史清单；每项退役有独立证据 |
| .vscode | 原目标及真实Primary隔离，缺统一调试模板 | 重写Testing发现/任务/LLDB模板 | 每进程一用例，真实函数断点；GUI实测另记 |
| docs及archive | 历史证据有价值，活动文档互相依赖过多 | 活动文档收口README及本记录；历史留archive/Git | 不将历史材料作普通用例运行依赖 |

旧五条FAIL是生产行为/契约问题：移除/替卡后的旧READY回包、通用RIL错误被当成密码结论、替卡后的旧PIN结果、Refresh公开返回契约冲突。新27项首轮及新50项运行仍检出同五条FAIL。Primary的对象图未释放属于单独的生产生命周期失败，不能归入这五条，也不能用新Fixture问题解释。

## 当前目标与实现边界

普通用例只include sim_test.h，使用Card资源、AddCard/Start/ChangeCard/UnlockPin/ExpectCardState/ExpectStateNotification。Card表示测试资源；physicalSlot仅是测试环境的物理路由，不建立subscription/simId/cardId映射。普通操作直接进入私有Environment中的真实生产方法，无Scenario/Contract/Oracle/Adapter分发。高级竞态用sim_advanced_test.h明确控制状态回包、延迟、错误和重建；高级能力不强加给普通测试。

Host实际复用原SimStateManager、SimStateHandle、IccState、ObserverHandler、CoreManagerInner、TelephonyExtWrapper、TelEventHandler/Queue。生产C++没有复制进测试目录。测试源码/平台Spy/Device事实与生产实现分开编译。沿用可复用testonly source_set，共享wrapper单独成为一个source_set，避免不同切片重复列源/重复定义；GoogleTest仍为static_library。Host与Device应各自编译同源实现，不能跨ABI复用静态库。正式tel_core_service是完整产品组件，不适合作为独立Host的依赖；本批不扩大生产GN接入。

试将真实容量Manager与PrimaryEntry的Manager seam同链，实际暴露MultiSimController/MultiSimMonitor未链接方法。没有新增业务Stub消除链接错误。容量、入口、真实Primary和两种Core查询保持独立SUT程序，统一执行/结果协议。native组件测试与运行服务SA/IPC集成测试也必须独立标注。

Host用显式源清单，update_case_sources自动生成，新建顶层cases/*.cpp时不手写GN；追加TEST_F只改一个文件。run_gtest按发现结果逐例独立进程，验证实际XML的测试ID/退出码/skip，并记录源码摘要和二进制摘要。Mock严格，状态来自真实查询，通知来自生产出站Spy。清理继续检查实例、订阅、事件worker和回调异常。主要同步使用现有有界事件队列/条件变量，无新增业务sleep。

## 本批结果与未完成项

- 新sim_test_host：50项45 PASS/5 FAIL，其中49项来自原64项，新增DualSimPinUnlock一项；48项原始状态、失败及观测完全一致，逆序枚举一项仅external_calls跨槽顺序不同（status:1;status:0与status:0;status:1），差异未归一化。原用例没有跨槽请求全序断言，实际业务状态/回包注入/退出结果保持；原始DIFFERENT状态保留供审查。
- 新README构建入口已独立冷构建成功，随后通过统一执行器运行DualSimPinUnlock单例PASS；原始构建/运行记录见cli-smoke.tar.gz。该单例是入口验证，不额外增加业务覆盖计数。
- 原64项中15项容量/Primary入口仍在原程序中，尚未计为新框架迁移完成；G2a、真实Primary、生命周期、CI及静态检查的统一切换也未完成。
- LLVM正执行审计11个原生产方法全部非零，含Init、UnlockPin/UnlockPuk、Refresh、ProcessEvent及GetUnlockResult。该结果仅证明本切片正执行，不是全SIM覆盖。
- 两项原生产源码变异（READY映射改LOCKED、PUK记忆hook操作改错）均使对应新用例FAIL/exit1；恢复后两项PASS，恢复二进制SHA与正常50项运行的二进制完全一致。原始突变源码、GN参数、命令、退出码、XML及日志见[变异/调试/正执行证据](archive/full_replacement_20261116/verification/verification.json)。
- LLDB实际在SimStateManager::UnlockPin停住，slotId=1，调用栈进入真实源码后继续，DualSimPinUnlock通过。TestMate按本机4.27.1 schema配置maxTestsPerExecutable=1及统一LLDB模板；没有GUI点击验收，也没有“基础C++开发人员”独立验收。
- Device Framework、sim_test_device、产品GN同源编译、SA/IPC/权限/HDI、设备资源声明/恢复尚未实现或验收。本机没有hb/hdc及匹配产品树；不能将此标为成功，Host迁移继续推进不依赖这些条件。
- 旧执行体系尚未退役，当前允许的短期参考阶段尚未结束。不得将本批提交称为Full Replacement完成；不得提前删除失败测试、降低清理断言或停留为长期双框架。

切换顺序：核对本批已保存的两项变异/恢复证据→逐项迁移剩余15项及G2a/Primary/生命周期→改统一调度/CI与历史完整性检查→按逐项等价证据退役旧Adapter/Scenario/Driver及重复Runner→重跑全部可执行验收；产品环境门禁单列。

## 工作区保护事件

本批一次手工LLDB调用漏设LLVM_PROFILE_FILE，覆盖了已有未跟踪default.profraw。原摘要bbb8d8d54bc9586adf968048bcdf7f08cf72cd1321d4766bafe30b8814ef5f1e；发现时摘要d35445fd920ef489cfb908b6968c9d3dea874688ea39daa53328c9f76be2d675。已检查475份可访问profraw未找到原摘要匹配副本，不能声称原字节已恢复。该文件不纳入提交。新执行器、VS Code和后续调试显式指定独立profile输出。
