#include "parameter.h"
#include "parameter_mock.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
namespace {
std::atomic<unsigned int> reads{0};
SimHost::MockParameterService *active = nullptr;
}
namespace SimHost {
ScopedParameterService::ScopedParameterService(MockParameterService &service)
{
    if (active != nullptr) throw std::logic_error("Parameter scenario already installed");
    reads.store(0);
    active = &service;
}
ScopedParameterService::~ScopedParameterService()
{
    active = nullptr;
    reads.store(0);
}
int CopyParameterValue(const char *text, char *value, unsigned int length)
{
    if (text == nullptr || value == nullptr || length == 0) return -EINVAL;
    const auto size = std::strlen(text);
    if (size >= length) return -ERANGE;
    std::memcpy(value, text, size + 1);
    return static_cast<int>(size);
}
}
extern "C" unsigned int SimHostParameterReads() { return reads.load(); }
extern "C" int GetParameter(const char *key, const char *fallback, char *value, unsigned int length)
{
    if (key == nullptr || fallback == nullptr || value == nullptr || length == 0) return -EINVAL;
    if (active == nullptr) {
        // Explicit process bootstrap input: production global constructors read this
        // before a scenario can install its strict mock. Never infer SIM state.
        const char *slots = std::getenv("SIM_HOST_BOOT_SLOT_COUNT");
        if (slots && std::strcmp(key, "const.telephony.slotCount") == 0) {
            if (std::strcmp(slots, "1") != 0 && std::strcmp(slots, "2") != 0) return -EINVAL;
            return SimHost::CopyParameterValue(slots, value, length);
        }
        return -ENODEV;
    }
    reads.fetch_add(1);
    return active->Read(key, fallback, value, length);
}
// SetParameter remains unsupported for this read-only scenario.
