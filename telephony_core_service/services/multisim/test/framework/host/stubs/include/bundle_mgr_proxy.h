// Minimal local Bundle API declaration, bundle_framework@2fa234a9; no service/proxy implementation.
#pragma once
#include "compile_declarations.h"
namespace OHOS::AppExecFwk {
namespace Constants { inline constexpr int8_t ALL_USERID = -3; }
enum BundleFlag { GET_BUNDLE_DEFAULT = 0x00000000 };
struct ApplicationInfo { bool isSystemApp = false; };
struct BundleInfo { ApplicationInfo applicationInfo; };
class IBundleMgr : public IRemoteBroker {
public:
    virtual bool GetBundleInfo(const std::string &, BundleFlag, BundleInfo &, int32_t) = 0;
};
}
