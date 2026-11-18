# 第二阶段：真实 Stop 与最终回归

最终命令 `python3 -B telephony_core_service/services/multisim/test/scripts/test.py all --ci --output /tmp/sim-stop-complete` 实际 exit0：124/124 PASS、CI准入PASS、96项技术检查PASS。最终强RIL Stop17/17及双激活业务/冻结契约7/7 PASS。原始报告见 [final-result.json](final-result.json)、[比较裁决](final-comparison.json)、[命令索引](index.json)；完整XML/日志/覆盖率导出在 `final-ci.tar.gz`，压缩前目录前缀为 `final-ci/`。早期失败和未完成轮次不计最终通过。

输入 main/origin `4fd8162bc0ea6f6ce57b6d080e52e1d4bc5db23a`，在当前工作区适配仓内历史 Stop 候选；不继承历史通过结论。`adaptation-plan.json` 记录逐文件适配及五处上下文冲突，源码保留第一阶段响应关联/错误/刷新修复与 C0 析构兜底。当前电话簿预加载线程另纳入所属任务组。

SimManager::Stop 关闭新业务准入，允许已接纳业务及其重试/协议回调完成，排空组件自有队列、任务和线程，解除对象图订阅/参数观察并释放自身根引用；并发、重复调用受协调。Core 的独立 RIL 引用由 Fixture 在确认仍保留后显式解除；不停止全局 FFRT、独立 eSIM 控制器或其他组件。参数观察使用不可复用 token、弱 owner 和在途回调屏障；注销失败返回 false 并保持 Stopping，可重试。生产代码无生命周期测试探针，采样仅在 Host 端口且持有弱引用。

原 A～H 的业务/资源断言保留，新增九项回归：迟到参数回调、部分注册失败、注销失败重试、观察者析构重入、C1 三个阶段与 Stop 并发、在途回调排空/自停止拒绝、任务闭包析构屏障。`primary-strong.tar.gz` 为本阶段初轮强 RIL 订阅模型下 37/37 PASS；`stop-repeat.tar.gz` 为 A～H 加九项再次 17/17 PASS；`dual-active.tar.gz` 为双激活七项 GoogleTest 和冻结富业务契约全部 PASS。`editor.tar.gz` 为七个程序 124 个源码位置检查 PASS，未运行 GUI。

`source_patches/` 相对固定上游导出完整零上下文差异，包含此前已发布修复；上游校验 1029 个原字节文件、37 个逐文件指纹修复、2 个显式新增头文件。新增头文件同样锁 SHA，测试保证错误路径、内容漂移或冒充上游文件不能通过。

历史 Golden、coverage 和 known_failures 原字节保留。`accepted_case_repairs.json` 逐项列明原 22 个 FAIL 的授权修复及允许观测字段；原始 DIFFERENT 仍保留，只有原 FAIL→当前 PASS、有效 XML、exit0、无断言失败/不安全运行且观测变化限于对应字段才接受。未列入或回退均 FAIL；新增六项正负控。

首轮编译有源读取/UTF 诊断，随后源码 UTF-8/无 NUL 检查正常；稳定源码后二、三轮及七程序构建 PASS。原始失败和成功日志分别 gzip 保留。首次统一入口使用仓内默认结果目录，架构脚本明确要求仓外输出而 BLOCKED；该次 124 例通过但未完成 CI，随后使用显式仓外 --output 重跑，不将前次当作通过证据。

原始逐例 XML/日志及 JSON 归档；覆盖率逐进程原始 profile 不重复入库，独立 SHA 清单保留，最终统一入口另保留生产执行审计。可按仓内源码/依赖锁及命令重建验证。

原生 hb/hdc 匹配环境、产品构建、设备、ASan/TSan、SO 卸载和 VS Code GUI 未执行。内部类布局/虚表有变更，产品须整体重编；Host 通过不代替产品或安全卸载验收。

最后源码复核删除 ObserverHandler 中未启用的历史采样宏及其无用包含；Host 采样位置保持。`final-ci.tar.gz` 保存修正后的完整统一入口及最终源码七程序124例CI复核；中途的重复分片运行未完成，不计验收。

完整门禁首轮随后检出两个基础设施问题，原始报告保留在 `guard-corrections.tar.gz`：profile 崩溃负控依赖已修复的 DEFECT_001，因此不再崩溃；改为健康用例启动后显式 SIGABRT，验证 exit=-6 且 sentinel 不变。变异恢复前的正常产物尚未包含最后的源码清理，恢复链接后摘要不一致；mutations 现在先以当前源码重建 baseline，再记录 SHA。两项变异实际都被检出，不将首次摘要失败改写为 PASS；修复后独立重跑和最终完整入口分别留证。

公开入口逐方法复核另补齐六个值返回查询及 UpdateDBActiveByIccId 的准入租约，防止 Stop 后访问已退役数组，或并发写入越过排空屏障。原 A～H 及新增安全场景的 VerifyStopped 均增加这些公开调用断言；用例数量仍为124，不靠新增重复用例增加通过数。最终完整入口包含此源码及断言。

新增停止后 EONS 调用断言令原本被 LTO 删除的网络态引用参与链接，首轮因此缺 GetNrState 符号失败；在 Host 外部运行时明确提供被调用即抛 unsupported 的网络态边界，仅检验停止后不会进入该路径，不模拟网络查询成功或替代生产 SIM 实现。失败日志保留。

`stop-final-strong.tar.gz` 为最终源码强RIL模式17/17 PASS，包含新增七个公开入口断言；`dual-final.tar.gz` 为最终源码强RIL模式的双激活七项业务及冻结契约复核。前述初轮/重复结果不叠加用例数。

sim_file.cpp 的上游差异含两行原始尾空格，补丁以 `.patch.gz` 无损保存并锁定压缩字节SHA；不修剪证据中的原始源码空白。其余源码补丁为纯文本。
