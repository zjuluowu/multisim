# D03 透明 ICCID EF Host 样板

输入为 D02 共用传输检查点 `338ac4a2f6931682dfae41b1179614bb2d3fdb6e`。
正式 `SimModule::LoadIccid` 用原卡应用会话及 `LoadSession` 发 GET RESPONSE 和 READ BINARY。
同一请求各步独立关联设备回包和租约。READ 长度来自透明 EF 元数据原字节。
完整和十进制视图沿用成熟的半字节处理。原设备回复尺寸或身份证据不成立时拒绝该轮接纳。
同域提交 Records 及可信观察，再由正式账户发现和原 Store 原票建立 Binding。
[运行源码](../../../../card/records/iccid_records_runtime.cpp) · [正式入口](../../../../infrastructure/assembly/sim_module_factory.cpp)

受控平台覆盖 SIM/USIM 透明 EF 路径、原卡态 ICCID 与带填充 EF 的等价读取、设备失败、短或畸形载荷、
同步及重复回包、局部 Refresh、双卡隔离、换卡及 Stop。实际 HDI 路由、其他卡型和产品适配另行验证。

独立审查发现 Refresh 可在原账户匹配回包前发生，旧观察仍能获得写权。修订在 R 卡提交域撤销
精确未绑定的 T 原观察，同时保留已发送 Store 责任。跳过撤销的独立变异 ELF 实际写入旧行并失败，
基线 ELF 同一用例通过。变异运行未修改工作区源码。
[变异](refresh-mutation.log.gz) · [恢复](refresh-restored.log.gz)。

审查同时发现同一卡会话的冲突 ICCID 可覆写 Records 并留下原 Binding，以及旧预留可能令新观察无法
进入 Discovery。现在先核对原身份，未获得 Discovery 原票的新观察按原序撤回，允许后续有效加载。
矛盾同步回包不用于继续发令，原设备效果保留到后续精确证据。

[LLDB 真实断点及单步](iccid-lldb.log.gz) · [实际执行计数](production-execution.json)。
独立验证覆盖一笔请求、两次设备交接、一次解码、一次 Records 接纳及一次实际 T DiscoverAccount。
图形界面为 GUI_NOT_RUN。

静态护栏发现新增请求库依赖未声明、Scope 独立编译缺默认值，以及纯规则的旧消费端缺新增短契约。
分别修复并复验。
[首轮 GN 依赖](initial-gn-dependency.log.gz) · [首轮独立语法](initial-independent-syntax.log.gz) ·
[首轮规则夹具](initial-pure-rule-adapter.log.gz)。

前一 D02 共用传输检查点的 [Both 24/24](d02-transport-both.json.gz) 与
[冻结 Oracle 差异 0](d02-transport-differential.json) 同时保存。正式 B/C1、PIN 和全图关闭仍待实施。

[Target 全量 195/195](target-full.json.gz)，含 13 项正式 ICCID Host 用例。
[ASan 87/87](asan.json.gz) · [TSan 87/87](tsan.json.gz)，范围为 ICCID、号码和 B/C1 组件。
[Host 源码归属](host-archive.log) 核对 LoadSession 已进入独立 Target 生产归档。

[结构、纯规则、GN/Ninja 复验](architecture.log.gz) 通过。
