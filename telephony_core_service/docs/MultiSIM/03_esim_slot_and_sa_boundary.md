# OpenHarmony eSIM 与 Slot 对应关系及独立 SA 设计分析

> **文档目标**  
> 汇总两个核心问题的架构分析：  
> 1) 在当前模型中，eSIM 是如何与 slot 对应的；  
> 2) 为什么存在独立的 `TELEPHONY_ESIM_SERVICE_SYS_ABILITY_ID`，而不是与物理 SIM 采用同一个 SA。  
>
> **仓库**：`openharmony/telephony_core_service`  
> **会话分析基线（出现过）**：`90ad4f1f...`、`feaed741...`、`c6683b2d...`  
> **文档日期**：2026-09-06

---

## 1. 执行摘要

### 1.1 问题一结论：当前 eSIM 如何对应 Slot

当前不是通过一个显式稳定的 `EuiccId -> SlotId` 关系来建模，而是通过**多层隐式映射**：

```text
slotId
→ eSIM能力判断(IsSupported(slotId))
→ ATR检测(该Slot当前是否是eUICC)
→ eSIM运行时对象(esimFiles_[slotId])
→ Port维度(portIndex)
→ Profile维度(iccId)
→ 账户记录映射(SimRdbInfo.slotIndex / isEsim / simId)
```

即：**slot 是一级入口，port/profile 是二级定位，Subscription/账户映射是后置结果**。

### 1.2 问题二结论：为什么有独立 eSIM SA

独立 `TELEPHONY_ESIM_SERVICE_SYS_ABILITY_ID`（66250）主要对应 **eUICC Profile 管理能力**（下载、切换、OSU、SM-DP+相关操作等），而物理 SIM 相关的基础运行时能力（SIM状态、RIL、默认卡、主卡、账户等）依旧主要在 Core Service 侧。

更准确的架构事实是：

- 不是“物理 SIM 一个 SA、eSIM 另一个 SA 完全切开”；
- 而是“eSIM 业务编排能力独立 SA + Core Service 内仍有 eSIM/RIL 运行时能力”。

---

## 2. 当前模型中 eSIM 与 Slot 的对应机制

## 2.1 总体机制图

```mermaid
flowchart TB
    SLOT[slotId]
    CAP[EsimServiceClient.IsSupported(slotId)]
    ATR[读取 gsm.sim.hw_atrN]
    PARSE[ATR解析 IsEuiccAvailable]
    EFILE[esimFiles_[slotId]]
    PORT[portIndex]
    PROFILE[Profile/ICCID]
    ACCOUNT[SimRdbInfo]
    BIND[slotIndex/isEsim/simId]

    SLOT --> CAP
    SLOT --> ATR
    CAP --> PARSE
    ATR --> PARSE
    PARSE --> EFILE
    EFILE --> PORT
    PORT --> PROFILE
    PROFILE --> ACCOUNT
    ACCOUNT --> BIND
```

---

## 2.2 第1层：按 Slot 判断 eSIM 能力

`EsimManager::OnInit()` 中按 `slotId` 遍历，满足支持条件才创建 `EsimFile`：

```c++ name=services/sim/src/esim_manager.cpp url=https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/sim/src/esim_manager.cpp#L31-L57
slotCount_ = slotCount;
esimFiles_.resize(slotCount_);
esimFilesLowPriority_.resize(slotCount_);
for (int32_t slotId = 0; slotId < slotCount_; slotId++) {
    // slot2 保留给天际通
    if (DelayedRefSingleton<EsimServiceClient>::GetInstance().IsSupported(slotId) == TELEPHONY_ERR_SUCCESS &&
        slotId != SIM_SLOT_2) {
        esimFiles_[slotId] = std::make_shared<EsimFile>(telRilManager_, slotId);
        esimFilesLowPriority_[slotId] = std::make_shared<EsimFile>(telRilManager_, slotId);
    }
}
```

**要点**：运行时 eSIM 入口是 `esimFiles_[slotId]`，不是 `euiccMap[euiccId]`。

---

## 2.3 第2层：按 Slot 的 ATR 判断“当前是否 eUICC”

`MultiSimHelper::IsEsim(slotId)` 读取不同 slot 对应 ATR 参数（`gsm.sim.hw_atr*`）并解析：

```c++ name=services/sim/src/multi_sim_helper.cpp url=https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/multi_sim_helper.cpp#L225-L263
propAtr = (slotId == SLOT_ID_0) ? GSM_SIM_ATR : propAtr;
propAtr = (slotId == SLOT_ID_1) ? GSM_SIM_ATR1 : propAtr;
propAtr = (slotId == SLOT_ID_2) ? GSM_SIM_ATR2 : propAtr;
propAtr = (slotId == SLOT_ID_3) ? GSM_SIM_ATR3 : propAtr;

GetParameter(propAtr.c_str(), "", buf, CARD_ATR_LEN);
ResetResponse resetResponse;
resetResponse.AnalysisAtrData(cardAtr);
return resetResponse.IsEuiccAvailable();
```

