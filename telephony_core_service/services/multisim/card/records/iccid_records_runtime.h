/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#ifndef OHOS_TELEPHONY_MULTISIM_ICCID_RECORDS_RUNTIME_H
#define OHOS_TELEPHONY_MULTISIM_ICCID_RECORDS_RUNTIME_H

#include <string>

namespace OHOS::Telephony::MultiSim {
struct IccidRecords {
    std::string full;
    std::string decimal;
};

IccidRecords DecodeIccid(const std::string &raw);
bool TrustedIdentity(const IccidRecords &records);
} // namespace OHOS::Telephony::MultiSim
#endif
