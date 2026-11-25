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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 仅接入 testonly 骨架构建，尚未接入生产，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §8.2、§9.2、§13.2。
// Host discovery and metadata use the configured store and Provider ports.

#include "account/account_store_adapter.h"

namespace OHOS::Telephony::MultiSim {
namespace {
constexpr auto kSimInfoUri = "datashare:///com.ohos.simability/sim/sim_info";
AccountStoreWriteResult NotImplemented()
{ return {-1, false, false}; }
}

AccountStoreWriteResult AccountStoreAdapter::WriteDisplayNumber(const AccountNumberPatch &) { return NotImplemented(); }
AccountMetadataOutcome AccountStoreAdapter::WriteDisplayNumber(const AccountMetadataOperation &operation,
    const AccountNumberPatch &patch)
{
    if (!metadata_) { return {-1, true, EffectKnowledge::NotSent}; }
    return metadata_->UpdateAccountField(operation, kSimInfoUri, "sim_id",
        std::to_string(patch.subscription.legacySimId), "phone_number", patch.number);
}

AccountStoreWriteResult AccountStoreAdapter::WriteActiveProjection(const AccountActivePatch &) { return NotImplemented(); }
AccountStoreWriteResult AccountStoreAdapter::WriteDisplayName(const AccountNamePatch &) { return NotImplemented(); }
AccountStoreWriteResult AccountStoreAdapter::WriteLabel(const AccountLabelPatch &) { return NotImplemented(); }
AccountMetadataOutcome AccountStoreAdapter::WriteDisplayName(const AccountMetadataOperation &operation,
    const AccountNamePatch &patch)
{
    if (!metadata_) { return {-1, true, EffectKnowledge::NotSent}; }
    return metadata_->UpdateAccountField(operation, kSimInfoUri, "sim_id",
        std::to_string(patch.subscription.legacySimId), "show_name", patch.name);
}

AccountMetadataOutcome AccountStoreAdapter::WriteLabel(const AccountMetadataOperation &operation,
    const AccountLabelPatch &patch)
{
    if (!metadata_) { return {-1, true, EffectKnowledge::NotSent}; }
    return metadata_->UpdateAccountField(operation, kSimInfoUri, "icc_id", patch.iccid,
        "sim_label_index", patch.legacyLabel);
}

std::optional<AccountMetadataEvidence> AccountStoreAdapter::ReadMetadataEvidence(
    const AccountMetadataOperation &operation)
{
    return metadata_ ? metadata_->ReadMetadataEvidence(operation) : std::nullopt;
}

AccountMetadataOutcome AccountStoreAdapter::WriteImsPreference(const AccountMetadataOperation &operation,
    const AccountImsPreferencePatch &patch)
{
    if (!metadata_) { return {-1, true, EffectKnowledge::NotSent}; }
    return metadata_->UpdateAccountField(operation, kSimInfoUri, "icc_id", patch.iccid, "ims_switch", patch.preference);
}
AccountLoadResult AccountStoreAdapter::LoadAccounts(const AccountLoadRequest &request)
{ return discovery_ ? discovery_->LoadAccounts(request) : legacy_.load(*legacy_.store, request); }
AccountStoreWriteResult AccountStoreAdapter::WriteRemoval(const AccountRemovalPatch &) { return NotImplemented(); }
ImsPreferenceRead AccountStoreAdapter::QueryImsPreferenceByIccid(const std::string &iccid)
{
    return metadata_ ? metadata_->QueryAccountField(kSimInfoUri, "icc_id", iccid, "ims_switch") : ImsPreferenceRead {};
}

AccountIdentityMatch AccountStoreAdapter::MatchAccountByIccid(const std::string &iccid)
{
    return discovery_ ? discovery_->MatchAccountByIccid(iccid) : legacy_.match(*legacy_.store, iccid);
}

AccountDiscoveryStoreResult AccountStoreAdapter::WriteDiscoveredAccount(const AccountDiscoveryWritePlan &plan)
{
    return discovery_ ? discovery_->WriteDiscoveredAccount(plan) : legacy_.write(*legacy_.store, plan);
}

/* MatchAccountByIccid/WriteDiscoveredAccount delegate to the configured discovery source.
 * WriteDisplayNumber的计划复用位置：
 * 与 MultiSimController::SetShowNumberToDB 相同，转换number到原编码；
 * DataShareValuesBucket只Put SimData::PHONE_NUMBER，调用
 * SimRdbHelper::UpdateDataBySimId(patch.subscription.legacySimId, values)。
 * 保留INVALID_VALUE->原DATABASE_WRITE_FAIL，其余返回的旧解释；不宣称CAS。
 * DB返回后交T接纳缓存，Adapter不改localCacheInfo_/allLocalCacheInfo_。
 * LoadAccounts(request)：Current走原QueryAllValidData，Historical走QueryAllData；
 * 每次分别返回原请求view/attempt、结果和T字段片段，同时保留同次查询的storedRoles给A。
 * complete按原Query成功判断；legacyReturn另按result != INVALID_VALUE保留。
 * 一次失败不替换另一投影，空成功与失败分开；不从A当前状态补造旧DB读取的角色行。
 * MatchAccountByIccid沿原QueryDataByIccId，区分查询失败与确知无匹配；不分配新身份。
 * WriteDiscoveredAccount只执行T已准备字段与A已授权角色片段的一次明确原Insert/Update；
 * 新建时回传原分配simId，A初始角色计划不要求先有该simId。Adapter不自行决定初始角色。
 * WriteRemoval沿原同次解绑/标签/active补丁，拒绝夹带其他Owner字段；部分效果逐项返回。
 * provider重连/初始化/用户切换不可借Adapter整行回灌A角色或覆盖较新T字段。
 */
} // namespace OHOS::Telephony::MultiSim
