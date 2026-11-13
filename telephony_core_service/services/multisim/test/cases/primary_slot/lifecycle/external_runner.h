#pragma once
namespace Sim028Host {
enum class RunnerState { Running, Stopping, Stopped };
// Explicit host platform boundary lifecycle, not a production SimManager Stop.
bool StartExternalRunner();
bool StopExternalRunner(); // false on its own callback thread (self-join forbidden).
RunnerState ExternalRunnerState();
}
