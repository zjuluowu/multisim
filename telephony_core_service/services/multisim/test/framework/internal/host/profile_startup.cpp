// Host startup ABI only. Never link this preinit hook into an OHOS target.
#include <cstring>
// LLVM may initialize/truncate its output before main. ELF preinit runs before
// profile constructors; use only libc here, not C++ globals or filesystem code.
#ifndef SIM_PROFILE_DEFAULT
#define SIM_PROFILE_DEFAULT "/data/local/tmp/sim-test-profiles/direct-%p.profraw"
#endif
static char PROFILE_STARTUP_ENV[] = "LLVM_PROFILE_FILE=" SIM_PROFILE_DEFAULT;
static void IsolateProfileBeforeConstructors(int, char **, char **environment)
{
    // libc may not have assigned environ yet. Sanitize the initial ELF envp
    // itself, so later libc/profile initialization sees the safe value.
    constexpr size_t prefix = sizeof("LLVM_PROFILE_FILE=") - 1;
    for (char **entry = environment; entry && *entry; ++entry) {
        if (std::strncmp(*entry, "LLVM_PROFILE_FILE=", prefix) != 0) continue;
        const char *path = *entry + prefix;
        if (path[0] != '/' || !std::strstr(path, "%p")) *entry = PROFILE_STARTUP_ENV;
        return;
    }
}
__attribute__((section(".preinit_array"), used))
static void (*const PROFILE_PREINIT)(int, char **, char **) = IsolateProfileBeforeConstructors;
