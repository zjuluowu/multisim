#pragma once
#include "ffrt.h"

namespace SimHost {
struct SchedulerEvidence {
    unsigned workers;
    unsigned taskFailures;
};
SchedulerEvidence GetSchedulerEvidence();
struct QueueEvidence {
    uintptr_t address;
    std::string name;
    unsigned pending, running, cancelled;
    int64_t nextDelayMs;
};
// Read-only registry snapshot. Holds queue states transiently, never SIM objects.
std::vector<QueueEvidence> GetQueueEvidence();
// Drain all host queues, including tasks submitted by other tasks. Future delayed
// work counts as pending. False means timeout or a recorded task failure.
bool AwaitSchedulerIdle(std::chrono::milliseconds budget);
bool AwaitSchedulerStopped(std::chrono::milliseconds budget);
void StopCallingThreadTasks();
void ResetSchedulerEvidence(); // Requires all queue owners/workers released.
}
