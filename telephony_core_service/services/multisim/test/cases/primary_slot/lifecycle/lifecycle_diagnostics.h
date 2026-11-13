#pragma once
#include "sim_manager.h"
#include "ffrt_runtime.h"
#include <iostream>
namespace Sim028Host {
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
