/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "card/records/iccid_records_runtime.h"

namespace OHOS::Telephony::MultiSim {
IccidRecords DecodeIccid(const std::string &raw)
{
    // Preserve the separate full and decimal views from IccFile.
    IccidRecords result;
    if (raw.size() < 2) { return {raw, raw}; }
    bool decimalFinished = false;
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        const auto high = raw[i + 1];
        const auto low = raw[i];
        result.full += high;
        result.full += low;
        if (decimalFinished || high > '9') {
            decimalFinished = true;
            continue;
        }
        result.decimal += high;
        if (low == 'F') { continue; }
        if (low > '9') {
            decimalFinished = true;
        } else {
            result.decimal += low;
        }
    }
    return result;
}

bool TrustedIdentity(const IccidRecords &records)
{
    const auto padding = records.full.find_first_not_of("0123456789");
    return !records.decimal.empty() && records.full.compare(0, records.decimal.size(), records.decimal) == 0 &&
        (padding == std::string::npos || (padding == records.decimal.size() &&
            records.full.find_first_not_of('F', padding) == std::string::npos));
}
} // namespace OHOS::Telephony::MultiSim
