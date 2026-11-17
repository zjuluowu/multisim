#include "bootstrap_probe.h"
#include "sim_manager.h"
namespace SimHost {
BootstrapObservation ObserveBootstrap()
{
    OHOS::Telephony::SimManager production(nullptr);
    OHOS::Telephony::ISimManager &component = production;
    BootstrapObservation result{};
    result.hasCard = true;
    result.hasCardResult = component.HasSimCard(0, result.hasCard);
    auto state = OHOS::Telephony::SimState::SIM_STATE_UNKNOWN;
    result.stateResult = component.GetSimState(0, state);
    result.state = static_cast<int>(state);
    std::vector<OHOS::Telephony::IccAccountInfo> accounts;
    result.accountsResult = component.GetActiveSimAccountInfoList(false, accounts);
    result.accountsCount = accounts.size();
    return result;
}
}
