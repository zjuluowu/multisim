/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef DATA_STORAGE_SIM_DATA_H
#define DATA_STORAGE_SIM_DATA_H

namespace OHOS {
namespace Telephony {
class SimData {
public:
    static constexpr const char *SIM_ID = "sim_id";
    static constexpr const char *ICC_ID = "icc_id";
    static constexpr const char *CARD_ID = "card_id";
    static constexpr const char *SLOT_INDEX = "slot_index";
    static constexpr const char *SHOW_NAME = "show_name";
    static constexpr const char *PHONE_NUMBER = "phone_number";
    static constexpr const char *COUNTRY_CODE = "country_code";
    static constexpr const char *LANGUAGE = "language";
    static constexpr const char *IMSI = "imsi";
    static constexpr const char *CARD_TYPE = "card_type";
    static constexpr const char *IMS_SWITCH = "ims_switch";
    static constexpr const char *IS_ACTIVE = "is_active";
    static constexpr const char *IS_MAIN_CARD = "is_main_card";
    static constexpr const char *IS_VOICE_CARD = "is_voice_card";
    static constexpr const char *IS_MESSAGE_CARD = "is_message_card";
    static constexpr const char *IS_CELLULAR_DATA_CARD = "is_cellular_data_card";
};

constexpr const char *TABLE_SIM_INFO = "sim_info";
constexpr const char *SIM_URI = "datashare:///com.ohos.simability";
} // namespace Telephony
} // namespace OHOS
#endif // DATA_STORAGE_SIM_DATA_H