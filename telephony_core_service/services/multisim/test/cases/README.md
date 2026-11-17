普通业务从 sim_card_test.cpp、primary_test.cpp 或 query_test.cpp 复制 TEST_F，只用 sim_test.h。卡的物理绑定由框架处理，新增源文件由构建自动发现。

精确Primary时序参考 primary_protocol_test.cpp，状态竞态和异步生命周期参考 lifecycle_test.cpp、response_test.cpp，额外使用 sim_protocol_test.h。internal 中保留接口/ABI边界、对象图和平台替身专项，不属于普通开发接口。architecture 保留既有testonly GN可见性标签。运行、调试及结果说明见[开发入口](../README.md)。
