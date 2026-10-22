# SIM 管理对外接口说明——源码核实版

## 1. 核实范围与结论

### 1.1 本次工作性质

本次仅执行源码读取、接口比对与文档整理：

- 不修改接口名称和签名；
- 不新增、删除或合并接口；
- 不调整 IPC 事务码；
- 不修改枚举、常量、结构体和序列化格式；
- 不修改实现、调用方、构建配置或权限；
- 不向仓库写入文档或代码。

本文按能力归类，仅为阅读组织方式，不代表建议在代码中重新划分或修改接口。

### 1.2 源码基线

| 项目 | 内容 |
|---|---|
| 仓库 | `zjuluowu/multisim` |
| 核实日期 | 2026-10-22 |
| 本次读取版本 | `db876e551a3fac0922f98c7b358392326663ccc1` |
| 代码目录 | `telephony_core_service/` |
| 历史报告基线 | DEV core_service `738f51f6` |

本次读取版本与 SIM-ANL-004 的历史基线不同。

因此：

1. 当前源码中的声明可以作为本次接口存在性证据。
2. 历史报告中的接口、数量、调用方和行号不能��接套用。
3. 当前声明中未出现的历史接口，应标记为“历史报告记载，本次基线未确认”，不能据此认定该接口在所有版本中不存在。
4. 当前仓库不包含历史报告中所有消费仓，无法重新证实其全部调用关系。

### 1.3 本次核实深度

已读取并比对：

- `i_core_service.h`
- `core_service_client.h`
- `core_service_ipc_interface_code.h`
- `core_manager_inner.h`
- `core_manager_inner.cpp`
- `i_sim_manager.h`
- `i_esim_manager.h`
- `IEsimService.idl`
- `esim_service_client.cpp`
- `sim_state_type.h`

同时检查了部分 SIM、RIL 方法声明与实现片段。

本文不是完整逐方法运行时验证报告，尚未逐项核对所有：

- proxy/stub 分发；
- 条件编译与链接导出；
- 权限检查；
- 错误码和完成语义；
- 外部消费者；
- 产品部署和设备运行结果。

接口声明存在，不等于已完成上述验证。

---

## 2. 接口分层

不能将“定义在 interfaces 目录”“CMI 可调用”“SA4010 可调用”和“应用公开 API”视为同一含义。

| 层级 | 代码载体 | 本文解释 |
|---|---|---|
| 应用绑定层 | JS/ETS、仓颉等绑定及 kits 声明 | 面向应用的语言入口，需另核 API 级别和权限 |
| SA4010 服务契约 | `ICoreService`、`CoreServiceClient`、IPC 枚举 | 跨进程服务声明与客户端入口 |
| CMI 协作契约 | `CoreManagerInner` | manager 协作访问入口，不等同于 SA4010 IPC 方法集 |
| manager 抽象 | `ISimManager`、`IEsimManager` | 模块实现契约，含初始化和内部协作方法 |
| eSIM 服务契约 | `IEsimService.idl`、`EsimServiceClient` | 连接 SA66250，与 SA4010 分开记录 |
| 下层设备契约 | `ITelRilManager`、`TelRilSim`、HDI | 底层卡操作，不应冒充同名上层接口 |

### 2.1 CMI 调用形态的核实

本次读取的 `CoreManagerInner`：

- 在 `OnInit` 中保存 manager 对象；
- 已核对的 SIM 方法直接调用 `simManager_`；
- eSIM 方法直接调用 `esimManager_`；
- 对象为空时返回对应错误或默认值。

因此，上版“CMI 进程内直调/跨进程经代理”的概括不适合作为本次基线结论。

应表述为：

> 本次核对的 CMI 实现为对象委托调用；跨进程访问应单独依据 CoreServiceClient/Proxy 链路确认。

证据：S5，40～62、1981～2050、2404～2475、2789～2833 行。

---

## 3. SA4010 已声明的 SIM 接口

本节主要依据 `ICoreService`，列出当前已经确认声明的相关方法。

“已声明”不表示所有方法均已完成 proxy/stub、权限和运行时验证。

### 3.1 卡存在性、状态与类型

| 方法名 | 功能 |
|---|---|
| `HasSimCard` | 查询卡是否存在 |
| `GetSimState` | 查询卡状态 |
| `GetCardType` | 查询卡类型 |
| `IsSimActive` | 查询卡激活态 |

