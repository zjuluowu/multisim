# OpenHarmony 多 SIM 工程

本仓包含telephony_core_service fork。成熟生产业务仍在services/sim；services/multisim是testonly目标骨架，尚未替代生产。设计、代码、Host测试及产品设备验收分别判断。

- [开发文档入口](telephony_core_service/docs/README.md) 汇集业务开发、源码定位、测试和迁移的阅读路径。
- [唯一执行状态](telephony_core_service/docs/SIM_Refactor_Execution_State.md#current-status) 记录当前基线、验证、阻塞与下一批任务。
- [源码导航](telephony_core_service/services/multisim/README.md)：五组物理目录、Owner及GN边界。
- [测试开发](telephony_core_service/services/multisim/test/README.md)：复制TEST_F、构建、单例运行与调试。

从仓库根使用统一入口：

```sh
python3 -B telephony_core_service/services/multisim/test/scripts/test.py all --ci
```

依赖准备见测试README。已发布Host验收为124/124 PASS、CI准入PASS，修复前的原始失败证据保留。完整入口将架构产物保存在本次报告的architecture/。不能以CI退出或Host结果推定产品通过；产品原生组件和真实SA/IPC测试另行构建执行。

协作及发布遵循[仓库AGENTS](AGENTS.md)和[部件指引](telephony_core_service/AGENTS.md)，保护未提交文件和历史证据。
