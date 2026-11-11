// Linux external runtime bridges. SIM/CoreManager/Controller implementations are production sources.
#include "sim_manager.h"
#include "pdp_profile_rdb_helper.h"
#include "telephony_data_helper.h"
#include "tel_event_handler.h"
#include "telephony_state_registry_client.h"
#include "parameter.h"
#include "parameters.h"
#include "config_policy_utils.h"
#include "ability_manager_client.h"
#include <stdexcept>
#include <codecvt>
#include <locale>
#include <iostream>
#include "ipc_skeleton.h"
#include "lifecycle/external_runner.h"
#include <mutex>
#include <condition_variable>
namespace Sim028Host {
void Diagnostic(const std::string &name) { std::cout << "DIAGNOSTIC " << name << '\n'; }
}
namespace OHOS {
// Original classes explicitly friend the platform singleton; normal constructors/destructors run.
template<> std::shared_ptr<Telephony::TelephonyDataHelper> DelayedSingleton<Telephony::TelephonyDataHelper>::GetInstance()
{
    static auto instance = std::shared_ptr<Telephony::TelephonyDataHelper>(new Telephony::TelephonyDataHelper,
        [](auto *p) { delete p; });
    return instance;
}
template<> std::shared_ptr<Telephony::PdpProfileRdbHelper> DelayedSingleton<Telephony::PdpProfileRdbHelper>::GetInstance()
{
    static auto instance = std::shared_ptr<Telephony::PdpProfileRdbHelper>(new Telephony::PdpProfileRdbHelper,
        [](auto *p) { delete p; });
    return instance;
}
int32_t IPCSkeleton::GetCallingUid() { return 1001; }
int32_t IPCSkeleton::GetCallingPid() { return 101; }
SystemAbilityManagerClient &SystemAbilityManagerClient::GetInstance()
{ static SystemAbilityManagerClient instance; return instance; }
sptr<ISystemAbilityManager> SystemAbilityManagerClient::GetSystemAbilityManager()
{ static auto instance = sptr<ISystemAbilityManager>(new ISystemAbilityManager); return instance; }
sptr<IRemoteObject> ISystemAbilityManager::GetSystemAbility(int32_t id)
{
    if (id != TELEPHONY_CORE_SERVICE_SYS_ABILITY_ID) return nullptr;
    static auto token = sptr<IRemoteObject>(new IRemoteObject);
    return token; // Creator identity token only: no SA/Binder dispatch.
}
int32_t ISystemAbilityManager::SubscribeSystemAbility(int32_t, const sptr<ISystemAbilityStatusChange> &)
{ return -1; }
int32_t ISystemAbilityManager::UnSubscribeSystemAbility(int32_t, const sptr<ISystemAbilityStatusChange> &)
{ return -1; }
namespace AppExecFwk {
std::shared_ptr<EventRunner> EventRunner::Create(const std::string &, ThreadMode)
{ return std::make_shared<EventRunner>(); }
namespace {
struct RunnerRuntime {
    std::mutex mutex;
    std::condition_variable changed;
    Sim028Host::RunnerState state = Sim028Host::RunnerState::Stopped;
    std::unique_ptr<ffrt::queue> queue;
};
RunnerRuntime &Runtime() { static RunnerRuntime runtime; return runtime; }
thread_local bool onExternalRunner = false;
bool SubmitExternal(std::function<void()> task, int64_t delay)
{
    auto &runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (runtime.state != Sim028Host::RunnerState::Running) return false;
    if (!runtime.queue) runtime.queue = std::make_unique<ffrt::queue>("sim028-platform-runner");
    runtime.queue->submit_h([task = std::move(task)] {
        struct Guard {
            Guard() { onExternalRunner = true; }
            ~Guard() { onExternalRunner = false; }
        } guard;
        task();
    }, ffrt::task_attr().delay(delay * 1000));
    return true;
}
}
bool StartSim028ExternalRunner()
{
    auto &runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (runtime.state == Sim028Host::RunnerState::Stopping) return false;
    runtime.state = Sim028Host::RunnerState::Running;
    return true;
}
Sim028Host::RunnerState Sim028ExternalRunnerState()
{
    auto &runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    return runtime.state;
}
bool StopSim028ExternalRunner()
{
    // Synchronous shutdown belongs to the owning/control thread. A callback
    // cannot join its own worker; fail explicitly without changing the state.
    if (onExternalRunner) return false;
    auto &runtime = Runtime();
    std::unique_ptr<ffrt::queue> retiring;
    {
        std::unique_lock<std::mutex> lock(runtime.mutex);
        runtime.changed.wait(lock, [&] { return runtime.state != Sim028Host::RunnerState::Stopping; });
        if (runtime.state == Sim028Host::RunnerState::Stopped) return true;
        runtime.state = Sim028Host::RunnerState::Stopping;
        retiring = std::move(runtime.queue);
    }
    // Cancel queued work and join in-flight work normally. No admission lock
    // is held while callbacks complete, including callbacks attempting a post.
    retiring.reset();
    {
        std::lock_guard<std::mutex> lock(runtime.mutex);
        runtime.state = Sim028Host::RunnerState::Stopped;
    }
    runtime.changed.notify_all();
    return true;
}
bool EventHandler::SendEvent(InnerEvent::Pointer &event, int64_t delay, EventQueue::Priority priority)
{
    if (auto tel = std::dynamic_pointer_cast<Telephony::TelEventHandler>(shared_from_this()))
        return tel->SendEvent(event, delay, priority);
    if (!event) return false;
    std::weak_ptr<EventHandler> owner = shared_from_this();
    auto envelope = std::make_shared<InnerEvent::Pointer>(std::move(event));
    return SubmitExternal([owner, envelope] { if (auto target = owner.lock()) target->ProcessEvent(*envelope); }, delay);
}
bool EventHandler::PostTask(const std::function<void()> &task, const std::string &, int64_t delay)
{
    return SubmitExternal(task, delay);
}
bool Configuration::AddItem(const std::string &, const std::string &) { return false; }
int32_t AppMgrClient::UpdateConfiguration(const Configuration &) { return -1; }
}
namespace Telephony {
TelephonyStateRegistryClient::TelephonyStateRegistryClient() = default;
TelephonyStateRegistryClient::~TelephonyStateRegistryClient() = default;
int32_t TelephonyStateRegistryClient::UpdateSimState(int32_t, CardType, SimState, LockReason)
{ return -1; }
int32_t TelephonyStateRegistryClient::UpdateIccAccount() { Sim028Host::Diagnostic("ACCOUNT_NOTIFY"); return 0; }
int32_t TelephonyStateRegistryClient::UpdateCfuIndicator(int32_t, bool) { return -1; }
int32_t TelephonyStateRegistryClient::UpdateVoiceMailMsgIndicator(int32_t, bool) { return -1; }
}
namespace system {
std::string GetParameter(const std::string &key, const std::string &fallback)
{
    char value[256] = {};
    int result = ::GetParameter(key.c_str(), fallback.c_str(), value, sizeof(value));
    return result < 0 ? fallback : std::string(value);
}
bool GetBoolParameter(const std::string &key, bool fallback)
{
    auto value = GetParameter(key, fallback ? "true" : "false");
    return value == "true" || value == "1" || value == "yes" || value == "on" || value == "y";
}
bool SetParameter(const std::string &key, const std::string &value)
{ return ::SetParameter(key.c_str(), value.c_str()) == 0; }
template<> int GetIntParameter<int>(const std::string &key, int fallback, int minimum, int maximum)
{
    int value;
    if (!StrToInt(GetParameter(key, ""), value) || value < minimum || value > maximum) return fallback;
    return value;
}
}
namespace AAFwk {
std::shared_ptr<AbilityManagerClient> AbilityManagerClient::GetInstance()
{ static auto instance = std::make_shared<AbilityManagerClient>(); return instance; }
int32_t AbilityManagerClient::StartExtensionAbility(const Want &, sptr<IRemoteObject>, int,
    AppExecFwk::ExtensionAbilityType) { return -1; }
}
}
extern "C" int32_t GetIntParameter(const char *key, int32_t fallback)
{ return OHOS::system::GetIntParameter<int>(key, fallback); }
extern "C" int WatchParameter(const char *, ParameterChgPtr, void *) { return -1; }
extern "C" int RemoveParameterWatcher(const char *, ParameterChgPtr, void *) { return -1; }
extern "C" CfgFiles *GetCfgFilesEx(const char *, int, const char *) { return nullptr; }
extern "C" void FreeCfgFiles(CfgFiles *) {}
// Serialization is not reached by this business slice. Fail explicitly if it is.
namespace OHOS {
#define UNSUPPORTED_PARCEL(ret, method, args) ret Parcel::method args { throw std::logic_error("Host Parcel: " #method " unsupported"); }
UNSUPPORTED_PARCEL(bool, WriteInt32, (int32_t))
UNSUPPORTED_PARCEL(bool, ReadInt32, (int32_t &))
UNSUPPORTED_PARCEL(int32_t, ReadInt32, ())
UNSUPPORTED_PARCEL(bool, WriteInt64, (int64_t))
UNSUPPORTED_PARCEL(int64_t, ReadInt64, ())
UNSUPPORTED_PARCEL(bool, WriteBool, (bool))
UNSUPPORTED_PARCEL(bool, ReadBool, (bool &))
UNSUPPORTED_PARCEL(bool, ReadBool, ())
UNSUPPORTED_PARCEL(bool, WriteString, (const std::string &))
UNSUPPORTED_PARCEL(std::string, ReadString, ())
UNSUPPORTED_PARCEL(bool, ReadString, (std::string &))
UNSUPPORTED_PARCEL(bool, WriteString16, (const std::u16string &))
UNSUPPORTED_PARCEL(std::u16string, ReadString16, ())
UNSUPPORTED_PARCEL(bool, ReadString16, (std::u16string &))
UNSUPPORTED_PARCEL(bool, WriteStringVector, (const std::vector<std::string> &))
UNSUPPORTED_PARCEL(bool, ReadStringVector, (std::vector<std::string> *))
UNSUPPORTED_PARCEL(bool, WriteInt32Vector, (const std::vector<int32_t> &))
UNSUPPORTED_PARCEL(bool, ReadInt32Vector, (std::vector<int32_t> *))
UNSUPPORTED_PARCEL(bool, WriteInt64Vector, (const std::vector<int64_t> &))
UNSUPPORTED_PARCEL(bool, ReadInt64Vector, (std::vector<int64_t> *))
#undef UNSUPPORTED_PARCEL
}

namespace Sim028Host {
bool StartExternalRunner() { return OHOS::AppExecFwk::StartSim028ExternalRunner(); }
bool StopExternalRunner() { return OHOS::AppExecFwk::StopSim028ExternalRunner(); }
RunnerState ExternalRunnerState() { return OHOS::AppExecFwk::Sim028ExternalRunnerState(); }
}

namespace Sim028Host {
__attribute__((noinline)) void LifecycleCheckpoint(const char *stage,
    OHOS::Telephony::SimManager *manager, OHOS::Telephony::ITelRilManager *ril)
{
    std::cout << "LIFE_CHECKPOINT stage=" << stage << " manager=" << manager << " ril=" << ril << '\n';
}
}