**要点**：`slotId -> ATR参数 -> eUICC可用性`，属于运行时推导关系。

---

## 2.4 第3层：eSIM API 以 `slotId + portIndex (+ iccId)` 调用

接口定义（`IEsimManager`）大量体现此模式：

```c++ name=services/sim/include/esim_manager.h url=https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/sim/include/esim_manager.h#L37-L85
int32_t GetEid(int32_t slotId, std::u16string &eId) override;
int32_t GetProfile(int32_t slotId, int32_t portIndex, const std::u16string &iccId, EuiccProfile &eUiccProfile) override;
int32_t SwitchToProfile(int32_t slotId, int32_t portIndex, const std::u16string &iccId,
    bool forceDisableProfile, int32_t &enumResult) override;
int32_t GetEsimPortIndex(int32_t slotId, int32_t &portIndex) override;
```

实现中也先取 `esimFiles_[slotId]`：

```c++ name=services/sim/src/esim_manager.cpp url=https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/esim_manager.cpp#L105-L126
if ((!IsValidSlotId(slotId, esimFiles_)) || (esimFiles_[slotId] == nullptr)) {
    return TELEPHONY_ERR_LOCAL_PTR_NULL;
}
portIndex = esimFiles_[slotId]->GetEsimPortIndex();

enumResult = esimFiles_[slotId]->DisableProfile(portIndex, iccId);
int32_t simId = CoreManagerInner::GetInstance().GetSimId(slotId);
```

**要点**：虽然有 Port 维度，但 eSIM 的一级路由仍是 slot。

---

## 2.5 第4层：底层 APDU/RIL 仍由 Slot 驱动

`EsimFile` 最终通过 `slotId_` 调 RIL：

```c++ name=services/sim/src/esim_file.cpp url=https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/esim_file.cpp#L373-L383
if (!isSupportEsimMEP) {
    telRilManager_->SimOpenLogicalChannel(slotId_, appId, PARAMETER_TWO, response);
} else {
    telRilManager_->SimOpenLogicalChannelWithPort(slotId_, appId, PARAMETER_TWO, COMMAND_PORT, response);
}
```

**要点**：RIL 入口是 slot，MEP 仅在此基础上增加 port 维度。

---

## 2.6 第5层：账户数据库映射 eSIM（isEsim + slotIndex）

`SimRdbInfo`（会话已分析）存储 `isEsim`、`slotIndex`、`simId` 等。  
`InsertEsimData` / `UpdateDataByIccId(slotId, iccId)` 体现 profile 与 slot 的动态绑定：

```c++ name=services/sim/src/multi_sim_controller.cpp url=https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/multi_sim_controller.cpp#L470-L499
DataShare::DataShareValuesBucket values;
DataShare::DataShareValueObject slotObj(slotId);
values.Put(SimData::SLOT_INDEX, slotObj);
...
return simDbHelper_->UpdateDataByIccId(newIccId, values);
```

**要点**：Profile/账户最终通过 slotIndex 回写到“当前 slot 位置”，不是稳定 `EuiccId` 映射。

---

## 2.7 当前映射特征总结

- 映射是“运行时推导 + 状态回写”，不是“显式拓扑实体”；
- 一级键是 `slotId`；
- 二级键是 `portIndex`；
- Profile 主要通过 `iccId`；
- eSIM账户映射落到 `SimRdbInfo` 的 `slotIndex/isEsim/simId`；
- 对 MEP/多订阅场景表达能力有限（slot-centric约束明显）。

---

## 3. 为什么有独立 eSIM SA，而不是和物理 SIM 统一一个 SA

## 3.1 代码事实：独立 SA 存在

`EsimServiceClient` 明确加载独立 SA：

```c++ name=frameworks/native/src/esim_service_client.cpp url=https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/frameworks/native/src/esim_service_client.cpp#L29-L83
constexpr int32_t TELEPHONY_ESIM_SERVICE_SYS_ABILITY_ID = 66250;
...
int32_t result = sam->LoadSystemAbility(TELEPHONY_ESIM_SERVICE_SYS_ABILITY_ID, callback);
```

---

## 3.2 独立 SA 的接口语义：eUICC Profile 管理

`IEsimService.idl` 的操作集中在 Profile 生命周期与远程配置：

```text
GetDownloadableProfileMetadata
GetDownloadableProfiles
DownloadProfile
SwitchToProfile
DeleteProfile
ResetMemory
SetDefaultSmdpAddress
StartOsu
GetOsuStatus
...
```

参考定义：

```idl name=interfaces/innerkits/IEsimService.idl url=https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/interfaces/innerkits/IEsimService.idl#L25-L76
interface OHOS.Telephony.IEsimService {
    void GetEid([in] int slotId, [in] IEsimServiceCallback listener);
    void GetDownloadableProfiles([in] int slotId, [in] int portIndex, ...);
    void DownloadProfile([in] int slotId, ...);
    void SwitchToProfile([in] int slotId, [in] int portIndex, [in] String iccId, ...);
    void ResetMemory([in] int slotId, [in] int resetOption, ...);
    ...
}
```

