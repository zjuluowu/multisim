#ifndef SIM_HOST_PARAMETER_MOCK_H
#define SIM_HOST_PARAMETER_MOCK_H
#include <gmock/gmock.h>
namespace SimHost {
// External OHOS parameter service boundary; never mocks SIM business logic.
class MockParameterService {
public:
    MOCK_METHOD(int, Read, (const char *key, const char *fallback, char *value, unsigned int length));
};
// Single-threaded capacity slice: scope must enclose production object lifetime.
class ScopedParameterService {
public:
    explicit ScopedParameterService(MockParameterService &service);
    ~ScopedParameterService();
    ScopedParameterService(const ScopedParameterService &) = delete;
    ScopedParameterService &operator=(const ScopedParameterService &) = delete;
};
int CopyParameterValue(const char *text, char *value, unsigned int length);
}
#endif
