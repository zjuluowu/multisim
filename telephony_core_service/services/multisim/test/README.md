# SIM 业务测试

新建 Target 专属测试 CPP 可放在 `cases/target/`。下一次 Host 构建自动更新 GN 源表。可移植业务序列从 `cases/shared/` 复用，两个后端保持独立装配。参见 [M9 候选交接记录](../../../docs/AF2_M9_Developer_Handoff_Acceptance_Report.md)。

## Legacy / Target 双后端

统一入口的全量操作默认使用 Legacy。传入精确用例名时，CLI 尝试根据当前测试源码和 GN 目标识别后端，可用 `--backend` 或 `--target` 明确选择。Target 不支持的场景返回 BLOCKED。

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
python3 -B "$TEST/scripts/test.py" all --backend legacy --ci
```

构建需要 Clang/LLVM、GN/Ninja 和固定依赖。首次 `editor --backend both` 可接收 `--gtest-root`（也可用 `SIM_GTEST_ROOT`）、`--openssl-root` 和 `--runtime-root`。依赖要求见 [锁文件](framework/internal/host/configs/dependency_lock.json)。验证后的路径记录在构建输出的 `replacement.json`。默认输出为 `.host-out`，`--out` 和 `--output` 分别控制构建与报告目录。

在 VS Code Remote-WSL 打开仓库，首次运行 **Prepare Testing Panel**。打开测试 CPP，在 Testing 面板找到对应程序的用例，点击 Run 或 Debug Test。每个程序绑定自己的增量任务。添加用例后构建所属程序，再刷新 Testing 面板。使用 `@doc` 按当前 CPP 筛选。参见 [Host 使用说明](docs/Host_Usage.md#testing-面板源码定位和断点)。


常规 Primary 参考 cases/primary_test.cpp，同样只用 sim_test.h。精确阶段、迟到回包或错误恢复参考 cases/primary_protocol_test.cpp，额外用 sim_protocol_test.h 的 PrimaryProtocol；它只暴露请求事实与应答控制。卡状态竞态和生命周期参考 cases/lifecycle_test.cpp、cases/response_test.cpp，同样只用公开协议头的 StateProtocol。其他 internal 是平台、ABI/入口或对象图专项，普通业务测试不依赖其中头文件。cases/architecture 保留既有生产 testonly 可见性标签，不参与普通用例编写。

只读业务参考 cases/query_test.cpp，不要求双卡或 HIL。设备变更才按需检查资源、权限与安全恢复；缺能力是 BLOCKED/UNSUPPORTED。Host 与原生组件目标共用同一源清单，运行中服务的 SA/IPC 目标单独验证。原生调度使用 `test.py device --ohos-root <完整产品树>`，已有原生程序用 `--device-binary`；控制资源用私有 --resource-file。当前产品树/工具链/设备阻塞见验收记录。

每个运行目录保存 `result.json`、单例 XML、日志、退出码和计时。精确单例的默认开发报告保存 ELF 文件身份；`--full-evidence` 额外计算 ELF 和源码内容哈希。完整回归、双后端对比及 CI 保留完整内容证据。已发布的业务结果和本次开发效率实测分别见 [唯一执行状态](../../../docs/SIM_Refactor_Execution_State.md)及[专项报告](docs/SIM_Host_Development_Efficiency_Optimization_Report.md)。

[完整依赖审计、前后对比、原始证据与设备限制](docs/Full_Replacement.md)。

Testing 面板首次准备使用 `test.py editor`（VS Code 任务 `Prepare Testing Panel`）；源码定位及断点操作见[Host 使用说明](docs/Host_Usage.md#testing-面板源码定位和断点)。
