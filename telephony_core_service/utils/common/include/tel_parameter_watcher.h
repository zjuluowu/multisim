// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "tel_lifecycle.h"
#include "parameter.h"
namespace OHOS { namespace Telephony {
// Typed external C-callback context. It never owns a business object, and its
// opaque token is never reused or dereferenced after unregister.
template<class Owner> class TelParameterWatcher : public std::enable_shared_from_this<TelParameterWatcher<Owner>> {
    using Self=TelParameterWatcher<Owner>;
    struct Registry {ffrt::mutex mutex;std::atomic<uintptr_t> next{1};std::map<uintptr_t,std::weak_ptr<Self>> values;};
    static Registry &Contexts(){static Registry r;return r;}
public:
    using Action=std::function<void(Owner &,const char *,const char *)>;
    static std::shared_ptr<Self> Create(std::shared_ptr<Owner> owner,std::shared_ptr<TelLifecycle> group,Action action) {
        auto self=std::shared_ptr<Self>(new Self);self->owner_=owner;self->group_=std::move(group);self->action_=std::move(action);
        auto &r=Contexts();self->token_=r.next.fetch_add(1);std::lock_guard<ffrt::mutex> lock(r.mutex);r.values[self->token_]=self;return self;
    }
    bool Register(const std::string &key) {
        TelLifecycle::Lease admission(group_,TelLifecycle::Kind::Registration);if(!admission)return false;
        if(registered_)return true;
        key_=key;registered_=WatchParameter(key.c_str(),Dispatch,reinterpret_cast<void *>(token_))==0;return registered_;
    }
    static void Dispatch(const char *key,const char *value,void *token) {
        std::shared_ptr<Self> self;auto &r=Contexts();
        {std::lock_guard<ffrt::mutex> lock(r.mutex);auto it=r.values.find(reinterpret_cast<uintptr_t>(token));if(it!=r.values.end())self=it->second.lock();}
        if(!self)return;
        TelLifecycle::Lease fence(self->fence_,TelLifecycle::Kind::Callback);if(!fence)return;
        TelLifecycle::Lease callback(self->group_,TelLifecycle::Kind::Callback);if(!callback)return;
        if(auto owner=self->owner_.lock())self->action_(*owner,key,value);
    }
    bool Stop() {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(75);
        if(!fence_->BeginStop(deadline))return false;
        if(!fence_->SealWhenIdle([]{return true;},deadline)){fence_->FinishStop(false);return false;}
        bool ok=!registered_ || RemoveParameterWatcher(key_.c_str(),Dispatch,reinterpret_cast<void *>(token_))==0;
        if(ok)registered_=false;fence_->FinishStop(ok);if(!ok)return false;
        auto &r=Contexts();std::lock_guard<ffrt::mutex> lock(r.mutex);r.values.erase(token_);return true;
    }
private:
    uintptr_t token_=0;std::string key_;bool registered_=false;
    std::weak_ptr<Owner> owner_;std::shared_ptr<TelLifecycle> group_;
    std::shared_ptr<TelLifecycle> fence_=std::make_shared<TelLifecycle>();Action action_;
};
} }
