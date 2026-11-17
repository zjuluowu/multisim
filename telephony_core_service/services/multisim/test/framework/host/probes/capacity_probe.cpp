#include "capacity_probe.h"
#include "sim_manager.h"
namespace SimHost {
int ObserveCapacity()
{
    // Public constructor only. This read-only query does not require OnInit in this version.
    OHOS::Telephony::SimManager production(nullptr);
    OHOS::Telephony::ISimManager &component = production;
    return component.GetMaxSimCount();
}
}
