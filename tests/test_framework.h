/// @file test_framework.h
/// @brief A tiny unit-test framework, so the project needs no test library.
///
/// Usage:
/// @code
///   TEST(addition_works) {
///       CHECK(1 + 1 == 2);
///       CHECK_NEAR(0.1f + 0.2f, 0.3f, 1e-6f);
///   }
/// @endcode
/// Every `TEST` registers itself automatically; test_main.cpp runs them all.

#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

namespace ant::test {

/// One registered test: a name and the function to run.
struct TestCase {
    const char* name;
    void (*run)();
};

/// The list of every test in the program.
inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

/// Total number of failed checks so far.
inline int& failureCount() {
    static int count = 0;
    return count;
}

/// Adds a test to the registry when the program starts.
struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

/// Records and prints one failed check.
inline void reportFailure(const char* file, int line, const char* expression) {
    ++failureCount();
    std::printf("    FAILED %s:%d: %s\n", file, line, expression);
}

}  // namespace ant::test

/// Defines and registers a test.
#define TEST(name)                                                    \
    static void name();                                               \
    static const ant::test::Registrar name##_registrar(#name, name);  \
    static void name()

/// Fails the test if `condition` is false. The test keeps running.
#define CHECK(condition)                                                \
    do {                                                                \
        if (!(condition)) {                                             \
            ant::test::reportFailure(__FILE__, __LINE__, #condition);   \
        }                                                               \
    } while (false)

/// Fails the test unless `a` and `b` differ by at most `tolerance`.
#define CHECK_NEAR(a, b, tolerance) CHECK(std::fabs((a) - (b)) <= (tolerance))
