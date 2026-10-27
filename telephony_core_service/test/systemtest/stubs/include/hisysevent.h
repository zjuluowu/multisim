// Compile subset of hiviewdfx_hisysevent@c1534f0d. No diagnostic runtime/success stub.
#pragma once
#include <cstdint>
#include <string>
namespace OHOS::HiviewDFX {
class HiSysEvent {
public:
    struct Domain { inline static constexpr char TELEPHONY[] = "TELEPHONY"; };
    enum EventType { FAULT = 1, STATISTIC = 2, SECURITY = 3, BEHAVIOR = 4 };
    template<class... Types> static int Write(const char *, int64_t, const std::string &,
        const std::string &, EventType, Types...);
};
}
// Uses the verified domain-string overload; compile only, no domain masking/serialization.
#define HiSysEventWrite(domain, eventName, type, ...) \
    OHOS::HiviewDFX::HiSysEvent::Write(__FUNCTION__, __LINE__, domain, eventName, type, ##__VA_ARGS__)
