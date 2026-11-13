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

#ifndef STATE_REGISTRY_ERRORS_H
#define STATE_REGISTRY_ERRORS_H

#include <errors.h>

namespace OHOS {
namespace Telephony {
constexpr int64_t TELEPHONY_DATA_STORAGE = 0x10;
// error codes
constexpr int OPERATION_ERROR = -1;
constexpr int OPERATION_OK = 0;
constexpr int RDB_OBJECT_EMPTY = -2;
constexpr int PARAMETER_EMPTY = -3;

// Error code for telephony data storage
constexpr ErrCode CORE_DATA_STORAGE_ERR_OFFSET = ErrCodeOffset(SUBSYS_TELEPONY, TELEPHONY_DATA_STORAGE);
const int VERSION_NO_CHANGE = -2;
const int DATA_STORAGE_ERROR = -1;
const int DATA_STORAGE_ERR_PERMISSION_ERR = -3;
const int FILE_HASH_NO_CHANGE = -4;
const int DATA_STORAGE_SUCCESS = 0;
enum class LoadProFileErrorType {
    OPEN_FILE_ERROR = CORE_DATA_STORAGE_ERR_OFFSET,
    CLOSE_FILE_ERROR,
    LOAD_FILE_ERROR,
    FILE_PARSER_ERROR,
    ITEM_SIZE_IS_NULL,
    SAVE_APN_VERSION_FAIL,
    TEL_PROFILE_UTIL_IS_NULL,
    PDP_PROFILE_VERSION_IS_NULL,
    PDP_PROFILE_VERSION_IS_OLD,
    OPERATOR_IS_NULL,
    RESET_APN_FAIL,
    PREFER_APN_FAIL,
    DELETE_THIRTY_DATA_FAIL,
    SET_CARD_FAIL,
    REALPATH_FAIL
};
} // namespace Telephony
} // namespace OHOS
#endif // STATE_REGISTRY_ERRORS_H