证据：S1，65～89 行。

注意：

- 当前方法名为 `GetCardType`。
- `GetSimState` 使用 `SimState`，不能与 `IccSimStatus` 混写。
- `GetSimIccStatus` 出现在 CMI/manager 契约中，不在本次读取的 `ICoreService` 声明中。

### 3.2 卡身份与卡信息

| 方法名 | 功能 |
|---|---|
| `GetSimOperatorNumeric` | 查询卡侧运营商编码 |
| `GetISOCountryCodeForSim` | 查询卡侧国家码 |
| `GetSimSpn` | 查询服务提供商名称 |
| `GetSimIccId` | 查询 ICCID |
| `GetIMSI` | 查询 IMSI |
| `IsCTSimCard` | 查询指定卡类型判断结果 |
| `GetLocaleFromDefaultSim` | 获取默认卡语言相关信息 |
| `GetSimGid1` / `GetSimGid2` | 查询组标识 |
| `GetSimEons` | 查询增强运营商名称 |
| `GetSimTelephoneNumber` | 查询卡号码 |
| `GetSimTeleNumberIdentifier` | 查询号码标识 |
| `GetOpKey` / `GetOpKeyExt` / `GetOpName` | 查询卡关联运营商信息 |

证据：S1，83～97、126～127、145～147 行。

这些卡侧信息接口与普通网络注册、信号和搜网接口应分别归类。

### 3.3 账户、显示信息和卡标签

| 方法名 | 功能 |
|---|---|
| `GetSimAccountInfo` | 查询指定槽位账户 |
| `GetActiveSimAccountInfoList` | 查询激活账户列表 |
| `GetAllSimAccountInfoList` | 查询全部账户列表 |
| `GetSlotId` | 由 simId 查询 slotId |
| `GetSimId` | 由 slotId 查询 simId |
| `SetShowNumber` / `GetShowNumber` | 设置/查询显示号码 |
| `SetShowName` / `GetShowName` | 设置/查询显示名称 |
| `GetSimLabel` | 查询卡标签 |
| `SetSimLabelIndex` | 设置标签索引 |

证据：S1，90～91、102～114、170～179 行。

`GetSlotId(simId)` 不应被解释成内外槽映射接口。

同名方法在不同层的参数不一定相同。例如：

- SA4010 的 `SetSimLabelIndex` 使用 simId；
- CMI 的 `SetSimLabelIndex` 使用 ICCID 字符串。

二者不得在文档中合成一个虚构的统一签名。

证据：S1，178～179 行；S4，359 行；S5，2742～2748 行。

### 3.4 默认语音卡与主卡

| 方法名 | 功能 |
|---|---|
| `SetDefaultVoiceSlotId` | 设置默认语音卡槽位 |
| `GetDefaultVoiceSlotId` | 查询默认语音卡槽位 |
| `GetDefaultVoiceSimId` | 查询默认语音卡账户标识 |
| `SetPrimarySlotId` | 设置主卡槽位 |
| `GetPrimarySlotId` | 查询主卡槽位 |

证据：S1，103～107 行。

默认短信卡、默认数据卡方法确实出现在 CMI 和 ISimManager 中，但本次读取的 `ICoreService` 和 `CoreServiceClient` 未声明对应同名方法。

IPC 枚举中仍有 `SET_DEFAULT_DATA_SLOTID`、`GET_DEFAULT_DATA_SLOTID`，但“枚举存在”不足以证明当前完整可调用路径。

因此，这些方法在本文中列入 CMI 契约，不直接写成已核实的 SA4010 服务入口。

### 3.5 激活与状态刷新

| 方法名 | 功能 |
|---|---|
| `RefreshSimState` | 刷新 SIM 状态 |
| `SetActiveSim` | 设置激活态 |
| `SetActiveSimSatellite` | 卫星场景的激活态入口 |

证据：S1，116～118 行。

方法声明不能证明具体内部仓正在调用这些接口。历史 d2d_service 等消费者继续按历史证据记录。

### 3.6 PIN、PUK 与卡锁

| 方法名 | 功能 |
|---|---|
| `UnlockPin` | PIN 解锁 |
| `UnlockPuk` | PUK 解锁 |
| `AlterPin` | 修改 PIN |
| `UnlockPin2` | PIN2 解锁 |
| `UnlockPuk2` | PUK2 解锁 |
| `AlterPin2` | 修改 PIN2 |
| `SetLockState` | 设置锁状态 |
| `GetLockState` | 查询锁状态 |
| `UnlockSimLock` | 个性化卡锁解锁 |

