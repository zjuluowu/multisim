// SPDX-License-Identifier: Apache-2.0
#include "account/account_store_adapter.h"
#include "sim_data.h"
#include "sim_rdb_helper.h"
#include "sim_rdb_info.h"
#include "telephony_errors.h"
#include "telephony_types.h"

namespace OHOS::Telephony::MultiSim {
namespace {
constexpr int32_t kInvalid = -1;
AccountStorageRow FromLegacy(const SimRdbInfo &row)
{
    return {{row.simId}, row.iccId,
        row.slotIndex >= 0 ? std::make_optional<SlotId>({row.slotIndex}) : std::nullopt,
        std::u16string(row.showName.begin(), row.showName.end()),
        std::u16string(row.phoneNumber.begin(), row.phoneNumber.end()), row.isActive != 0,
        row.simLabelIndex >= 0 ? std::make_optional(row.simLabelIndex) : std::nullopt,
        std::make_optional(row.imsSwitch)};
}
bool Confirmed(const SimRdbInfo &row, const AccountDiscoveryWritePlan &plan,
    const std::optional<SubscriptionId> &identity)
{
    if (!identity || row.simId != identity->legacySimId || row.iccId != plan.accountFields.iccid ||
        row.slotIndex != plan.accountFields.legacyPosition.legacySlotId ||
        static_cast<bool>(row.isActive) != plan.accountFields.active ||
        static_cast<bool>(row.isEsim) != plan.accountFields.isEsim) {
        return false;
    }
    if (plan.accountFields.legacyLabel && row.simLabelIndex != *plan.accountFields.legacyLabel) {
        return false;
    }
    return !plan.roleFields.includeInStorage ||
        (static_cast<bool>(row.isVoiceCard) == plan.roleFields.voice &&
         static_cast<bool>(row.isMessageCard) == plan.roleFields.sms &&
         static_cast<bool>(row.isMainCard) == plan.roleFields.mainCardColumn &&
         static_cast<bool>(row.isCellularDataCard) == plan.roleFields.dataCardColumn);
}
} // namespace

AccountIdentityMatch MatchLegacyAccountByIccid(SimRdbHelper &store, const std::string &iccid)
{
    SimRdbInfo row {};
    const int32_t result = store.QueryDataByIccId(iccid, row);
    if (result != TELEPHONY_ERR_SUCCESS) { return {result, false, std::nullopt}; }
    return {result, true, row.iccId.empty() ? std::nullopt : std::make_optional(FromLegacy(row))};
}

AccountDiscoveryStoreResult WriteLegacyDiscoveredAccount(SimRdbHelper &store,
    const AccountDiscoveryWritePlan &plan)
{
    DataShare::DataShareValuesBucket values;
    values.Put(SimData::SLOT_INDEX, DataShare::DataShareValueObject(plan.accountFields.legacyPosition.legacySlotId));
    values.Put(SimData::IS_ACTIVE, DataShare::DataShareValueObject(plan.accountFields.active ? 1 : 0));
    values.Put(SimData::IS_ESIM, DataShare::DataShareValueObject(plan.accountFields.isEsim));
    if (plan.accountFields.legacyLabel) {
        values.Put(SimData::SIM_LABEL_INDEX, DataShare::DataShareValueObject(*plan.accountFields.legacyLabel));
    }
    if (plan.roleFields.includeInStorage) {
        values.Put(SimData::IS_MAIN_CARD, DataShare::DataShareValueObject(plan.roleFields.mainCardColumn ? 1 : 0));
        values.Put(SimData::IS_VOICE_CARD, DataShare::DataShareValueObject(plan.roleFields.voice ? 1 : 0));
        values.Put(SimData::IS_MESSAGE_CARD, DataShare::DataShareValueObject(plan.roleFields.sms ? 1 : 0));
        values.Put(SimData::IS_CELLULAR_DATA_CARD, DataShare::DataShareValueObject(plan.roleFields.dataCardColumn ? 1 : 0));
    }
    int32_t result = kInvalid;
    if (plan.existingSubscription) {
        result = store.UpdateDataByIccId(plan.accountFields.iccid, values);
    } else {
        values.Put(SimData::ICC_ID, DataShare::DataShareValueObject(plan.accountFields.iccid));
        values.Put(SimData::CARD_ID, DataShare::DataShareValueObject(plan.accountFields.iccid));
        int64_t rowId = 0;
        result = store.InsertData(rowId, values);
    }

    SimRdbInfo row {};
    const int32_t readBack = store.QueryDataByIccId(plan.accountFields.iccid, row);
    std::optional<SubscriptionId> assigned;
    if (readBack == TELEPHONY_ERR_SUCCESS && !row.iccId.empty()) { assigned = {{row.simId}}; }
    if (plan.existingSubscription && assigned && !(*assigned == *plan.existingSubscription)) {
        assigned.reset();
    }
    const bool confirmed = Confirmed(row, plan, assigned);
    return {result, assigned, confirmed, true,
        confirmed ? EffectKnowledge::ConfirmedApplied : EffectKnowledge::Unknown};
}
} // namespace OHOS::Telephony::MultiSim
