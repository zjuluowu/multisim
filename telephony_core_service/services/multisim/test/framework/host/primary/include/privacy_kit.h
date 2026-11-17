#pragma once
#include <cstdint>
#include <string>
namespace OHOS::Security::AccessToken {
class PrivacyKit {
public:
    static int32_t AddPermissionUsedRecord(uint32_t token, const std::string &permission,
        int32_t successCount, int32_t failCount);
};
}