证据：S1，69～82、151 行。

本层不以 `ChangeSimPassword` 替代 `AlterPin`。

下层 `ChangeSimPassword` 的存在，是另一个层级的契约事实，不意味着代码应改名或接口应合并。

### 3.7 卡文件、认证与 APDU

| 方法名 | 功能 |
|---|---|
| `GetSimIO` | 卡文件/卡 I/O 请求 |
| `SimAuthentication` | SIM 认证 |
| `SendApduData` | APDU 数据发送 |

证据：S1，156～157、168～173 行。

上版将 `GetSimIO`、`SimAuthentication` 主要保留为下层能力的描述不完整，应补充本节已确认的上层声明。

逻辑通道相关方法在下层契约出现，但本次读取的 `ICoreService` 未声明同名逻辑通道入口，不能直接补入本表。

### 3.8 STK

| 方法名 | 功能 |
|---|---|
| `SendEnvelopeCmd` | 发送 Envelope 命令 |
| `SendTerminalResponseCmd` | 发送 Terminal Response |
| `SendCallSetupRequestResult` | 返回 STK 呼叫建立请求处理结果 |

证据：S1，148～150 行。

这些是 STK 命令交互接口。相应事件发布、载荷与会话时序仍需另外核实。

### 3.9 SIM 卡内号码与关联记录

| 方法名 | 功能 |
|---|---|
| `QueryIccDiallingNumbers` | 查询卡内号码 |
| `AddIccDiallingNumbers` | 新增卡内号码 |
| `DelIccDiallingNumbers` | 删除卡内号码 |
| `UpdateIccDiallingNumbers` | 更新卡内号码 |
| `GetVoiceMailIdentifier` | 查询语音信箱标识 |
| `GetVoiceMailNumber` | 查询语音信箱号码 |
| `GetVoiceMailCount` / `SetVoiceMailCount` | 查询/设置记录值 |
| `SetVoiceMailInfo` | 设置语音信箱信息 |
| `SetVoiceCallForwarding` | 与卡侧呼叫转移记录相关的入口，具体执行范围待核实 |

证据：S1，128～142 行。

本节保留代码中的卡记录访问方法，不据此扩展为完整通话控制接口。

### 3.10 多卡能力与运营商配置

| 方法名 | 功能 |
|---|---|
| `GetMaxSimCount` | 查询最大 SIM 数量 |
| `GetRealSimCount` | 查询实际 SIM 数量 |
| `GetDsdsMode` | 查询 DSDS 模式 |
| `IsMultiSimsCapabilitySupported` | 查询指定槽位多卡能力 |
| `GetOperatorConfigs` | 查询运营商配置 |
| `HasOperatorPrivileges` | 查询运营商权限相关结果 |

证据：S1，67、115、144、155、174、180 行。

数量、能力和权限是不同语义，不因文档归类而合并接口。

---

## 4. CMI 已声明的 SIM 协作接口

CMI 与 SA4010 方法集并不相同。以下为上层目录之外需单独记录的代表性协作接口。

### 4.1 默认卡

- `SetDefaultVoiceSlotId`
- `GetDefaultVoiceSlotId`
- `GetDefaultVoiceSimId`
- `SetDefaultSmsSlotId`
- `GetDefaultSmsSlotId`
- `GetDefaultSmsSimId`
- `SetDefaultCellularDataSlotId`
- `GetDefaultCellularDataSlotId`
- `GetDefaultCellularDataSimId`
- `SetPrimarySlotId`
- `GetPrimarySlotId`

证据：S4，330～342 行。

CMI 的 `SetPrimarySlotId` 含 `isUserSet` 参数及默认值；SA4010 对应声明只有 slotId 参数。文档不得统一改写二者签名。

### 4.2 初始化、状态和账户协作

- `InitExtraModule`
- `GetSimIccStatus`
- `SetModemInit`
- `ResetSimLoadAccount`
- `IsSetActiveSimInProgress`
- `IsSetPrimarySlotIdInProgress`
- `SavePrimarySlotId`
- `NotifySimSlotsMapping`
- `SetIccCardState`
- `SetTargetPrimarySlotId`
- `SetMatchSimStateTracker`

证据：S4，57、276～278、328、350～369 行。

