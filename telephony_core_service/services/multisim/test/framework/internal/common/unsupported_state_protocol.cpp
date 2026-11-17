#include "sim_protocol_test.h"
#include "internal/common/capability.h"
namespace SimGuard {
void StateProtocol::Start(int){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::StartWithRil(int,bool){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Stop(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Recreate(bool){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
StateSnapshot StateProtocol::Observe(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Observe(const char *){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Settle(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Complete(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Change(int,DeviceCard){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::DeviceChange(int,DeviceCard){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::ChangeWhileStopped(int,DeviceCard){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
UnlockResult StateProtocol::UnlockPin(int,const std::string &){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
UnlockResult StateProtocol::UnlockPuk(int,const std::string &,const std::string &){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
int StateProtocol::Refresh(int){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::Notify(int){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
size_t StateProtocol::Pending(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
size_t StateProtocol::RespondAll(bool){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
size_t StateProtocol::RespondMatching(const std::function<bool(int,const DeviceCard &)> &,bool){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::ReplayLast(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::FailPin(bool){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::NextStatus(StatusFault){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::NextPin(PinFault){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
unsigned StateProtocol::StatusRevision(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
bool StateProtocol::AwaitStatusAfter(unsigned){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
bool StateProtocol::AwaitHeldPin(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::ReleasePin(){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
void StateProtocol::SetPukRemaining(int){SkipCapability("UNSUPPORTED: this material has no controlled card-RIL injection boundary");}
}
