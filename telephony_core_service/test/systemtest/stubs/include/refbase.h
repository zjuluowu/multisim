// Declaration-only subset; reference commonlibrary_c_utils@3bdc11e00e7bf6e577cee300ad51045ca9958efd
// base/include/refbase.h:385-420. No OHOS intrusive reference-counting runtime.
#pragma once
namespace OHOS {
class RefBase {
public:
    RefBase();
    virtual ~RefBase();
};
}
