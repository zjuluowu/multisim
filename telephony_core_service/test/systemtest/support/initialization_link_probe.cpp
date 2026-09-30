// Dependency/link audit only: never execute without a complete controlled topology.
#include "sim_manager.h"
int main()
{
    auto production = std::make_shared<OHOS::Telephony::SimManager>(nullptr);
    OHOS::Telephony::ISimManager &component = *production;
    if (!component.OnInit(1)) return 1;
    OHOS::Telephony::SimState state = OHOS::Telephony::SimState::SIM_STATE_UNKNOWN;
    return component.GetSimState(0, state);
}
