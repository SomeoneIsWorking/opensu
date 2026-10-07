// A failed check prints what was expected and exits non-zero.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace iideck::test {

inline void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

inline void near(double actual, double wanted, const char* what, double tolerance = 1e-3) {
    if (std::abs(actual - wanted) > tolerance) {
        std::fprintf(stderr, "FAIL: %s: got %.6f, want %.6f\n", what, actual, wanted);
        std::exit(1);
    }
}

} // namespace iideck::test
