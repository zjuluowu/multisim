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

## 候选回归

候选源码 SHA 为 `b6f6c25eed8b690da53f5b86ef3e7883cbe46916`。Legacy CI 任务在前一阶段启动，
报告记录启动时的 Git SHA 和实际构建输入。候选另有按该 SHA 记录的两端全量运行。

| 执行项 | 结果及证据 |
|---|---|
| Legacy 全量及 CI | 147/147 [候选全量](legacy-candidate.json.gz) · [CI 启动记录](legacy-ci.json.gz) · [变异及恢复](legacy-mutation.json.gz) |
| Target 全量 | 171/171 [候选全量](target-verified.json.gz) |
| 共同契约差分 | 12 个场景、24/24，[Legacy](legacy-common.json.gz) · [Target](target-common.json.gz) · [Oracle 差分](common-differential.json.gz)，差异 0 |
| CD-01～03 | 原行为 [Legacy 2/2](legacy-approved.json.gz)，批准行为 [Target 8/8](target-approved.json.gz) |
| Sanitizer | [ASan 73/73](asan.json.gz) · [TSan 73/73](tsan.json.gz) |
| GN/Ninja、围栏与规则 | [架构全量](architecture.log.gz) · [技术门禁](technical.json.gz) |
| 真实执行取证 | [Legacy 兼容入口](legacy-real-audit.json.gz) · [Target 拓扑](target-topology-audit.json.gz) |
| 逐文件统计 | [源码](source-inventory.json) · [SIM/WU/P 台账](task-inventory.json) |

全量及 CI 报告保留每个执行命令、筛选条件、断言结果和构建资料。在仓库根目录执行的主要入口

```bash
T=telephony_core_service/services/multisim/test
python3 -B "$T/scripts/test.py" all --backend legacy --ci --out <Verify> --output <证据>
python3 -B "$T/scripts/test.py" run --backend legacy --no-incremental-build --full-evidence --out <Verify> --output <证据>
python3 -B "$T/scripts/test.py" run --backend target --no-incremental-build --full-evidence --out <Target> --output <证据>
python3 -B "$T/scripts/run_architecture.py" --with-gn --migration --output <证据>
python3 -B "$T/scripts/handoff_inventory.py" --compile-db <Target编译数据库> --legacy <Legacy报告> --target <Target报告> --output <清单>
```

## Submit 返回归属复验

在 `065c5930b11e37724013bb0a852eea3f4a74c7cf` 上复现了同步早回包到 Owner 后重入的问题。
首笔完成触发第二笔，第二笔确定未发。基线把首笔也判成未取得传输关联。
用单次返回取代共享 Last 查询后，两笔结果保持各自归属，设备原完成只投递一次。

[复现源码](submit-reentrant-probe.cpp) · [基线](submit-reentrant-before.log) ·
[修订后](submit-reentrant-after.log) · [Target 172/172](submit-target.json.gz) ·
[ASan 127/127](submit-asan.json.gz) · [TSan 127/127](submit-tsan.json.gz) · [架构](submit-architecture.log.gz)

运行探针时，基线从对应 Git 源使用 `-DBASELINE`，修订后省略此宏。

```bash
clang++ -std=c++17 -pthread -I"$M" "$E/submit-reentrant-probe.cpp" \
  "$M/device/resource/resource_barrier.cpp" "$M/device/resource/resource_radio_runtime.cpp" \
  -o /tmp/m9-submit-probe
/tmp/m9-submit-probe
```

本切片未接入正式 B/C1 图。A 在原有锁边界上仍需按已签收契约准备成员并做锁外交接。

独立反查以 `-D_GLIBCXX_DEBUG` 链接真实 A 与 R。双成员完整 C1 同步回包在完成后留下悬空成员
迭代器。该负控在 M9 输入及本切片均可复现，风险限制于当前组件路径，正式图仍未启用。
[探针](c1-inline-owner-probe.cpp) · [失败](c1-inline-owner.log.gz)

附加的 B/C1 执行审计在 Debug 构建发现 `RuntimeOperationStore::TryMarkTerminal` 的计数为零。
实际 ELF 选择了测试翻译单元生成的未插桩 inline 实例。该项取证保留失败，其他已记录的执行审计保持原范围。
[初次架构负控分类失败](submit-architecture-initial.log.gz) · [附加执行审计](submit-audit-before.json.gz)

## C1 同步回包修订复验

在父提交 `012e33a0f4a5a6907b787b306489458ed3a2da16` 的候选工作区复验。原独立探针
使用真实 AssignmentService、ResourceRadioRuntime 和 ResourceBarrier。修订后分别提交
CHECK、UPDATE、COMPLETE 各两笔，共发令 6 次，只完成一次。原请求完成后无悬空迭代器。
Debug 审计将终态提交收归生产翻译单元，指定 B/C1 和 Radio 用例实际命中 TryMarkTerminal 93 次。

[独立探针复验](c1-inline-owner-fixed.log) · [LLDB 命令](c1-inline-owner.lldb) ·
[LLDB 实录](c1-inline-owner-lldb.log.gz) · [组件执行审计](c1-audit.json.gz) ·
[审计用例 61/61](c1-audit-cases.json.gz) · [GN/Ninja 与架构](c1-architecture.log.gz)

[Target 173/173](c1-target.json.gz) · [ASan 128/128](c1-asan.json.gz) ·
[TSan 128/128](c1-tsan.json.gz) · [最新 Target 共同契约](c1-common-target.json.gz) ·
[共同契约 24/24，Oracle 差异 0](c1-differential.json.gz) · [源码清单](c1-source-inventory.json)

独立探针和实际用例的重跑入口

```bash
mapfile -t sources < <(sed -n 's|^  "\$sim_target_root/\(.*\.cpp\)",$|\1|p' telephony_core_service/sim_target_sources.gni)
for i in "${!sources[@]}"; do sources[$i]="$M/${sources[$i]}"; done
clang++ -std=c++17 -D_GLIBCXX_DEBUG -pthread -I"$M" "$E/c1-inline-owner-probe.cpp" "${sources[@]}" -o /tmp/m9-c1-inline
/tmp/m9-c1-inline
python3 -B "$M/test/scripts/test.py" run --backend target --debug-build --filter TargetPrimaryB.C1InlineRepliesFinishWithoutAccessingRetiredRound
```
