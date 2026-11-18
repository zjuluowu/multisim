#pragma once
#include "sim_manager.h"
#include "ffrt_runtime.h"
#include <iostream>
#include <map>
#include <mutex>
namespace Sim028Host {
struct TelLifetimeSample { uintptr_t address; std::string type; std::weak_ptr<void> object; };
class TelLifetimeEvidence {
    struct Registry { std::mutex mutex; std::map<uintptr_t, TelLifetimeSample> records; };
    static Registry &Data() { static Registry registry; return registry; }
public:
    static void Remember(const char *type, const std::shared_ptr<void> &object)
    {
        auto &r = Data(); std::lock_guard<std::mutex> lock(r.mutex);
        const auto address = reinterpret_cast<uintptr_t>(object.get());
        r.records[address] = {address, type, object};
    }
    static std::vector<TelLifetimeSample> Live()
    {
        auto &r = Data(); std::lock_guard<std::mutex> lock(r.mutex);
        std::vector<TelLifetimeSample> result;
        for (const auto &entry : r.records)
            if (!entry.second.object.expired()) result.push_back(entry.second);
        return result;
    }
};

// Debugger rendezvous only. No production mutation, extra shared ownership, or cleanup.
void LifecycleCheckpoint(const char *stage, OHOS::Telephony::SimManager *manager,
    OHOS::Telephony::ITelRilManager *ril);
inline void DumpQueues(const char *stage)
{
    for (const auto &q : SimHost::GetQueueEvidence())
        std::cout << "LIFE_QUEUE stage=" << stage << " address=" << q.address << " name=" << q.name
            << " pending=" << q.pending << " running=" << q.running
            << " cancelled=" << q.cancelled << " next_ms=" << q.nextDelayMs << '\n';
}
}
