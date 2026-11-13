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

// SIM-OC-003/007/010/013/015: current public declarations, not a target ELF ABI test.
// Platform headers are compile-only host declarations. No SIM/device behavior is simulated.
#include <memory>
#include <string>
#include <type_traits>
#include "radio_event.h"
#include "i_icc_file.h"
#include "sim_state_type.h"
#include "telephony_ext_wrapper.h"

using namespace OHOS::Telephony;

static_assert(RADIO_SIM_STATE_CHANGE == 515);
static_assert(RADIO_SIM_STATE_READY == 516);
static_assert(RADIO_SIM_STATE_LOCKED == 517);
static_assert(RADIO_SIM_STATE_SIMLOCK == 518);
static_assert(RADIO_IMSI_LOADED_READY == 519);
static_assert(RADIO_SIM_RECORDS_LOADED == 520);
static_assert(RADIO_SIM_ACCOUNT_LOADED == 521);
static_assert(RADIO_SIM_OPKEY_LOADED == 528);
static_assert(RADIO_ESIM_SWITCH_CIRCUIT_BEGIN == 1100);
static_assert(RADIO_ESIM_SWITCH_CIRCUIT_END == 1101);
static_assert(RADIO_ESIM_SWITCH_END == 1120);
static_assert(RADIO_ESIM_ENABLING_PROFLIE_START == 1121);
static_assert(RADIO_ESIM_ENABLING_PROFLIE_END == 1122);
static_assert(RADIO_ESIM_DISABLING_PROFLIE_START == 1123);
static_assert(RADIO_ESIM_DISABLING_PROFLIE_END == 1124);
static_assert(RADIO_ESIM_DOWNLOADING_PROFLIE_START == 1125);
static_assert(RADIO_ESIM_DOWNLOADING_PROFLIE_END == 1126);
static_assert(RADIO_ESIM_RESET_MEMORY_START == 1127);
static_assert(RADIO_ESIM_RESET_MEMORY_END == 1128);
static_assert(RADIO_ESIM_ENABLED_PROFILE_NUM == 1129);
static_assert(RADIO_ESIM_END == 1140);

using FileChange = IIccFileExt::FileChangeType;
static_assert(FileChange::INVALID_FILE_OPE == 0);
static_assert(FileChange::ICCID_FILE_LOAD == 1);
static_assert(FileChange::GID1_FILE_LOAD == 2);
static_assert(FileChange::GID2_FILE_LOAD == 3);
static_assert(FileChange::SPN_FILE_LOAD == 4);
static_assert(FileChange::C_IMSI_FILE_LOAD == 5);
static_assert(FileChange::G_IMSI_FILE_LOAD == 6);
static_assert(FileChange::G_MCCMNC_FILE_LOAD == 7);
static_assert(FileChange::ALL_FILE_LOAD == 8);
static_assert(static_cast<int>(PinOperationType::SIM_ABSENT) == 0);
static_assert(static_cast<int>(PinOperationType::PIN_ENABLE) == 1);
static_assert(static_cast<int>(PinOperationType::PIN_DISABLE) == 2);
static_assert(static_cast<int>(PinOperationType::PIN_ALTER) == 3);
static_assert(static_cast<int>(SimSlotType::INVALID_SLOT_ID) == -1);
static_assert(static_cast<int>(SimSlotType::VSIM_SLOT_ID) == 2);

using PinMemoryHook = void (*)(int32_t, const std::string &, PinOperationType, const std::string &);
using FileFactoryHook = void (*)(int32_t, std::shared_ptr<IIccFileExt>);
using SwitchObserverHook = void (*)(int32_t, const std::shared_ptr<OHOS::AppExecFwk::EventHandler> &, int32_t);
static_assert(std::is_same_v<TelephonyExtWrapper::CacheAssetPinForUpgrade, PinMemoryHook>);
static_assert(std::is_same_v<TelephonyExtWrapper::CREATE_ICC_FILE_EXT, FileFactoryHook>);
static_assert(std::is_same_v<TelephonyExtWrapper::RegisterEsimSwitchNotify, SwitchObserverHook>);
static_assert(std::is_same_v<TelephonyExtWrapper::GetResetActiveFlagFunc, bool (*)(int32_t, bool &)>);
static_assert(std::is_same_v<TelephonyExtWrapper::GetRealSimCountExtFunc, int32_t (*)(int32_t)>);
static_assert(std::is_same_v<TelephonyExtWrapper::SwapM0M2SimCardsFunc, int32_t (*)(int32_t)>);
static_assert(std::is_same_v<TelephonyExtWrapper::GetSimLabelIndexFromLsiCfgFunc, bool (*)(int32_t, int32_t &)>);
static_assert(std::is_same_v<TelephonyExtWrapper::SetActiveSim, void (*)(int32_t, int32_t)>);
static_assert(std::is_same_v<decltype(&IIccFileExt::SetIccFile),
    void (IIccFileExt::*)(std::shared_ptr<IIccFileExt> &)>);
static_assert(std::is_same_v<decltype(&IIccFileExt::OnOpkeyLoad),
    void (IIccFileExt::*)(const std::string, const std::string)>);
static_assert(std::is_same_v<decltype(&IIccFileExt::ExecutOriginalSimIoRequest),
    bool (IIccFileExt::*)(int32_t, int)>);
