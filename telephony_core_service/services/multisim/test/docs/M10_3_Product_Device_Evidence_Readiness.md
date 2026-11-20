# M10.3 Radio 产品与 Device 证据可行性报告

基线：`82a76db12`。本报告是源码/环境取证报告；未取得真实设备 trace，结论不是 VERIFIED_DEVICE。

## 1. 当前源码事实

### B：SetPrimarySlot

| 项目 | 证据 | 等级 |
| --- | --- | --- |
| 业务入口 | `MultiSimController::SetPrimarySlotIdWithoutModemReboot` → `SetPrimarySlotToRil` → `SendSetPrimarySlotEvent` | VERIFIED_SOURCE |
| RIL 层 | `TelRilManager::SetPrimarySlot(slotId,event)` → `TelRilSim::SetPrimarySlot` → HDI `V1_5::IRil::SetPrimarySlot` | VERIFIED_SOURCE |
| serial | `TelRilBase::CreateTelRilRequest` 创建原子 serial 并插入 `requestMap_`，同时保存 InnerEvent/owner | VERIFIED_SOURCE |
| response | `SetPrimarySlotResponse` → `TelRilBase::Response` → 按 serial 取回并删除 requestMap，发送原 event | VERIFIED_SOURCE |
| Legacy 业务含义 | `OnRilSetPrimarySlotDone` 读取 `RadioResponseInfo.error`，false/success 唤醒原等待 | VERIFIED_SOURCE |
| 命令结束 | response 证明该 serial 有回包，不证明 Modem 已静止 | OPEN |
| 当前状态 | 现源码无 response 后的物理状态确认字段 | OPEN |
| 可复用 | 现源码无资源/通道复用证明 | OPEN |

### C1 CHECK / UPDATE / COMPLETE

| 阶段 | 请求与关联 | 接收事实 | 资源证据缺口 |
| --- | --- | --- | --- |
| CHECK | `RadioProtocolController::ExecuteCheckCommunication` 生成共享 sessionId，按真实 slot 发 `SetRadioProtocol(phase=CHECK)`；每次 RIL 仍有独立 serial | `ProcessCheckRadioProtocol` 校验响应；全部成员后决定 UPDATE 或 COMPLETE | VERIFIED_SOURCE；response 不证明旧命令或各物理资源静止 |
| UPDATE | 为全部成员发送 phase=UPDATE；RIL response handler 只处理响应，不推进阶段 | `SimRadioProtocolUpdated` 通过 RadioProtocol NOTIFY，`ProcessRadioProtocolNotify` 按 sessionId 收齐成员 | VERIFIED_SOURCE；NOTIFY 无独立 ACK、serial 等价关系或资源复用证明 |
| COMPLETE | 为全部成员发送 success/fail COMPLETE | `ProcessSetRadioProtocolComplete` 校验响应并收齐成员后结束协议 | VERIFIED_SOURCE；业务协议结束不证明 Modem/channel 可复用 |

UPDATE 的 Target 关联矩阵必须使用 sessionId + Step/attempt + member + phase 2 + NOTIFY；不得编造 serial。

### Adapter → vendor 证据

`d039c7ce` 的 HRil 层进一步证明：

- `HRilBase::RequestVendor` 先验证 vendor function pointer；缺实现时同步通过 `CommonErrorResponse` 返回 `RIL_ERR_VENDOR_NOT_IMPLEMENT`；否则调用 `CreateHRilRequest(serial, slot, request)` 后才进入 vendor。
- `HRilManager::CreateHRilRequest` 保存 serial、slot 和 request 到 `requestList_`；Response 路径从报告中取原 `ReqDataInfo`，构造 `RilRadioResponseInfo.serial/error/type` 后释放 request record。Vendor 实例位于 HDF 边界，报告中的 `slotId` 由 HRil submodule 提供；这证明逻辑 slot/adapter instance 关联，不证明物理 Modem/通道复用矩阵。
- `HRilSim::SetRadioProtocol` 把 sessionId/phase/technology/modemId/status 原样转换成 `HRilRadioProtocol` 后进入 `RequestVendor(HREQ_SIM_SET_RADIO_PROTOCOL)`。
- 同仓示例 AT vendor 的 `ReqSetRadioProtocol` 对非 UPDATE 立即报 response；对 UPDATE 把 phase 改为 NOTIFY、status 改为 SUCCESS，并发送 `HNOTI_SIM_RADIO_PROTOCOL_UPDATED`。`HRilBase::RequestVendor` 在调用 vendor function 时没有防止同步报告，因此 Submit-return-before-callback 不是可假定的顺序；Target 的提交缓冲是必要契约。该参考 vendor 是源码语义来源，不是任一真实 modem 的结束证据。
- `HRilSim::SetPrimarySlot` 只定义 HDI→vendor 入口和 `SetPrimarySlotResponse`；该 `d039c7ce` 仓的 `services/vendor` 中没有 `SetPrimarySlot` 实现。B 的 vendor 具体实现仍为外部/产品侧缺口。

