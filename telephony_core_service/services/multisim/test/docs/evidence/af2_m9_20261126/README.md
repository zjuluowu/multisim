# AF-2 M9 实施与回归证据

输入为 `bf7d0c689f7e12320c864a6866a2c8d92d0bd660`。M8 Host 签收仍使用原历史证据。

独立审查从固定输入的 Git 内容构建两个资源探针。AF2-03 负控在 P 的 Submit 中重复 P 并
提交未发出的 Q。基线重复进入平台、提前释放共享资源，原 P 回执失去关联。修订后 P、Q
各一次，竞争请求发令为零，P 原序列保留。另一个负控让第一笔投递失败与第二笔投递成功
交错。基线重复交付第一笔，修订后只交付各一次。
[探针](af203-probe.cpp) · [AF2-03 修订前](af203-before.log) · [修订后](af203-after.log) ·
[投递探针](radio-receipt-probe.cpp) · [修订前](radio-receipt-before.log) · [修订后](radio-receipt-after.log)

独立无 LTO Host Target 首批全量 170/170，含新文件中自动枚举的回执负控。
[实际报告](target-first.json.gz)

```bash
M="$PWD/telephony_core_service/services/multisim"
E="$M/test/docs/evidence/af2_m9_20261126"
clang++ -std=c++17 -pthread -I"$M" "$E/af203-probe.cpp" \
  "$M/device/resource/resource_barrier.cpp" "$M/device/resource/resource_radio_runtime.cpp" -o /tmp/m9-af203
/tmp/m9-af203
clang++ -std=c++17 -pthread -I"$M" "$E/radio-receipt-probe.cpp" \
  "$M/device/resource/resource_barrier.cpp" "$M/device/resource/resource_radio_runtime.cpp" -o /tmp/m9-radio-receipt
/tmp/m9-radio-receipt
```

预身份会话用例证明真实模块可以先接纳物理应用，保持无账户和 Binding。旧代次完成被拒绝，
新会话可在同一代次接纳 ICCID 并进入 T。Target 全量 171/171，ASan 73/73。
[全量](target-application.json.gz) · [ASan](asan.json.gz) · [LLDB 命令](handoff-application.lldb) ·
[单步输出](lldb.log.gz)。LLDB 字符串格式器有摘要解析错误，断点、代次标量、真实函数及
单例退出 0 已实际观察。

M6 的共同测试体由真实 Legacy 与 Target 包装，原 Scenario、Golden 和 Oracle 保留。
两端 2/2，差异为零。 [Legacy](metadata-legacy.json.gz) · [差分](metadata-differential.json)

在独立干净 worktree `7264a3940` 用 `test.py run --backend target --debug-build --filter
TargetRadioReceipt.RefusedPostDoesNotRemoveAnotherRequestsReceipt` 实测冷跑 57.66 秒。
显式复用已有 ELF 时热跑 0.51 秒。新增一个 Target CPP 后增量跑 23.97 秒。
[冷跑](cli-cold.log) · [热跑](cli-hot.log) · [增量](cli-incremental.log) · [新增用例](clean-discovery-case.cpp.legacy)

在该 worktree 从 Target 源表移除 SlotCatalog 的 CPP。原归档工具留下旧成员，链接仍成功。
更换为全量重建 archive 后，同一变异得到预期的未定义符号，恢复源表后重新链接通过。
[原错误通过](archive-stale-before.log) · [负控](missing-target-source.log) · [恢复](archive-restore.log.gz)
