# AF-2 M7B 验证证据

输入本地及远端 main 为 `cd04fbad61941d8584fd8884457910e9447ac8fe`。
Developer 与 Verify 使用独立构建输出。首次贯通用例和后续原效果回归分别执行。

Developer 首次贯通 5/5，随后完整专项 59/59，含 M7B 8/8。真实 LLDB 组合用例走过
兼容受理、原会话、fence、Provider 写、字段接纳、回调及析构，进程退出 0。
[Developer 报告](developer.json.gz) · [LLDB 命令](show-number.lldb) · [原始日志](show-number-lldb.log.gz)

Legacy Verify ELF 的 `RealSimPrimary.ShowNumberReadsAccountAfterMsisdnFailure` 在双激活模型
实际 1/1。原 MSISDN EF 发令一次，错误后没有 Provider 更新，真实查询读取已有账户号码
`555123`，原结果为 0。这个观察用于独立的新场景和 Oracle。初次链接暴露旧 Host 没有 IMS
外部符号。补充「IMS 服务缺席」的 Host 边界后重跑通过。
[原报告](legacy-number.json.gz) · [XML](legacy-number.xml.gz) · [原日志](legacy-number.log.gz) ·
[首轮链接](legacy-initial-link.log.gz) · [兼容待决](compatibility-pending.json)

Legacy 执行审计首轮漏认 `SimFileManager::GetSimTelephoneNumber[abi:cxx11]`。
在原覆盖产物中核对到该生产符号有实际执行。审计匹配补充准确的 C++ ABI 标签后，
同一 Verify ELF 及 Profile 的复审通过，保留原方法正向覆盖要求。
[首轮审计](legacy-initial-audit.json) · [修正复审](legacy-corrected-audit.json)
