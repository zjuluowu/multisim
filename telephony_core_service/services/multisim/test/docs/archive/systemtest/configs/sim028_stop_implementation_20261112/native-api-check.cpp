#include "tel_lifecycle.h"
void Probe(){auto g=std::make_shared<OHOS::Telephony::TelLifecycle>();auto p=std::make_shared<int>(1);g->Submit(p,[]{},false);g->Submit(p,[]{},true);g->JoinOwnedWork();}
