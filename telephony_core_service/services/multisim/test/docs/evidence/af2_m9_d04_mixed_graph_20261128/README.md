# 正式混合对象图关闭与跨实例重建

输入 main 为 `aa2d9052a6d16404a5e04fb0282ed4d95ec6e950`。同一个正式 Host 图同时执行两条原 C1 CHECK、
PIN、ICCID READ BINARY 和 MSISDN，并保留 Historical Reload 结果及可追溯的账户发现 Store 未决效果。
局部 Refresh 失效旧 ICCID 轮次，换卡使原 PIN 和原身份观察失效。随后封闭新请求，保留旧证据入口。

原 Owner 尚未消费、设备效果未结、Store 未决三个阶段分别得到对应的关闭结果。MSISDN 原物理写和数据库失败
按旧接口返回一次，PIN 旧结果没有越过新会话，EF 原传输结清后重复证据被拒。受控整轮未应用证明释放原角色设备效果。
此时仍因原 Store 未结而阻止重建。可归因的原 Store 核对结束后，正式工厂才在前驱证明下创建新图。

新图同时执行反向 C1 角色请求和新 ICCID 加载。重放旧 Radio 回包、整轮恢复证明和 EF 原票都被拒绝，
不会结束新租约或修改新 Records。新图依据真实设备回包完成自身两条链路并排空。

Target 全量 264/264，C1、EF 及卡组合专项 ASan/TSan 各 18/18。
独立精确执行及追加重复 20/20 通过。冻结资产和合同完整性通过。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [独立执行](independent.log) [完整性](integrity.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 跳过初始前驱终止和复用要求 | 两次旧责任尚存时创建了冲突实例 | [变异](allow_missing_predecessor_proof-mutated.log) [恢复](allow_missing_predecessor_proof-restored.log) |
| 关闭观察遗漏发现 Store 票和该票的角色责任 | 原 Store 未定即排空，重建错误获准，原写证据随后被拒 | [变异](lose_original_discovery_effect-mutated.log) [恢复](lose_original_discovery_effect-restored.log) |

本批新增正式组合回归。逐项关闭诊断和其余未决整轮结果仍待实施。平台及前驱证明来自受控 Host 适配器。
