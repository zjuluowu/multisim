# SetPrimarySlotId Host 验证（SIM-028）

目前有两个互补目标，复用同一 GN/GTest 工程：

- `sim_primary_slot_entry_test`：14 条入口契约，执行真实 CoreServiceSim/Permission/Capability，ISimManager 为 StrictMock。不能推定真实 Manager/设备链通过。
- `sim_primary_slot_real_test`：7 条真实对象场景，公开构造 SimManager，按照 `CoreService::Init` 的 `SIM_SLOT_COUNT_MD + 1` 装配三个逻辑槽对象，对应两个物理槽。通过公开 CoreServiceSim::SetSimManager、CoreManagerInner::OnInit 连接同一真实 Manager/RIL。不使用私有注入或复制业务实现。

真实目标当前 **1 PASS / 6 FAIL**。六条在卡场景的业务断言及严格 RIL 期望验证通过，退出时同一真实订阅对象图残留，28 个 Host 工作线程未结束、RIL 引用数为10，清理断言失败。保持这些红灯和 GMock 泄漏诊断，不使用 AllowLeak、不忽略退出码。每个场景必须独立进程，runner 完整发现全部用例后逐例执行，防止残留回调进入下一场景。

| 真实场景 | 已实际验证的业务终点 | 整例结果 |
| --- | --- | --- |
| 初始化，无 RIL/Provider | 真实 OnInit；Manager→Controller 返回 NO_SIM_CARD；对象和线程可销毁 | PASS |
| C0，无需切换 | 由真实 GetRadioProtocol 查询应答确认槽1为M0；无 SetPrimarySlot/SetRadioProtocol；主卡参数、ICCID SHA256、Data 参数及主卡→Data广播 | FAIL，清理 |
| B，直接命令成功 | 精确槽1；原请求 owner/event 关联异步应答；应答前不保存；成功后提交及通知；无三阶段命令 | FAIL，清理 |
| B，设备失败 | 真实失败应答返回 LOCAL_PTR_NULL；保留旧主卡；无成功广播；setDone 只表示阶段结束 | FAIL，清理 |
| B，超时 | 等待生产45秒定时事件；返回失败、保留旧主卡、结束进行中状态 | FAIL，清理 |
| C1，成功/过期应答 | CHECK/UPDATE/COMPLETE各两成员；槽、modem、technology、session及原event关联；过期session不推进；UPDATE通知后COMPLETE；最终提交及Data联动 | FAIL，清理 |
| C1，CHECK失败 | 无UPDATE；失败COMPLETE使用旧资源；恢复旧槽/ICCID参数；五次真实重试，六个独立session、24条协议请求；失败不发成功广播 | FAIL，清理 |

精确结果、源码/二进制摘要、命令、XML及变异证据见 [本轮验收记录](../configs/sim028_real_chain_validation_20261111.json)。历史 [20261111入口记录](../configs/primary_slot_validation_20261111.json) 保持原字节；其中45对象/14字段阻塞是当时事实。

## 依赖与 Provider 契约

根 `bundle.json` 和生产 GN 实际依赖 `telephony_data:tel_telephony_data`；旧 Host 锁定的是 `telephony_data_storage` 老头。现在对初始化目标单独优先包含真实 `telephony_telephony_data@74b8597` 的公开头，保留旧锁和旧头。14字段的消费者函数、声明、SQL列/迁移默认值、URI及错误协议逐项见 [provider/contract.json](provider/contract.json)，公开头与许可证原样保存。50个 SIM 初始化对象已经编译通过，新目标实际链接并运行。

Host Provider 仅提供外部关系行读写，严格匹配真实 URI/谓词/列，按照 Provider 的 Update **状态码**协议返回；不代替 SIM 初始化、选卡或状态机。模拟一条已保存的激活账户，由真实 ICCID 通知/Monitor/RdbHelper 重新绑定并建立缓存。PIN锁定卡状态来自原 RIL 请求的应答；EF_ICCID/LI/PBR读取明确返回设备错误，STK也返回错误，不伪造文件内容。未运行完整 Records、真实RDB持久化/升级、Provider权限或eSIM。

