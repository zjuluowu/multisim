#pragma once
#include "ffrt.h"

namespace SimHost {
struct SchedulerEvidence {
    unsigned workers;
    unsigned taskFailures;
};
SchedulerEvidence GetSchedulerEvidence();
std::string CurrentQueueName();
struct DetachedThreadEvidence { unsigned active; uint64_t revision; };
DetachedThreadEvidence GetDetachedThreadEvidence();
bool AwaitDetachedThreadsIdle(std::chrono::milliseconds budget);
struct QueueEvidence {
    uintptr_t address;
    std::string name;
    unsigned pending, running, cancelled;
    int64_t nextDelayMs;
};
// Read-only registry snapshot. Holds queue states transiently, never SIM objects.
std::vector<QueueEvidence> GetQueueEvidence();
// Test-only late-dispatch injection: copies pending external scheduler closures.
// Does not mutate task status, invoke them, or fabricate a production result.
std::vector<std::function<void()>> CapturePendingActions(const std::string &queueName);
// Drain all host queues, including tasks submitted by other tasks. Future delayed
// work counts as pending. False means timeout or a recorded task failure.
bool AwaitSchedulerIdle(std::chrono::milliseconds budget);
// Fixture admission barrier: drain running and already-due work, leaving future
// production timers intact. This does not cancel tasks or change business state.
bool AwaitSchedulerReadyIdle(std::chrono::milliseconds budget);
bool AwaitSchedulerStopped(std::chrono::milliseconds budget);
void StopCallingThreadTasks();
void ResetSchedulerEvidence(); // Requires all queue owners/workers released.
}
