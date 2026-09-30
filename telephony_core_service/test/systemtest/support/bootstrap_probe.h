#pragma once
#include <cstddef>
namespace SimHost {
struct BootstrapObservation {
    int hasCardResult;
    bool hasCard;
    int stateResult;
    int state;
    int accountsResult;
    std::size_t accountsCount;
};
BootstrapObservation ObserveBootstrap();
}
