#pragma once
#include <functional>
#include <string>
#include <vector>
namespace Sim028Host {
struct WatchedCallback { std::string key; std::function<void()> invoke; };
void EnableWatchBoundary(bool enabled);
std::vector<WatchedCallback> WatchCallbacks();
unsigned ActiveWatchers();
void FailWatcherRegistration(int slot);
void FailWatcherRemoval(bool fail);
struct WatchBoundaryScope {
    WatchBoundaryScope() {EnableWatchBoundary(true);}
    ~WatchBoundaryScope() {EnableWatchBoundary(false);}
};
}
