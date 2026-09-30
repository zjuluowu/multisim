#include "external_services_mock.h"
#include <atomic>
#include <stdexcept>
namespace {
std::atomic<SimHost::MockExternalServices *> active{nullptr};
}
namespace SimHost {
ScopedExternalServices::ScopedExternalServices(MockExternalServices &services)
{
    MockExternalServices *expected = nullptr;
    if (!active.compare_exchange_strong(expected, &services))
        throw std::logic_error("External service scenario already installed");
}
ScopedExternalServices::~ScopedExternalServices() { active.store(nullptr); }
}
namespace OHOS::EventFwk {
bool CommonEventManager::PublishCommonEvent(const CommonEventData &data)
{
    return PublishCommonEvent(data, CommonEventPublishInfo());
}
bool CommonEventManager::PublishCommonEvent(const CommonEventData &data, const CommonEventPublishInfo &info,
    const std::shared_ptr<CommonEventSubscriber> &subscriber)
{
    // Ordered-result callbacks and subscriber runtime are not implemented.
    if (subscriber != nullptr) return false;
    auto *services = active.load();
    return services != nullptr && services->Publish(data, info);
}
}
namespace OHOS::DataShare {
std::shared_ptr<DataShareHelper> DataShareHelper::Creator(const sptr<IRemoteObject> &token,
    const std::string &uri, const std::string &extension, int waitTime, bool isSystem)
{
    auto *services = active.load();
    return services == nullptr ? nullptr : services->CreateDataShare(token, uri, extension, waitTime, isSystem);
}
}
