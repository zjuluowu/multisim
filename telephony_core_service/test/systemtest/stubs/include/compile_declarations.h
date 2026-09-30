// Linux compile-only platform declarations. See docs/dependencies.md.
// Unused APIs have NO implementation; entering them is a link error, never success.
#pragma once
#include <atomic>
#include <list>
#include <array>
#include <set>
#include <typeinfo>
#include <type_traits>
#include <cstring>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <shared_mutex>
#include <pthread.h>
namespace ffrt {
class mutex {
public:
    mutex() { pthread_mutex_init(&value_, nullptr); }
    ~mutex() { pthread_mutex_destroy(&value_); }
    void lock() { pthread_mutex_lock(&value_); }
    void unlock() { pthread_mutex_unlock(&value_); }
    bool try_lock() { return pthread_mutex_trylock(&value_) == 0; }
private:
    pthread_mutex_t value_;
};
class shared_mutex {
public:
    shared_mutex() { pthread_rwlock_init(&value_, nullptr); }
    ~shared_mutex() { pthread_rwlock_destroy(&value_); }
    void lock() { pthread_rwlock_wrlock(&value_); }
    void unlock() { pthread_rwlock_unlock(&value_); }
    void lock_shared() { pthread_rwlock_rdlock(&value_); }
    void unlock_shared() { pthread_rwlock_unlock(&value_); }
    bool try_lock() { return pthread_rwlock_trywrlock(&value_) == 0; }
    bool try_lock_shared() { return pthread_rwlock_tryrdlock(&value_) == 0; }
private:
    pthread_rwlock_t value_;
};
using condition_variable = std::condition_variable_any;
using cv_status = std::cv_status;
void submit(std::function<void()> task);
using thread = std::thread;
}
namespace OHOS {
class Parcel;
class Parcelable { public: virtual ~Parcelable() = default; virtual bool Marshalling(Parcel&) const = 0; };
class Parcel {
public:
    bool WriteInt32(int32_t); bool ReadInt32(int32_t&); int32_t ReadInt32();
    bool WriteBool(bool); bool ReadBool(bool&); bool ReadBool();
    bool WriteString(const std::string&); std::string ReadString(); bool ReadString(std::string&);
    bool WriteString16(const std::u16string&); std::u16string ReadString16(); bool ReadString16(std::u16string&);
    bool WriteString16Vector(const std::vector<std::u16string>&);
    bool ReadString16Vector(std::vector<std::u16string>*);
};
class MessageParcel : public Parcel {};
template<class T> using sptr = std::shared_ptr<T>;
template<class T> using wptr = std::weak_ptr<T>;
class IRemoteObject {
public:
    class DeathRecipient { public: virtual ~DeathRecipient() = default; virtual void OnRemoteDied(const wptr<IRemoteObject>&) = 0; };
    virtual ~IRemoteObject() = default;
};
class IRemoteBroker { public: virtual ~IRemoteBroker() = default; sptr<IRemoteObject> AsObject(); };
class MessageOption;
template<class T> class IRemoteStub : public T { public: virtual int OnRemoteRequest(uint32_t, MessageParcel&, MessageParcel&, MessageOption&); };
template<class T> class IRemoteProxy : public T {};
class MessageOption;
class ISystemAbilityStatusChange { public: virtual ~ISystemAbilityStatusChange() = default; };
class SystemAbilityStatusChangeStub : public ISystemAbilityStatusChange { public: virtual ~SystemAbilityStatusChangeStub() = default; virtual void OnAddSystemAbility(int32_t,const std::string&) = 0; virtual void OnRemoveSystemAbility(int32_t,const std::string&) = 0; };
class ISystemAbilityManager;
class Uri;
namespace AppExecFwk {
class EventRunner;
class EventHandler;
class InnerEvent {
public:
    using Pointer = std::unique_ptr<InnerEvent, void(*)(InnerEvent*)>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static Pointer Get(uint32_t id, int64_t param = 0)
    {
        return Pointer(new InnerEvent(id, param), [](InnerEvent *event) { delete event; });
    }
    template<class T> static Pointer Get(uint32_t id, const std::shared_ptr<T> &object, int64_t param = 0)
    {
        auto event = Get(id, param);
        event->payload_ = std::make_unique<SharedPayload<T>>(object);
        return event;
    }
    template<class T> static Pointer Get(uint32_t id, const std::weak_ptr<T> &object, int64_t param = 0)
    {
        auto event = Get(id, param);
        event->payload_ = std::make_unique<WeakPayload<T>>(object);
        return event;
    }
    template<class T, class D> static Pointer Get(uint32_t id, std::unique_ptr<T, D> &object, int64_t param = 0)
    {
        auto event = Get(id, param);
        event->payload_ = std::make_unique<UniquePayload<T, D>>(std::move(object));
        return event;
    }
    template<class T, class D> static Pointer Get(uint32_t id, std::unique_ptr<T, D> &&object, int64_t param = 0)
    {
        return Get(id, object, param);
    }
    template<class T> std::shared_ptr<T> GetSharedObject() const
    {
        if (auto *shared = dynamic_cast<SharedPayload<T> *>(payload_.get())) return shared->object;
        if (auto *weak = dynamic_cast<WeakPayload<T> *>(payload_.get())) return weak->object.lock();
        return nullptr;
    }
    template<class T, class D = std::default_delete<T>> std::unique_ptr<T, D> GetUniqueObject() const
    {
        if (auto *unique = dynamic_cast<UniquePayload<T, D> *>(payload_.get())) return std::move(unique->object);
        return std::unique_ptr<T, D>();
    }
    void SetOwner(const std::shared_ptr<EventHandler> &owner) { owner_ = owner; }
    std::shared_ptr<EventHandler> GetOwner() const { return owner_.lock(); }
    uint32_t GetInnerEventId() const { return id_; }
    int64_t GetParam() const { return param_; }
    void SetHandleTime(TimePoint time) { time_ = time; }
    TimePoint GetHandleTime() const { return time_; }
private:
    InnerEvent(uint32_t id, int64_t param) : id_(id), param_(param) {}
    struct Payload { virtual ~Payload() = default; };
    template<class T> struct SharedPayload : Payload {
        explicit SharedPayload(std::shared_ptr<T> value) : object(std::move(value)) {}
        std::shared_ptr<T> object;
    };
    template<class T> struct WeakPayload : Payload {
        explicit WeakPayload(std::weak_ptr<T> value) : object(std::move(value)) {}
        std::weak_ptr<T> object;
    };
    template<class T, class D> struct UniquePayload : Payload {
        explicit UniquePayload(std::unique_ptr<T, D> value) : object(std::move(value)) {}
        std::unique_ptr<T, D> object;
    };
    uint32_t id_;
    int64_t param_;
    TimePoint time_ = Clock::now();
    std::weak_ptr<EventHandler> owner_;
    mutable std::unique_ptr<Payload> payload_;
};

enum class ThreadMode { FFRT };
class EventRunner { public: static std::shared_ptr<EventRunner> Create(const std::string&, ThreadMode = ThreadMode::FFRT); };
class EventQueue { public: enum class Priority { IMMEDIATE, HIGH, LOW, IDLE }; };
class EventHandler : public std::enable_shared_from_this<EventHandler> {
public:
    using Priority = EventQueue::Priority;
    explicit EventHandler(const std::shared_ptr<EventRunner>& = nullptr);
    virtual ~EventHandler();
    virtual void ProcessEvent(const InnerEvent::Pointer&);
    std::shared_ptr<EventRunner> GetEventRunner() const;
    bool SendEvent(InnerEvent::Pointer&, int64_t = 0, Priority = Priority::LOW);
    bool SendEvent(uint32_t, int64_t = 0, int64_t = 0, Priority = Priority::LOW);
    bool PostTask(const std::function<void()>&, const std::string& = "", int64_t = 0);
    void RemoveEvent(uint32_t);
    void RemoveTask(const std::string&);
};
}
namespace AAFwk { class Want; }
namespace EventFwk { class CommonEventData; class CommonEventSubscriber; class CommonEventSubscribeInfo; }
namespace system { template<class T> T GetIntParameter(const std::string&, T); bool GetBoolParameter(const std::string&, bool); std::string GetParameter(const std::string&, const std::string&); }
namespace NativeRdb { class ResultSet; }
namespace DataShare { class DataShareHelper; class DataSharePredicates; class DataShareResultSet; class DataShareValuesBucket; }
template<class T> class DelayedSingleton { public: static std::shared_ptr<T> GetInstance(); };
template<class T> class DelayedRefSingleton { public: static T& GetInstance() { static T value; return value; } };
std::string Str16ToStr8(const std::u16string&);
std::u16string Str8ToStr16(const std::string&);
}
// Source consumers require descriptor/singleton declarations, not host IPC behavior.
#define DECLARE_INTERFACE_DESCRIPTOR(value) static const std::u16string& GetDescriptor() { static const std::u16string descriptor(value); return descriptor; }
#define DECLARE_DELAYED_SINGLETON(type) friend class OHOS::DelayedSingleton<type>; type(); ~type()
#define DECLARE_DELAYED_REF_SINGLETON(type) friend class OHOS::DelayedRefSingleton<type>; type(); ~type()
#define DISALLOW_COPY_AND_MOVE(type) type(const type&) = delete; type& operator=(const type&) = delete; type(type&&) = delete; type& operator=(type&&) = delete
struct cJSON;
struct _xmlNode; using xmlNode = _xmlNode; using xmlNodePtr = xmlNode*;
struct _xmlDoc; using xmlDoc = _xmlDoc; using xmlDocPtr = xmlDoc*;

#include "linux_platform_types.h"
