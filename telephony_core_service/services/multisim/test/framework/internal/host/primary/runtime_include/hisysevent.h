// Host diagnostic sink; records event names, never decides SIM business results.
#pragma once
#include "../../stubs/include/hisysevent.h"
#include <functional>
namespace Sim028Host {
void Diagnostic(const std::string &name);
void ObserveCallingThreadDiagnostics(std::function<void(const std::string &)> observer);
}
namespace OHOS::HiviewDFX {
template<class... Types> int HiSysEvent::Write(const char *, int64_t, const std::string &,
    const std::string &name, EventType, Types...)
{
    Sim028Host::Diagnostic(name);
    return 0;
}
}
