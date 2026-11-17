// Linux Host thread accounting only. Real functions and return codes are
// forwarded unchanged; no cancellation, joining, or SIM state mutation.
#include "ffrt_runtime.h"
#include <pthread.h>
#include <dlfcn.h>
#include <cstdlib>
#include <map>
#include <memory>

namespace {
struct Ticket {
    void *(*entry)(void *);
    void *argument;
    bool detached = false;
    bool finished = false;
};
struct Threads {
    std::mutex mutex;
    std::condition_variable changed;
    std::map<pthread_t, std::shared_ptr<Ticket>> tickets;
    uint64_t revision = 0;
};
Threads &Registry() { static auto *threads = new Threads; return *threads; }
void *Run(void *opaque)
{
    auto ticket = *static_cast<std::shared_ptr<Ticket> *>(opaque);
    delete static_cast<std::shared_ptr<Ticket> *>(opaque);
    struct Completion {
        std::shared_ptr<Ticket> ticket;
        ~Completion()
        {
            auto &threads = Registry();
            std::lock_guard<std::mutex> lock(threads.mutex);
            ticket->finished = true;
            ++threads.revision;
            threads.changed.notify_all();
        }
    } completion{ticket};
    return ticket->entry(ticket->argument);
}
template<class T> T Next(const char *name)
{
    auto symbol = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (!symbol) std::abort(); // unavailable Host runtime is a hard failure
    return symbol;
}
}

extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
    void *(*entry)(void *), void *argument) noexcept
{
    static auto real = Next<decltype(&pthread_create)>("pthread_create");
    auto ticket = std::make_shared<Ticket>(Ticket{entry, argument});
    int detachState = PTHREAD_CREATE_JOINABLE;
    if (attributes && pthread_attr_getdetachstate(attributes, &detachState) == 0)
        ticket->detached = detachState == PTHREAD_CREATE_DETACHED;
    auto holder = new std::shared_ptr<Ticket>(ticket);
    const int result = real(thread, attributes, Run, holder);
    if (result) { delete holder; return result; }
    auto &threads = Registry();
    std::lock_guard<std::mutex> lock(threads.mutex);
    threads.tickets[*thread] = ticket;
    ++threads.revision;
    threads.changed.notify_all();
    return result;
}

extern "C" int pthread_detach(pthread_t thread) noexcept
{
    static auto real = Next<decltype(&pthread_detach)>("pthread_detach");
    std::shared_ptr<Ticket> ticket;
    auto &threads = Registry();
    {
        std::lock_guard<std::mutex> lock(threads.mutex);
        auto found = threads.tickets.find(thread);
        if (found != threads.tickets.end()) ticket = found->second;
    }
    const int result = real(thread);
    if (!result) {
        std::lock_guard<std::mutex> lock(threads.mutex);
        // A terminated native handle can be reused immediately after detach.
        // Update the captured ticket, never a newer thread with the same ID.
        if (ticket) ticket->detached = true;
        ++threads.revision;
        threads.changed.notify_all();
    }
    return result;
}

namespace SimHost {
DetachedThreadEvidence GetDetachedThreadEvidence()
{
    auto &threads = Registry();
    std::lock_guard<std::mutex> lock(threads.mutex);
    unsigned active = 0;
    for (const auto &[id, ticket] : threads.tickets) active += ticket->detached && !ticket->finished;
    return {active, threads.revision};
}
bool AwaitDetachedThreadsIdle(std::chrono::milliseconds budget)
{
    auto &threads = Registry();
    std::unique_lock<std::mutex> lock(threads.mutex);
    return threads.changed.wait_for(lock, budget, [&] {
        for (const auto &[id, ticket] : threads.tickets)
            if (ticket->detached && !ticket->finished) return false;
        return true;
    });
}
}
