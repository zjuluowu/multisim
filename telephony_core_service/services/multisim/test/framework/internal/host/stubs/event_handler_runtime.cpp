// Platform base lifecycle only. TelEventHandler/TelEventQueue provide the production transport.
#include "event_handler.h"
#include <stdexcept>
namespace OHOS::AppExecFwk {
EventHandler::EventHandler(const std::shared_ptr<EventRunner> &runner) : runner_(runner) {}
EventHandler::~EventHandler() = default;
void EventHandler::ProcessEvent(const InnerEvent::Pointer &) {}
std::shared_ptr<EventRunner> EventHandler::GetEventRunner() const { return runner_; }
// Required by the real SendTelEvent fallback branch; never silently accept unsupported dispatch.
bool EventHandler::SendEvent(uint32_t, int64_t, int64_t, EventQueue::Priority)
{
    throw std::logic_error("EventRunner-backed dispatch is unsupported by the host");
}
}
