# 依赖解决清单、源码target与兼容偏差

## GoogleTest：已解决并独立验证

当前仓库没有可复用GoogleTest源码/目标。扩大搜索发现另一工程host/tests/gtest的头/预编译库，但版本不可核实，且不属于当前仓公共设施，未引用。选择官方v1.15.2，git ls-remote解析并固定提交`b514bdc898e2951020cbdca1304b75f5950d1f59`；来源https://github.com/google/googletest，BSD-3-Clause。锁定archive SHA256=`9257316d65d6259f6596bc4fa5464092305ab3f3d9dc6d7e5780ab462d8baa64`，完整源码tree摘要在configs/dependency_lock.json。

显式获取（构建脚本不会执行下载）：下载lock中archive_url到输出目录；离线把同archive传入prepare_dependencies.py --archive ... --destination ...，验证SHA256后解压。runner再次校验整个source tree，防止使用未知机器库。实际GN编译官方googletest/src/gtest-all.cc成独立static_library；自己的main，不使用GMock。未通过CMake/Meson构建。

## 平台接口：不是GoogleTest问题

configs/platform_inventory.json记录sim_manager.cpp文本递归include扫描：93个生产文件，40个未在本仓include目录解析的名字；其中stdint.h/stdlib.h是标准库，libxml来自Linux安装，不算平台缺失。该清单含条件分支，不冒称预处理后最小闭包。

优先尝试真实头：本仓拥有所有SIM/InnerKit等生产头，直接引用；Linux libxml2/include可用（仅本次编译声明，未执行xml业务）；OHOS FFRT/EventHandler/DataShare等原接口头在当前仓和已查路径缺失。官方errors.h使用C Utils固定SHA `3bdc11e00e7bf6e577cee300ad51045ca9958efd`的base/include/errors.h，保留Apache-2.0版权/许可证，文件SHA在lock。未声称该master SHA匹配hmos fork，错误码路径不在本次Oracle/运行范围。

其他平台兼容声明是**编译用最小声明，不是原OHOS ABI SDK**，依据当前生产头/调用表达式；原接口提供者按根BUILD.gn external_deps定位，具体源版本未取得。未支持的API没有函数定义，不能返回任意success；若新增场景调用，会在链接阶段失败。不得对实际平台结果作兼容性承诺。部分基础原语有真实Linux实现，支持范围如下。

| 头/模块 | 原提供者/本仓证据 | host支持和偏差 | 实际验证 |
|---|---|---|---|
| parameter.h | init:libbegetutil；telephony_types.h:104/:113/:127 | 只读GetParameter，单槽与virtual-modem=false脚本；缺key取调用方default，越界/空参数返回负值；SetParameter无实现 | 实际生产参数解析运行；没有SIM业务结果桩 |
| hilog/log.h | hilog；utils/log/include/telephony_log_wrapper.h | 仅HILOG_IMPL sink丢弃格式及参数，保护脱敏；不是业务API | 构造/析构日志不输出身份 |
| ffrt.h、ffrt_inner.h | ffrt；sim_manager.h及源 | mutex/shared_mutex映射pthread，condition_variable_any；thread类型映射std::thread；submit只有声明，无调度器 | 本例只创建/销毁生产对象的lock载体；无生产任务/线程，不覆盖调度 |
| event_handler.h、event_runner.h、inner_event.h | eventhandler；i_sim_manager.h、i_tel_ril_manager.h、tel_event_handler.h | EventHandler/EventRunner/InnerEvent/EventQueue类型和方法声明；无发送/处理实现。Pointer删除器、优先级等只用于编译类型检查 | 编译生产头成功；无运行事件语义 |
| parcel.h、message_parcel.h | c_utils/ipc；sim_state_type.h、dialling_numbers_info.h | Parcelable基类、Parcel读写签名；无序列化实现 | 编译成功；没有IPC/parcel测试 |
| iremote_object/broker/proxy/stub.h | ipc；sim_account_callback.h、satellite callback头 | shared_ptr/weak_ptr型host持有者与remote类声明；不是真实sptr对象模型/Binder | 仅编译，未构造remote对象/鉴权 |
| system_ability_status_change_stub.h、if_system_ability_manager.h、iservice_registry.h、system_ability_definition.h | safwk/samgr；sim_file_manager.h等 | 类型声明，无注册/发现/回调服务实现 | 仅编译；SA全部未覆盖 |
| datashare_helper/predicates/result_set/values_bucket.h、result_set.h、uri.h | data_share/c_utils；sim_rdb_helper.h、telephony_data_helper.h | DataShare/NativeRdb/Uri前向类型，无查询/写/连接实现 | 只编译声明；账户初始化路径仍BLOCKED |
| want.h | ability_base；sim_state_handle.h、ext_wrapper.h | AAFwk::Want前向声明 | 仅编译，无Ability运行 |
| common_event*.h | common_event_service；multi_sim_monitor/handler头 | EventFwk前向声明，无路由/发布成功替身 | 事件路径未覆盖 |
| singleton.h、nocopyable.h | c_utils；TelephonyExtWrapper等真实类声明 | 提供类型声明、真实静态local singleton模型及禁止copy/move声明；DelayedSingleton只有获取声明；不改生产访问级别 | 编译；ext初始化未执行；有更宽生命周期偏差 |
| string_ex.h、securec.h、parameters.h | c_utils/init；sim_char_decode.h、telephony_types.h | Str16/8、安全字符串、system参数函数只有声明；EOK语义0用于编译已知比较 | 无解码/字符串执行；不能用它声称fuzzer通过 |
| cJSON.h、config_policy_utils.h、os_account_manager_wrapper.h | cJSON/config_policy/platform账户；operator_file_parser.h及共同头 | 只暴露前向声明，无实现；包装入口引用compile_declarations.h，不是空头掩盖符号 | 仅编译；不运行相关业务 |
| errors.h | 固定官方C Utils SHA，见lock | 原头保留不重写；尚非匹配fork SDK | 编译；错误码业务未验证 |
| libxml/parser.h、xmlmemory.h | Linux libxml2本地include | 使用真实已安装头；运行时XML库未链接也未使用 | 编译成功；无XML业务执行 |

