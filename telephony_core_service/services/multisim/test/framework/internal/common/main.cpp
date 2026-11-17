// SPDX-License-Identifier: Apache-2.0
#include <gmock/gmock.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
extern "C" void __llvm_profile_set_filename(const char *) __attribute__((weak));
int main(int argc, char **argv)
{
    // Even direct execution with an unsafe caller environment must not write in
    // the current directory. Build flags also provide an absolute fallback.
    const auto executable = std::filesystem::canonical(argv[0]);
    const auto directory = executable.parent_path() / "profiles";
    const char *requested = std::getenv("LLVM_PROFILE_FILE");
    std::string profile;
    if (requested && std::filesystem::path(requested).is_absolute() &&
        std::string(requested).find("%p") != std::string::npos) profile = requested;
    else profile = (directory / ("direct-" + std::to_string(getpid()) + ".profraw")).string();
    std::filesystem::create_directories(std::filesystem::path(profile).parent_path());
    setenv("LLVM_PROFILE_FILE", profile.c_str(), 1);
    if (__llvm_profile_set_filename) __llvm_profile_set_filename(std::getenv("LLVM_PROFILE_FILE"));
    testing::InitGoogleMock(&argc, argv);
    const auto filter = testing::GTEST_FLAG(filter);
    if (!testing::GTEST_FLAG(list_tests) && filter.find_first_of("*?:") != std::string::npos) {
        std::cerr << "Run one exact GoogleTest per process; use scripts/test.py for groups.\n";
        return 2;
    }
    if (!testing::GTEST_FLAG(list_tests)) {
        bool found = false;
        auto *unit = testing::UnitTest::GetInstance();
        for (int i = 0; i < unit->total_test_suite_count(); ++i) {
            const auto *suite = unit->GetTestSuite(i);
            for (int j = 0; j < suite->total_test_count(); ++j)
                if (std::string(suite->name()) + "." + suite->GetTestInfo(j)->name() == filter) found = true;
        }
        if (!found) { std::cerr << "Selected test does not exist\n"; return 2; }
    }
    return RUN_ALL_TESTS();
}
