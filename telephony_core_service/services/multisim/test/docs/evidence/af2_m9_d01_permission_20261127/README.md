# Radio 发令许可与原超时

输入为 `a83a77f2a16d324080bd6bd337ef97e9713fcf43`。验证范围为 Target Host
Assignment/Radio 组件。设备路由由受控适配器提供。

## 实现边界

`AssignmentService::SubmitRadioOutsideCommit` 离开 A 后调用 R。R 先登记原 StepRef、
成员、租约和 transport token。平台在核对路由版本和原成员时调用同步发令许可。
A 核对原步骤、轮次、阶段和共享写权，T 核对同一 Binding 及激活状态，R 在短锁域登记
一次许可。A、T、R 的锁都不跨实际平台交接。许可在先的原命令仍按原票结清。
独立复查发现，A 进入 T 后等待时，Stop 可以先写入 A 封口而 R 尚未封口。
因此 R 在最终登记点读取 A 封口，此次读取与 Stop 的原子写确定先后。

超时先消费原步骤并保留唯一 Completion 权，然后在 A 锁外结束平台 SenseSwitch。
正常结果结尾也在锁外结束。原结果已经消费时，后到超时不会接管该步骤。

## 确定性回归

- `RemovedBindingDeniesSendEvenWhenNativeRoutesStayCurrent` 在 B 和 C1 两条路径保持原生
  路由不变，在实际许可点前撤销 Binding，检查零发令及原未发租约可取消。
- `RevocationAfterPermissionKeepsFirstMemberAndDeniesRemainingMembers` 在第一成员取得
  许可后撤销目标，剩余成员不发令。第一成员按原票提供设备结束和复用证据，后继角色不受污染。
- `C1StopWithCheckInFlightDoesNotAuthorizeAnotherPhase` 先封 A，再消费已发 CHECK。
  没有 UPDATE 或 COMPLETE 发令，原轮次仍阻挡 Drain。
- `StopDuringTopologyQualificationDeniesFinalPermission` 在 T 已核对原目标、尚未登记许可时
  封闭 A，验证 R 此时仍能拒绝原发送。
- `TimeoutSealsPermissionBeforeBlockingInPlatformEnd` 并行阻塞平台结束路径并让 Submit
  进入许可核对，验证 A 已释放锁、原 Submit 可先返回、只交付一份超时结果。

原组件测试曾在 Stop 后仍假设可以执行 UPDATE 和 COMPLETE。首次回归分别在这三个场景失败。
现在这些用例令已获得许可的 COMPLETE 在 Stop 后正常结清。上述新增 Stop 用例保留了在
CHECK 时停止的独立反例。原冻结 Oracle、Golden 和 Legacy 实现保留。

## 运行时负控

分别独立重链工作区外的 Target ELF，只有一个生产对象被替换。移除 T 最终核验导致两项
换卡测试实际发出未许可的命令。移除 R 最终 A Stop 门读取使暂停在 T 的原命令发出。
在超时平台 End 路径保留 A 锁时，Submit 等待 A 至屏障超时。四次精确执行均在预期断言失败，
原二进制同一测试退出 0。
[换卡失败](binding-mutated.log) [恢复](binding-restored.log)
[Stop 失败](stop-mutated.log) [恢复](stop-restored.log)
[A 锁反例](end-mutated.log) [恢复](end-restored.log)

## 阶段回归

Radio/Primary 专项 ASan 74/74、TSan 74/74。Target 开发构建在 PIN 共用测试边界整理前
为 212/212。保护资产检查发现该共用头直接依赖测试框架内部类型。已改用调用方的返回类型，
两端断言保留相同输入及结果语义。整理后 Target 全量仍为 212/212。
保护资产检验通过，复核 317 份归档原件、299 项迁移去向和 8 项冻结契约及参考资产。
静态结构、源码演进、107 个独立语法单元及组合头、逻辑 GN/Ninja 均通过。
[Target](target.json.gz) [整理前](pre-pin-boundary-target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz)
[护栏原始失败](integrity-before.log) [护栏恢复](integrity.log) [架构](architecture.log)
[统计](inventory.json.gz)

## 未决范围

正式 SimModule 仍封闭 B/C1。部分交接和 Unknown 后的组结束还需传递可归因的 A 收据。
C1 被中止后仍需核对原协议效果和原 Store 写入，不能凭一个 CHECK 成功清除整轮责任。
独立复查还识别到正常设备结果已经由 R 接纳、A 超时先于 Owner 消费时，A 当前不能接纳为
原晚证据。该原收据缺口与本批发令许可分开验证。
原生 RIL/HDI、产品构建和设备协议验证均为 NOT_RUN。
