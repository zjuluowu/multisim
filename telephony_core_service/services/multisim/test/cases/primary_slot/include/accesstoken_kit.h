// Host declaration subset for the external AccessToken service, not SIM policy.
#pragma once
#include <cstdint>
#include <string>
namespace OHOS::Security::AccessToken {
using AccessTokenID = uint32_t;
enum ATokenTypeEnum { TOKEN_INVALID = -1, TOKEN_HAP = 0, TOKEN_NATIVE, TOKEN_SHELL, TOKEN_TYPE_BUTT };
enum PermissionState { PERMISSION_DENIED = -1, PERMISSION_GRANTED = 0 };
class AccessTokenKit {
public:
    static ATokenTypeEnum GetTokenTypeFlag(AccessTokenID token);
    static int VerifyAccessToken(AccessTokenID token, const std::string &permission);
};
}
