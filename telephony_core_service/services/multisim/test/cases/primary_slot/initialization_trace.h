#pragma once
#include "ffrt_runtime.h"
#include <atomic>
#include <iostream>
#include <sstream>
#include <mutex>
#include <functional>

namespace Sim028Host {
// Observe the real SimManager success-notification boundary, not a fabricated
// result. The probe only owns a weak bookkeeping state, never SIM objects.
void SetInitializationAccountProbe(std::function<void()> probe);
void SignalInitializationAccountProbe();
inline uint64_t InitRequestId()
{ static std::atomic<uint64_t> next{0}; return ++next; }
inline void InitTrace(const char *stage, int slot, const void *object, uint32_t event = 0,
    uint64_t request = 0, const std::string &detail = "")
{
    static std::mutex output;
    static uint64_t sequence = 0;
    std::lock_guard<std::mutex> lock(output);
    std::cout << "INIT_TRACE seq=" << ++sequence
        << " us=" << std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()
        << " thread=" << std::this_thread::get_id() << " queue=" << SimHost::CurrentQueueName()
        << " stage=" << stage << " slot=" << slot << " object=" << object
        << " event=" << event << " request=" << request << ' ' << detail << '\n';
}
}
