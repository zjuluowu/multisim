// Explicit vendor ABI boundary for the G2a query test only. No SIM business function is replaced.
#include <cstdint>
#include <cstdlib>
namespace { int32_t input = 0; int calls = 0; }
extern "C" int32_t GetRealSimCountExt(int32_t originalCount)
{
    input = originalCount;
    ++calls;
    const char *value = std::getenv("SIM_G2A_VENDOR_RESULT");
    return value ? std::atoi(value) : -77; // absent scenario configuration is a failure value, never success.
}
extern "C" int SimG2aVendorInput() { return input; }
extern "C" int SimG2aVendorCalls() { return calls; }