因此可验证的是 serial/session 的完整跨层传递；HandoffAccepted 是否代表设备接收，以及响应/NOTIFY 是否已包含硬件结束和状态复用证明，仍为 OPEN。

## 2. 共享域与真实资源键

- `RadioProtocolController` 对 C1 使用一个控制器、一个 sessionId 和成员计数；这只证明协议编排共享，不证明底层 RIL instance/Modem 是否是同一物理域。
- `TelRilManager` 按 slot 选择 `TelRilSim`；当前源码未给出 slot → 物理 Modem 的所有产品映射。
- `RadioDomainId`/slot 是业务/装配标识，不能直接作为独立物理资源键。
- 当前 Target 使用保守单共享 Host key，安全但不证明产品并发边界。
- B/C1 是否共用同一 Modem、C1 两成员是否共用一个 HDI/firmware channel、卡激活是否共享通道，均为 OPEN。

## 3. Host 适配边界

`RadioPlatformPort::SubmitPrimaryRadio` 返回：

- `ConfirmedNotSent`
- `HandoffAccepted + transportSerial?`
- `HandoffUnknown`

`ResourceRadioRuntime` 保存 transport token ↔ serial，普通 Response 要求 serial 一致；UPDATE 使用 NOTIFY 身份关系而不编造 serial。Owner 回调进入独立 dispatcher，平台 Submit/设备等待不在 R/A 锁内。

这已是 Host 可执行契约，但尚未将真实 `TelRilManager`/HDI 对象注入为产品适配。`RadioSubmitResult` 尚未证明“命令到达设备”。

## 4. 证据缺口

| 证据 | 当前状态 | 必须补充层次 |
| --- | --- | --- |
| handoff 越过本地边界 | 产品适配未接 | TelRil/HDI 调用返回与 requestMap 插入/发送失败因果 |
| commandEnded | 只存在 Host 输入字段 | HDI response 的完成语义 + 必要的 Modem/driver 完成观察 |
| currentStateVerified | 只存在 Host 输入字段 | 产品可接受的 read-back/state trace |
| resourceReusable | 只存在 Host 输入字段 | channel close、command quiescence 或产品明确复用契约 |
| physical resource key | 保守 Host key | slot/RIL instance/Modem/driver 路由证据 |
| old serial reuse safety | TelRilBase serial 回绕、current requestMap 事实已核实 | 业务 epoch + transport scope 的产品适配；旧 real trace |

## 5. 产品环境盘点

- 当前环境另有可访问的 `/mnt/e/github/telephony/telephony_ril_adapter`。其 Git HEAD 为 `d039c7ce8518487bfcec54c9777e6172c0c9f684`，与主设计 F38 引用的固定提交一致；该仓工作区有大量非本任务修改，本报告只读取 `git show HEAD:` 的已提交内容。
- 当前 `core_service`/adapter 树没有可构成完整 OHOS build 的 `drivers_interface` 仓及 `ril/v1_5/IRil.idl`。因此“可读同版 adapter”是 VERIFIED_SOURCE，但与某个实际产品 manifest 的版本匹配仍 OPEN。
- `gn`、`ninja`、Clang Host 工具可用。
- 当前 PATH 无 `hb`。实际命令 `hb build -T //base/telephony/core_service:tel_core_service` 返回 127 `command not found`。
- 未发现匹配的 Modem/RIL device、vendor adapter、HDI daemon 或授权 trace 环境。
- 未尝试安装系统依赖、替换工具链或伪造 HDI 适配。

结论：**PRODUCT_BUILD_BLOCKED；DEVICE_BLOCKED**。

## 6. M11 前置条件

1. 可复现的同版本 core_service、telephony_ril、drivers_interface 源。
2. 产品 `hb` 环境和真实依赖图可用。
3. slot → RIL instance → Modem/channel 的核实映射。
4. 每类命令的 handoff、end、current-state、reuse 来源。
5. UPDATE NOTIFY 与普通 serial response 的产品关联适配。
6. Core/SA、共享 RIL、Stop/Drain 的产品生命周期。
7. 至少一个匹配设备的真实 B/C1 trace 和恢复/超时 trace。

当前只满足 Host 契约和源码静态准备，不满足 M10.3 或 M11 关闭门禁。
