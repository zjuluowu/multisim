// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/primary_slot.h"
#include "core_service_sim.h"
#include "telephony_errors.h"

namespace SimGuard::Primary {
// Transparent test-side decorator; normal CoreServiceSim assembly and call stay intact.
class LegacyAdapter final : public Adapter {
public:
    static std::string ErrorSymbol(int result)
    {
        using namespace OHOS::Telephony;
        if (result == TELEPHONY_ERR_SUCCESS) return "TELEPHONY_ERR_SUCCESS";
        if (result == TELEPHONY_ERR_LOCAL_PTR_NULL) return "TELEPHONY_ERR_LOCAL_PTR_NULL";
        if (result == TELEPHONY_ERR_NO_SIM_CARD) return "TELEPHONY_ERR_NO_SIM_CARD";
        return "UNMAPPED:" + std::to_string(result);
    }
    explicit LegacyAdapter(Observation &observed) : observed_(observed) {}
    void SetSimManager(const std::shared_ptr<OHOS::Telephony::ISimManager> &manager)
    { core_.SetSimManager(manager); }
    int SetPrimarySlotId(int slot) override
    {
        observed_.Add("api_input", {{"slot", std::to_string(slot)}});
        const int result = core_.SetPrimarySlotId(slot);
        observed_.Add("api_return", {{"result", std::to_string(result)}, {"symbol", ErrorSymbol(result)}});
        return result;
    }
private:
    Observation &observed_;
    OHOS::Telephony::CoreServiceSim core_;
};
} // namespace SimGuard::Primary