各包装头引用同一compile_declarations.h；没有生产内部类替身。声明依据与consumer逐项对应见platform_inventory.json。由于当前源码依赖多种平台类，声明集比此例运行依赖多；这种编译便利不意味着所有API可运行。无private/public宏，无include生产.cpp，无业务controller方法桩，无忽略undefined-symbol链接选项。

## 实际编译的生产源码与理由

- `services/sim/src/sim_manager.cpp`：真实ISimManager实现、公开构造/析构、GetMaxSimCount查询。编译完整未修改源；不复制/截取函数。保留原条件宏默认（未定义eSIM/卫星/ext支持宏），没有HOST_BUILD业务分支。
- `services/telephony_ext_wrapper/src/telephony_ext_wrapper.cpp`：真实扩展包装生命周期与容量fallback所引用的符号；未用桩替代。当前单槽路径无需加载ext服务。
- 真实 `interfaces/innerkits/include/i_sim_manager.h`、`sim_manager.h`、`telephony_types.h`等直接include。没有重新声明ISimManager或SimManager。

source_set("sim_production_host")独立于platform_boundaries、component_launcher、googletest、sim_component_test。原:tel_core_service无法在缺OHOS根情况下复用，原因已记录；后续支持完整OnInit需加入实际SIM生产闭包（configs/production_sources_candidate.json是保守候选，不是已编译名单）。当前仅容量方法可支持；原五条不运行。

## 工具链实测与差异

GN1000 / Ninja1.11.1 / Clang18.1.3 / GNU gold / C++17 / full LTO，实际由GN产生Ninja。GCC13.3可编译，但未用子模块会被虚表保留导致link unresolved，记录在调查尝试。最终Clang使用whole-program-vtables、virtual-function-elimination、hidden visibility和gc-sections，正常移除未调用函数；没有改源、虚表手工补丁或链接忽略未定义符号。扩大测试到OnInit或状态/账户时必须重新链接真实业务源码，不可依赖当前虚函数移除来声称这些路径可执行。

host O2/full-LTO、默认exceptions/RTTI与根生产Os/no-exceptions/no-rtti不同；已有GTest模板允许exceptions。原OHOS默认C++标准缺模板仍未知，当前C++17由编译验证（生产使用weak_from_this等），不伪称同配置。添加Clang源级coverage instrumentation仅用于参与执行审计；内部符号/执行次数不是业务正确性Oracle。真实编译flags、命令、link map、XML/JSON见输出目录。


当前更新：Linux 场景通过固定版本 GoogleMock 构造外部参数边界，生产目标不变；详见 [Linux GoogleMock 方案](linux_gmock_scenarios.md)。早期手写桩描述属于历史记录。原五条业务场景仍 BLOCKED。

## 当前增量：正式协议设施与初始化审计

新增GoogleMock `MockTelRilManager`，签名来自本仓真实 ITelRilManager，显式生成/`--check`核实126个override；未配置默认返回失败，使用StrictMock。它只替换外部RIL，不替换SIM业务；正式状态响应设施自检单独标INFRA。

InnerEvent由原先仅声明改为最小事件信封适配，类型匹配、shared/weak/unique所有权及元数据有独立自检；仍没有平台EventHandler调度运行实现。来源、偏差和生命周期见event_adapter.md。std::mutex的标准头仅满足未修改生产头中的已有类型；未新增std::mutex锁对象。

securec.h/securectype.h改用固定官方仓库61baa7a3eb9347efca58397d1d0504faa96d79f0真实头文件，保留Mulan PSL v2原文，完整checksum见dependency_lock.json；不声称匹配hmos release，无runtime实现。RefBase只声明已查真实参考头的构造/虚析构；没有intrusive refcount运行能力或ABI兼容声明。

OpenSSL真实Linux头文件由Ubuntu libssl-dev=3.0.13-0ubuntu3.16/amd64固定deb显式离线解包，不安装系统包、不在构建下载。archive SHA256及135个头文件SHA256均固定。只有可选初始化编译审计使用其include目录，没有执行crypto。离线命令：

```sh
python3 test/systemtest/scripts/prepare_openssl.py --archive /tmp/sim-libssl-dev_3.0.13-0ubuntu3.16_amd64.deb --destination /tmp/sim-systemtest-openssl
```

目标目录必须全新；获取URL、许可证与校验和见lock。`audit_initialization.py --openssl-root`逐个验证真实header摘要；GoogleTest缺失和平台接口缺失分别报告。

本地找到的公开上游data_storage header少14个当前fork字段，未用于编译，不自行定义其数据协议值；SHA/差异见configs/platform_resolution.json。完整匹配SDK/源码树路径尚待提供，或者需要业务负责人给出这些外部边界的正式契约。设备不是Linux层级前提。

可选 `sim_initialization_production_host` 编译当前生产BUILD的50个无条件SIM源（sim_manager由现有sim_production_host提供），列表逐项列在BUILD.gn与候选清单；理由为完整公共OnInit的真实业务依赖闭包探测，不声称最终最小链接集合已经固定。生产TelEventHandler/TelEventQueue/Observer/CoreManagerInner等进一步源依赖尚待纳入；不得以桩替换这些业务/内部事件实现。
