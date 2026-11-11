# SetPrimarySlotId：入口覆盖与整链阻塞

本增量对应 SIM-028。输入说明是仓内 `docs/SetPrimarySlotId_重构前后实现对比说明.md`；按整个 telephony_core_service 部件理解用户要求，包括 Stub/CoreService/CoreServiceSim、SimManager/Controller、RadioProtocol/TelRil。**当前尚不能保证这条整链完整执行。** 没有修改生产实现或通过私有访问绕过装配。

已经可执行的是 `sim_primary_slot_entry_test`：编译原始 `core_service_sim.cpp`、`telephony_permission.cpp` 和 `multi_sims_capability_manager.cpp`。14条用例通过 `CoreServiceSim::SetSimManager` 这一现有公开装配接口接入明确的内部 ISimManager 测试接缝。后者不是真实 SimManager，不能把该套件称作整个部件端到端测试。外部 AccessToken/IPC身份/系统参数由严格Mock提供；SIM入口、权限策略及槽能力策略均执行真实生产函数。

## 运行

从Git根执行；固定GTest依赖沿用 [HOST.md](../HOST.md)，不自动下载或安装：

```sh
# 快速执行（自动增量构建）；也可通过 VS Code Run Selected Test 选择新目标。
./telephony_core_service/services/multisim/test/systemtest/tools/run_host_tests.sh selected \
  --target sim_primary_slot_entry_test --filter 'PrimaryEntry.*'

# 覆盖审计：要求入口六个if的两侧和全部代码区域执行、四个真实函数正执行。
# 默认整链验收仍返回2/BLOCKED，entry_contract_status另列PASS。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_primary_slot.py \
  --output /tmp/sim-primary-audit --mutation-check

# 明确只验收入口接缝时使用；全套14例和覆盖检查通过才返回0。
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_primary_slot.py \
  --output /tmp/sim-primary-audit --entry-only
```

每次在output下建立独立execution目录，保存XML、日志、覆盖和result.json；每个输出目录持独占锁。`--mutation-check`只在仓外生产源码副本将实际转发的 `isUserSet=true` 改为false，必须触发断言失败，随后重建原生产源码并重新通过。入口审计PASS不改变whole_component_chain=BLOCKED。完整仓内调用链的缺失执行函数逐一登记；聚合函数计数也不等于单一场景的跨线程/IPC因果证明。

## 用例和断言

| 范围 | 覆盖内容 |
| --- | --- |
| 错误顺序 | 非系统应用先于权限；权限先于空Manager；空Manager先于扩展槽能力；能力失败先于SIM访问 |
| 业务守卫 | 无卡不查激活、不写偏好；未激活不写偏好、不发切换；严格Mock拒绝多余调用 |
| 正常转发 | 先在卡、再激活、再写USER_PREFER_PRIMARY_SLOT=true、最后精确slotId和userSet=true；下层返回原样交付 |
| 调用身份 | HAP系统应用、native、shell执行真实身份策略；扩展槽使用源码常量SLOT_3_INDEX，而非推测数字 |
| 当前行为刻画 | HasSimCard返回码被忽略、SetParameter失败仍转发；明确标characterization，不批准这些行为为未来业务规则 |
| 能力拒绝分支 | 外部SystemApp判断在入口与能力复查之间由true变false，显式模拟策略服务变化/异常；稳定同一调用者通常无法走到该拒绝分支，不能说它是普通产品场景 |

`unscripted_sim_manager.h`按真实ISimManager自动生成；除三个脚本化方法外，所有接口调用均抛错，没有SIM业务实现。`generate_manager_seam.py --check`拒绝接口漂移。`primary_slot/include`中的头是外部API声明子集，实际运行仅涉及token查询/验证和SetParameter；Bundle/Privacy相关未覆盖API没有成功实现。头路径仅对新目标生效，不修改原测试使用的平台头。它们不证明OHOS ABI或真实身份服务正确性。

## 整链为什么仍阻塞

实际重跑初始化编译审计，显式复用并校验本地锁定OpenSSL开发包，未安装系统包。45个SIM对象编译成功、没有缺失头文件，但有43条诊断：Provider的14个SimData字段不匹配（PHY_CARD/LSI/SIM_LABEL_INDEX/IS_ESIM/MNC_LEN/OPERATOR_NAME/MCC/MNC/GID1/GID2/SPN/OPKEY/EHPLMN/EFUST）。该闭包未链接、未运行，不猜测字段或Provider成功值。

CoreService/Stub原始源码另作Host编译审查，现有边界还缺system_ability.h、BrokerDelegator、IRemoteProxy构造、MessageParcel接口令牌及IPCObjectStub等。进一步执行CoreService::Init还会触及真实RIL、账户Provider、CommonEventHub和搜网装配。CoreService的CoreServiceSim成员私有，合法成功装配发生在Init；不能靠重写CoreService、替换仓内业务对象或私有指针注入宣称整链覆盖。

最小后续条件是提供匹配本fork的Provider字段/URI/schema/错误协议及可验证的SA/Binder/HDI Host运行边界，或在匹配OHOS产品树中运行真实装配。A/B/C0/C1分支、三阶段协议、参数保存/恢复、Data联动、重试、账户通知及新multisim执行链均未由本套件运行。当前结果和实际阻塞日志见 [primary_slot_validation_20261111.json](../configs/primary_slot_validation_20261111.json)；既有五条SIM Guard缺陷保持独立红灯。
