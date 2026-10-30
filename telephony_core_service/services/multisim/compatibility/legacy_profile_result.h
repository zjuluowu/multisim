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
// PROPOSED: C的原双结果/out参数投影；没有业务执行、等待或状态写权。
#ifndef OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_PROFILE_RESULT_H
#define OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_PROFILE_RESULT_H

#include "application/card_access/local_card_requests.h"

namespace OHOS::Telephony::MultiSim {
inline int32_t AssignLocalProfileLegacyResult(const EuiccPrimitiveResult &result, int32_t &enumResult)
{
    if (result.cardEnumResult) { enumResult = *result.cardEnumResult; }
    return result.outerLegacyResult;
}
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_COMPATIBILITY_LEGACY_PROFILE_RESULT_H