---

## 3.3 代码事实：Core Service 内仍有 eSIM 管理对象

Core Service 初始化中仍创建 `EsimManager`（条件编译开启时）：

```c++ name=services/core/src/core_service.cpp url=https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/core/src/core_service.cpp#L107-L115
#ifdef CORE_SERVICE_SUPPORT_ESIM
esimManager_ = std::make_shared<EsimManager>(telRilManager_);
esimManager_->OnInit(slotCount);
CoreManagerInner::GetInstance().SetEsimManagerObj(esimManager_);
#endif
simManager_ = std::make_shared<SimManager>(telRilManager_);
simManager_->OnInit(slotCount);
```

**关键点**：当前是“独立 eSIM SA + Core 内 eSIM能力”共存，不是完全二选一。

---

## 3.4 为什么通常会拆独立 SA（架构层面解释）

### A. 生命周期差异
- 物理SIM基础能力：系统启动关键路径、常驻；
- eSIM profile管理：按需频次高、可按需加载（`LoadSystemAbility`）。

### B. 业务复杂度差异
eSIM 独有复杂流程：
- 下载/安装/切换/删除 Profile；
- SM-DP+ / OSU / 会话取消；
- 安全认证与远程策略。

### C. 安全权限边界
eSIM操作涉及更高权限与敏感数据，独立 SA 更易收敛权限和审计。

### D. 故障隔离与演进节奏
独立 SA 可以减少 eSIM业务故障对核心电话基础能力的冲击，并支持独立演进。

---

## 3.5 为什么“不直接合并为同一个SA”

若全部并入核心 SA，潜在问题包括：
- 核心服务膨胀；
- 启动/常驻资源压力增大；
- eSIM复杂逻辑故障影响面扩大；
- 可选能力裁剪困难；
- 权限边界变宽。

因此“独立 eSIM SA”在部署层面是合理的工程选择。

---

## 4. 关键澄清：部署边界 ≠ 领域边界

虽然有独立 eSIM SA，但目前接口和内部流程仍大量以 `slotId` 为中心（slot-centric）。  
这意味着：

- **独立 SA 并不自动解决领域建模问题**；
- 仍可能出现 eSIM Profile / Subscription / Role / Modem 的跨模块一致性难题。

---

## 5. 两个问题的统一结论（供后续方案设计引用）

1. **当前 eSIM-Slot 对应方式**：是以 `slotId` 为入口的多层隐式映射（能力判断、ATR判定、port/profile参数、账户回写），缺少稳定显式拓扑实体。  
2. **独立 eSIM SA 的意义**：主要是 eUICC profile 管理的生命周期、安全与隔离需求；不是要替代 Core 的全部 SIM/runtime 能力。  
3. **后续优化方向**：保留合理的 SA 隔离，同时把领域主键从 `slotId` 迁移到 `Subscription`，将 `LogicalSlotBinding / ModemAssignment / RoleAssignment` 显式化，避免扩展与维护成本持续上升。

---

## 6. 对后续大模型分析的直接提示语（可复制）

```text
请重点验证以下三点：
1) 当前eSIM与slot关系是否主要靠 slotId + ATR + portIndex + iccId 的隐式推导；
2) 独立 eSIM SA 是否主要承载 profile 生命周期管理，而核心SIM/runtime能力仍在 Core Service；
3) 在保留独立SA前提下，如何将模型从 slot-centric 迁移到 subscription-centric，并给出渐进兼容方案。
```

---

## 7. 参考链接（本次会话关键文件）

- `frameworks/native/src/esim_service_client.cpp`  
  https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/frameworks/native/src/esim_service_client.cpp
- `interfaces/innerkits/IEsimService.idl`  
  https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/interfaces/innerkits/IEsimService.idl
- `services/sim/src/esim_manager.cpp`  
  https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/sim/src/esim_manager.cpp
- `services/sim/include/esim_manager.h`  
  https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/sim/include/esim_manager.h
- `services/sim/src/multi_sim_helper.cpp`  
  https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/multi_sim_helper.cpp
- `services/sim/src/multi_sim_controller.cpp`  
  https://github.com/openharmony/telephony_core_service/blob/90ad4f1f005191eae3843e441de18388703cb9a5/services/sim/src/multi_sim_controller.cpp
- `services/core/src/core_service.cpp`  
  https://github.com/openharmony/telephony_core_service/blob/feaed7419494c7f3bcdcc046a035dcab59290fd6/services/core/src/core_service.cpp

---

## 8. 备注

- 本文基于会话内已检索到的源码证据整理；
- 代码搜索工具单次结果有返回上限，未覆盖的调用点建议在仓库中继续全量检索确认；
- 对独立 eSIM SA 的部分“设计意图”属于结合代码形态的工程推断，建议在对应 SA 实现仓库中二次验证。
