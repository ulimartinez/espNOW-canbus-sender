#pragma once

// Minimal header-only assertion framework for host-side (g++, no Arduino
// dependency) unit tests. Each test .cpp is its own standalone executable
// with its own main() -- see tests/run_tests.sh.

#include <cmath>
#include <cstdio>

static int g_testsRun = 0;
static int g_testsFailed = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        g_testsRun++;                                                    \
        if (!(cond)) {                                                   \
            g_testsFailed++;                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

#define TEST_SUMMARY()                                                          \
    do {                                                                        \
        std::printf("%d/%d checks passed\n", g_testsRun - g_testsFailed, g_testsRun); \
        return g_testsFailed == 0 ? 0 : 1;                                      \
    } while (0)
