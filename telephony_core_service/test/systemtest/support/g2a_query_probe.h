#ifndef SIM_HOST_G2A_QUERY_PROBE_H
#define SIM_HOST_G2A_QUERY_PROBE_H
namespace SimHost {
struct G2aQueryObservation {
    int emptyManagerCount;
    int directManagerCount;
    int originalCoreCount;
    int attachedCoreCount;
    int bindingCount;
    int resetCount;
    int vendorInput;
    int vendorCalls;
    bool managerConstructorRetainedRil;
    bool nullBindingRejected;
    bool attached;
    bool failedAttach;
    bool managerIdentityPreserved;
    bool rilIdentityPreserved;
    bool sameAddressDifferentControlBlockRejected;
    bool resetReleasedOriginalManager;
    bool zeroConfigurationWasReRead;
};
// Real CoreServiceSim, SimManager constructor/query and EXT execute. This does not call Manager::OnInit.
G2aQueryObservation ObserveReadOnlyAttachment(bool vendorBoundaryEnabled);
}
#endif
