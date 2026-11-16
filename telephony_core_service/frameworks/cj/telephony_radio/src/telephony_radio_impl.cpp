/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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


#include "telephony_radio_impl.h"

#include <map>

#include "core_service_client.h"
#include "core_service_errors.h"
#include "telephony_config.h"
#include "telephony_ext_utils_wrapper.h"
#include "telephony_log_wrapper.h"

namespace OHOS {
namespace Telephony {

namespace {
    struct ErrorCodeMapping {
        int32_t legacyCode;
        int32_t romApiCode;
    };
    static const std::map<int32_t, ErrorCodeMapping> RADIO_ERROR_CODE_MAP = {
        // Preserve success explicitly before applying the unknown-error fallback.
        {TELEPHONY_SUCCESS, {TELEPHONY_SUCCESS, TELEPHONY_SUCCESS}},
        // Parameter errors.
        {TELEPHONY_ERR_ARGUMENT_MISMATCH, {CJ_ERROR_TELEPHONY_ARGUMENT_ERROR, CJ_ERROR_TELEPHONY_ARGUMENT_ERROR}},
        {TELEPHONY_ERR_ARGUMENT_INVALID, {CJ_ERROR_TELEPHONY_ARGUMENT_ERROR, CJ_ERROR_TELEPHONY_ARGUMENT_ERROR}},
        {TELEPHONY_ERR_ARGUMENT_NULL, {CJ_ERROR_TELEPHONY_ARGUMENT_ERROR, CJ_ERROR_TELEPHONY_ARGUMENT_ERROR}},
        {TELEPHONY_ERR_SLOTID_INVALID, {CJ_ERROR_TELEPHONY_ARGUMENT_ERROR, CJ_ERROR_TELEPHONY_ARGUMENT_ERROR}},
        {ERROR_SLOT_ID_INVALID, {CJ_ERROR_TELEPHONY_ARGUMENT_ERROR, CJ_ERROR_TELEPHONY_ARGUMENT_ERROR}},
        // Service connection errors.
        {TELEPHONY_ERR_DESCRIPTOR_MISMATCH, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_WRITE_DESCRIPTOR_TOKEN_FAIL,
         {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_WRITE_DATA_FAIL, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_WRITE_REPLY_FAIL, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_READ_DATA_FAIL, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_IPC_CONNECT_STUB_FAIL, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_REGISTER_CALLBACK_FAIL,
         {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_CALLBACK_ALREADY_REGISTERED,
         {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_UNINIT, {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        {TELEPHONY_ERR_UNREGISTER_CALLBACK_FAIL,
         {CJ_ERROR_TELEPHONY_SERVICE_ERROR, CJ_ERROR_TELEPHONY_SERVICE_ERROR}},
        // System errors.
        {TELEPHONY_ERR_VCARD_FILE_INVALID, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_MEMCPY_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_MEMSET_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_STRCPY_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_LOCAL_PTR_NULL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_SUBSCRIBE_BROADCAST_FAIL,
         {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_PUBLISH_BROADCAST_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_ADD_DEATH_RECIPIENT_FAIL,
         {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_STRTOINT_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_RIL_CMD_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_DATABASE_WRITE_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_DATABASE_READ_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_UNKNOWN_NETWORK_TYPE, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {ERROR_SERVICE_UNAVAILABLE, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {ERROR_NATIVE_API_EXECUTE_FAIL, {CJ_ERROR_TELEPHONY_SYSTEM_ERROR, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        // Other errors shared by both entry types.
        {TELEPHONY_ERR_NO_SIM_CARD, {CJ_ERROR_TELEPHONY_NO_SIM_CARD, CJ_ERROR_TELEPHONY_NO_SIM_CARD}},
        {TELEPHONY_ERR_AIRPLANE_MODE_ON,
         {CJ_ERROR_TELEPHONY_AIRPLANE_MODE_ON, CJ_ERROR_TELEPHONY_AIRPLANE_MODE_ON}},
        {TELEPHONY_ERR_NETWORK_NOT_IN_SERVICE,
         {CJ_ERROR_TELEPHONY_NETWORK_NOT_IN_SERVICE, CJ_ERROR_TELEPHONY_NETWORK_NOT_IN_SERVICE}},
        {TELEPHONY_ERR_PERMISSION_ERR,
         {CJ_ERROR_TELEPHONY_PERMISSION_DENIED, CJ_ERROR_TELEPHONY_PERMISSION_DENIED}},
        // Intentional differences: {legacy result, ROM API result}.
        {TELEPHONY_ERR_ILLEGAL_USE_OF_SYSTEM_API,
         {CJ_ERROR_TELEPHONY_PERMISSION_DENIED, CJ_ERROR_ILLEGAL_USE_OF_SYSTEM_API}},
        {CORE_SERVICE_SEND_CALLBACK_FAILED, {CORE_SERVICE_SEND_CALLBACK_FAILED, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {CORE_SERVICE_RADIO_PROTOCOL_TECH_UNKNOWN,
         {CORE_SERVICE_RADIO_PROTOCOL_TECH_UNKNOWN, CJ_ERROR_TELEPHONY_SYSTEM_ERROR}},
        {TELEPHONY_ERR_POLICY_DISABLED, {TELEPHONY_ERR_POLICY_DISABLED, CJ_ERROR_TELEPHONY_POLICY_DISABLED}},
    };
} // namespace

    // isRomApi=false preserves the compatibility package's legacy error codes.
    // isRomApi=true uses N-API aligned error codes for the new system ROM APIs.
    // Keep false as the default so existing callers retain their original behavior.
    static int32_t ConvertCJErrCode(int32_t errCode, bool isRomApi = false)
    {
        const auto it = RADIO_ERROR_CODE_MAP.find(errCode);
        if (it != RADIO_ERROR_CODE_MAP.end()) {
            return isRomApi ? it->second.romApiCode : it->second.legacyCode;
        }
        // Unmapped errors stay unchanged for legacy callers and become unknown errors for ROM APIs.
        return isRomApi ? CJ_ERROR_TELEPHONY_UNKNOW_ERROR : errCode;
    }

    static inline bool IsValidSlotId(int32_t slotId)
    {
        return ((slotId >= DEFAULT_SIM_SLOT_ID) && (slotId < SIM_SLOT_COUNT));
    }

    static inline bool IsValidSlotIdEx(int32_t slotId)
    {
        // One more slot for VSim.
        return ((slotId >= DEFAULT_SIM_SLOT_ID) && (slotId < SIM_SLOT_COUNT + 1));
    }

    static int32_t WrapRadioTech(int32_t radioTechType)
    {
        RadioTech techType = static_cast<RadioTech>(radioTechType);
        switch (techType) {
            case RadioTech::RADIO_TECHNOLOGY_GSM:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_GSM);
            case RadioTech::RADIO_TECHNOLOGY_LTE:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_LTE);
            case RadioTech::RADIO_TECHNOLOGY_WCDMA:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_WCDMA);
            case RadioTech::RADIO_TECHNOLOGY_1XRTT:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_1XRTT);
            case RadioTech::RADIO_TECHNOLOGY_HSPA:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_HSPA);
            case RadioTech::RADIO_TECHNOLOGY_HSPAP:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_HSPAP);
            case RadioTech::RADIO_TECHNOLOGY_TD_SCDMA:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_TD_SCDMA);
            case RadioTech::RADIO_TECHNOLOGY_EVDO:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_EVDO);
            case RadioTech::RADIO_TECHNOLOGY_EHRPD:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_EHRPD);
            case RadioTech::RADIO_TECHNOLOGY_LTE_CA:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_LTE_CA);
            case RadioTech::RADIO_TECHNOLOGY_IWLAN:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_IWLAN);
            case RadioTech::RADIO_TECHNOLOGY_NR:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_NR);
            default:
                return static_cast<int32_t>(RatType::RADIO_TECHNOLOGY_UNKNOWN);
        }
    }

