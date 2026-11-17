# SIM 业务测试

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

构建需要 Clang/LLVM、GN/Ninja 和固定依赖；环境维护者提供 SIM_GTEST_ROOT，以及首次构建的 --openssl-root、--runtime-root，锁见 framework/internal/host/configs/dependency_lock.json。路径保存到输出目录 replacement.json。默认输出 .host-out；可用 --out 和 --output 隔离构建及报告。普通用例不管理 Mock、线程或资源清理。

VS Code Remote-WSL 打开仓库，执行 **Build Host**，Testing 面板刷新后点击单例 Run/Debug。真实 Primary 用 **Build Real SIM Host**；Testing 面板会自动关联程序，每例独立进程。在真实生产函数设断点即可，无需逐例修改 launch.json。CLI/LLDB 和 GUI 的实测状态分别记录，未把配置检查算作 GUI 操作成功。

常规 Primary 参考 cases/primary_test.cpp，同样只用 sim_test.h。精确阶段、迟到回包或错误恢复参考 cases/primary_protocol_test.cpp，额外用 sim_protocol_test.h 的 PrimaryProtocol；它只暴露请求事实与应答控制。卡状态竞态和生命周期参考 cases/lifecycle_test.cpp、cases/response_test.cpp，同样只用公开协议头的 StateProtocol。其他 internal 是平台、ABI/入口或对象图专项，普通业务测试不依赖其中头文件。cases/architecture 保留既有生产 testonly 可见性标签，不参与普通用例编写。

只读业务参考 cases/query_test.cpp，不要求双卡或 HIL。设备变更才按需检查资源、权限与安全恢复；缺能力是 BLOCKED/UNSUPPORTED。Host 与原生组件目标共用同一源清单，运行中服务的 SA/IPC 目标单独验证。原生调度使用 `test.py device --ohos-root <完整产品树>`，已有原生程序用 `--device-binary`；控制资源用私有 --resource-file。当前产品树/工具链/设备阻塞见验收记录。

每个结果目录含 result.json、单例 XML、日志及退出码。五个冻结 Host 缺陷保持原始 FAIL；只接受其精确签名。真实 Primary 生命周期失败继续阻止完整 CI，不因业务断言通过而消失。

[完整依赖审计、前后对比、原始证据与设备限制](docs/Full_Replacement.md)。

Testing 面板首次准备使用 `test.py editor`（VS Code 任务 `Prepare Testing Panel`）；源码定位及断点操作见[Host 使用说明](docs/Host_Usage.md#testing-面板源码定位和断点)。
