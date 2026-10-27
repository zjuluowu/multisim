// Platform base lifecycle only. TelEventHandler/TelEventQueue provide the production transport.
#include "event_handler.h"
namespace OHOS::AppExecFwk {
EventHandler::EventHandler(const std::shared_ptr<EventRunner> &runner) : runner_(runner) {}
EventHandler::~EventHandler() = default;
void EventHandler::ProcessEvent(const InnerEvent::Pointer &) {}
std::shared_ptr<EventRunner> EventHandler::GetEventRunner() const { return runner_; }
// EventRunner-backed dispatch is deliberately not implemented: using it remains a link error.
}
