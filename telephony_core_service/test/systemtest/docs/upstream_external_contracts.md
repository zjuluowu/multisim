# GitHub外部接口契约核实与Linux适配

FACT：当前实施仓为zjuluowu/multisim HEAD 2841c6f60635853a929c1755dc08f5543a89eb83，工作树新增systemtest。此次直接访问GitHub、git ls-remote master并下载固定SHA头/实现；完整路径、SHA、文件SHA256、链接及许可证见configs/upstream_contracts.json。没有把上游master当作当前hmos_trunk版本。

## DataShare：数据协议，不是SIM业务

SOURCE_EVIDENCE：openharmony/distributeddatamgr_data_share@81761d5a2eb78282decd31f4b1e9f6fc1574782b。

- consumer/include/datashare_helper.h:69–70 Creator(token, URI, extURI, waitTime, isSystem)；:106 Release；:224–225 Query返回resultset或空，columns为可变引用，businessError可选。Creator已deprecated，当前fork仍使用它；Linux兼容仍保留当前消费入口，不改生产调用。
- common/include/datashare_value_object.h:53–54明确variant类型；datashare_values_bucket.h:50–53 Put使用insert，重复key不会覆盖，不能假定upsert。
- native/common/src/datashare_result_set.cpp:157–174定位越界返回E_ERROR；:403–408 Close释放bridge。abs_result_set.cpp:99–102首行等同GoToRow(0)。零行合法查询的GetRowCount可返回E_OK/count=0，但GoToFirstRow不能返回成功。
- common/include/datashare_errno.h:32–42 E_OK=0、E_ERROR=1001；使用原始固定头，避免臆造错误码。

适配职责：只运载values/predicates、提供场景指定的行/类型与游标，Query和Creator经GoogleMock配置；不按SIM状态产生账户、不读生产DB表判PASS。支持请求URI、谓词与投影捕获；可注入nullptr/error/零行/合成行。未装配外部服务时Creator=nullptr、Query不隐式成功。运行模型为同步边界；Release由Mock定义，游标Close后不再读。局限：无真实DataShare/Binder、权限、SQLite、跨用户和真实persist恢复；最小查询子集不等于完整上游ABI。

## CES + Want：公共输出载体

SOURCE_EVIDENCE：notification_common_event_service@372695ec9fbe779aea0a42ecdad849bc372b38c7；ability_ability_base@7088813d55e1592093557d2b2d5e40a47177b7a0。

- common_event_manager.h:92–93 Publish(data,publishInfo,subscriber)，返回bool；:153/:177订阅/退订。
- common_event_data.h:55–90 Want/code/data的setter/getter；publish_info.h:60–99 sticky/ordered属性。
- common_event_support.cpp:1240 SIM_STATE_CHANGED action为usual.event.SIM_STATE_CHANGED。
- want.h:307–314 action、:555–579整数参数、:720–736字符串参数；参数由外部载体保存，不能用其内部字段判SIM正确性。

适配只保存输入属性并将发布边界委派给GoogleMock；未配置服务返回false。不自动派生SIM事件、不强制次数；subscriber分发、ordered/sticky重放、权限暂未实现，不声称兼容真实CES调度。当前仅支持发布所需数据及最小订阅声明；进入未实现订阅路径链接失败。身份字段不进入诊断，自定义PrintTo只输出redacted类别。

## FFRT / InnerEvent：调度与信封

SOURCE_EVIDENCE：resourceschedule_ffrt@1d559e70feda31fa8afd45d564daa242660e4edd；notification_eventhandler@9c2641763fd4c598637d8f68bd908e5912f96583。

queue.h:316–332 submit_h返回task_handle；:406–419 cancel/wait；InnerEvent头:78–81为steady_clock/unique Pointer，:426–446 owner弱生命周期，:553–592共享/唯一类型payload。

现有信封适配有自检；task queue尚未实现。下一步必须保留真实TelEventHandler/TelEventQueue，只替换外部ffrt调度，不得重写SIM内部事件业务。delay、取消、owner销毁后事件排空是设施契约；不以调度顺序作为SIM Oracle。

## 其他边界

