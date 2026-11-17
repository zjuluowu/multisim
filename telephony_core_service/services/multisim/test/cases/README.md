普通业务测试参考 sim_card_test.cpp 和 developer_copy_test.cpp，只需 sim_test.h 和 TEST_F。新增 TEST_F 不改注册表；新增根目录 .cpp 由构建自动加入显式源清单。

精确回包/超时参考 lifecycle_test.cpp 的高级接口。primary、primary_entry、capacity、query 为独立真实生产切片，使用对应公开 Fixture 头；所有平台复用相同文件。运行与调试命令见上级 README。
