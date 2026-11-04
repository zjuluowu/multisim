// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "contracts/sim_sut.h"
#include <mutex>
namespace SimGuard {
State NormalizeState(int state);
CardKind NormalizeKind(int kind);
struct PlatformSpy {
    std::mutex mutex;
    std::vector<StateNotification> notifications;
    unsigned commonEvents = 0;
};
void BindPlatformSpy(PlatformSpy *spy);
class ScopedPlatformSpy {
public:
    explicit ScopedPlatformSpy(PlatformSpy &spy) { BindPlatformSpy(&spy); }
    ~ScopedPlatformSpy() { BindPlatformSpy(nullptr); }
    ScopedPlatformSpy(const ScopedPlatformSpy &) = delete;
    ScopedPlatformSpy &operator=(const ScopedPlatformSpy &) = delete;
};
}
