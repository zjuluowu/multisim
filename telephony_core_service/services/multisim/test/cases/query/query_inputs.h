// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sim_query_test.h"
inline const SimHost::QueryInput QUERY_INPUTS[] = {
    {"single", "1", nullptr}, {"dual", "2", nullptr}, {"zero", "0", nullptr},
    {"negative_atoi", "-2", nullptr}, {"malformed_atoi", "invalid", nullptr},
    {"vendor_override", "2", "5"}, {"vendor_result_propagation", "2", "-31"},
};
