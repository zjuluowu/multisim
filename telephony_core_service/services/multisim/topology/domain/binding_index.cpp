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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §4.3、§10.1。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/domain/binding_index.h"

namespace OHOS::Telephony::MultiSim {
BindingIndex::BindingIndex(const SlotCatalog &catalog) : catalog_(catalog) {}
// Find/AcceptBinding 等仅声明：迁移时由 TopologyService 在同一次短提交中
// 核查旧/新绑定键及账户约束；不因目录存在就自动发现卡、建账或发设备命令。
} // namespace OHOS::Telephony::MultiSim
