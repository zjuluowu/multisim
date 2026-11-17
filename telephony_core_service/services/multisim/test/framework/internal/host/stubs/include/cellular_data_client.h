// Compile subset of telephony_cellular_data@be17a60e; no actual service connection.
#pragma once
#include "compile_declarations.h"
namespace OHOS::Telephony {
class CellularDataClient : public DelayedRefSingleton<CellularDataClient> {
    DECLARE_DELAYED_REF_SINGLETON(CellularDataClient);
public:
    bool IsConnect();
    int32_t EnableCellularData(bool enable);
};
}
