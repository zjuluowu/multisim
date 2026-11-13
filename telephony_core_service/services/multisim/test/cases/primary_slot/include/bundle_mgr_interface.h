// Compile-only subset; methods intentionally have no Host implementation.
#pragma once
#include "compile_declarations.h"
namespace OHOS::AppExecFwk {
enum class GetBundleInfoFlag { GET_BUNDLE_INFO_WITH_SIGNATURE_INFO = 0x00000080 };
struct BundleInfo { struct { std::string appIdentifier; } signatureInfo; };
class IBundleMgr : public IRemoteBroker {
public:
    int32_t GetNameForUid(int32_t uid, std::string &name);
    int32_t GetBundleInfoV9(const std::string &name, int flags, BundleInfo &info, int32_t userId);
};
}
