# SIM 测试（全量重建中）

普通测试从[cases/sim_card_test.cpp](cases/sim_card_test.cpp)复制一条TEST_F，只改业务准备、操作和断言：

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

从仓库根运行，SIM_GTEST_ROOT由环境提供，指向已准备的GoogleTest源码：

```sh
TEST=telephony_core_service/services/multisim/test
python3 -B "$TEST/scripts/build_replacement.py"
python3 -B "$TEST/scripts/run_gtest.py" --binary .host-out/build/sim_test_host \
  --filter SimTest.DualSimPinUnlock --output /tmp/sim-test-results
```

追加TEST_F只改用例文件；新建cases/*.cpp后，构建工具自动更新显式GN源清单。用例结果目录含逐例XML、日志和result.json；每条用例在独立进程运行。完整新Host50项包含5个保留的生产缺陷FAIL。

VS Code通过Remote-WSL打开仓库，运行“Build Replacement Host”，Testing面板刷新后点击单例Run/Debug；在真实生产函数设置断点即可。统一LLDB模板自动传入选中的用例，无需逐条修改launch.json。真实Primary继续每进程一个测试。GUI点击与独立开发人员验收尚未完成。

需要精确控制旧回包、延迟、错误或重建时，参考cases/lifecycle_test.cpp的高级接口。普通用例不需要理解这些设施，也不需要维护历史Golden或注册表。

**尚未完成全量替换。** 当前已迁移原64项中的49项并新增双卡PIN场景；其余容量/主卡、G2a、真实生命周期、CI切换和旧框架退役仍需完成。Device Framework/产品GN和设备执行尚未交付。[完整旧资产处置、基线、实测、失败及待办](docs/Full_Replacement.md)为当前重建记录。旧工具暂作对照，不得把双体系作为最终交付。
