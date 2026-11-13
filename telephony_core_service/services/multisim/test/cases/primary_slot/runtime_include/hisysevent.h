// Host diagnostic sink; records event names, never decides SIM business results.
#pragma once
#include "../../../framework/host/stubs/include/hisysevent.h"
namespace Sim028Host { void Diagnostic(const std::string &name); }
namespace OHOS::HiviewDFX {
template<class... Types> int HiSysEvent::Write(const char *, int64_t, const std::string &,
    const std::string &name, EventType, Types...)
{
    Sim028Host::Diagnostic(name);
    return 0;
}
}
