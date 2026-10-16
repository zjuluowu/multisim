# SIM重构与防护网当前进展

日期：2026-10-15。工作仓zjuluowu/multisim/main@df8f2f017f83c70145b20255b4e9042f7a5fd923，工作树有未提交变更；这是普通Core目录，不采用旧记录中的另一仓库gitlink/分支状态。

## 1. 当前设计

[SAD v0.3.1](../SIM_Refactored_Software_Architecture_Design_Version3.md)及[类设计](../SIM_Layered_Architecture_Class_Design_Version3.md)采用四Context、三聚合、三类目标和一个ACL，详见[统一裁决](08_design_unification_decisions.md)。设计尚未实施，Phase0 NOT READY。

## 2. 当前生产实现

| 项目 | 当前工作树状态 |
|---|---|
| Subscription Shadow/Registry/Role Projection | 未发现旧文档宣称的对应生产类型或追加接口；其他分支记录不算本仓实现 |
| 三聚合与Owner、SlotRegistry、内部DomainEventBus | 候选设计，未落地 |
| SIM独立生产SO与工厂/Stop协议 | 未落地；SIM仍编入Core目标 |
| 公共API/IPC/SA/schema迁移 | 本轮未变更，目标冻结 |
| 四槽、NULL迁移、默认策略/恢复 | 条件目标/待补决策与提供方证据，不宣称产品支持 |

旧分支的完整Shadow记录保存在[历史07](../history/pre_unification_20261015/MultiSIM/07_subscription_refactoring_status.md)，不丢失，但不继续以“当前已完成”展示。

## 3. 独立Linux测试实际记录

来源：[run_record](../../test/systemtest/docs/run_record.md)中的2026-10-08记录。本轮没有重跑测试，不把文档更新当新一次执行。

| 分组 | 设计 / 执行 | 覆盖状态 |
|---|---|---|
| SIM-COMP-001容量 | 1 PASS，执行SimManager::GetMaxSimCount生产逻辑 | B组件；未OnInit，不代表无卡/就绪 |
| 平台适配设施 | 16 PASS | INFRA，不增加SIM业务完成数 |
| 容量检查器负控 | 1项预期FAIL，检测能力已验证 | 观察证据受控破坏，不是生产故障或新业务例 |
| SIM-B-001服务入口 | BLOCKED，未运行 | capability/实际入口依赖仍独立阻塞，不绕过 |
| SIM-B-002无卡+账户 | BLOCKED，未运行 | 初始化/存储协议/正式RIL装配缺口 |
| SIM-B-003 Ready | BLOCKED，未运行 | 同一生产初始化闭包及身份/账户Oracle前置 |
| SIM-B-004双卡隔离 | BLOCKED，未运行 | 产品拓扑、隔离契约与初始化前置 |
| SIM-B-005 PIN Locked | BLOCKED，未运行 | 受控锁状态协议与初始化前置；不操作真实PIN |
| SIM-CHAR-001未初始化候选 | BLOCKED，未执行 | 未采集Legacy基线，不冒充Absent |
| Legacy/Split双版本 | 仅有容量比较工具；无真实Split结果 | 不复制同一结果替代双运行 |

初始化审计45个生产对象已编译，整体仍BLOCKED/未执行；43条诊断涉及14个SimData协议符号，链接阶段未到达。对象数不是用例数，也不能保证补头后即可运行。匹配HUKS/参数等声明编译不等于平台服务已验证。

## 4. 分层和限制

B默认：独立GoogleTest进程→ISimManager组件→真实SIM生产逻辑→GoogleMock正式外部边界。工程固定test/systemtest/，GN/Ninja+固定GoogleTest/GoogleMock，不依赖旧test目标/Mock/Fixture。

未覆盖真实SA注册发现、Client/Proxy/Stub、未经过的服务入口检查、真实权限/capability、Modem和实际设备行为。A设备测试单独验收；没有实体设备不成为B场景的阻塞理由。

## 5. 下一步

1. 补D-/X-决策与匹配SimData来源，确认无卡/未就绪/账户语义和正式输入。
2. 审计真实生产链接与受控外部运行服务，禁止用SIM业务mock补内部符号。
3. 通过组件公开初始化、正式RIL响应建立Absent，公开查询状态和账户，有界收敛、隔离清理、XML/脱敏JSON与有效负控齐全。
4. G1通过后才推进G2同进程SO机械拆分，再按三个Owner逐条迁移；产品扩容属于G4。

当前不使用输入“3/35”统计，因为当前仓缺少其分母与执行主档。任何未执行项保持BLOCKED/SKIP，不删除失败例、绕过capability或升级为完成。
