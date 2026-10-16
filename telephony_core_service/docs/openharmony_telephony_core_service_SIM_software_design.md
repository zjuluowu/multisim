# SIM 子系统软件设计：现行文档导航

统一版本：2026-10-15。此路径不再独立维护一份与MultiSIM重复、可能漂移的2395行设计。

现状看[当前源码基线](MultiSIM/01_sim_subsystem_baseline.md)，问题看[演进分析](MultiSIM/02_current_architecture_and_evolution.md)，目标看[总体架构](SIM_Refactored_Software_Architecture_Design_Version3.md)，实现细化看[类设计](SIM_Layered_Architecture_Class_Design_Version3.md)。所有冲突选择以[统一裁决](MultiSIM/08_design_unification_decisions.md)为准。

旧版完整功能说明、4+1视图、API/事件/存储资料保存在[历史原稿](history/pre_unification_20261015/openharmony_telephony_core_service_SIM_software_design.md)。其上游基线、代码行号和改进建议是历史资料，不直接等于本仓当前事实或现行方案。

现行目标为四Context、三聚合、三类目标、一个逻辑ACL和十类内部异步领域事件；不新增公开API/IPC/SA/进程/schema/运行期Feature Flag。CoreManagerInner消费侧兼容，卡状态、文件和安全等生产逻辑继续复用。Phase0未就绪，容量组件测试通过不代表状态/账户或设备系统测试完成。

此文件仅导航，不重复定义规则；后续更新各专题，避免重新生成竞争总设计。
