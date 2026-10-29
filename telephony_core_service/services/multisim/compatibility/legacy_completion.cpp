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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §9.1～9.4、§15.1、§16.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "compatibility/legacy_completion.h"

namespace OHOS::Telephony::MultiSim {
/* CompleteShowNumber 仅声明；伪代码：
 * 只有 completion.legacyResult 为原整体成功且计划要求原Manager通知时，
 * output_.NotifyOriginalAccountChanged()；随后 output_.TransferOriginalShowNumberResult。
 * 顺序对应 SimManager::SetShowNumber 的 NotifySimAccountChanged 在返回后
 * CoreServiceSim::SetShowNumber 才 callback->Transfer(ret)。通知投递/消费者到达不保证先后。
 * 卡成功但DB/缓存失败不发成功账户通知；卡晚到不再次进入此原完成出口。
 * 原通知或callback交付失败不反转设备/DB结果，不增加持久outbox或自动重试。
 */
} // namespace OHOS::Telephony::MultiSim
