#pragma once
#include "real_scenario.h"
namespace Sim028Host {
// Test-owned composition, using only public unregistration APIs while all owners
// are valid. This is NOT a Stop implementation for the opaque SimManager graph.
class OwnedCardGraph {
public:
    std::shared_ptr<SimStateManager> state;
    std::shared_ptr<SimFileManager> files;
    std::shared_ptr<IccFile> records;
    std::shared_ptr<MockTelRilManager> ril;
    bool stopped = false;
    int slot = 0;
    explicit OwnedCardGraph(const std::shared_ptr<MockTelRilManager> &radio, int slotId = 0) : ril(radio), slot(slotId)
    {
        state = std::make_shared<SimStateManager>(ril); state->Init(slot);
        files = SimFileManager::CreateInstance(ril, state); files->Init(slot);
        records = files->GetIccFile();
    }
    void Stop()
    {
        if (stopped) return;
        stopped = true;
        std::cout << "CARD_LIFE before_detach state=" << state.get() << " refs=" << state.use_count()
            << " files=" << files.get() << " refs=" << files.use_count()
            << " records=" << records.get() << " refs=" << records.use_count() << '\n';
        if (records) {
            records->UnInit();
            records->UnRegisterCoreNotify(files, RadioEvent::RADIO_SIM_RECORDS_LOADED);
        }
        state->UnRegisterCoreNotify(files, RadioEvent::RADIO_CARD_TYPE_CHANGE);
        state->UnRegisterCoreNotify(files, RadioEvent::RADIO_SIM_ICCID_LOADED);
        ril->UnRegisterCoreNotify(slot, files, RadioEvent::RADIO_VOICE_TECH_CHANGED);
        ril->UnRegisterCoreNotify(slot, files, RadioEvent::RADIO_ICC_REFRESH);
        std::cout << "CARD_LIFE after_public_detach state_refs=" << state.use_count()
            << " files_refs=" << files.use_count() << " records_refs=" << records.use_count() << '\n';
        files.reset(); records.reset(); state.reset(); ril.reset();
    }
    ~OwnedCardGraph() { Stop(); }
};
}