需要固定版本 GTest、OpenSSL头及三个真实工具库源码；不安装系统包，不自动下载。准备流程：

```sh
# 从Git根执行。变量均指定你实际拥有的依赖路径。
SIM_TEST=telephony_core_service/services/multisim/test/systemtest
SIM_SOURCE_CACHE=/path/to/explicit-source-cache
SIM_NATIVE=/tmp/sim028-native
SIM_SSL=/tmp/sim028-openssl
SIM_GTEST=/path/to/pinned-googletest

python3 -B "$SIM_TEST/scripts/prepare_openssl.py" \
  --archive /path/to/libssl-dev_3.0.13-0ubuntu3.16_amd64.deb --destination "$SIM_SSL"
python3 -B "$SIM_TEST/primary_slot/prepare_runtime.py" \
  --source-cache "$SIM_SOURCE_CACHE" --output "$SIM_NATIVE"

# Debug构建，参数保存到.host-out/host.json；clangd读取真实编译数据库。
python3 -B "$SIM_TEST/tools/host.py" build --target sim_primary_slot_real_test \
  --gtest-root "$SIM_GTEST" --openssl-root "$SIM_SSL" --runtime-root "$SIM_NATIVE"

# 日常运行：自动增量构建、逐例独立进程、XML/日志及非零失败码。
"$SIM_TEST/tools/run_host_tests.sh" selected --target sim_primary_slot_real_test \
  --filter 'RealSimPrimary.*'
"$SIM_TEST/tools/run_host_tests.sh" all --with-real-primary

# 覆盖/因果证据及仓外Controller变异；当前整例有生命周期FAIL，退出1。
python3 -B "$SIM_TEST/scripts/run_primary_real.py" \
  --output /tmp/sim028-real-audit --build-dir /tmp/sim028-real-build \
  --gtest-root "$SIM_GTEST" --openssl-root "$SIM_SSL" --runtime-root "$SIM_NATIVE" \
  --mutation-check

# 编译/链接审计，不执行测试，不替代上面的运行验收。
python3 -B "$SIM_TEST/scripts/audit_initialization.py" \
  --target sim_primary_slot_real_test --output /tmp/sim028-link-build \
  --gtest-root "$SIM_GTEST" --openssl-root "$SIM_SSL" --runtime-root "$SIM_NATIVE"
```

源码缓存内须有以下干净Git仓库，脚本拒绝错误版本、暂存或未暂存的修改，仅编译跟踪的源码；可从其官方仓库获取对应提交。依赖路径/编译命令/归档SHA写入 `runtime_dependencies.json`：

| 缓存目录 | 固定提交 |
| --- | --- |
| third_party_bounds_checking_function | 61baa7a3eb9347efca58397d1d0504faa96d79f0 |
| third_party_cJSON | 28dba43de56de279d1b5b74565f4277c89c50f35 |
| commonlibrary_c_utils | 3bdc11e00e7bf6e577cee300ad51045ca9958efd |

编译使用真实 securec/cJSON/string_ex/unicode_ex，OpenSSL链接当前Linux `libcrypto.so.3`。C++17、Clang18、libstdc++。没有禁用访问检查或忽略链接错误；未实现的Parcel序列化一旦进入就抛错。完整编译数据库包含可选目标，精确链接闭包以审计 `target-commands.log`、link map和函数执行记录为准。

## 入口回归与变异

```sh
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh selected \
  --target sim_primary_slot_entry_test --filter 'PrimaryEntry.*'
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_primary_slot.py \
  --output /tmp/sim-primary-entry --entry-only --mutation-check
```

上面的筛选是12条 PrimaryEntry；独立入口审计运行全部14条。入口覆盖六个if的12侧、31个代码区域；原userSet true→false变异由7条断言检出。新Controller变异只在仓外副本将保存目标变成slotId+1，由 `PRIMARY_COMMIT_TARGET` 新失败检出；判定比较业务断言，不能把已有清理失败的非零码当作变异检出。原源码及原二进制摘要保持不变。

