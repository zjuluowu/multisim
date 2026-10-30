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

#ifndef OHOS_TELEPHONY_MULTISIM_APPLICATION_QUERY_REAL_SIM_COUNT_QUERY_H
#define OHOS_TELEPHONY_MULTISIM_APPLICATION_QUERY_REAL_SIM_COUNT_QUERY_H

#include <cstdint>

namespace OHOS::Telephony::MultiSim {
// G2a read-only seam. The host preserves the existing Manager read source and
// result; the application neither calculates capacity nor introduces a cache.
class RealSimCountHostPort {
public:
    virtual ~RealSimCountHostPort() = default;
    virtual int32_t ReadLegacyRealSimCount() = 0;
};

class RealSimCountQuery final {
public:
    explicit RealSimCountQuery(RealSimCountHostPort &host) : host_(host) {}
    int32_t Read();

private:
    RealSimCountHostPort &host_;
};
} // namespace OHOS::Telephony::MultiSim

#endif // OHOS_TELEPHONY_MULTISIM_APPLICATION_QUERY_REAL_SIM_COUNT_QUERY_H
