# 服务入口 capability 阻塞独立记录

FACT，基线2841c6f60635853a929c1755dc08f5543a89eb83：CoreService::GetSimState(slot, callback)在core_service.cpp:479–481转发CoreServiceSim；core_service_sim.cpp:91–120检查非空manager/callback，然后调用MultiSimsCapabilityMgr::IsMultiSimsCapabilitySupported(slot)。对Slot0/1静态推导返回TELEPHONY_ERR_SLOTID_INVALID，且不会进入异步任务；**尚未运行真实服务入口**，该返回是静态推导而非已采集记录。

请求前提：slot=0或1，manager和callback均非空；否则更早返回LOCAL_PTR_NULL。账户入口:376–391同样检查capability；active list:644–666先查账户再过滤不可支持slot，不能把过滤后的组件结果当服务结果。

来源链：capability_manager.cpp:28–43是无状态static函数；.h:23定义SLOT_3_INDEX=3。非3直接false；等于3才调用IPCSkeleton::GetCallingTokenID/GetCallingFullTokenID、AccessTokenKit::GetTokenTypeFlag、TokenIdKit::IsSystemAppByFullTokenID。native/shell接受，其他按system app身份判断。没有在当前函数发现运行时Slot能力表、初始化方法或参数开关；slotCount来自telephony_types.h/GetParameter，不能与该检查混为一谈。

ENVIRONMENT_LIMITATION：Linux没有核实真实IPC caller/token/AccessToken服务和SA；但Slot0/1拒绝分支在访问这些提供者之前，补充token桩不能改变该静态控制流。拒绝归因为**尚未确定**，不认定为生产缺陷、不推定是环境配置缺失，也不修改生产检查。

未来外部能力提供者：仅通过真实IPCSkeleton/AccessToken边界提供明确TOKEN_NATIVE/TOKEN_SHELL/系统应用身份，真实生产判断仍运行；只证明该受控身份下slot3的判定，不证明真实鉴权，也不能使slot0/1合法。若业务方说明应有另一个能力提供者/规则，需先找到真实依赖，再单独设计。当前组件切片完全不经过该检查，不能反证或证明它。

状态：原SIM-B-001..005服务入口候选仍BLOCKED；本次无服务请求实测，没有删除、绕过或强制通过检查。
