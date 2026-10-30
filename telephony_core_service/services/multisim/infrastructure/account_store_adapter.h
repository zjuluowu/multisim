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
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3、§9.2、§13.1～13.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#ifndef OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ACCOUNT_STORE_ADAPTER_H
#define OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ACCOUNT_STORE_ADAPTER_H

#include "topology/ports/account_store.h"

namespace OHOS::Telephony { class SimRdbHelper; }
namespace OHOS::Telephony::MultiSim {
// I实现T字段级端口，借用现有helper，不直连新RDB，不保存业务缓存。
class AccountStoreAdapter final : public AccountStore {
public:
    explicit AccountStoreAdapter(SimRdbHelper &legacyStore) : legacyStore_(legacyStore) {}
    AccountStoreWriteResult WriteDisplayNumber(const AccountNumberPatch &patch) override;
    AccountStoreWriteResult WriteActiveProjection(const AccountActivePatch &patch) override;
    AccountStoreWriteResult WriteDisplayName(const AccountNamePatch &patch) override;
    AccountStoreWriteResult WriteLabel(const AccountLabelPatch &patch) override;
    AccountStoreWriteResult WriteImsPreference(const AccountImsPreferencePatch &patch) override;
    AccountLoadResult LoadAccounts(const AccountReloadTicket &ticket) override;
    AccountMatchResult MatchAccountByIccid(const std::string &iccid) override;
    AccountRowResult ApplyAccountRow(const AccountRowPlan &plan) override;
    int32_t QueryImsPreferenceByIccid(const std::string &iccid, int32_t &value) override;
private:
    SimRdbHelper &legacyStore_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_INFRASTRUCTURE_ACCOUNT_STORE_ADAPTER_H
