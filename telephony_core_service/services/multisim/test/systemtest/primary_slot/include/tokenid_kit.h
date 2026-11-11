#pragma once
#include <cstdint>
namespace OHOS::Security::AccessToken {
class TokenIdKit {
public:
    static bool IsSystemAppByFullTokenID(uint64_t token);
};
}