入口套件保留系统应用/权限/对象/能力/在卡/激活的错误优先级、原slot及userSet、返回码和偏好写入顺序，包含原HasSimCard返回码未使用、偏好写失败仍转发的characterization；不将当前缺陷批准为未来规则。生成的内部接缝仍可用 `generate_manager_seam.py --check` 审查。

## 调试和未解除的边界

VS Code在Remote-WSL打开Git根。先执行一次上述依赖配置，然后用 `Build Real SIM Host`、`Run Selected Test` 或 `Run All Tests + Real SIM`；F5选择 `SIM-028: real C0 (WSL, known lifetime failure)`。CLI：

```sh
SIM_HOST_BOOT_SLOT_COUNT=2 lldb -- .host-out/build/sim_primary_slot_real_test \
  --gtest_filter=RealSimPrimary.C0CoreToRealManagerAndController
# LLDB: breakpoint set -f multi_sim_controller.cpp -l 1485
# run; bt; frame variable slotId isUserSet
```

`SIM_HOST_BOOT_SLOT_COUNT=2`为生产全局构造阶段的显式外部参数输入，必须在进程启动前设置；不重置生产静态缓存。runner和F5已经设置。二进制拒绝通配符多例运行，使用runner逐例隔离。TestMate可通过GTest列表发现7例，GUI单例运行/断点/变量仍需人工检查；扩展直接把通配符传给二进制的“全部运行”应改用上述Task，不宣称GUI整组运行通过。

真实残留链为 `SimStateHandle → ObserverHandler(强引用) → MultiSimMonitor → Controller/StateManager → SimStateHandle`，Monitor同时订阅File。真实SimManager析构已执行，Monitor/Controller/StateManager析构未执行，28线程及10个RIL引用可复核。Monitor有公开UnRegisterSimNotify，但Manager未提供可访问的完整停止入口。需要另行授权生产生命周期方案：停止新回调/排空在途工作，再在释放强引用前解除订阅；本轮不修改生产析构，也不通过私有访问清环。

| 边界 | 当前证据与最小条件 |
| --- | --- |
| A厂商映射 | 未运行；真实swapM0M2SimCards厂商函数/库及四槽配置未提供，不能把wrapper缺省-1说成映射验证 |
| CoreService/Stub | compile-only重跑仍缺system_ability.h、BrokerDelegator/Proxy构造及Parcel/IPCObjectStub运行契约；真实CoreService::Init没有执行 |
| Host IPC | 仅Creator身份令牌；SA订阅明确返回不可用、其他SA不存在、Parcel不支持；不是IPC分发模拟 |
| TelRilManager/TelRilSim/HDI | compile-only重跑缺v1_5/iril.h；RIL接口请求/应答由严格Mock隔离，session是业务协议session，未生成或验证真实RIL serial |
| OHOS IPC/产品 | 独立fork无匹配完整产品树、samgr/Binder/HDI；本WSL无/dev/binder、/dev/hwbinder；未做产品/设备集成验收 |

在匹配OHOS完整树中挂载为 `base/telephony/core_service`，按仓内构建规则配置产品后运行 `hb build -T //base/telephony/core_service:tel_core_service` 和 `hb build -T //base/telephony/core_service/test:unittest`，在对应设备运行生成的CoreService/SIM/TelRil套件并核查真实SA、权限、reply、serial/HDI。当前Host套件不是OHOS产品测试目标，不能直接用hb宣称运行了这7条。完整产品树、生成HDI头及设备是继续原生集成的必要输入。

C1的UPDATE/COMPLETE异常、C1超时、任意重复/乱序组合、在途销毁重建和跨重启的持久化恢复本轮NOT_RUN。既有五条独立业务缺陷继续单列；六个新增清理失败共享同一个残留对象图原因，未将其归入原五条缺陷或从统计中移除。
