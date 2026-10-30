#include "g2a_query_probe.h"
#include <dlfcn.h>
#include <stdexcept>
#include <gmock/gmock.h>
#include "core_service_sim.h"
#include "legacy_sim_query_binding.h"
#include "mock_tel_ril_manager.h"
#include "sim_manager.h"
#include "telephony_ext_wrapper.h"

extern "C" unsigned int SimHostParameterReads();
namespace SimHost {
G2aQueryObservation ObserveReadOnlyAttachment(bool vendorBoundaryEnabled)
{
    using namespace OHOS::Telephony;
    G2aQueryObservation seen {};
    void *vendor = nullptr;
    using ObserveVendor = int (*)();
    ObserveVendor vendorInput = nullptr;
    ObserveVendor vendorCalls = nullptr;
    if (vendorBoundaryEnabled) {
        // An explicit external ABI boundary, loaded by the unmodified production wrapper.
        TELEPHONY_EXT_WRAPPER.InitTelephonyExtWrapper();
        vendor = dlopen("libtelephony_ext_service.z.so", RTLD_NOW | RTLD_NOLOAD);
        if (!vendor) { throw std::runtime_error("Controlled vendor boundary was not loaded"); }
        vendorInput = reinterpret_cast<ObserveVendor>(dlsym(vendor, "SimG2aVendorInput"));
        vendorCalls = reinterpret_cast<ObserveVendor>(dlsym(vendor, "SimG2aVendorCalls"));
        if (!vendorInput || !vendorCalls) { throw std::runtime_error("Controlled vendor evidence exports missing"); }
    }
    auto platformRil = std::make_shared<testing::StrictMock<MockTelRilManager>>();
    std::shared_ptr<ITelRilManager> sharedRil = platformRil;
    const auto referencesBeforeManager = sharedRil.use_count();
    auto producer = std::make_shared<SimManager>(sharedRil); // real production constructor; one Manager.
    seen.managerConstructorRetainedRil = sharedRil.use_count() == referencesBeforeManager + 1;
    std::shared_ptr<ISimManager> manager = producer;
    std::weak_ptr<ISimManager> originalLifetime = manager;
    CoreServiceSim core;
    seen.emptyManagerCount = core.GetRealSimCount(); // original macro fallback before Manager publication.
    seen.failedAttach = !core.AttachReadOnlyQueries(sharedRil);
    core.SetSimManager(manager); // the original publication API, prior to private attachment.
    seen.originalCoreCount = core.GetRealSimCount();
    seen.directManagerCount = manager->GetRealSimCount();
    seen.managerIdentityPreserved = core.GetSimManager().get() == manager.get() &&
        !core.GetSimManager().owner_before(manager) && !manager.owner_before(core.GetSimManager());
    seen.nullBindingRejected = !LegacySimQueryBinding::Create(nullptr, sharedRil) &&
        !LegacySimQueryBinding::Create(manager, nullptr);
    auto binding = LegacySimQueryBinding::Create(manager, sharedRil);
    seen.rilIdentityPreserved = binding && binding->MatchesSharedRil(sharedRil) &&
        binding->SharedRil().get() == sharedRil.get();
    seen.managerIdentityPreserved = seen.managerIdentityPreserved && binding->MatchesManager(manager) &&
        binding->Manager().get() == manager.get();
    // Same address without the original ownership block must not match. No second object is constructed.
    auto wrongManagerOwner = std::shared_ptr<ISimManager>(manager.get(), [](ISimManager *) {});
    auto wrongRilOwner = std::shared_ptr<ITelRilManager>(sharedRil.get(), [](ITelRilManager *) {});
    seen.sameAddressDifferentControlBlockRejected = !binding->MatchesManager(wrongManagerOwner) &&
        !binding->MatchesSharedRil(wrongRilOwner);
    seen.bindingCount = binding->Read();
    seen.attached = core.AttachReadOnlyQueries(sharedRil);
    const auto readsBefore = SimHostParameterReads();
    seen.attachedCoreCount = core.GetRealSimCount();
    core.GetRealSimCount();
    seen.zeroConfigurationWasReRead = SimHostParameterReads() > readsBefore;
    binding.reset();
    wrongManagerOwner.reset();
    producer.reset();
    manager.reset();
    core.SetSimManager(nullptr); // must destroy the old attachment as well as the original publication reference.
    seen.resetReleasedOriginalManager = originalLifetime.expired();
    seen.resetCount = core.GetRealSimCount();
    seen.failedAttach = seen.failedAttach && !core.AttachReadOnlyQueries(sharedRil);
    if (vendor) {
        seen.vendorInput = vendorInput();
        seen.vendorCalls = vendorCalls();
        dlclose(vendor); // the original production wrapper still owns its own handle until process exit.
    }
    return seen;
}
}
