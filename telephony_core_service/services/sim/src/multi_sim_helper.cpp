/*
 * Copyright (C) 2026-2026 Huawei Device Co., Ltd.
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

#include "multi_sim_helper.h"
#include "telephony_ext_wrapper.h"
#include "core_manager_inner.h"
#include "sim_data.h"
#include <cstdlib>
#include <openssl/sha.h>

#ifdef  CORE_SERVICE_SUPPORT_ESIM
#include "reset_response.h"
#endif

namespace OHOS {
namespace Telephony {
static const std::string PARAM_SIMID = "simId";
static const std::string PARAM_SLOTID = "slotId";
static const int32_t EVENT_CODE = 1;
static constexpr int32_t DEC_TYPE = 10;
static const std::string DEFAULT_SMS_SIMID_CHANGED = "defaultSmsSimIdChanged";
static const std::string DEFAULT_MAIN_SIMID_CHANGED = "defaultMainSimIdChanged";
static const std::string DEFAULT_VOICE_SIMID_CHANGED = "defaultVoiceSimIdChanged";
static const std::string DEFAULT_CELLULAR_DATA_SIMID_CHANGED = "defaultCellularDataSimIdChanged";
static const std::string PARAM_SET_PRIMARY_STATUS = "setDone";
static const std::string PARAM_SET_PRIMARY_IS_USER_SET = "isUserSet";
static const std::string SIM_LABEL_STATE_PROP = "persist.ril.sim_switch";
static const std::string ESIM_LSI_CFG_PROP = "persist.ril.esim_lsi_cfg";
static const std::string TSTS_MODE_PROP = "persist.telephony.tsts_mode";
static const std::string SUPPORT_ESIM_MEP = "const.ril.sim.esim_support_mep";
static const std::string ESIM_SUPPORT_PARAM = "const.ril.esim_type";
static constexpr const char *TYPE_ESIM_3_SEL_2 = "3";
static constexpr int32_t ESIM_LSI_CFG_ESIM_SLOTID_COUNT = 1;
static constexpr int32_t ESIM_LSI_CFG_GROUP_COUNT = 4;
static constexpr int32_t ESIM_LSI_CFG_ITEM_PER_GROUP = 2;
static constexpr int32_t ESIM_LSI_CFG_ITEM_COUNT =
    ESIM_LSI_CFG_ESIM_SLOTID_COUNT + ESIM_LSI_CFG_GROUP_COUNT * ESIM_LSI_CFG_ITEM_PER_GROUP;
static constexpr int32_t ESIM_LSI_CFG_SCI_MIN = 0;
static constexpr int32_t ESIM_LSI_CFG_SCI_MAX = 2;
static constexpr int32_t ESIM_LSI_CFG_SCI_INVALID = 255;
static constexpr int32_t SIM_SCI_0 = 0;
static const int32_t PROP_LEN = 65;
static constexpr int32_t SLOT_ID_0 = 0;
static constexpr int32_t SLOT_ID_1 = 1;
static constexpr int32_t SLOT_ID_2 = 2;
static constexpr int32_t SLOT_ID_3 = 3;
constexpr int32_t PSIM1 = 1;
constexpr int32_t PSIM2 = 2;
constexpr int32_t PSIM1_PSIM2 = 0;
constexpr int32_t PSIM1_ESIM = 1;
constexpr int32_t PSIM2_ESIM = 2;
static constexpr int32_t TSTS_MODE_OFF = 0;
static constexpr int32_t TSTS_MODE_ON = 1;
static constexpr int32_t MEP_ESIM_SCI_ID = 1;
static constexpr int32_t TSTS_ESIM_SCI_ID = 2;

#ifdef CORE_SERVICE_SUPPORT_ESIM
static const std::string GSM_SIM_ATR = "gsm.sim.hw_atr";
static const std::string GSM_SIM_ATR1 = "gsm.sim.hw_atr1";
static const std::string GSM_SIM_ATR2 = "gsm.sim.hw_atr2";
static const std::string GSM_SIM_ATR3 = "gsm.sim.hw_atr3";
#endif

MultiSimHelper::MultiSimHelper()
{
    tstsMode_ = OHOS::system::GetIntParameter(TSTS_MODE_PROP, TSTS_MODE_OFF);
}

MultiSimHelper::~MultiSimHelper()
{}

bool MultiSimHelper::AnnounceDefaultSmsSimIdChanged(int32_t simId)
{
    AAFwk::Want want;
    want.SetParam(PARAM_SIMID, simId);
    want.SetAction(EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_SMS_SUBSCRIPTION_CHANGED);
    int32_t eventCode = EVENT_CODE;
    std::string eventData(DEFAULT_SMS_SIMID_CHANGED);
    return PublishSimFileEvent(want, eventCode, eventData);
}

bool MultiSimHelper::AnnounceDefaultCellularDataSimIdChanged(int32_t simId)
{
    AAFwk::Want want;
    want.SetParam(PARAM_SIMID, simId);
    want.SetAction(EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_DATA_SUBSCRIPTION_CHANGED);
    int32_t eventCode = EVENT_CODE;
    std::string eventData(DEFAULT_CELLULAR_DATA_SIMID_CHANGED);
    return PublishSimFileEvent(want, eventCode, eventData);
}

bool MultiSimHelper::AnnouncePrimarySimIdChanged(int32_t simId, int32_t slotId)
{
    AAFwk::Want want;
    want.SetParam(PARAM_SIMID, simId);
    want.SetParam(PARAM_SLOTID, slotId);
    want.SetAction(EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_MAIN_SUBSCRIPTION_CHANGED);
    int32_t eventCode = EVENT_CODE;
    std::string eventData(DEFAULT_MAIN_SIMID_CHANGED);
    return PublishSimFileEvent(want, eventCode, eventData);
}

bool MultiSimHelper::AnnounceDefaultVoiceSimIdChanged(int32_t simId)
{
    AAFwk::Want want;
    want.SetParam(PARAM_SIMID, simId);
    want.SetAction(EventFwk::CommonEventSupport::COMMON_EVENT_SIM_CARD_DEFAULT_VOICE_SUBSCRIPTION_CHANGED);
    int32_t eventCode = EVENT_CODE;
    std::string eventData(DEFAULT_VOICE_SIMID_CHANGED);
    return PublishSimFileEvent(want, eventCode, eventData);
}

void MultiSimHelper::PublishSetPrimaryEvent(bool setDone, bool isUserSet)
{
    AAFwk::Want want;
    want.SetAction(EventFwk::CommonEventSupport::COMMON_EVENT_SET_PRIMARY_SLOT_STATUS);
    want.SetParam(PARAM_SET_PRIMARY_STATUS, setDone);
    want.SetParam(PARAM_SET_PRIMARY_IS_USER_SET, isUserSet);
    EventFwk::CommonEventData data;
    data.SetWant(want);

    EventFwk::CommonEventPublishInfo publishInfo;
    publishInfo.SetSticky(true);
    bool publishResult = EventFwk::CommonEventManager::PublishCommonEvent(data, publishInfo, nullptr);
    TELEPHONY_LOGI("setDone: %{public}d, isUserSet: %{public}d, result: %{public}d", setDone, isUserSet, publishResult);
}

bool MultiSimHelper::PublishSimFileEvent(const AAFwk::Want &want, int eventCode, const std::string &eventData)
{
    EventFwk::CommonEventData data;
    data.SetWant(want);
    data.SetCode(eventCode);
    data.SetData(eventData);
    EventFwk::CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool publishResult = EventFwk::CommonEventManager::PublishCommonEvent(data, publishInfo, nullptr);
    return publishResult;
}

std::string MultiSimHelper::EncryptIccId(const std::string &iccid)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, iccid.c_str(), iccid.size());
    SHA256_Final(hash, &sha256);
    std::string encryptIccId = SIMUtils::BytesConvertToHexString(hash, SHA256_DIGEST_LENGTH);
    return encryptIccId;
}

void MultiSimHelper::SimDataBuilder(int32_t slotId, DataShare::DataShareValuesBucket &values,
    const std::string &iccId, int32_t simLabel, bool isEsim)
{
    DataShare::DataShareValueObject slotObj(slotId);
    DataShare::DataShareValueObject iccidObj(iccId);
    DataShare::DataShareValueObject valueObj(ACTIVE);
    DataShare::DataShareValueObject simLabelIndexObj(simLabel);
    DataShare::DataShareValueObject isEsimObj(isEsim);
    values.Put(SimData::SLOT_INDEX, slotObj);
    values.Put(SimData::ICC_ID, iccidObj);
    values.Put(SimData::CARD_ID, iccidObj); // iccId == cardId by now
    values.Put(SimData::IS_ACTIVE, valueObj);
    values.Put(SimData::IS_ESIM, isEsimObj);
    values.Put(SimData::SIM_LABEL_INDEX, simLabelIndexObj);
    if (SIM_SLOT_COUNT == 1) {
        DataShare::DataShareValueObject mainCardObj(MAIN_CARD);
        values.Put(SimData::IS_MAIN_CARD, mainCardObj);
        values.Put(SimData::IS_VOICE_CARD, mainCardObj);
        values.Put(SimData::IS_MESSAGE_CARD, mainCardObj);
        values.Put(SimData::IS_CELLULAR_DATA_CARD, mainCardObj);
    } else {
        DataShare::DataShareValueObject notMainCardObj(NOT_MAIN);
        values.Put(SimData::IS_MAIN_CARD, notMainCardObj);
        values.Put(SimData::IS_VOICE_CARD, notMainCardObj);
        values.Put(SimData::IS_MESSAGE_CARD, notMainCardObj);
        values.Put(SimData::IS_CELLULAR_DATA_CARD, notMainCardObj);
    }
}

void MultiSimHelper::BuildSimPresentValues(int32_t slotId, DataShare::DataShareValuesBucket &values,
    const std::string &iccId)
{
    DataShare::DataShareValueObject slotObj(INVALID_VALUE);
    DataShare::DataShareValueObject iccidObj(iccId);
    DataShare::DataShareValueObject valueObj(ACTIVE);
    DataShare::DataShareValueObject simLabelIndexObj(slotId + 1);
    DataShare::DataShareValueObject isEsimObj(false);
    DataShare::DataShareValueObject notMainCardObj(NOT_MAIN);
    values.Put(SimData::SLOT_INDEX, slotObj);
    values.Put(SimData::ICC_ID, iccidObj);
    values.Put(SimData::CARD_ID, iccidObj);
    values.Put(SimData::IS_ACTIVE, valueObj);
    values.Put(SimData::SIM_LABEL_INDEX, simLabelIndexObj);
    values.Put(SimData::IS_ESIM, isEsimObj);
    values.Put(SimData::IS_MAIN_CARD, notMainCardObj);
    values.Put(SimData::IS_VOICE_CARD, notMainCardObj);
    values.Put(SimData::IS_MESSAGE_CARD, notMainCardObj);
    values.Put(SimData::IS_CELLULAR_DATA_CARD, notMainCardObj);
}

bool MultiSimHelper::IsValidSlotString(const char* slotIdStr)
{
    if (slotIdStr == nullptr) {
        return false;
    }
    bool isValid = true;
    for (int i = 0; slotIdStr[i] != '\0'; ++i) {
        if (!std::isdigit(slotIdStr[i])) {
            isValid = false;
            break;
        }
    }
    return isValid;
}

int32_t MultiSimHelper::GetPsimLabelIndex(int slotId)
{
    if (IsEsim(slotId)) {
        TELEPHONY_LOGE("GetPsimLabelIndex error sim type");
        return PSIM1;
    }
    if (tstsMode_) {
        int32_t simLabelindex = PSIM1;
        if (!TELEPHONY_EXT_WRAPPER.GetSimLabelIndexFromLsiCfg(slotId, simLabelindex)) {
            TELEPHONY_LOGE("TELEPHONY_EXT_WRAPPER failed");
        }
        TELEPHONY_LOGI("GetPsimLabelIndex simLabelindex = %{public}d", simLabelindex);
        return simLabelindex;
    }

    int32_t simLabelState = OHOS::system::GetIntParameter(SIM_LABEL_STATE_PROP, PSIM1_PSIM2);
    if (simLabelState == PSIM1_PSIM2) {
        return slotId == 0 ? PSIM1 : PSIM2;
    } else {
        return simLabelState == PSIM1_ESIM ? PSIM1 : PSIM2;
    }
}

bool MultiSimHelper::IsMff2Esim(int32_t slotId, bool isTstsMode, bool isSupportEsimMep)
{
    if (slotId < SLOT_ID_0 || slotId > SLOT_ID_3) {
        TELEPHONY_LOGE("esim slotId invalid, slotId: %{public}d", slotId);
        return false;
    }
    std::vector<int> sci;
    GetEsimSci(sci);
    if (sci.size() != static_cast<size_t>(ESIM_LSI_CFG_GROUP_COUNT)) {
        TELEPHONY_LOGE("get esim sci failed, slotId: %{public}d", slotId);
        return false;
    }
    const int32_t slotSci = sci[slotId];
    TELEPHONY_LOGI("slot%{public}d esim sci: %{public}d", slotId, slotSci);
 
    if (isTstsMode) {
        TELEPHONY_LOGI("Tsts product, slot%{public}d sci: %{public}d", slotId, slotSci);
        return slotSci == TSTS_ESIM_SCI_ID;
    }
 
    const int32_t simLabelState = OHOS::system::GetIntParameter(SIM_LABEL_STATE_PROP, PSIM1_PSIM2);
    if (isSupportEsimMep && (simLabelState != PSIM1_PSIM2) && (slotSci == MEP_ESIM_SCI_ID)) {
        TELEPHONY_LOGI("mep product, slot%{public}d sci: %{public}d", slotId, slotSci);
        return true;
    }
    return false;
}
 
bool MultiSimHelper::IsEsimSlot(int32_t slotId, bool isEuiccAtr)
{
    const bool isTstsMode = OHOS::system::GetIntParameter(TSTS_MODE_PROP, TSTS_MODE_OFF) == TSTS_MODE_ON;
    const bool isSupportEsimMep = OHOS::system::GetBoolParameter(SUPPORT_ESIM_MEP, false);
    if (isEuiccAtr) {
        // MEP和TSTS模式设备
        if (isTstsMode || isSupportEsimMep) {
            return IsMff2Esim(slotId, isTstsMode, isSupportEsimMep);
        }
 
        // SEP模式设备
        const std::string esimType = OHOS::system::GetParameter(ESIM_SUPPORT_PARAM, "");
        const int32_t simLabelState = OHOS::system::GetIntParameter(SIM_LABEL_STATE_PROP, PSIM1_PSIM2);
        if (esimType == TYPE_ESIM_3_SEL_2) {
            if (simLabelState == PSIM1_PSIM2) {
                return false;
            }
            return slotId == SLOT_ID_1;
        }
    }
    return isEuiccAtr;
}
 
bool MultiSimHelper::IsEsim(int32_t slotId) const
{
#ifdef CORE_SERVICE_SUPPORT_ESIM
    if (!CoreManagerInner::GetInstance().IsSupported(slotId)) {
        return false;
    }
    std::string propAtr = "";
    propAtr = (slotId == SLOT_ID_0) ? GSM_SIM_ATR : propAtr;
    propAtr = (slotId == SLOT_ID_1) ? GSM_SIM_ATR1 : propAtr;
    propAtr = (slotId == SLOT_ID_2) ? GSM_SIM_ATR2 : propAtr;
    propAtr = (slotId == SLOT_ID_3) ? GSM_SIM_ATR3 : propAtr;
    if (propAtr.empty()) {
        TELEPHONY_LOGE("slotId %{public}d invalid, can't get atr prop.", slotId);
        return false;
    }

    char buf[PROP_LEN + 1] = {0};
    if (GetParameter(propAtr.c_str(), "", buf, PROP_LEN) < 0) {
        TELEPHONY_LOGE("get card atr failed, slotId: %{public}d", slotId);
        return false;
    }
    const std::string cardAtr(buf);
    if (cardAtr.empty()) {
        TELEPHONY_LOGE("card atr is empty.");
        return false;
    }

    ResetResponse resetResponse;
    if (!resetResponse.AnalysisAtrData(cardAtr)) {
        TELEPHONY_LOGE("analysis card atr failed, slotId: %{public}d", slotId);
        return false;
    }
    const bool isEsim = IsEsimSlot(slotId, resetResponse.IsEuiccAvailable());
    TELEPHONY_LOGI("slot%{public}d isEsim: %{public}s", slotId, isEsim ? "true" : "false");
    return isEsim;
#else
    return false;
#endif
}

bool MultiSimHelper::IsValidSci(int32_t sci)
{
    return (sci >= ESIM_LSI_CFG_SCI_MIN && sci <= ESIM_LSI_CFG_SCI_MAX) || sci == ESIM_LSI_CFG_SCI_INVALID;
}

bool MultiSimHelper::ParseSci(const std::string &sciStr, int32_t &sci)
{
    errno = 0;
    const char *str = sciStr.c_str();
    char *endPtr = nullptr;
    long value = std::strtol(str, &endPtr, DEC_TYPE);
    if (errno == ERANGE || endPtr == nullptr || endPtr == str || *endPtr != '\0') {
        return false;
    }
    if (value < static_cast<long>(ESIM_LSI_CFG_SCI_MIN) || value > static_cast<long>(ESIM_LSI_CFG_SCI_INVALID)) {
        return false;
    }
    sci = static_cast<int32_t>(value);
    return IsValidSci(sci);
}

void MultiSimHelper::GetEsimSci(std::vector<int> &sci)
{
    sci.clear();
    char buf[PROP_LEN + 1] = {0};
    GetParameter(ESIM_LSI_CFG_PROP.c_str(), "", buf, PROP_LEN);
    std::string lsiCfg(buf);
    if (lsiCfg.empty()) {
        TELEPHONY_LOGE("esim lsi cfg is empty.");
        return;
    }
 
    std::vector<std::string> items;
    std::string item;
    std::stringstream ss(lsiCfg);
    while (std::getline(ss, item, ',')) {
        items.push_back(item);
    }
    // format: 1 skipped item + 4 groups for slotId 0-3, the first number of each group is the sci of the slotId.
    if (items.size() != static_cast<size_t>(ESIM_LSI_CFG_ITEM_COUNT)) {
        TELEPHONY_LOGE("esim lsi cfg format invalid, size %{public}zu", items.size());
        return;
    }
 
    for (int32_t slotId = SLOT_ID_0; slotId <= SLOT_ID_3; slotId++) {
        size_t index = static_cast<size_t>(ESIM_LSI_CFG_ESIM_SLOTID_COUNT + slotId * ESIM_LSI_CFG_ITEM_PER_GROUP);
        int32_t value = 0;
        if (!ParseSci(items[index], value)) {
            TELEPHONY_LOGE("esim lsi cfg sci invalid, slotId %{public}d, sciStr %{public}s",
                slotId, items[index].c_str());
            sci.clear();
            return;
        }
        sci.push_back(value);
    }
    TELEPHONY_LOGI("esim lsi cfg sci size %{public}zu", sci.size());
}

}
}
