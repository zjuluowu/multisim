# C0 生命周期失败修复（2026-11-17）

输入 main/origin@97702ae1fd93fe3a3dd3a7121418a5f2a7ca4bda；用户在确认 C0 业务通过、资源清理失败后要求修复。该次授权仅落实本节两处旧实现退出/注销修复，不扩展生产迁移或新 Stop API。已有三个未跟踪用户文件不改、不提交。

## 原因与改动

ObserverHandler 对订阅者持强引用；旧 SimManager 空析构未解开 Monitor/电话簿/Records/File 订阅关系。SimStateHandle 能注册 RADIO_SIM_ICCID_LOADED，却缺少对应注销分支，使 File→Records→State→观察者→File 仍构成持有环。SimManager 析构现在配对解除所属订阅并调用现有 Records::UnInit；SimStateHandle 补齐 ICCID 注销。没有全局改弱引用、修改业务断言、屏蔽 Mock 泄漏或扩大 API。

协议用例的结果返回可能早于生产分离线程捕获引用释放。PrimaryEnvironment 在根释放前等待已有 AwaitDetachedThreadsIdle 屏障，并对 RIL 引用计数和账户观察者释放增加直接断言；这些原先只记录为属性的条件现在也会令 GTest 失败。strict-cleanup/ 保留 C0 和两条协议用例均 worker=0、RIL引用=1、object_lifetime=PASS 的原始结果。

构造后只查询容量/卡数的三个窄 Host 切片从未调用 OnInit；新增析构使其原链接缺少图方法。uninitialized_sim_graph.cpp 为这三个切片补充被调用即抛错的链接边界，真实 Primary 和 bootstrap 不链接它。首次重建的三个链接失败保留，补齐后重新构建通过；未以假成功代替真实图清理。

上游校验区分1064个原字节文件和2个明确指纹本地修复：Upstream_Sync_Test_Adaptation.json 锁定上游字节、当前字节及本目录两个补丁。未列出的源码变化仍失败；6项负控/正控技术测试覆盖原文件漂移、修复文件/补丁/基线漂移和未知路径，未扩展忽略规则。

## 证据与复现

baseline/：修复前 C0 FAIL，28 workers/10 RIL引用、观察者未释放。first-candidate/：仅解订阅后仍FAIL，15 workers/10 RIL引用；补齐 ICCID 注销后 final-single/ 的 C0及新增回归均PASS。primary-regression/ 是增加直接资源断言前的中间回归，其两项协议用例的资源属性为FAIL，不能称完整生命周期通过；最终以 strict-cleanup/ 和 primary-final/ 为准。

source-negative/：实际删除新增ICCID注销分支、构建、单独执行新增回归，出现 retiring.expired() 失败；恢复原字节并重建后，C0及回归均PASS，恢复产物SHA与变异前完全一致。两个 debugger-* 目录保留早先无效的调试器跳过实验（未成功改变执行效果，结果FAIL），不作为变异检出证据。

复现入口：`python3 -B telephony_core_service/services/multisim/test/scripts/test.py build`；然后用 run_gtest.py 指定 `.host-out/build/sim_primary_slot_real_test`、`--filter RealSimPrimary.C0CoreToRealManagerAndController --env SIM_HOST_BOOT_SLOT_COUNT=2 --env SIM028_INIT_MODEL=single-active --output <仓外目录>`。固定依赖及IDE操作沿用 Host_Usage.md。原始日志以gzip保留，其他六个程序的JSON/XML/日志在 other-business-reports.tar.gz。

架构88单元/25逻辑target、17结构测试、5编译负控、纯规则、GN/Ninja及迁移检查PASS；技术84项及历史317资产/299处置/8合同完整性PASS。native Device 静态检查PASS，但无 hb/hdc 和匹配产品环境，产品构建与设备执行NOT_RUN。VS Code枚举114项及源码位置检查PASS，GUI未实测。default.profraw SHA保持d35445fd920ef489cfb908b6968c9d3dea874688ea39daa53328c9f76be2d675。完整业务结果以 full-business-result.json 为准，既有五缺陷和八个缺失生产Stop API的未来契约不豁免、不作PASS。

最终七程序114项101 PASS/13 FAIL；Primary28项20 PASS/8 FAIL。八条准入失败原因仅为ShutdownContract缺少Stop API，另五项Host旧缺陷原签名保持。bootstrap-final/是最后清理断言版本的重验结果，other-business-reports中同名旧运行作为过程保留。最终Primary三个真实方法正执行审计PASS，见execution/。

发布前补丁上下文中的原CRLF空行触发空白检查；改为零上下文差异（应用时使用git apply --unidiff-zero），源码字节不变，补丁指纹同步更新。源码仍保留原行尾，最终按cr-at-eol检查。
