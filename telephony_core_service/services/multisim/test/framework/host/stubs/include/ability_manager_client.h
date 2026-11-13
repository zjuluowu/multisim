// Compile subset of ability_runtime@969de904; no runtime implementation/default success.
#pragma once
#include "compile_declarations.h"
namespace OHOS::AAFwk {
class AbilityManagerClient {
public:
    static std::shared_ptr<AbilityManagerClient> GetInstance();
    int32_t StartExtensionAbility(const Want &, sptr<IRemoteObject>, int32_t = -1,
        AppExecFwk::ExtensionAbilityType = AppExecFwk::ExtensionAbilityType::UNSPECIFIED);
};
}
