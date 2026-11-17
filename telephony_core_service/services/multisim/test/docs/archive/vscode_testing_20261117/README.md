# VS Code Testing 面板链路验证（2026-11-17）

输入 main@4e51425096ed5ea7e96c864150e032e6a1486c5e，叠加前一轮未提交 settings/执行状态修改；未跟踪用户文件保持。最终 `test.py editor` 在仓库 .host-out 构建七个程序，发现113项，全部源码绝对路径及行号可定位。final-build-result.json 和 discovery/ 保存实际命令、XML和映射；不执行113项业务断言。

首次构建中 sim_test_host 在修改公共 main 的 --help 行为前已经编译，first-discovery.json 保留其帮助识别失败；重新链接后最终枚举PASS。入口检查保留 --help exit0、无过滤及不存在用例 exit2，见 entry-checks.json。原版本 --help 虽打印帮助但被精确用例护栏拒绝并 exit2，该误报已修复。

final-source-line-lldb.log 证明最终产物在 developer_copy_test.cpp:6 命中断点，继续后单例PASS；final-production-result.json/日志证明真实 UnlockPin 断点、slotId=1 和继续执行PASS。无 final 前缀的调试日志来自首次链接产物，作为前后过程保留。技术检查两组共78项PASS，原始输出在 technical-*。配置名与本机TestMate 4.27.1 schema核对，导航6项及差异空白检查通过。

复现：按锁准备固定依赖；首次执行 `python3 -B telephony_core_service/services/multisim/test/scripts/test.py editor --gtest-root <固定源码> --openssl-root <准备目录> --runtime-root <准备目录>`，后续直接 `test.py editor` 复用校验后的配置。VS Code操作详见 ../../Host_Usage.md。

未操作GUI、未重跑完整业务回归，保留既有22项业务/生命周期失败与产品/设备门禁。本轮不修改生产实现。default.profraw SHA256保持d35445fd920ef489cfb908b6968c9d3dea874688ea39daa53328c9f76be2d675。

发布前完整暂存差异空白检查检出原始LLDB日志的CRLF、源码空行和空格/Tab指示箭头，完整检查FAIL；保留原始日志不改写。排除本证据目录后的配置/源码/文档空白检查PASS，不将原始日志检查失败称为全量PASS。
