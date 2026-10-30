/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// TARGET ARCHITECTURE SKELETON / PROPOSED: 尚未接入生产或构建，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §9.3、§15.1～15.3。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "compatibility/output/legacy_output_adapter.h"

namespace OHOS::Telephony::MultiSim {
/* DeliverOriginalInt仍仅声明：沿host callbacks_.DeliverOriginalInt(route, legacyResult)
 * 分别交付原callback、同步等待结果或内部回执；不把RequestId当旧int返回值。
 * 同步等待由C在原调用线程/预算处理，不占用T/A接纳域；输出失败不回滚卡/DB效果。
 */
/* 方法仅声明，计划复用：
 * NotifyOriginalAccountChanged -> MultiSimMonitor::NotifySimAccountChanged；
 * TransferOriginalShowNumberResult -> Core CallbackHostPort 保持原 callback->Transfer(Int32)。
 * A输出与T激活Hook分别随assignment/adapters和topology/adapters，原节点各由Owner决定。
 * 本C输出Adapter不持业务缓存、不补发，也不让输出失败触发设备重试。
 */
} // namespace OHOS::Telephony::MultiSim
