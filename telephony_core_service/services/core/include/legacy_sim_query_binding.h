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

#ifndef OHOS_TELEPHONY_CORE_LEGACY_SIM_QUERY_BINDING_H
#define OHOS_TELEPHONY_CORE_LEGACY_SIM_QUERY_BINDING_H

#include <memory>
#include "application/query/real_sim_count_query.h"

namespace OHOS::Telephony {
class ISimManager;
class ITelRilManager;

// Core owns this private attachment. It borrows the original Manager and shared
// RIL control blocks; it never creates, initializes or stops either object.
class LegacySimQueryBinding final : public MultiSim::RealSimCountHostPort {
public:
    LegacySimQueryBinding(const LegacySimQueryBinding &) = delete;
    LegacySimQueryBinding &operator=(const LegacySimQueryBinding &) = delete;
    LegacySimQueryBinding(LegacySimQueryBinding &&) = delete;
    LegacySimQueryBinding &operator=(LegacySimQueryBinding &&) = delete;
    static std::shared_ptr<LegacySimQueryBinding> Create(const std::shared_ptr<ISimManager> &manager,
        const std::shared_ptr<ITelRilManager> &sharedRil);
    bool MatchesManager(const std::shared_ptr<ISimManager> &manager) const;
    bool MatchesSharedRil(const std::shared_ptr<ITelRilManager> &sharedRil) const;
    const std::shared_ptr<ISimManager> &Manager() const { return manager_; }
    const std::shared_ptr<ITelRilManager> &SharedRil() const { return sharedRil_; }
    int32_t Read();
    int32_t ReadLegacyRealSimCount() override;

private:
    LegacySimQueryBinding(const std::shared_ptr<ISimManager> &manager,
        const std::shared_ptr<ITelRilManager> &sharedRil);
    std::shared_ptr<ISimManager> manager_;
    std::shared_ptr<ITelRilManager> sharedRil_;
    MultiSim::RealSimCountQuery query_;
};
} // namespace OHOS::Telephony

#endif // OHOS_TELEPHONY_CORE_LEGACY_SIM_QUERY_BINDING_H
