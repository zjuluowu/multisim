#include "security_boundary.h"
#include "ipc_skeleton.h"
#include "tokenid_kit.h"
#include "privacy_kit.h"
#include "parameter.h"
#include <stdexcept>
namespace {
PrimarySlotHost::ExternalIdentity *active = nullptr;
PrimarySlotHost::ExternalIdentity &Identity()
{
    if (!active) throw std::logic_error("No external identity scenario installed");
    return *active;
}
}
namespace PrimarySlotHost {
IdentityScope::IdentityScope(ExternalIdentity &identity)
{
    if (active) throw std::logic_error("Overlapping identity scenarios");
    active = &identity;
}
IdentityScope::~IdentityScope() { active = nullptr; }
}
namespace OHOS {
uint32_t IPCSkeleton::GetCallingTokenID() { return 101; }
uint64_t IPCSkeleton::GetCallingFullTokenID() { return 0x100000065ULL; }
}
namespace OHOS::Security::AccessToken {
ATokenTypeEnum AccessTokenKit::GetTokenTypeFlag(AccessTokenID token) { return Identity().TokenType(token); }
int AccessTokenKit::VerifyAccessToken(AccessTokenID token, const std::string &permission)
{ return Identity().Verify(token, permission); }
bool TokenIdKit::IsSystemAppByFullTokenID(uint64_t token) { return Identity().SystemApp(token); }
int32_t PrivacyKit::AddPermissionUsedRecord(uint32_t, const std::string &, int32_t, int32_t)
{ throw std::logic_error("Privacy reporting is outside the primary-slot scenario"); }
}
extern "C" int SetParameter(const char *key, const char *value)
{
    if (!key || !value) throw std::invalid_argument("null parameter payload");
    return Identity().WriteParameter(key, value);
}
