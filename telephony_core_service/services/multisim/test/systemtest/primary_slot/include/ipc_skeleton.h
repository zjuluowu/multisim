#pragma once
#include <cstdint>
#include <string>
namespace OHOS {
class IPCSkeleton {
public:
    static uint32_t GetCallingTokenID();
    static uint64_t GetCallingFullTokenID();
    static std::string ResetCallingIdentity();
    static bool SetCallingIdentity(std::string &identity, bool flag = false);
};
}
