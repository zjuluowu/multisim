// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace SimGuard
{
enum class State
{
    Unknown,
    Absent,
    Locked,
    NotReady,
    Ready
};
enum class CardKind
{
    Unknown,
    Sim,
    Usim
};
enum class Lock
{
    None,
    Pin,
    Puk
};
enum class Unlock
{
    Ok,
    Incorrect,
    Failure
};
struct UnlockResult
{
    int error;
    Unlock result;
    int remaining;
};
} // namespace SimGuard
