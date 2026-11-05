// A failed check prints what was expected and exits non-zero.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace opensu::test {

[[noreturn]] inline void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

/// The value of `value` (an optional or a `net::Result`), or a failed check when it holds none.
template <class T> const auto& need(const T& value, const char* what) {
    if (!value) {
        fail(what);
    }
    return *value;
}

inline void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

inline void near(double actual, double wanted, const char* what, double tolerance = 1e-3) {
    if (std::abs(actual - wanted) > tolerance) {
        std::fprintf(stderr, "FAIL: %s: got %.6f, want %.6f\n", what, actual, wanted);
        std::exit(1);
    }
}

} // namespace opensu::test
