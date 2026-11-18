// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <ffrt.h>
#include <ffrt_inner.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include <exception>
#include <string>
namespace OHOS { namespace Telephony {
// A component-owned admission/task group. The weak context lookup is technical
// logical-task local storage, not an owner registry or business dispatcher.
class TelLifecycle : public std::enable_shared_from_this<TelLifecycle> {
public:
    enum class State { Running, Stopping, Stopped };
    enum class Kind { Read, Business, Registration, Callback };
    struct Key {
        uint64_t task; std::thread::id thread;
        bool operator<(const Key &other) const { return task != other.task ? task < other.task : thread < other.thread; }
    };
    struct Frame { std::weak_ptr<TelLifecycle> owner; bool business; };
    struct Contexts { ffrt::mutex mutex; std::map<Key, std::vector<Frame>> values; };
    static Contexts &ContextsForProcess() { static Contexts registry; return registry; }
    static Key CurrentKey() { auto task = ffrt::this_task::get_id(); return {task, task ? std::thread::id{} : std::this_thread::get_id()}; }
    static std::shared_ptr<TelLifecycle> Current()
    {
        auto &r = ContextsForProcess(); std::lock_guard<ffrt::mutex> lock(r.mutex);
        auto it = r.values.find(CurrentKey()); return it == r.values.end() || it->second.empty() ? nullptr : it->second.back().owner.lock();
    }
    static std::shared_ptr<TelLifecycle> CurrentOrNew() { auto p = Current(); return p ? p : std::make_shared<TelLifecycle>(); }
    bool InContext(bool requireBusiness = false) const
    {
        auto &r = ContextsForProcess(); std::lock_guard<ffrt::mutex> lock(r.mutex);
        auto it = r.values.find(CurrentKey()); if (it == r.values.end()) return false;
        for (auto f = it->second.rbegin(); f != it->second.rend(); ++f)
            if (f->owner.lock().get() == this && (!requireBusiness || f->business)) return true;
        return false;
    }
    class Context {
    public:
        Context(const std::shared_ptr<TelLifecycle> &g, bool business) : key_(CurrentKey())
        { auto &r = ContextsForProcess(); std::lock_guard<ffrt::mutex> lock(r.mutex); r.values[key_].push_back({g,business}); }
        ~Context() { auto &r=ContextsForProcess(); std::lock_guard<ffrt::mutex> lock(r.mutex); auto it=r.values.find(key_); if (it!=r.values.end()) { it->second.pop_back(); if(it->second.empty()) r.values.erase(it); } }
        Context(const Context &)=delete;
    private: Key key_;
    };
    class Lease {
    public:
        Lease(const std::shared_ptr<TelLifecycle> &g, Kind kind) : group_(g)
        {
            bool nested=g->InContext(), business=g->InContext(true);
            if(!nested)g->ReapCompleted();
            { std::lock_guard<ffrt::mutex> lock(g->mutex_);
              bool allow = kind==Kind::Callback ? !g->sealed_ : g->state_==State::Running ||
                  (kind==Kind::Read && nested && !g->sealed_) || (kind==Kind::Business && business && !g->sealed_);
              if (!allow) return; ++g->active_; active_=true; }
            context_=std::make_unique<Context>(g, kind==Kind::Business || business);
        }
        ~Lease() { context_.reset(); if(active_) { std::lock_guard<ffrt::mutex> lock(group_->mutex_); --group_->active_; group_->changed_.notify_all(); } }
        explicit operator bool() const { return active_; }
        Lease(const Lease &)=delete;
    private: std::shared_ptr<TelLifecycle> group_; bool active_=false; std::unique_ptr<Context> context_;
    };
    struct Ticket {
        std::shared_ptr<TelLifecycle> group; bool finished=false;
        explicit Ticket(std::shared_ptr<TelLifecycle> p):group(std::move(p)){}
        void Finish() { std::lock_guard<ffrt::mutex> lock(group->mutex_); if(!finished) {finished=true;--group->active_;group->changed_.notify_all();} }
        ~Ticket(){Finish();}
    };
    std::shared_ptr<Ticket> Reserve()
    {
        bool continuation=InContext(true); std::lock_guard<ffrt::mutex> lock(mutex_);
        if(sealed_ || (state_!=State::Running && !continuation)) return nullptr;
        ++active_; return std::make_shared<Ticket>(shared_from_this());
    }
    template<class Owner, class F> bool Submit(std::shared_ptr<Owner> owner,F fn,bool detached=false)
    {
        auto publisher=Reserve();if(!publisher)return false;
        auto ticket=Reserve(); if(!ticket) return false; auto group=shared_from_this();
        auto action=[owner=std::move(owner),ticket,group,fn=std::move(fn)]() mutable {
            struct Completion {
                std::shared_ptr<Owner> &owner; std::shared_ptr<Ticket> &ticket;
                ~Completion() {owner.reset();ticket->Finish();}
            } completion{owner,ticket};
            Context context(group,true); auto callable=std::move(fn);callable();
        };
        if(detached) {
            auto thread=std::make_unique<ffrt::thread>(std::move(action));
            std::lock_guard<ffrt::mutex> lock(mutex_);threads_.push_back({std::move(thread),ticket});
        } else {
            auto handle=ffrt::submit_h(std::move(action));
            std::lock_guard<ffrt::mutex> lock(mutex_);tasks_.push_back({handle,ticket});
        }
        publisher->Finish();
        return true;
    }
    // The body counter is an admission barrier; scheduler handles additionally
    // fence callable/capture destruction. No lifecycle lock is held while joining.
    void JoinOwnedWork() {
        std::vector<OwnedTask> tasks;std::vector<OwnedThread> threads;
        {std::lock_guard<ffrt::mutex> lock(mutex_);tasks.swap(tasks_);threads.swap(threads_);}
        for(auto &task:tasks)ffrt::wait({task.handle});
        for(auto &thread:threads)if(thread.handle->joinable())thread.handle->join();
    }
    // Reclaim completed scheduler handles at an external admission boundary.
    // Never join from an owned callback: it could be a child of that executor.
    // The maintenance reservation also makes a racing Stop wait for this reap.
    void ReapCompleted() {
        std::vector<OwnedTask> tasks;std::vector<OwnedThread> threads;
        {
            std::lock_guard<ffrt::mutex> lock(mutex_);
            if(state_!=State::Running || active_!=0)return;
            ++active_;
            for(auto it=tasks_.begin();it!=tasks_.end();) {
                if(it->completion.expired()){tasks.push_back(std::move(*it));it=tasks_.erase(it);}else ++it;
            }
            for(auto it=threads_.begin();it!=threads_.end();) {
                if(it->completion.expired()){threads.push_back(std::move(*it));it=threads_.erase(it);}else ++it;
            }
        }
        struct Finish {TelLifecycle *group;~Finish(){std::lock_guard<ffrt::mutex> lock(group->mutex_);--group->active_;group->changed_.notify_all();}} finish{this};
        for(auto &task:tasks)ffrt::wait({task.handle});
        for(auto &thread:threads)if(thread.handle->joinable())thread.handle->join();
    }
    State GetState() const { std::lock_guard<ffrt::mutex> lock(mutex_);return state_; }
    bool Sealed() const { std::lock_guard<ffrt::mutex> lock(mutex_);return sealed_; }
    void Pulse() { std::lock_guard<ffrt::mutex> lock(mutex_);changed_.notify_all(); }
    // Only the component coordinator seals callbacks. Child queue shutdown does not stop siblings.
    bool BeginStop(std::chrono::steady_clock::time_point deadline)
    {
        if(InContext()) return false;
        std::unique_lock<ffrt::mutex> lock(mutex_);
        while(stopper_) {if(!(CurrentKey()<stopKey_) && !(stopKey_<CurrentKey()))return false;
            if(!changed_.wait_until(lock,deadline,[&]{return !stopper_;}))return false;}
        if(state_==State::Stopped) return true;
        state_=State::Stopping;stopper_=true;stopKey_=CurrentKey();changed_.notify_all();return true;
    }
    bool SealWhenIdle(const std::function<bool()> &noPendingBusiness,std::chrono::steady_clock::time_point deadline)
    {
        std::unique_lock<ffrt::mutex> lock(mutex_);
        if(!changed_.wait_until(lock,deadline,[&]{return active_==0 && noPendingBusiness();})) return false;
        sealed_=true; return true;
    }
    void FinishStop(bool success) {std::lock_guard<ffrt::mutex> lock(mutex_);if(success)state_=State::Stopped;stopper_=false;changed_.notify_all();}
private:
    struct OwnedTask {ffrt::task_handle handle;std::weak_ptr<Ticket> completion;};
    struct OwnedThread {std::unique_ptr<ffrt::thread> handle;std::weak_ptr<Ticket> completion;};
    std::vector<OwnedTask> tasks_;
    std::vector<OwnedThread> threads_;
    mutable ffrt::mutex mutex_; ffrt::condition_variable changed_;
    Key stopKey_{};
    State state_=State::Running;bool sealed_=false,stopper_=false;unsigned active_=0;
};

class TelStopOnce {
public:
    template<class F> bool Run(F fn)
    {
        {std::unique_lock<ffrt::mutex> lock(mutex_);if(running_ && !(TelLifecycle::CurrentKey()<owner_) && !(owner_<TelLifecycle::CurrentKey()))return false;
          changed_.wait(lock,[&]{return !running_;});if(stopped_)return true;running_=true;owner_=TelLifecycle::CurrentKey();}
        struct Completion {
            TelStopOnce *owner;bool ok=false;
            ~Completion(){std::lock_guard<ffrt::mutex> lock(owner->mutex_);owner->stopped_=ok;owner->running_=false;owner->changed_.notify_all();}
        } completion{this};
        completion.ok=fn();return completion.ok;
    }
private: TelLifecycle::Key owner_{};ffrt::mutex mutex_;ffrt::condition_variable changed_;bool running_=false,stopped_=false;
};
} }
