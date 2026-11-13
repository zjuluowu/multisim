# INFRA-FFRT-002 生产同步任务与隔离

## 1. Test Case Metadata

Case ID=INFRA-FFRT-002；Name=生产同步任务与隔离；Domain=External Scheduling Infrastructure；Requirement ID=HOST-SCHEDULER；Contract ID=FFRT-GENERAL；Invariant ID=HOST-ISOLATION；Priority=P0；Level=INFRA（非B业务用例）；Type=设施契约一致性；拓扑=Linux原生、合成载体/GoogleMock；Owner=TBD；Status=AUTOMATED（执行结果独立见run_record）。

## 2. Business Intent

为后续正式RIL→生产SIM处理提供可执行调度基础。受保护行为=生产同步任务与隔离。Out of Scope=SIM业务、账户、服务入口、IPC、真实平台/设备、所有并发交错。

## 3. External Contract

Trigger/Input：生产TelFFRTUtils::SubmitSync执行合成任务。Expected Observable Output：wait返回后合成完成标志为true；关闭general队列并回收。Given=无残留worker；When=该设施输入；Then=观察指定输出；And=scope退出后清理。所有强制断言关联FFRT-GENERAL。

## 4. System Preconditions

卡数量、pSIM/eSIM、Profile、Slot、SimId/Subscription、Active、默认Voice/SMS/Data、Primary、Radio、网络、权限、用户及SIM持久化环境全部N/A：设施没有初始化SIM或访问平台服务。工具版本/固定外部源见dependency_lock与upstream_contracts。

## 5. Initial State

开始前scheduler workers=0，显式reset诊断计数；独立scope和合成输入。上一例清理未完成不得继续复用。

## 6. Test Steps

1. 构造独立外部观察者/闸门和queue owner；确认初态。
2. 输入边界：生产TelFFRTUtils::SubmitSync执行合成任务；验证接口为回调/公开cancel或wait/完成标志。
3. 验证结果：wait返回后合成完成标志为true；关闭general队列并回收（FFRT-GENERAL）。
4. 释放owner和general队列，在2s内验证外部设施清理（FFRT-GENERAL）。

## 7. Expected Final State

workers=0且taskFailures=0。允许操作系统排程延迟，不允许污染后续scope；无持久化或卡身份修改。

## 8. Business Invariants and State Transition

仅设施隔离不变量，适用前提为本受控调度子集；初态空，过渡为受控pending/running，终态停止。不推导SIM业务不变量。

## 9. Event/Callback

wait返回后合成完成标志为true；关闭general队列并回收。回调次数仅约束取消设施的禁止递送或允许到达，不作为SIM公共事件Oracle。必要偏序来自输入/完成闸门；超时2s，不要求毫秒精确时序。

## 10. Multi-SIM Isolation

N/A：本例无SIM实例；不能代替SIM2操作对SIM1影响验证。

## 11. Persistence/Failure/Concurrency

无persist，task异常记诊断而非吞掉；待执行取消和运行取消分开。每queue单worker，不代表真实FFRT并行模型；析构回收与runner看门狗限制残留。

## 12. Verification Evidence and Pass/Fail Criteria

GoogleTest自动检查上述FFRT-GENERAL条件，失败/超时为FAIL；XML及脱敏JSON记录Case/Oracle，profile/map审计原生产参与。日志只诊断。无真实卡数据。

## 13. Architecture Independence

没有生产私有访问、内部字段/容器断言或数据库断言。直接生产事件类路径只用于INFRA，未来SIM业务主路径仍为ISimManager/正式RIL回复。固定task.h wait声明与当前生产SubmitSync包装；不验证完整FFRT祖先任务语义。

## 14. Traceability and Definition of Done

Host scheduling → FFRT-GENERAL → HOST-ISOLATION → INFRA-FFRT-002 → event_transport_test.cpp。需编译/实际执行/清理/证据；不宣称SIM原始第一阶段完成。

## 15. Suite Matrix/Combination Coverage

INFRA、Linux单进程、合成输入、AUTOMATED；执行数独立统计。扩展前提为匹配生产初始化闭包与外部协议。原SIM矩阵状态不变。
