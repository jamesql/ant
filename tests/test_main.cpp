/// @file test_main.cpp
/// @brief Runs every registered test and reports the result.

#include <cstdio>

#include "test_framework.h"

/// Returns 0 if every test passed, 1 otherwise (so CTest can tell).
int main() {
    int failedTests = 0;
    for (const ant::test::TestCase& test : ant::test::registry()) {
        const int failuresBefore = ant::test::failureCount();
        test.run();
        const bool passed = ant::test::failureCount() == failuresBefore;
        std::printf("[%s] %s\n", passed ? " ok " : "FAIL", test.name);
        failedTests += passed ? 0 : 1;
    }
    std::printf("\n%zu tests, %d failed\n", ant::test::registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