    static int32_t WrapRegState(int32_t nativeState)
    {
        RegServiceState state = static_cast<RegServiceState>(nativeState);
        switch (state) {
            case RegServiceState::REG_STATE_IN_SERVICE: {
                return RegStatus::REGISTRATION_STATE_IN_SERVICE;
            }
            case RegServiceState::REG_STATE_EMERGENCY_ONLY: {
                return RegStatus::REGISTRATION_STATE_EMERGENCY_CALL_ONLY;
            }
            case RegServiceState::REG_STATE_POWER_OFF: {
                return RegStatus::REGISTRATION_STATE_POWER_OFF;
            }
            default:
                return RegStatus::REGISTRATION_STATE_NO_SERVICE;
        }
    }

    static int32_t WrapSignalInformationType(SignalInformation::NetworkType type)
    {
        switch (type) {
            case SignalInformation::NetworkType::GSM:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_GSM);
            case SignalInformation::NetworkType::CDMA:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_CDMA);
            case SignalInformation::NetworkType::LTE:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_LTE);
            case SignalInformation::NetworkType::WCDMA:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_WCDMA);
            case SignalInformation::NetworkType::TDSCDMA:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_TDSCDMA);
            case SignalInformation::NetworkType::NR:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_NR);
            default:
                return static_cast<int32_t>(NetworkType::NETWORK_TYPE_UNKNOWN);
        }
    }

    static std::string ToUtf8(std::u16string str16)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        // std::u16string -> std::string
        std::string result = convert.to_bytes(str16);
        return result;
    }

    CNetworkRadioTech TelephonyRadioImpl::GetRadioTech(int32_t slotId, int32_t &errCode)
    {
        int32_t psRadioTech = static_cast<int32_t>(RadioTech::RADIO_TECHNOLOGY_INVALID);
        int32_t csRadioTech = static_cast<int32_t>(RadioTech::RADIO_TECHNOLOGY_INVALID);
        CNetworkRadioTech networkRadioTech = {
            .psRadioTech = WrapRadioTech(psRadioTech),
            .csRadioTech = WrapRadioTech(csRadioTech)
        };
        if (!IsValidSlotId(slotId)) {
            TELEPHONY_LOGE("TelephonyRadioImpl::GetRadioTech slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID);
            return networkRadioTech;
        }
        int32_t psResult =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetPsRadioTech(slotId, psRadioTech);
        int32_t csResult =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetCsRadioTech(slotId, csRadioTech);
        if (psResult == TELEPHONY_SUCCESS && csResult == TELEPHONY_SUCCESS) {
            networkRadioTech.csRadioTech = WrapRadioTech(csRadioTech);
            networkRadioTech.psRadioTech = WrapRadioTech(psRadioTech);
        }
        errCode = ConvertCJErrCode(csResult);
        return networkRadioTech;
    }

    CNetworkState TelephonyRadioImpl::GetNetworkState(int32_t slotId, int32_t &errCode)
    {
        return GetNetworkState(slotId, errCode, false);
    }

    CNetworkState TelephonyRadioImpl::GetNetworkState(int32_t slotId, int32_t &errCode, bool isRomApi)
    {
        CNetworkState cnetworkState = {
            .longOperatorName = nullptr,
            .shortOperatorName = nullptr,
            .plmnNumeric = nullptr,
            .isRoaming = false,
            .nsaState = static_cast<int32_t>(NsaState::NSA_STATE_NOT_SUPPORT),
            .isCaActive = false
        };
        if (!IsValidSlotIdEx(slotId)) {
            TELEPHONY_LOGE("NativeGetNetworkState slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID, isRomApi);
            return cnetworkState;
        }
        sptr<NetworkState> networkState = nullptr;
        int32_t result = DelayedRefSingleton<CoreServiceClient>::GetInstance().GetNetworkState(slotId, networkState);
        errCode = ConvertCJErrCode(result, isRomApi);
        if (errCode != TELEPHONY_SUCCESS) {
            TELEPHONY_LOGE("NativeGetNetworkState errorCode = %{public}d", result);
            return cnetworkState;
        }
        if (networkState == nullptr) {
            TELEPHONY_LOGE("NativeGetNetworkState networkState is nullptr");
            errCode = ConvertCJErrCode(ERROR_NATIVE_API_EXECUTE_FAIL, isRomApi);
            return cnetworkState;
        }
        cnetworkState.longOperatorName = MallocCString(networkState->GetLongOperatorName());
        cnetworkState.shortOperatorName = MallocCString(networkState->GetShortOperatorName());
        cnetworkState.plmnNumeric = MallocCString(networkState->GetPlmnNumeric());
        cnetworkState.isRoaming = networkState->IsRoaming();
        cnetworkState.isEmergency = networkState->IsEmergency();
        cnetworkState.regState = WrapRegState(static_cast<int32_t>(networkState->GetRegStatus()));
        cnetworkState.cfgTech = WrapRadioTech(static_cast<int32_t>(networkState->GetCfgTech()));
        return cnetworkState;
    }

    int32_t TelephonyRadioImpl::GetNetworkSelectionMode(int32_t slotId, int32_t &errCode)
    {
        return GetNetworkSelectionMode(slotId, errCode, false);
    }

    int32_t TelephonyRadioImpl::GetNetworkSelectionMode(int32_t slotId, int32_t &errCode, bool isRomApi)
    {
        int32_t selectMode = NETWORK_SELECTION_UNKNOWN;
        if (!IsValidSlotId(slotId)) {
            TELEPHONY_LOGE("NativeGetNetworkSelectionMode slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID, isRomApi);
            return selectMode;
        }
        auto asyncContext = std::make_shared<GetSelectModeContext>();
        asyncContext->slotId = slotId;
        std::unique_ptr<GetNetworkSearchModeCallback> callback =
            std::make_unique<GetNetworkSearchModeCallback>(asyncContext);
        std::unique_lock<std::mutex> callbackLock(asyncContext->callbackMutex);
        asyncContext->errorCode = DelayedRefSingleton<CoreServiceClient>::GetInstance().GetNetworkSelectionMode(
            asyncContext->slotId, callback.release());
        if (asyncContext->errorCode == TELEPHONY_SUCCESS) {
            asyncContext->cv.wait_for(
                callbackLock,
                std::chrono::seconds(WAIT_TIME_SECOND),
                [asyncContext] { return asyncContext->callbackEnd; });
            TELEPHONY_LOGI("NativeGetNetworkSelectionMode after callback end");
        }
        // Preserve the legacy FFI timeout result; only the new Cangjie entry reports an incomplete query as failure.
        if (isRomApi && !asyncContext->resolved && asyncContext->errorCode == TELEPHONY_SUCCESS) {
            TELEPHONY_LOGE("NativeGetNetworkSelectionMode did not complete successfully");
            asyncContext->errorCode = TELEPHONY_ERR_FAIL;
        }
        errCode = ConvertCJErrCode(asyncContext->errorCode, isRomApi);
        selectMode = asyncContext->selectMode;
        return selectMode;
    }

    char* TelephonyRadioImpl::GetISOCountryCodeForNetwork(int32_t slotId, int32_t &errCode)
    {
        if (!IsValidSlotId(slotId)) {
            TELEPHONY_LOGE("NativeGetCountryCode slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID);
            return nullptr;
        }
        std::u16string countryCode;
        errCode =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetIsoCountryCodeForNetwork(slotId, countryCode);
        std::string code = ToUtf8(countryCode);
        char* result = MallocCString(code);
        errCode = ConvertCJErrCode(errCode);
        return result;
    }

    int32_t TelephonyRadioImpl::GetPrimarySlotId(int32_t &errCode)
    {
        return GetPrimarySlotId(errCode, false);
    }

    int32_t TelephonyRadioImpl::GetPrimarySlotId(int32_t &errCode, bool isRomApi)
    {
        int32_t slotId = DEFAULT_SIM_SLOT_ID;
        errCode =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetPrimarySlotId(slotId);
        errCode = ConvertCJErrCode(errCode, isRomApi);
        return slotId;
    }

    CNetworkRadioTech TelephonyRadioImpl::GetRadioTechSync(int32_t slotId)
    {
        int32_t psRadioTech = static_cast<int32_t>(RadioTech::RADIO_TECHNOLOGY_INVALID);
        int32_t csRadioTech = static_cast<int32_t>(RadioTech::RADIO_TECHNOLOGY_INVALID);
        CNetworkRadioTech networkRadioTech = {
            .psRadioTech = WrapRadioTech(psRadioTech),
            .csRadioTech = WrapRadioTech(csRadioTech)
        };
        if (IsValidSlotIdEx(slotId)) {
            int32_t psResult =
                DelayedRefSingleton<CoreServiceClient>::GetInstance().GetPsRadioTech(slotId, psRadioTech);
            int32_t csResult =
                DelayedRefSingleton<CoreServiceClient>::GetInstance().GetCsRadioTech(slotId, csRadioTech);
            if (psResult == TELEPHONY_SUCCESS && csResult == TELEPHONY_SUCCESS) {
                networkRadioTech.psRadioTech = WrapRadioTech(psRadioTech);
                networkRadioTech.csRadioTech = WrapRadioTech(csRadioTech);
            }
        }
        return networkRadioTech;
    }

    char* TelephonyRadioImpl::GetISOCountryCodeForNetworkSync(int32_t slotId)
    {
        std::u16string countryCode;
        if (IsValidSlotIdEx(slotId)) {
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetIsoCountryCodeForNetwork(slotId, countryCode);
        }
        return MallocCString(ToUtf8(countryCode));
    }

    CArraySignalInformation TelephonyRadioImpl::GetSignalInfoList(int32_t slotId, int32_t &errCode)
    {
        std::vector<sptr<SignalInformation>> signalInfoList;
        CArraySignalInformation csignalInfoList = {
            .head = nullptr,
            .size = 0
        };
        if (!IsValidSlotIdEx(slotId)) {
            TELEPHONY_LOGE("NativeGetSignalInfoList slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID);
            return csignalInfoList;
        }
        errCode =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetSignalInfoList(slotId, signalInfoList);
        errCode = ConvertCJErrCode(errCode);
        size_t infoSize = signalInfoList.size();
        TELEPHONY_LOGD("NativeGetSignalInfoList size = %{public}zu", signalInfoList.size());
        CSignalInformation* head = nullptr;
        if (infoSize > 0) {
            head = reinterpret_cast<CSignalInformation *>(malloc(sizeof(CSignalInformation) * infoSize));
        }
        if (head == nullptr && infoSize > 0) {
            TELEPHONY_LOGE("NativeGetSignalInfoList malloc failed!");
            return csignalInfoList;
        }
        int i = 0;
        for (const sptr<SignalInformation>& infoItem : signalInfoList) {
            int32_t signalType = static_cast<int32_t>(NetworkType::NETWORK_TYPE_UNKNOWN);
            int32_t signalLevel = 0;
            int32_t signalIntensity = 0;
            if (infoItem != nullptr) {
                signalType = WrapSignalInformationType(infoItem->GetNetworkType());
                signalLevel = infoItem->GetSignalLevel();
                signalIntensity = infoItem->GetSignalIntensity();
            }
            head[i].signalType = signalType;
            head[i].signalLevel = signalLevel;
            head[i].dBm = signalIntensity;
            i++;
        }
        csignalInfoList.size = static_cast<int64_t>(infoSize);
        csignalInfoList.head = head;
        return csignalInfoList;
    }

    bool TelephonyRadioImpl::IsNRSupported()
    {
        bool isNrSupported = false;
        TelephonyConfig telephonyConfig;
        isNrSupported =
            telephonyConfig.IsCapabilitySupport(
                static_cast<int32_t>(TelephonyConfig::ConfigType::MODEM_CAP_SUPPORT_NR));
#ifdef OHOS_BUILD_ENABLE_TELEPHONY_EXT
        TELEPHONY_EXT_UTILS_WRAPPER.InitTelephonyExtUtilsWrapper();
        if (TELEPHONY_EXT_UTILS_WRAPPER.isChipsetNrSupported_ != nullptr) {
            isNrSupported = isNrSupported && TELEPHONY_EXT_UTILS_WRAPPER.isChipsetNrSupported_();
        }
#endif
        return isNrSupported;
    }

    bool TelephonyRadioImpl::IsRadioOn(int32_t slotId, int32_t &errCode)
    {
        bool result = false;
        if (!IsValidSlotId(slotId)) {
            TELEPHONY_LOGE("NativeIsRadioOn slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID);
            return result;
        }
        auto asyncContext = std::make_shared<IsRadioOnContext>();
        asyncContext->slotId = slotId;
        std::unique_ptr<GetRadioStateCallback> callback = std::make_unique<GetRadioStateCallback>(asyncContext);
        std::unique_lock<std::mutex> callbackLock(asyncContext->callbackMutex);
        asyncContext->errorCode =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetRadioState(slotId, callback.release());
        if (asyncContext->errorCode == TELEPHONY_SUCCESS) {
            asyncContext->cv.wait_for(
                callbackLock,
                std::chrono::seconds(WAIT_TIME_SECOND),
                [asyncContext] { return asyncContext->callbackEnd; });
            TELEPHONY_LOGI("NativeIsRadioOn after callback end");
        }
        errCode = ConvertCJErrCode(asyncContext->errorCode);
        result = asyncContext->isRadioOn;
        return result;
    }

    char* TelephonyRadioImpl::GetOperatorName(int32_t slotId, int32_t &errCode)
    {
        if (!IsValidSlotId(slotId)) {
            TELEPHONY_LOGE("NativeGetOperatorName slotId is invalid");
            errCode = ConvertCJErrCode(ERROR_SLOT_ID_INVALID);
            return nullptr;
        }
        std::u16string u16OperatorName = u"";
        errCode =
            DelayedRefSingleton<CoreServiceClient>::GetInstance().GetOperatorName(slotId, u16OperatorName);
        errCode = ConvertCJErrCode(errCode);
        std::string operatorName = ToUtf8(u16OperatorName);
        if (operatorName.size() > BUF_SIZE) {
            operatorName = operatorName.substr(0, BUF_SIZE);
        }
        char* result = MallocCString(operatorName);
        return result;
    }
}
}
