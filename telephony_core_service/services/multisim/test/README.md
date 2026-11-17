# SIM 测试

普通用例只引用 `sim_test.h`。从 `cases/sim_card_test.cpp` 或 `cases/developer_copy_test.cpp` 复制一个 `TEST_F`，修改准备、操作和断言：

```cpp
#include "sim_test.h"
using namespace SimGuard;
TEST_F(SimTest, MyDualSimPinUnlock)
{
    auto a = AddCard("A", 0, CardKind::Usim, State::Ready);
    auto b = AddCard("B", 1, CardKind::Usim, State::Locked, Lock::Pin);
    Start();
    EXPECT_EQ(UnlockPin(b, "1234").result, Unlock::Ok);
    ExpectCardState(a, State::Ready);
    ExpectCardState(b, State::Ready);
    ExpectStateNotification(b, State::Ready);
}
```

从仓库根运行。需要 Linux x86_64、Clang/LLVM、GN/Ninja，以及固定依赖（见 `framework/host/configs/dependency_lock.json`）。`SIM_GTEST_ROOT` 指向已准备的 GoogleTest 源码；完整 Primary 另需 `--openssl-root` 和 `--runtime-root`，首次构建后路径保存在输出目录的 replacement.json。

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/test.py" build --target sim_test_host
python3 -B "$TEST/scripts/test.py" run --target sim_test_host --filter SimTest.DualSimPinUnlock
python3 -B "$TEST/scripts/test.py" debug --target sim_test_host --filter SimTest.DualSimPinUnlock
python3 -B "$TEST/scripts/test.py" all --ci
```

追加 TEST_F 只改用例文件；新建 cases/*.cpp 后，构建自动更新显式 GN 清单。每个用例独立进程，结果目录保存 XML、日志、退出码与 result.json。五个原 Host 缺陷仍为 FAIL；只有精确冻结签名可由 CI 接受，其他失败阻止准入。真实 Primary 生命周期失败仍阻止当前完整 CI。

VS Code Remote-WSL 打开仓库，运行 **Build Host**（真实 Primary 用 **Build Real SIM Host**），Testing 面板刷新后点击单例 Run/Debug。在 `SimStateManager::UnlockPin` 等生产函数打断点；无需逐条配置 launch.json。当前已验证 CLI 发现/单例执行及 LLDB，GUI 点击与独立初级开发人员验收尚未执行。

真实 Primary、容量、入口、查询和生命周期使用各自公开 Fixture 头；独立生产切片保留多个二进制，均由 test.py 调度。精确迟到/重复回包参考 cases/lifecycle_test.cpp；普通场景无需高级设施。

OpenHarmony 原生目标在 test/BUILD.gn，复用相同源清单。`sim_test_device` 为真实 SA/IPC 集成，`sim_test_device_component` 及其他组件目标为受控组件层。通过 `test.py device --ohos-root <完整产品树> --native-target <目标>` 构建；已有原生程序用 `--device-binary <程序>` 部署。真实服务场景另需 `--resource-file` 指定受控设备资源，不能自动制造的状态会 BLOCKED/UNSUPPORTED。当前缺完整产品树、工具链、HDC/设备和资源控制器，原生编译及运行未验收。

完整结果、逐例映射、退役清单和资源协议见 [Full_Replacement.md](docs/Full_Replacement.md)。历史证据不参与活动执行。
