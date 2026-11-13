// SPDX-License-Identifier: Apache-2.0
#include "platform_spy.h"
#include "core_service_hisysevent.h"
#include "telephony_state_registry_client.h"
#include "core_service_errors.h"
#include "iservice_registry.h"
#include <atomic>
#include <stdexcept>

namespace { std::atomic<SimGuard::PlatformSpy *> active{nullptr}; }
namespace SimGuard {
void BindPlatformSpy(PlatformSpy *spy)
{
    PlatformSpy *expected = nullptr;
    if (spy && !active.compare_exchange_strong(expected, spy)) throw std::logic_error("overlapping platform fixtures");
    if (!spy) active.store(nullptr);
}
State NormalizeState(int value)
{
    using OHOS::Telephony::SimState;
    switch (static_cast<SimState>(value)) {
        case SimState::SIM_STATE_UNKNOWN: return State::Unknown;
        case SimState::SIM_STATE_NOT_PRESENT: return State::Absent;
        case SimState::SIM_STATE_LOCKED: return State::Locked;
        case SimState::SIM_STATE_NOT_READY: return State::NotReady;
        case SimState::SIM_STATE_READY: return State::Ready;
        default: throw std::runtime_error("unmapped public SIM state");
    }
}
CardKind NormalizeKind(int value)
{
    using OHOS::Telephony::CardType;
    switch (static_cast<CardType>(value)) {
        case CardType::UNKNOWN_CARD: return CardKind::Unknown;
        case CardType::SINGLE_MODE_SIM_CARD: return CardKind::Sim;
        case CardType::SINGLE_MODE_USIM_CARD: return CardKind::Usim;
        default: throw std::runtime_error("unmapped public card type");
    }
}
}
// External service absent is explicit; no SA/IPC integration is claimed.
namespace OHOS {
SystemAbilityManagerClient &SystemAbilityManagerClient::GetInstance()
{ static SystemAbilityManagerClient client; return client; }
sptr<ISystemAbilityManager> SystemAbilityManagerClient::GetSystemAbilityManager() { return nullptr; }
sptr<IRemoteObject> ISystemAbilityManager::GetSystemAbility(int32_t) { return nullptr; }
}
namespace OHOS::Telephony {
TelephonyStateRegistryClient::TelephonyStateRegistryClient() = default;
TelephonyStateRegistryClient::~TelephonyStateRegistryClient() = default;
int32_t TelephonyStateRegistryClient::UpdateSimState(int32_t slot, CardType kind, SimState state, LockReason reason)
{
    auto *spy = active.load();
    if (!spy) return TELEPHONY_ERR_IPC_CONNECT_STUB_FAIL;
    SimGuard::Lock lockReason = SimGuard::Lock::None;
    if (reason == LockReason::SIM_PIN) lockReason = SimGuard::Lock::Pin;
    else if (reason == LockReason::SIM_PUK) lockReason = SimGuard::Lock::Puk;
    else if (reason != LockReason::SIM_NONE) throw std::runtime_error("unmapped public lock reason");
    std::lock_guard<std::mutex> guard(spy->mutex);
    spy->notifications.push_back({slot, SimGuard::NormalizeState(static_cast<int>(state)),
        SimGuard::NormalizeKind(static_cast<int>(kind)), lockReason});
    return TELEPHONY_ERR_SUCCESS;
}
void CoreServiceHiSysEvent::WriteSimStateBehaviorEvent(int32_t, int32_t) {}
}
