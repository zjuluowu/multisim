# SIM 子系统当前源码基线

日期：2026-10-15；工作仓zjuluowu/multisim，main@df8f2f017f83c70145b20255b4e9042f7a5fd923，加未提交工作树。本文为静态FACT摘要，来源文件摘要见[清单](design_alignment_manifest.json)。不将旧上游cd3bca9或另一分支Shadow当成本仓现状。

## 1. 功能与仓库边界

Core SA 4010承接SIM和搜网公开入口。SIM生产实现包括卡状态、PIN/PUK/认证、UICC文件、账户、多卡默认角色、主卡/RadioProtocol、SIM短信存储、电话簿、STK、运营商配置及条件eSIM运行能力。

完整通话、短彩信、蜂窝数据连接和数据提供方业务属于独立仓；本仓的SIM短信存储与RIL短信信令不能等同短信服务整体。网络注册、通话、数据和IMS权威状态不迁入SIM聚合。

## 2. 构建和部署事实

| 事实 | 当前源码依据 | 限制 |
|---|---|---|
| SIM源仍编入Core共享库 | BUILD.gn:30–118及后续条件sources | 未形成独立SIM生产SO，不证明host可直接构建OHOS目标 |
| 4010与66250配置共宿主 | services/etc/init/telephony_trust.json | 配置不是设备装载/故障隔离实测 |
| Core组装RIL、eSIM、SIM、搜网 | services/core/src/core_service.cpp:92–128 | 当前装配不是目标工厂/Stop协议已完成 |
| eSIM客户端加载66250 | frameworks/native/src/esim_service_client.cpp:25–90 | 服务实现、任务恢复需外仓核实 |

eSIM、卫星、EXT类型保持各自条件编译。不新增SA、进程或IPC；同进程SO只形成链接和依赖边界。

## 3. 入口和数据流

公开服务路径为Framework/Client→Proxy/Stub→Core服务入口→ISimManager/IEsimManager→生产Manager/Handler/RIL。共宿主CoreManagerInner还提供直接C++委托，不能只审IPC码表。

ISimManager是公开InnerKit组件接口，OnInit、状态、安全、账户和角色请求可进入SimManager；组件接口与真实SA/IPC/权限不是同一层级。当前两个Manager接口无虚析构，具体类型共享控制块或工厂deleter管理销毁，不能delete接口裸指针。

SIM经CoreManagerInner获取非SIM能力、公共事件等；目标机械拆分需保留消费侧门面并以内部注入端口处理反向符号依赖，不能直接删门面或移动全部Core实现。

## 4. 当前对象与身份

SimManager:61–66按slot组织状态、文件、短信、账户、电话簿、STK等集合；MultiSimController维护账户缓存、默认角色和主卡/Radio联动。当前位置索引、账户标识和设备资源不能当同一种身份。

- 现有simId是不透明账户ID，不等于slot；跨删除/重装稳定性待确认。
- 空槽、PIN/PUK、卡应用初始化可在没有有效账户时发生。
- Slot合法性、是否配置、卡在位、账户可见、权限和底层能力分别判断。
- 当前参数默认槽数1，含分布式多槽分支；RadioProtocol:40初始化最多两个槽。不能宣布所有产品固定三卡或已支持四卡/MEP。

## 5. 状态与异步

公开SimState为UNKNOWN、NOT_PRESENT、LOCKED、NOT_READY、READY、LOADED六值。HasSimCard、Active、Profile启用、Records完成、角色和网络就绪不是等价状态。

Handle接收RIL状态并更新外部状态；Manager安全流程可能设置NOT_READY；IccFile/SimFile的加载路径回写LOADED并发事件。SimFile:244–261中Records通知与LOADED回写有先后，但不能据静态顺序推导所有外部消费者的全序。

当前同时存在std::mutex、ffrt::mutex/shared_mutex；新增同步采用仓库要求的ffrt原语，不声称迁移已彻底完成。风险入口包括技术超时进入通用完成通道、未判end的日志映射、同状态身份更新及加载轮次；均需合法边界注入才能确定运行影响。

## 6. 存储与默认角色

SimRdbHelper:94–136通过DataShareHelper执行账户CRUD。本轮当前SIM范围未检出NativeRdb/RdbStore直写事务实现；不能因名称RdbHelper断言本仓已有直接DB连接或统一事务。参数、缓存和提供方存储的原子性分别审计。

MultiSimController:1522、1689显示Primary成功/保存路径会联动默认Data；目标模型保持该兼容行为，不将Primary统一替代全部Voice/SMS/Data。缺卡时角色值、账户保留及Profile停用可见性仍为OPEN。

## 7. 事件与安全

Core内有Registry更新、CES发布、RadioEvent、账户回调、Hub及eSIM回执等不同机制。STK发布可由StkController:361–557核实；消费者及Registry中继/sticky需外仓证据。不擅自统一去重或用目标总线重排完成点。

capability_mgr:28–41对非SLOT_3_INDEX返回false，随后检查token类型或系统应用。该源码规则与当前服务入口阻塞保留；组件测试不覆盖、绕过或认定其为缺陷。敏感身份只作内存关系校验，不进入结果明文。

## 8. 当前实现与执行状态

当前生产目录没有新三聚合、SlotRegistry、Subscription Shadow、DomainEventBus及相应追加InnerKit方法。旧文档“已完成”属于另一仓库/分支，见[进展索引](07_subscription_refactoring_status.md)。

2026-10-08记录：容量查询B层1 PASS、设施16 PASS、容量检查器负控预期FAIL；初始化目标45个对象已编译但整体BLOCKED，原五业务场景未运行。未经过真实SA、Client/Proxy/Stub、真实权限/capability、Modem和设备行为。本轮文档刷新未重跑测试。

## 9. 当前与目标的使用规则

[SAD](../SIM_Refactored_Software_Architecture_Design_Version3.md)和[类设计](../SIM_Layered_Architecture_Class_Design_Version3.md)是目标，不覆盖上述现状；[统一裁决](08_design_unification_decisions.md)规定如何迁移。旧功能/API全量资料保存在[历史01](../history/pre_unification_20261015/MultiSIM/01_sim_subsystem_baseline.md)，使用时必须复核源码与契约。
