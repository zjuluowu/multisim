# 多 SIM 架构边界与纯规则检查

从仓库根目录运行：

```sh
python3 telephony_core_service/test/multisim_architecture_rules/run.py
```

需要 Python 3.9+ 和支持 C++17 的 Clang（可用 `--compiler` 指定编译器）。默认先运行 include/GN 源表静态检查及护栏负控，再编译、链接并执行四套纯规则。编译产物默认放在临时目录并清理；没有测试框架或第三方依赖。

- topology：空位置绑定版本/ABA、装配与目录分离、current/historical投影、字段相关重载、发现/失效顺序、激活请求清理资格。
- runtime_protocol：会话期望生命周期、旧会话预检、窄结果sink、发布顺序与shared_ptr控制块身份；不创建真实生产Manager。
- resource：Checking、完整集合预留、发送前撤销、正常结束、Unknown隔离、查询丢载荷、错误Owner/epoch/fence及伪造租约负控。
- assignment：四种Primary模式及原分支优先级、参数前后位置、未知资源、请求命名空间、完成路由、激活冲突租约与角色资格收尾负控。

这些检查链接真实纯模型，包含关键交错和负控；不创建生产State/File/Controller，也不模拟卡效果成功。PASS只证明这些纯规则，不能替代生产链接、原接口差分、设备结束证据或OHOS测试。

需要进一步验证目录和源码边界时：

```sh
python3 telephony_core_service/test/multisim_architecture_rules/check_structure.py --migration
python3 telephony_core_service/test/multisim_architecture_rules/run.py --with-gn --migration
```

`--migration` 依赖 source_migration.json 指定的已提交基线历史，核对全部 73 个原文件去向、类型定义及实现 token 顺序；普通边界/规则检查不需要该历史。清单还记录每个文件的职责、实现状态和目标 GN 范围。

`--with-gn` 另需 GN/Ninja，会检查所有头/源的独立语法及合并头 TU，并从 gn/.gn 独立入口执行 `gn gen --check` 与 Ninja 全骨架对象构建。可用 `--output <仓库外路径>` 保存产物。没有产品库链接或生产启动；所有骨架 target 均为 testonly，根产品 BUILD.gn 保持原状。

边界扫描覆盖纯值/协议/机制、types/contracts/domain/cases/adapters、跨 Owner 私有 include、GN public/deps 可达性、入口源集合覆盖、include/target 环和关键类型单一定义；无法解析的宏 include 或动态 GN 列表明确失败。test_structure.py 以临时副本实际注入违规依赖等反例，覆盖检查器自身。详细模块图、保留项和覆盖限制见[源码 README](../../services/multisim/README.md)。
