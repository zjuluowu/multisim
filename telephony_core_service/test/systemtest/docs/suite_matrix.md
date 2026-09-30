# Suite Matrix v1：计划覆盖，当前无自动化

基线为source_manifest。计数是实际/合成卡数量，与配置Slot数量分开；0卡也需至少一个有效配置Slot。产品参数默认1（services/etc/param/telephony.para:14），telephony_types.h:49–56存在3与分布式8常量，:100–151按产品/虚拟Modem参数计算；不据常量宣布支持4张物理卡。slot索引3的capability特殊处理不等同三张卡。

状态定义：AUTOMATED=已实现自动化（执行结果另列）；DEVICE_REQUIRED=该真实平台/硬件结论必须设备验证；BLOCKED=计划存在但代码/条件/Oracle缺失；NOT_APPLICABLE=有具体理由。首批服务入口AUTOMATED=0，五个首批设计均未执行。批准后的独立组件容量AUTOMATED=1，见文末增量。

| ID | 组合/维度 | 计划Case或扩展 | 当前状态 | 执行/原因 |
|---|---|---|---|---|
| M-ENTRY | 启动/一配置Slot/尚无状态报告 | SIM-B-001 | BLOCKED | host/入口问题；可访问不等就绪 |
| M-0PSIM | 0卡/pSIM槽/Absent | SIM-B-002 | BLOCKED | RIL/存储/合法协议未实现 |
| M-1PSIM | 1 pSIM/Ready/身份/账户 | SIM-B-003 | BLOCKED | 文件加载SDK与Oracle审核 |
| M-2PSIM | 2 pSIM/Ready/局部改名/隔离 | SIM-B-004 | BLOCKED | 2-slot产品能力和隔离Oracle待确认 |
| M-PIN | 1 pSIM/PIN Locked/合成响应 | SIM-B-005 | BLOCKED | 合成PIN协议/host未实现 |
| M-3 | 3卡/pSIM或混合/各Slot映射 | 后续 | BLOCKED | 需核实具体产品、权限、额外槽语义；不扩大桩冒充支持 |
| M-4 | 4卡/物理或虚拟分布式 | 后续 | BLOCKED | 本产品未证实支持；不能直接标NOT_APPLICABLE |
| M-E1 | 1 eSIM/Ready或Locked | 后续 | BLOCKED | eSIM宏/SDK/SA边界与profile契约 |
| M-E2 | eSIM Disabled/Profile inactive/账户可见性 | 后续 | BLOCKED | O-Q04；SIM Active≠Profile enabled |
| M-MIX | pSIM+eSIM/MEP/2或更多profile | 后续 | BLOCKED | 产品port/slot能力未核实 |
| M-UNKNOWN | UNKNOWN与NOT_READY、迟到初始响应 | 后续 | BLOCKED | O-Q01；正式合法输入待核实 |
| M-DISABLED | pSIM Active=false/禁用与账户 | 后续 | BLOCKED | Disable语义/账户与角色Oracle |
| M-VOICE | 默认Voice设置/无卡/故障 | 后续 | BLOCKED | 独立角色失效值/联动契约 |
| M-SMS | 默认SMS设置/无卡/故障 | 后续 | BLOCKED | O-Q03 |
| M-DATA | 默认Data/Primary设置/重试 | 后续 | BLOCKED | O-Q02/O-Q03，保留radio_protocol生产逻辑 |
| M-ACTIVE | Activate/Deactivate/双卡隔离 | 后续 | BLOCKED | 对角色联动/跨模块通知待审 |
| M-RESTART | Linux服务进程重启/持久化恢复 | 后续 | BLOCKED | 可保存隔离store，但不能当设备重启 |
| M-REPLAY | 桩重置/受控RIL重新响应 | 后续 | BLOCKED | 只证明脚本重放，不是Modem恢复 |
| M-DELAY | 启动/卡变化/延迟通知与响应 | 后续 | BLOCKED | 有界收敛Oracle预算待校准 |
| M-DUP | 重复合法RIL通知/公共事件 | 后续 | BLOCKED | O-Q06，不能强制exactly-once |
| M-ORDER | 乱序/迟到响应/不同slot关联 | 后续 | BLOCKED | 先定义合法请求生命周期和关联语义 |
| M-RIL-FAIL | GetSimStatus/文件/PIN错误、失联 | 后续 | BLOCKED | 需错误传播/恢复Oracle |
| M-DS-FAIL | DataShare连接、查询、写失败 | 后续 | BLOCKED | 不能用DB表结构PASS；错误/回滚契约待审 |
| M-SR-FAIL | State Registry更新失败 | 后续 | BLOCKED | 状态/回调/重试联动Oracle待审 |
| M-CE-FAIL | 公共事件发布失败 | 后续 | BLOCKED | 事件必达与业务返回关系待审 |
| M-AUTH | native/system/app、GET/SET拒绝、slot3 | 后续 | BLOCKED | 保留真实权限/capability判断，O-Q08 |
| M-CONCUR | 双卡并发查询/改名/角色操作 | 后续 | BLOCKED | 只合法终态集合和必要偏序；不承诺所有交错 |
| A-E2E | 真实SA/Binder、Modem、实体卡、平台服务 | 设备套件 | DEVICE_REQUIRED | 当前没有设备运行能力 |
| A-BOOT | 设备重启/真实持久化/Modem恢复 | 设备套件 | DEVICE_REQUIRED | Linux进程重启不能覆盖 |
| A-PIN | 真实PIN错误重试/PUK | 独立授权设备实验 | DEVICE_REQUIRED | 默认禁用，当前未授权 |
| A-PROFILE | Profile删除/eUICC重置 | 独立授权设备实验 | DEVICE_REQUIRED | 默认禁用，当前未授权 |
| NA-PROFILE | 首批pSIM场景的Profile/Port设置 | SIM-B-001..005 | NOT_APPLICABLE | 本切片关闭eSIM，不是整个产品不支持 |
| NA-NET | 首批场景的真实无线网络注册 | SIM-B-001..005 | NOT_APPLICABLE | 只保护SIM生产边界，无真实Modem路径 |

