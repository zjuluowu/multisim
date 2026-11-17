#pragma once
#include "internal/common/sim_types.h"
namespace SimGuard
{
struct CardObservation
{
    int slot;
    bool present;
    State state;
    CardKind kind;
    std::string iccid;
};
struct StateNotification
{
    int slot;
    State state;
    CardKind kind;
    Lock reason;
};
struct StateSnapshot
{
    std::vector<CardObservation> cards;
};

enum class StatusFault { None, Malformed, Drop };
enum class PinFault { None, GenericTransportError, Hold };
struct DeviceCard {
    State state=State::Absent;
    CardKind kind=CardKind::Unknown;
    std::string iccid;
    Lock reason=Lock::None;
};
} // namespace SimGuard
