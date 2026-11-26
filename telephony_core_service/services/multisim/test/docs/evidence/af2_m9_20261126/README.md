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