组合策略：先0/1/2卡主路径，再Ready/Locked/Absent与单故障配对；3/4和eSIM必须先产品能力确认。未来能力扩展不能自动推导笛卡尔积覆盖；每条新组合都建立15项设计与Oracle。维护字段需包含环境、权限、卡数与Slot数、类型、Active/profile、角色、生命周期、故障、调度脚本、Case、Oracle、状态及实际执行结果。

未覆盖声明：原服务/状态场景没有执行生产逻辑；身份/账户/事件/故障/隔离/并发仍是设计。仅组件容量查询实际执行，C基线仅O-L06已采集。

## 批准后实施增量

M-CAPACITY：单槽产品参数/未OnInit/只读GetMaxSimCount；SIM-COMP-001；AUTOMATED；正常执行1 PASS，另有检查器负控1预期FAIL。并未建立任何卡状态，卡数维度N/A。原五条及所有状态/账户/设备矩阵状态不变。本文上文AUTOMATED=0和“未执行”是第一批历史调查状态；现有组件自动化总数为1，参见run_record.md。


当前更新：Linux 场景通过固定版本 GoogleMock 构造外部参数边界，生产目标不变；详见 [Linux GoogleMock 方案](linux_gmock_scenarios.md)。早期手写桩描述属于历史记录。原五条业务场景仍 BLOCKED。

## 当前增量矩阵

| 组合 | 层级 | 状态 | 实际执行 |
|---|---|---|---|
| 配置槽容量，只读，无卡状态输入 | B | AUTOMATED | 1 PASS |
| 未初始化状态/hasCard/active账户基线 | C | BLOCKED | 链接失败，0执行；不是无卡 |
| 正式RIL Absent + 状态/账户 | B | BLOCKED | 初始化生产闭包未通过，0执行 |
| Ready / 双pSIM隔离 / PIN Locked | B | BLOCKED | 同一生产闭包前置缺失，0执行 |
| 服务公开入口 | B | BLOCKED | capability/能力提供者未核实，0执行 |
| 事件payload/所有权、RIL正式响应自检 | INFRA | IMPLEMENTED | 4 PASS，单独计数，不算SIM业务 |

以上B场景设计无需实体设备；缺少外部正式接口/生产初始化适配不能改写成DEVICE_REQUIRED。真正Modem/SIM/eSIM/设备重启仍属A独立验证。
