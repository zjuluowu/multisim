# SIM-028 双卡前态复验

当前基线39d63590；生产字节保持本批开始状态，未应用隔离修复，无提交/推送。

- `validation.json`：最终50轮/350进程分层统计；`final-validation.json`含所有逐例结果、精确命令、工具、190份生产摘要与二进制SHA。
- `final-raw.tar.gz`：全部350份原始JUnit XML、日志及机器结果。业务350 PASS，完整GTest50 PASS/300生命周期FAIL；轮次与入口仍exit1。
- `final-run-golden-input.json`、`final-run-real-scenario.h`、`final-run-detached-threads.cpp`：最终运行输入快照；当前Golden在结果产生后更新，运行输入摘要保持原事实。
- `uncoordinated-dual.tar.gz`/`uncoordinated-validation.json`：307 PASS/1 FAIL/42 NOT_RUN，说明仅队列空闲不够。`coordinated-v1.tar.gz`及对应三份输入：另一独立版本50轮350业务PASS，不计入最终50轮。
- `initial-dual-smoke.tar.gz`：最初双卡单轮。原单卡350次/受控LLDB根因与反例在相邻`sim028_initialization_stability_20261112`目录，不被覆盖。
- `host-regression.tar.gz`/JSON：64条59 PASS/5既有FAIL，包含入口14/14；`known-defects.json`保存原失败签名。`pre-metadata-host-regression.tar.gz`保留首次新增测试未登记导致的门禁FAIL。
- `selected-c1/`及命令日志：最终原C1单例CLI实跑，业务/Strict RIL PASS、生命周期FAIL/exit1，未混入50轮。
- `thread-barrier.xml/log`：9项专项；`attr-detached-probe.cpp/log`为100次属性分离技术检查，不计入SIM业务数。分类器13项见`sim028-final-classifier-tests.log`。
- `repo-workspace-scan.json`、`product-version-check.json`：有界扫描与repo导出尝试，产品匹配UNKNOWN。`environment.json`保存本机工具路径与Git状态。
- `test-environment.patch`、`documentation.patch`、`changes.json`及`input-protection.json`：相对本轮实际起始字节的增量及保护校验，不是相对HEAD的生产修改。

初始化顺序：两卡真实加载 → 当前队列/分离调用完成 → 公开查询/真实准备Primary0和Data0 → 再次完成屏障 → 验证公开前态 → 原始业务断言及精确RIL规则。Host屏障不取消未来timer，不修生产缓存getter，不代表安全Stop。业务与生命周期始终分别判定，原始非零退出码不被吞掉。

只冻结明确双卡模型下的条件性Host业务基线。发布者捕获不等于消费者收到；内存Provider/参数不等于耐久存储；RIL弱订阅单目标不是原生多强订阅；无真实serial/SA/IPC/HDI/芯片与匹配产品轨迹。本轮ASan/TSan与析构后迟到回调安全未验证。新multisim尚未迁移或差分执行。

复现命令见`../../primary_slot/legacy_contract/README.md`，原始入口exit1是保留生命周期失败的预期事实，不能作为全链PASS。
