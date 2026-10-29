# 多 SIM 架构纯规则检查

从仓库根目录运行：

```sh
python3 telephony_core_service/test/multisim_architecture_rules/run.py
```

仅需要 Python 3 和支持 C++17 的 Clang（可用 `--compiler` 指定编译器）。编译产物放在临时目录并在执行后清理，不修改生产 GN，不引入测试框架或第三方依赖。

- topology：空位置绑定版本/ABA、装配与目录分离、current/historical投影、字段相关重载、发现/失效顺序、激活请求清理资格。
- resource：Checking、完整集合预留、发送前撤销、正常结束、Unknown隔离、查询丢载荷、错误Owner/epoch/fence及伪造租约负控。
- assignment：四种Primary模式及原分支优先级、参数前后位置、未知资源、请求命名空间、完成路由、激活冲突租约与角色资格收尾负控。

这些检查链接真实纯模型，包含关键交错和负控；不创建生产State/File/Controller，也不模拟卡效果成功。PASS只证明这些纯规则，不能替代生产链接、原接口差分、设备结束证据或OHOS测试。
