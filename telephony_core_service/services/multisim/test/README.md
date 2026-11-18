# SIM 业务测试

## Legacy / Target 双后端

统一入口默认执行 Legacy，CLI 覆盖配置。Target 不支持的场景返回 BLOCKED，绝不回退到 Legacy：

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" run --backend legacy
python3 -B "$TEST/scripts/test.py" run --backend target \
  --filter SIM028-LEGACY-C0-DUAL-PRIMARY0
python3 -B "$TEST/scripts/test.py" run --backend both \
  --filter SIM028-LEGACY-C0-DUAL-PRIMARY0
```

配置见 `config/test_backend.json`，支持矩阵见 `config/backend_capabilities.json`。两套实现独立构建/链接/进程：Legacy 使用 `services/sim`，Target 使用 `sim_target_production_host`。Target C0 已真实执行 `AssignmentService`、`AssignmentAggregate` 和 `PlanPrimarySwitch`；仍是 Host 组件闭环，不代表产品持久化、SA/IPC 或设备验收。详细审计、边界和迁移顺序见 [双后端架构](docs/Backend_Architecture.md)。

从 cases/sim_card_test.cpp 复制一条 `TEST_F`，只改准备、操作和断言：

```cpp
#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, MyPinCase)
{
    auto a = AddCard("A", CardKind::Usim, State::Ready);
    auto b = AddCard("B", CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    EXPECT_EQ(UnlockPin(b, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(b, State::Ready);
}
```

在既有文件追加测试只改一个函数。也可新建 cases/*.cpp；构建工具自动维护显式源表，并按业务 API 选择已有真实链接闭包，不需要改 GN、注册表或历史 Golden。

从仓库根运行（已准备的 Linux x86_64 / glibc / WSL 测试环境）：

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" build
python3 -B "$TEST/scripts/test.py" list --filter SimTest.MyPinCase
python3 -B "$TEST/scripts/test.py" run --filter SimTest.MyPinCase
python3 -B "$TEST/scripts/test.py" debug --filter SimTest.MyPinCase
python3 -B "$TEST/scripts/test.py" all --ci
```

构建需要 Clang/LLVM、GN/Ninja 和固定依赖；环境维护者提供 SIM_GTEST_ROOT，以及首次构建的 --openssl-root、--runtime-root，锁见 framework/internal/host/configs/dependency_lock.json。路径保存到输出目录 replacement.json。默认构建和报告输出 .host-out；可用 --out 和 --output 隔离构建及报告。报告位于仓内时，完整入口/architecture会在仓外临时目录执行架构检查，再将产物保存在本次报告的 architecture/；显式仓外报告直接在对应目录执行。因此默认Run All Tests无需额外指定仓外报告路径。普通用例不管理 Mock、线程或资源清理。

VS Code Remote-WSL 打开仓库，首次执行 **Prepare Testing Panel**（构建七个程序并检查124个源码位置），Testing 面板刷新后点击单例 Run/Debug。后续可用 **Build Host** 增量构建。真实 Primary 用 **Build Real SIM Host**；Testing 面板会自动关联程序，每例独立进程。在真实生产函数设断点即可，无需逐例修改 launch.json。CLI/LLDB 和 GUI 的实测状态分别记录，未把配置检查算作 GUI 操作成功。

常规 Primary 参考 cases/primary_test.cpp，同样只用 sim_test.h。精确阶段、迟到回包或错误恢复参考 cases/primary_protocol_test.cpp，额外用 sim_protocol_test.h 的 PrimaryProtocol；它只暴露请求事实与应答控制。卡状态竞态和生命周期参考 cases/lifecycle_test.cpp、cases/response_test.cpp，同样只用公开协议头的 StateProtocol。其他 internal 是平台、ABI/入口或对象图专项，普通业务测试不依赖其中头文件。cases/architecture 保留既有生产 testonly 可见性标签，不参与普通用例编写。

只读业务参考 cases/query_test.cpp，不要求双卡或 HIL。设备变更才按需检查资源、权限与安全恢复；缺能力是 BLOCKED/UNSUPPORTED。Host 与原生组件目标共用同一源清单，运行中服务的 SA/IPC 目标单独验证。原生调度使用 `test.py device --ohos-root <完整产品树>`，已有原生程序用 `--device-binary`；控制资源用私有 --resource-file。当前产品树/工具链/设备阻塞见验收记录。

每个结果目录含 result.json、单例 XML、日志及退出码。当前已发布Host验收为124/124 PASS、CI准入PASS，详见[执行状态§48](../../../docs/SIM_Refactor_Execution_State.md)和[完整证据](docs/archive/remaining_failures_20261118/phase_b/README.md)。五项旧业务缺陷已修复，resolved策略要求它们必须PASS；旧签名仅作历史来源。Stop契约、强订阅释放及新增安全回归均通过，任何新增失败、缺用例或不安全运行仍阻止CI。产品/设备及GUI不由Host结果推定通过。

[完整依赖审计、前后对比、原始证据与设备限制](docs/Full_Replacement.md)。

Testing 面板首次准备使用 `test.py editor`（VS Code 任务 `Prepare Testing Panel`）；源码定位及断点操作见[Host 使用说明](docs/Host_Usage.md#testing-面板源码定位和断点)。