SA Manager：systemabilitymgr_samgr@d697f4cbf8c1c37d44e69a6d5ff3d506b02a904d，GetSystemAbility(id)提供IRemoteObject。当前host指针模型仍是编译适配，不是真实Binder，也不证明4010注册/发现。

AppMgr/AbilityMgr：ability_ability_runtime@969de904c1f1ebabcfddd553ab589110b5c51811。Configuration来自ability_base，global_configuration_key.h:50–51固定MCC/MNC键。配置和启动请求是可捕获外部输出，当前只补最小真实签名声明，没有默认成功实现。

HiSysEvent：hiviewdfx_hisysevent@c1534f0d63d34b17bfd248fe6ddd4d70a26a9552；只属外部诊断输出，不作业务PASS Oracle。

CellularData：telephony_cellular_data@be17a60e5cfbdf50c3570baf4ef6680eb70618d0；client接口声明已读取，实际调用集需和当前fork逐项核对；不套用旧本地checkout。

State Registry：telephony_state_registry@0d8cdf08c890303c2e075c91f014bb77485e9890；当前fork自带i_telephony_state_notify/TelephonyStateRegistryClient。后续优先依据本仓UpdateSimState的类型与错误码适配边界，上游用于服务责任核实，不替换生产SimState转换逻辑。

## fork差异与限制

telephony_data_storage GitHub master仍为843a8be7f8d1dc304870a3999b754a03c6cce57d，sim_data.h缺14个当前fork字段。采用原始真实header可以核实公共字段，不能补猜IS_ESIM/SIM_LABEL_INDEX等扩展协议值。未支持字段维持编译诊断和OPEN_QUESTION；不以猜测schema生成SIM账户。

INFERENCE：DataShare/CES请求类型及同步返回契约足以先搭建Linux外部Mock与协议载体。ENVIRONMENT_LIMITATION：完整生产初始化、实际事件调度、fork扩展数据协议仍未运行。OPEN_QUESTION：fork字段来源、匹配StateRegistry/SA/权限模型、事件是否必须/是否允许重复，以及无卡账户永久需求。没有设备是B的既定拓扑，不能用作这些软件依赖缺失的替代解释。

## 本轮实现前设施用例设计

- INFRA-DATA-001 / DS-CURSOR：显式零行结果，count=0成功、首行失败、Close后读取失败；防止空结果假成功。N/A卡/Slot/身份，纯外部协议自检。
- INFRA-DATA-002 / DS-QUERY：经真实风格Helper::Creator/Query入口由GoogleMock提供nullptr与显式零行，验证URI/投影运载；不按SIM状态造账户。
- INFRA-DATA-003 / DS-VALUES：复用原始values header，合成key重复Put保留首值，与上游insert语义一致。
- INFRA-CES-001 / CES-PUBLISH：通过CommonEventManager捕获原action/code/ordered/sticky，失败值原样返回；不计事件次数。
- INFRA-CES-002 / CES-LIFETIME：未装配服务返回false；RAII退出后服务不可访问，fresh进程无残留。

层级均为INFRA，不计SIM业务场景；Oracle来源是上述固定源码协议。同步测试由runner 10s总截止保护；没有固定sleep。全部合成数据，日志/XML只记录case/oracle与布尔关系。设施失败阻止后续状态场景搭建；成功不等于SIM-B-002通过。

## 已实现子集的偏差与隔离

ResultSet仅接受显式配置的类型，不提供SQLite的字符串/数字强制转换；宽度不匹配拒绝构造。谓词只运载请求，不执行SQL或计算SIM账户。Want不实现Parcelable/Binder。SubscribeInfo权限及订阅方法只声明；发布回调subscriber非空明确返回false。Mock安装为进程作用域，拒绝嵌套；原子指针不保证并发销毁安全，退出scope前必须停止相关工作线程。当前设施同步执行且每例独立scope，不启动持久化服务。

这些Oracle属于固定上游接口/实现的设施一致性证据，不自动成为本fork永久SIM业务契约。DS-CURSOR、DS-VALUES的实现行为用于适配一致性；CES-PUBLISH验证载体及显式失败传播，不证明真实事件系统递送。
