# eSIM、Slot与服务边界：统一专项说明

日期：2026-10-15。采用[DU-06/08/10/15](08_design_unification_decisions.md)和[SAD](../SIM_Refactored_Software_Architecture_Design_Version3.md)。现状、产品目标与服务实现推断分别标明。

## 1. 已核实的现状

- frameworks/native/src/esim_service_client.cpp:29定义SA 66250，客户端通过既有系统能力加载路径获取服务。
- services/etc/init/telephony_trust.json将4010、66250等配置在telephony；SA身份独立不等于独立进程或故障隔离，未据此宣称设备已装载。
- CoreService::Init在eSIM条件分支创建EsimManager，并保留SimManager与RIL/搜网装配。
- interfaces/innerkits/IEsimService.idl声明下载、OSU、库存和SwitchToProfile(slotId,portIndex,iccId,...)等任务入口；IEsimManager是Core内运行接口，不能混为同一层。
- 当前eSIM对象与卡侧请求仍以slot路由，接口有Port并不证明账户持久映射、通知、权限和设备链路已完整MEP支持。

## 2. 产品配置与身份

目标产品可按两个pSIM位置0/1和从2起的eSIM逻辑位配置扩展；它是条件目标，不是全产品固定硬件事实。逻辑位不直接表示独立eUICC、物理槽或Modem。SlotRegistry只描述配置/事实，不决定调用者权限。

账户目标使用simId；空槽查询使用SlotTarget；PIN/PUK、EF/APDU等使用CardApplicationTarget。Port、Profile、路由引用按已验证来源解析，不从slot、ICCID、标签或simId猜测对应关系。历史账户可没有当前绑定。

## 3. 两个通道保持，事实Owner统一

| 能力 | 当前契约与目标协作 | 未覆盖边界 |
|---|---|---|
| 卡侧APDU/逻辑通道/原语 | 保留Core卡侧生产实现，由内部执行Adapter承接 | 不把所有原语转到66250形成递归 |
| 下载、OSU、配置层任务 | 保留IEsimService及回执；服务内步骤不由本仓复制 | 主编排/恢复需服务实现仓核实 |
| 账户和绑定 | 目标Topology接受观察并提交 | 不靠服务成功回执直接构造账户PASS |
| 主卡/角色/资源 | 目标Assignment按契约决策 | 不等同驻网或数据连接成功 |
| 本仓操作等待与门控 | 目标Operation关联请求、步骤、尝试和会话 | 不复制远端完整任务，也不假设可查询远端OperationId |

“单一事实源”不表示只有一个服务入口。单个业务命令只确定一个主编排；Core与66250不各自盲目补偿或重试，不能Core→66250→Core→66250循环转办。

## 4. 异步、重入与恢复

发起时固定既有回调/serial及内部上下文，不向旧IPC追加OperationId。命令响应走门控，非请求通知走Observation；绑定代次与同卡Records加载轮次分开。

Core锁内不得等待66250、DataShare或外部回调；反查与停止时要核对执行域和对象寿命。取消等待不等于真中断，超时不等于没有设备副作用。运行期操作重启后可丢失，依据既有持久事实和设备重查恢复，不承诺同ID重放。

恢复出厂是否联动66250仍需明确来源和授权；未确认不增加远端清理、Profile删除或eUICC重置。Linux默认只使用合成受控外部边界，不操作真实SIM。

## 5. SO与测试

Core内SIM/eSIM Runtime可作为同进程SO拆分候选；独立66250服务保持原身份/入口。工厂、销毁和停止排空必须设计；Manager接口无虚析构，不能裸接口delete。当前无独立SIM SO构建和真实Split执行结果。

B组件测试不经过实际SA/IPC/权限/Modem；A设备套件单独验证。完整旧分析保存在[历史03](../history/pre_unification_20261015/MultiSIM/03_esim_slot_and_sa_boundary.md)，其中推断隔离收益与主键迁移建议已由当前裁决取代。
