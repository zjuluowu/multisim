# BLOCKED项目处理记录（2026-10-08）

本轮基线HEAD=df8f2f017f83c70145b20255b4e9042f7a5fd923；工作树包括上轮未提交增量。修改仅位于test/systemtest。

## FACT / SOURCE_EVIDENCE

补齐固定版本真实外部头：HUKS六份头、ConfigPolicy、cJSON、C/C++参数及watcher、string_ex、OS账户wrapper。原始版权和许可保留；共21份原始平台头在dependency_lock记录SHA256，构建前验证摘要。HUKS、JSON和ConfigPolicy没有运行实现，因此本轮不宣称其服务行为验证。

Bundle、Parcel、native pointer与日志是明确局部适配。host_refptr保留GetRefPtr、weak identity与promote的可观察载体语义，以本地std control block管理生命周期，不能等同OHOS intrusive ABI。iface_cast只接受实际实现接口的本地对象，不创建Proxy。没有任意成功返回值用于伪造平台服务。

本轮工具/实际命令/执行状态见run_record.md；设施和业务分开统计。native_pointer_test为INFRA，三个用例各有15项设计及Oracle；不通过该设施证明SA/IPC或SIM正确性。

## 存储协议核实与未解决项

GitHub原参考仓telephony_data_storage/master@843a8be7f8d1dc304870a3999b754a03c6cce57d；额外核对Gitee同名仓master@bb397b28578cbaea30a5d957043d646962a59230。两份sim/include/sim_data.h字节相同；Gitee来源没有选入构建。公开分支调查没有找到能确认匹配当前fork的字段定义，不推断所有历史分支均不存在这些字段。

仍缺的14个符号、生产引用路径和行号见configs/storage_protocol_requirements.json。该清单仅用于源码/外部协议依赖定位；不是数据库表结构Oracle。不猜测字段字符串、不提供伪造schema、不用桩生成SIM状态或账户。需要匹配fork的header/仓库分支/可审查协议来源，相关信息问题已提出。

## INFERENCE / ENVIRONMENT_LIMITATION / OPEN_QUESTION

FACT：最终审计GN退出0、Ninja退出1，45个初始化生产对象编译成功，missing_headers=[]；全部43条编译诊断来自14个存储协议成员，分布于5个生产文件。编译尚未到链接阶段，不能声称只要补字段就能运行SIM初始化。

ENVIRONMENT_LIMITATION：没有设备不是B场景阻塞理由；这里缺少的是匹配软件接口和受控运行装配。链接依赖、参数/配置/JSON/HUKS/SA/CES运行模型仍可能在后续阶段暴露。

OPEN_QUESTION：fork存储协议来自何处；哪些字段与当前生产ABI一致；无卡是否保留账户、事件重复以及过渡态是否允许仍需独立业务Oracle审查。

## 后续顺序

1. 取得并固定匹配存储协议来源；若仍无法取得，保持相关编译与业务场景BLOCKED。
2. 完成真实生产目标链接审计，不以SIM内部业务替身补缺符号。
3. 按正式依赖边界实现参数/事件/状态注册/存储等必要受控运行服务，未装配提供者明确失败。
4. 通过ISimManager初始化，经GoogleMock RIL正式响应驱动Absent，观察真实状态与账户。实际运行前原五场景及未初始化C候选不更新为PASS。
5. 采集未确认可观察行为为LEGACY_BEHAVIOR；真实Legacy/Split比较需要两版本实际执行，不能用同一结果复制替代。