其中：

- `InitExtraModule` 的参数名称和类型为 `int32_t slotId`；
- 不能将其文档签名改成“模块类型参数”；
- `NotifySimSlotsMapping` 不能未经实现核实就等同于历史映射表写入接口。

### 4.3 eSIM 账户协作

- `InsertEsimData`
- `IsEsim`
- `ClearSimLabel`
- `UpdateSimPresent`
- `UpdateEsimOpName`
- `GetAllSimCardInfo`
- `SaveCardFileDetectData`
- `SetSpecifiedIccidBySlotId`

证据：S4，361～373 行。

这些是协作访问面，不应自动描述为应用公开 API。

### 4.4 卡内短信存储

- `AddSmsToIcc`
- `UpdateSmsIcc`
- `DelSmsIcc`
- `ObtainAllSmsOfIcc`

证据：S4，321～324 行。

卡内短信存储不同于短信发送和接收。排除短信业务控制时，不应连同卡内存储接口一起删除。

### 4.5 订阅与回调

- `RegisterCoreNotify`
- `UnRegisterCoreNotify`
- `RegisterSimAccountCallback`
- `UnregisterSimAccountCallback`
- `RegisterCommonEventCallback`
- `UnregisterCommonEventCallback`

证据：S4，47～64 行。

因此，对外通知契约不能只列 state_registry 与 CES，还需记录 CMI 提供的协作订阅入口。

实际外部订阅者、事件载荷和线程语义尚需逐项确认。

---

## 5. eSIM 必须分成两个契约

### 5.1 core 内 eSIM manager 协作面

`IEsimManager` 和 CMI 明确声明的能力包括：

| 分类 | 方法 |
|---|---|
| eUICC 信息 | `GetEid`、`GetEuiccInfo`、`GetEuiccInfo2`、`GetEuiccChallenge` |
| Profile 查询 | `GetEuiccProfileInfoList`、`GetProfile` |
| Profile 操作 | `DisableProfile`、`DeleteProfile`、`SwitchToProfile`、`SetProfileNickname` |
| 地址管理 | `GetSmdsAddress`、`GetDefaultSmdpAddress`、`SetDefaultSmdpAddress` |
| 下载协议步骤 | `AuthenticateServer`、`PrepareDownload`、`LoadBoundProfilePackage` |
| 通知管理 | `ListNotifications`、`RetrieveNotificationList`、`RetrieveNotification`、`RemoveNotificationFromList` |
| 其他 | `GetRulesAuthTable`、`CancelSession`、`ResetMemory`、`IsSupported`、`SendApduData` |
| 协作状态 | `GetContractInfo`、`GetEsimCaVerifyResult`、`SetEsimCaVerifyResult`、`GetEsimPortIndex` |
| 事件 | `RegisterCoreNotify`、`UnRegisterCoreNotify`、`PublishEsimProfileChange` |

证据：S6，35～87 行；S4，375～421 行。

`IEsimManager` 还包含 `OnInit`，其性质为初始化契约，不应计为应用业务 API。

当前读取的 `IEsimManager` 中没有 `DownloadProfile` 方法。

### 5.2 eSIM 服务面：SA66250

`IEsimService.idl` 声明的能力包括：

- `GetEid`
- `GetOsuStatus`
- `StartOsu`
- `GetDownloadableProfileMetadata`
- `GetDownloadableProfiles`
- `DownloadProfile`
- `GetEuiccProfileInfoList`
- `GetEuiccInfo`
- `DeleteProfile`
- `SwitchToProfile`
- `SetProfileNickname`
- `ResetMemory`
- `ReserveProfilesForFactoryRestore`
- `SetDefaultSmdpAddress`
- `GetDefaultSmdpAddress`
- `CancelSession`
- `IsSupported`
- `AddProfile`
- `GetSupportedPkids`
- `GetContractInfo`
- `GetEsimFreeStorage`

证据：S7，25～75 行。

`EsimServiceClient` 使用 SA ID `66250` 获取 `IEsimService` 代理；`DownloadProfile` 由该代理调用。

证据：S8，29、65～115、202～210 行。

应区分：

> 接口文件位于 core_service 仓中，不代表运行时服务实现属于 SA4010。

同名 eSIM 操作在不同契约中还存在回调形式和输出参数的差异，不能合并为同一个签名。

---

## 6. 历史报告记载但本次未确认的接口

