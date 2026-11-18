# 文档与测试配套刷新（2026-11-18）

输入main/origin@609c709a42c506bbbbab78408ecab5d06f7b2556。活动导航/开发指南统一引用已发布124/124 PASS及真实Stop修复，历史记录只加适用版本标记，未改写原始日志、Golden、签名或生产源码。详细设计区分具体SimManager Stop、尚未接入它的CoreService::OnStop，以及未来工厂/产品/SO验收。

配套修复：默认test.py报告位于仓内时，架构阶段在仓外临时目录执行，产物回收到本次architecture/，位置记入architecture-location.json；仓外报告直接执行，原失败/阻塞退出码保留。修复策略文件必需且包含全部五项已审查修复，缺失/删项/重复/未知项拒绝，不能退回历史失败接受。

105项技术检查PASS（100基础设施+5源码边界），含新增4项架构报告路由/证据保留和5项修复策略正负控。`technical.tar.gz`保留原始命令/日志/结果。默认命令`python3 -B telephony_core_service/services/multisim/test/scripts/test.py architecture`实际exit0，结构/独立语法/迁移/负控/四规则/GN全部通过；仓外临时目录已删除，仓内报告完整保留。原始日志及报告位置映射在`default-architecture.tar.gz`，只归档文字证据，编译对象/ELF不重复入库；原始命令可重建。

历史完整性317原始资产/299处置/8合同PASS；生产来源1029原字节+37指纹修复+2新增头文件PASS。当前门禁复核上一批124例原始报告PASS，逐项模拟五个旧精确失败签名均被拒绝，见[门禁复核](admission.json)。这是旧测量的新门禁复核，不是本批重新执行业务；124项执行证据仍见[上一批](../remaining_failures_20261118/phase_b/README.md)。

[本批索引](index.json)保存输入SHA、实际命令、适用范围和用户文件指纹。原生产品/设备/SO/GUI未执行；default.profraw与原稿(5)保持原摘要和未跟踪状态，lnk在本批开始已不存在。没有新增生产修复、迁移、工厂装配或产品验收结论。

现行导航六项检查及新增/修改本地链接检查通过；历史状态/W1来源索引中既存旧路径按原期保留，不称全部历史链接已恢复，范围记录见[链接复核](links.json)。
