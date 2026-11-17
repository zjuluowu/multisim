// Link boundary for constructor-only capacity/query slices. These executables
// never call OnInit and therefore never own a SIM graph. Any reached cleanup
// operation is a test configuration error, not a successful fake shutdown.
#include "sim_manager.h"
#include <stdexcept>

namespace OHOS::Telephony {
namespace {
[[noreturn]] void UnexpectedGraph()
{
    throw std::logic_error("UNINITIALIZED_QUERY_SLICE_REACHED_SIM_GRAPH_CLEANUP");
}
}
void MultiSimMonitor::UnRegisterSimNotify() { UnexpectedGraph(); }
void SimFileManager::UnRegisterCoreNotify(const std::shared_ptr<AppExecFwk::EventHandler> &, int)
{
    UnexpectedGraph();
}
std::shared_ptr<IccFile> SimFileManager::GetIccFile() { UnexpectedGraph(); }
void IccFile::UnInit() { UnexpectedGraph(); }
void SimStateManager::UnRegisterCoreNotify(const std::shared_ptr<AppExecFwk::EventHandler> &, int)
{
    UnexpectedGraph();
}
}