| 历史名称或结论 | 本次核实情况 | 文档处理 |
|---|---|---|
| `GetSimCardType` | 当前相关声明使用 `GetCardType` | 当前目录使用实际名称，保留历史差异说明 |
| `ChangeSimPassword` 作为上层 PIN 修改接口 | 上层使用 `AlterPin`/`AlterPin2`，下层 RIL 存在 `ChangeSimPassword` | 分层记录，不改代码名称 |
| `SetExternalSlotIdToInternalSlotIdMap` | 当前读取的关键服务声明中未见 | 历史接口待核对 |
| `GetInternalSlotId` | 当前读取的关键服务声明中未见 | 历史接口待核对 |
| `GetExternalSlotId` | 当前读取的关键服务声明中未见 | 历史接口待核对 |
| `SET_SLOT_ID_RELATIONSHIP` | 当前读取的 IPC 枚举未见 | 不写成当前已确认事务码 |
| `GetRadioProtocolSlotIdByModemId` 属于 SA4010 接口 | 在 ISimManager 中确认；当前 ICoreService 未声明 | 记录其实际层级 |
| `GetSlotIdByModemId` | 本次关键声明未确认 | 不与其他转换方法互相替代 |
| CMI 自动转跨进程代理 | 已核对实现为 manager 对象委托 | 修正文档描述 |
| `IccSimStatus` 为 14 值 | 当前声明为 13 个枚举项 | 按当前基线修正 |
| 全量方法数、调用站点数 | 未重新完成计数审计 | 不作为本次核实结论 |

当前 IPC 枚举中保留若干 eSIM 事务码，但本次读取的 `ICoreService` 并未给出完整对应 eSIM 方法集。

该差异应记录为后续 proxy/stub 和构建路径核实项，不修改、删除或重排任何事务码。

---

## 7. 不可见仓的处理边界

历史报告中 telephony_ext、esim_service、d2d_service、ims_service 等调用关系，本次未通过其源码重新取证。

因此：

1. 保留为“历史报告说明”，不升级为当前实测调用关系。
2. 不能因当前版本未出现历史接口，就认定内部消费者不再依赖它。
3. 不能为匹配历史文档向当前代码补接口。
4. 不能为统一接口目录而重命名、合并或删除接口。
5. 对基线差异，只补充版本、证据和待确认项。
6. 后续需要内部团队提供实际使用版本、接口声明或脱敏调用样例。

state_registry/CES、DataShare 与 vendor 动态库相关行为，仍需独立核对实现与配置，不能只凭本次头文件读取确认其全部运行时语义。

---

## 8. 后续文档维护规则

文档中每条接口记录至少区分：

- 所属类或 IDL；
- 实际方法名称；
- 原始签名所在位置；
- 所属服务或调用层；
- 声明已确认 / 实现已确认 / 消费者已确认 / 运行时已验证；
- 当前基线或历史基线。

不得：

- 将业务中文分类当作代码接口名称；
- 将不同层级的同名方法合成一个签名；
- 将声明存在当作 IPC 可用；
- 将枚举存在当作完整服务路径存在；
- 将当前未确认当作所有版本均不存在；
- 将架构建议写成现状接口行为。

## 9. 证据索引

以下链接均固定到本次读取版本。

S1：ICoreService 声明
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/i_core_service.h

S2：CoreServiceClient 声明
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/core_service_client.h

S3：IPC 事务码枚举
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/core_service_ipc_interface_code.h

S4��CoreManagerInner 声明
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/core_manager_inner.h

S5：CoreManagerInner 实现
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/frameworks/native/src/core_manager_inner.cpp

S6：IEsimManager 声明
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/i_esim_manager.h

S7：IEsimService IDL
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/IEsimService.idl

S8：EsimServiceClient 实现
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/frameworks/native/src/esim_service_client.cpp

S9：ISimManager 声明
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/i_sim_manager.h

S10：状态、锁与账户类型
https://github.com/zjuluowu/multisim/blob/db876e551a3fac0922f98c7b358392326663ccc1/telephony_core_service/interfaces/innerkits/include/sim_state_type.h

补充搜索：
本次代码搜索单次最多返回 10 条，结果可能不完整；接口存在性结论优先依据上述完整文件读取。
更多搜索结果：
https://github.com/search?q=repo%3Azjuluowu%2Fmultisim+GetInternalSlotId&type=code
