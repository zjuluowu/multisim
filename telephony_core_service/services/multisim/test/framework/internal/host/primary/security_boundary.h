#pragma once
#include <gmock/gmock.h>
#include "accesstoken_kit.h"
namespace PrimarySlotHost {
using OHOS::Security::AccessToken::ATokenTypeEnum;
class ExternalIdentity {
public:
    MOCK_METHOD(ATokenTypeEnum, TokenType, (uint32_t));
    MOCK_METHOD(bool, SystemApp, (uint64_t));
    MOCK_METHOD(int, Verify, (uint32_t, const std::string &));
    MOCK_METHOD(int, WriteParameter, (const std::string &, const std::string &));
};
class IdentityScope {
public:
    explicit IdentityScope(ExternalIdentity &identity);
    ~IdentityScope();
};
}
