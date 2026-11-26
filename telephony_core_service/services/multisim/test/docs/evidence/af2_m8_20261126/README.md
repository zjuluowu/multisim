# AF-2 M8 Target Host 验证证据

执行于 2026-11-26。输入 `9bebb54e82f9242203d530f997d1b58fc77cc8c4`。
首批代码发布 `e316298c045cd9b1c2d98e49238dd1638c663390`，登记为 PARTIAL。

| 检查 | 首批结果 |
| --- | --- |
| Developer 真实拓扑与 Reload 专项 | 32/32。包含上一批 T1/T2 和本轮首批 M8 用例。[原报告](developer-initial.json.gz) |
| LLDB | 实际断在 H、T、Store、Registry、A、Stop 及删除行，单步执行并观察进程退出 0。[命令](reload.lldb) · [原始日志](lldb.log.gz) |
| Developer 发布保护复验 | 33/33。覆盖六项 M8 用例。[原报告](developer-publication.json.gz) |
| ASan | 66/66，启用泄漏检测，含 M1～M8 及批准行为专项。使用独立无 LTO sanitizer 构建。[原报告](asan.json.gz) |

最新 LLDB 在带符号的 ASan 专项 ELF 单步，观察图像 10 和 14、原账号字段及删除点，进程
退出 0。LLDB 运行单独关闭泄漏检测，ASan 回归仍启用泄漏检测。
[命令](reload-final.lldb) · [原始日志](lldb-final.log.gz)

[开发及断点说明](../../M8_Account_Reload_Debug.md)

产品适配、硬件及生产迁移继续按原门禁。VS Code GUI 人工未验收。

## 命令

仓库根目录下的 Host 命令采用显式 GoogleTest、OpenSSL 和 runtime 目录。
下列 `G`、`S`、`R` 由仓库依赖准备脚本提供。Developer、Verify、ASan 和 TSan
分别使用独立构建输出。

```bash
T=telephony_core_service/services/multisim/test
OUT=/tmp/af2-m8
VERIFY=/tmp/af2-m7b-verify
DEV=/tmp/af2-m7b-developer
python3 -B "$T/scripts/test.py" run --backend target --gtest-root "$G" \
  --out "$DEV" --output "$OUT/developer-final-m8" --jobs 5 \
  --filter 'TargetTopologyIntegration.T*:AF2Reload.*:AF2VoiceSms.CurrentRoleVersionRejectsPreMutationReload'
python3 -B "$T/scripts/test.py" all --backend legacy --ci --out "$VERIFY" \
  --gtest-root "$G" --openssl-root "$S" --runtime-root "$R" --output "$OUT/verify-legacy-final" --jobs 4
python3 -B "$T/scripts/test.py" all --backend target --out "$VERIFY" \
  --gtest-root "$G" --openssl-root "$S" --runtime-root "$R" --output "$OUT/verify-target" --jobs 4
python3 -B "$T/scripts/test.py" run --backend both --out "$VERIFY" \
  --gtest-root "$G" --openssl-root "$S" --runtime-root "$R" --output "$OUT/both" --jobs 4 \
  --init-model dual-active-primary0
python3 -B "$T/scripts/run_architecture.py" --with-gn --migration --output "$OUT/architecture-final"
python3 -B "$T/scripts/technical.py" --output "$OUT/technical"
lldb --batch -s "$OUT/reload.lldb" "$DEV/build/sim_test_target_host"
```
