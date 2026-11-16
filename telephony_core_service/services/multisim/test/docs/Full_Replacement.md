# 测试防护网全量重建记录

本批输入：main@877162ff1b2425fee0c9db1aadd7221497fd32db。Linux x86_64 / WSL，Clang 18、GN/Ninja、固定 GoogleTest 1.15.2。**本记录是重建中的阶段交付，不是全量替换验收通过。**

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
