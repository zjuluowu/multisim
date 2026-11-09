/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef OHOS_ABILITY_RUNTIME_OS_ACCOUNT_MANAGER_WRAPPER_H
#define OHOS_ABILITY_RUNTIME_OS_ACCOUNT_MANAGER_WRAPPER_H

#include <string>
#include <vector>

#include "errors.h"
#include "singleton.h"

namespace OHOS {
namespace AppExecFwk {
class OsAccountManagerWrapper {
public:
    OsAccountManagerWrapper() = default;
    virtual ~OsAccountManagerWrapper() = default;

    /**
     * @brief Gets the local IDs of all activated OS accounts.
     *
     * @param ids The local IDs of all activated OS accounts.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode QueryActiveOsAccountIds(std::vector<int32_t>& ids);

    /**
     * @brief Gets the local ID of an OS account from the process UID
     *
     * @param uid The process UID.
     * @param id The local ID of the OS account associated with the specified UID.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode GetOsAccountLocalIdFromUid(const int32_t uid, int32_t &id);

    /**
     * @brief Gets the local ID of the current OS account.
     *
     * @param id The local ID of the current OS account.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode GetOsAccountLocalIdFromProcess(int &id);

    /**
     * @brief Checks whether the specified OS account exists.
     *
     * @param id The local ID of the OS account.
     * @param isOsAccountExists Indicates whether the specified OS account exists.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode IsOsAccountExists(const int id, bool &isOsAccountExists);

    /**
     * @brief Gets the local IDs of all created OS accounts.
     *
     * @param ids The local IDs of all created OS accounts.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode QueryAllCreatedOsAccounts(std::vector<int32_t> &ids);

    /**
     * @brief Creates an OS account using the local name and account type.
     *
     * @param name The name of the OS account to create.
     * @param osAccountUserId The local id of the created OS account.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode CreateOsAccount(const std::string &name, int32_t &osAccountUserId);

    /**
     * @brief Removes an OS account based on its local ID.
     *
     * @param id The local ID of the OS account.
     * @return error code, ERR_OK on success, others on failure.
     */
    static ErrCode RemoveOsAccount(const int id);

    /**
     * @brief Get the current active user ID.
     *
     * @return int32_t user ID
     */
    static int32_t GetCurrentActiveAccountId();
};
} // namespace AppExecFwk
} // namespace OHOS
#endif // OHOS_ABILITY_RUNTIME_OS_ACCOUNT_MANAGER_WRAPPER_H