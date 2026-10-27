#pragma once
#include "ffrt.h"

namespace SimHost {
struct SchedulerEvidence {
    unsigned workers;
    unsigned taskFailures;
};
SchedulerEvidence GetSchedulerEvidence();
bool AwaitSchedulerStopped(std::chrono::milliseconds budget);
void StopCallingThreadTasks();
void ResetSchedulerEvidence(); // Requires all queue owners/workers released.
}
