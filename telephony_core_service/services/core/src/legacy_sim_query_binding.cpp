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

#include "legacy_sim_query_binding.h"
#include "i_sim_manager.h"

namespace OHOS::Telephony {
namespace {
template<class T>
bool SameObjectAndControlBlock(const std::shared_ptr<T> &original, const std::shared_ptr<T> &candidate)
{
    return original && original.get() == candidate.get() &&
        !original.owner_before(candidate) && !candidate.owner_before(original);
}
} // namespace

LegacySimQueryBinding::LegacySimQueryBinding(const std::shared_ptr<ISimManager> &manager,
    const std::shared_ptr<ITelRilManager> &sharedRil)
    : manager_(manager), sharedRil_(sharedRil), query_(*this)
{}

std::shared_ptr<LegacySimQueryBinding> LegacySimQueryBinding::Create(const std::shared_ptr<ISimManager> &manager,
    const std::shared_ptr<ITelRilManager> &sharedRil)
{
    if (!manager || !sharedRil) {
        return nullptr;
    }
    return std::shared_ptr<LegacySimQueryBinding>(new LegacySimQueryBinding(manager, sharedRil));
}

bool LegacySimQueryBinding::MatchesManager(const std::shared_ptr<ISimManager> &manager) const
{
    return SameObjectAndControlBlock(manager_, manager);
}

bool LegacySimQueryBinding::MatchesSharedRil(const std::shared_ptr<ITelRilManager> &sharedRil) const
{
    return SameObjectAndControlBlock(sharedRil_, sharedRil);
}

int32_t LegacySimQueryBinding::Read()
{
    return query_.Read();
}

int32_t LegacySimQueryBinding::ReadLegacyRealSimCount()
{
    return manager_->GetRealSimCount();
}
} // namespace OHOS::Telephony
